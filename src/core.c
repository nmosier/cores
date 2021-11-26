#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <assert.h>

#include <mach-o/loader.h>

#include "core.h"
#include "util.h"
#include "macho.h"
#include "bound.h"
#include "symbols.h"

static int core_open_macho32(FILE *f, struct core *core);
static int core_open_macho64(FILE *f, struct core *core);
static int core_open_vm(struct core *core);

_Thread_local const char *errfn = NULL;

static void core_init(struct core *core, FILE *f) {
    core->f    = f;
    core->fmt  = CORE_INVALID;
    core->segc = 0;
    core->segv = NULL;
    core->vm   = NULL;
}

void core_perror(const char *s) {
    fprintf(stderr, "%s: %s: %s\n", s, errfn, strerror(errno));
}

static int core_reserve_segments(struct core *core, size_t count) {
    if ((core->segv = calloc(count, sizeof(struct core_segment))) == NULL) {
        return -1;
    }
    return 0;
}

int core_fopen(const char *path, struct core *core) {
    FILE *f;
    if ((f = fopen(path, "r")) == NULL) {
        errfn = "fopen";
        goto error;
    }
    int res;
    if ((res = core_open(f, core, NULL)) < 0) {
        fclose(f);
    }
    return res;
    
error:
    return -1;
}

int core_open(FILE *f, struct core *core, FILE *vm) {
    core_init(core, f);
    if (fseek(f, 0, SEEK_SET) < 0) {
        errfn = "fseek";
        goto error;
    }
    uint32_t magic;
    fread_one_chk(magic, f);
    fseek_chk(f, -4, SEEK_CUR);
    
    int res;
    if (magic == MH_MAGIC) {
        res = core_open_macho32(f, core);
    } else if (magic == MH_MAGIC_64) {
        res = core_open_macho64(f, core);
    } else {
        errfn = EINVAL;
        goto error;
    }
    
    if (vm == NULL) {
        if (core_open_vm(core) < 0) {
            goto error;
        }
    } else {
        core->vm = vm;
    }
    
    return res;
    
error:
    return -1;
}



static int core_open_macho32(FILE *f, struct core *core) {
    struct mach_header hdr;
    fread_one_chk(hdr, f);
    
#if 0
    if (hdr.filetype != MH_CORE) {
        errfn = __FUNCTION__;
        errno = EINVAL;
        goto error;
    }
#endif
    
    if (core_reserve_segments(core, hdr.ncmds) < 0) {
        goto error;
    }
    
    for (size_t i = 0; i < hdr.ncmds; ++i) {
        struct load_command *cmd;
        if ((cmd = macho_parse_lc(f)) == NULL) {
            goto error;
        }
        switch (cmd->cmd) {
            case LC_SEGMENT: {
                struct core_segment *cseg = &core->segv[core->segc++];
                struct segment_command *mseg = (struct segment_command *) cmd;
                cseg->filebase = mseg->fileoff;
                cseg->filesize = mseg->filesize;
                cseg->vmbase   = mseg->vmaddr;
                cseg->vmsize   = mseg->vmsize;
                cseg->prot     = mseg->initprot;
                strdup_chk(cseg->name, mseg->segname);
                break;
            }
                
            default:
                break;
        }
        
        free(cmd);
    }
    
    return 0;
    
error:
    return -1;
}

static int core_open_macho64(FILE *f, struct core *core) {
    struct mach_header_64 hdr;
    fread_one_chk(hdr, f);
    
#if 0
    if (hdr.filetype != MH_CORE) {
        errfn = __FUNCTION__;
        errno = EINVAL;
        goto error;
    }
#endif
    
    if (core_reserve_segments(core, hdr.ncmds) < 0) {
        goto error;
    }
    
    for (size_t i = 0; i < hdr.ncmds; ++i) {
        struct load_command *cmd;
        if ((cmd = macho_parse_lc(f)) == NULL) {
            goto error;
        }
        
        switch (cmd->cmd) {
            case LC_SEGMENT_64: {
                struct core_segment *cseg = &core->segv[core->segc++];
                struct segment_command_64 *mseg = (struct segment_command_64 *) cmd;
                cseg->filebase = mseg->fileoff;
                cseg->filesize = mseg->filesize;
                cseg->vmbase   = mseg->vmaddr;
                cseg->vmsize   = mseg->vmsize;
                cseg->prot     = mseg->initprot;
                strdup_chk(cseg->name, mseg->segname);
                break;
            }
        }
        
        free(cmd);
    }
    
    return 0;
    
error:
    return -1;
}

// find segment containing vmaddr
static struct core_segment *core_find_vmaddr(struct core *core, uint64_t vmaddr) {
    for (size_t i = 0; i < core->segc; ++i) {
        struct core_segment *seg = &core->segv[i];
        if (seg->vmbase <= vmaddr && vmaddr < seg->vmbase + seg->vmsize) {
            return seg;
        }
    }
    return NULL;
}

/* create vm file using funopen(3) */
typedef int core_vm_read_t(void *, char *, int);
typedef fpos_t core_vm_seek_t(void *, fpos_t, int);

struct core_vm {
    struct core *core;
    fpos_t pos;
};


static int core_vm_read(struct core_vm *vm, char *buf, int size) {
    FILE *core_f = vm->core->f;
    fpos_t *vmaddr = &vm->pos;
    
    int total = 0;
    while (size > 0) {
        /* find segment containing vm_addr */
        struct core_segment *seg;
        if ((seg = core_find_vmaddr(vm->core, *vmaddr)) == NULL) {
            break;
        }
        const uint64_t offset = *vmaddr - seg->vmbase;
        if (offset >= seg->filesize) {
            fprintf(stderr, "%s: internal error: filesize < vmsize\n", __FUNCTION__);
            abort();
        }
        const uint64_t fileoff = seg->filebase + offset;
        fseek_chk(core_f, fileoff, SEEK_SET);
        const int bytes_read = min(seg->filesize - offset, size);
        fread_chk(buf, bytes_read, core_f);
        
        size -= bytes_read;
        total += bytes_read;
        *vmaddr += bytes_read;
    }
    
    return total;
    
error:
    return -1;
}

static fpos_t core_vm_seek(struct core_vm *vm, fpos_t pos, int whence) {
    switch (whence) {
        case SEEK_SET:
            vm->pos = pos;
            break;
            
        case SEEK_CUR:
            vm->pos += pos;
            break;
            
        case SEEK_END:
        default:
            errno = EINVAL;
            goto error;
    }
    
    return vm->pos;
    
error:
    return -1;
}

static int core_open_vm(struct core *core) {
    struct core_vm *vm;
    malloc_chk(vm, sizeof(*vm));
    vm->core = core;
    vm->pos = 0;
    
    if ((core->vm = funopen(vm, (core_vm_read_t *) &core_vm_read, NULL, (core_vm_seek_t *) &core_vm_seek, NULL)) == NULL) {
        errfn = "funopen";
        goto error;
    }
    
    return 0;

error:
    return -1;
}


// TODO: get rid of this>?
off_t core_ftovm(const struct core *core, off_t fileoff) {
    for (size_t i = 0; i < core->segc; ++i) {
        const struct core_segment *seg = &core->segv[i];
        if (seg->filebase <= fileoff && fileoff < seg->filebase + seg->filesize) {
            fprintf(stderr, "%s: match: filebase=%08llx vmbase=%08llx\n", __FUNCTION__, seg->filebase, seg->vmbase);
            return seg->vmbase + (fileoff - seg->filebase);
        }
    }
    errfn = __FUNCTION__;
    errno = ERANGE;
    return -1;
}


ssize_t core_symbols(const struct core *core, char ***symvecp) {
    free(*symvecp);
    
    size_t count = 0;
    for (size_t i = 0; i < core->segc; ++i) {
        const struct core_segment *seg = &core->segv[i];
        
        if (seg->prot != (VM_PROT_READ | VM_PROT_EXECUTE)) {
            continue;
        }
        
        FILE *seg_f;
        if ((seg_f = lbound_open(core->vm, seg->vmbase)) == NULL) {
            goto error;
        }
        
        struct core incore;
        if (core_open(seg_f, &incore, seg_f) < 0) {
            continue;
        }
        
        struct symbols syms;
        if (symbols_open(&incore, &syms) < 0) {
            continue;
        }
        
        const size_t newcount = count + syms.symc;
        if ((*symvecp = reallocf(*symvecp, sizeof(char *) * newcount)) == NULL) {
            goto error;
        }
        
        for (size_t i = 0; i < syms.symc; ++i) {
            (*symvecp)[count + i] = syms.symv[i].name;
        }
        
        count = newcount;
    }
    
    return count;
    
error:
    return -1;
}
