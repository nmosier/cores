#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <assert.h>

#include <mach-o/loader.h>

#include "core.h"


#define fread_chk(ptr, nitems, file) do { \
if (fread(ptr, sizeof(*ptr), nitems, file) != nitems) { \
if (feof(file)) { \
errfn = __FUNCTION__; \
errno = EINVAL; \
} else { \
errfn = "fread"; \
} \
goto error; \
} \
} while (0)

#define fread_one_chk(var, file) fread_chk(&var, 1, file)

#define ftell_chk(var, file, err) do { \
if ((var = ftell(file)) < 0) { \
errfn = "ftell"; \
goto error; \
} \
} while (0)

#define fseek_chk(file, offset, whence, err) do { \
if (fseek(file, offset, whence) < 0) { \
errfn = "fseek"; \
goto error; \
} \
} while (0)

#define min(a, b) ((a) < (b) ? (a) : (b))

static int core_open_macho32(FILE *f, struct core *core);
static int core_open_macho64(FILE *f, struct core *core);
static int core_open_vm(struct core *core);

_Thread_local const char *errfn;

static void core_vm_init(struct core_vm *vm) {
    vm->f = NULL;
    vm->pos = 0;
}

static void core_init(struct core *core, FILE *f) {
    core->f    = f;
    core->fmt  = CORE_INVALID;
    core->segc = 0;
    core->segv = NULL;
    core_vm_init(&core->vm);
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
    if ((res = core_open(f, core)) < 0) {
        fclose(f);
    }
    return res;
    
error:
    return -1;
}

int core_open(FILE *f, struct core *core) {
    core_init(core, f);
    if (fseek(f, 0, SEEK_SET) < 0) {
        errfn = "fseek";
        goto error;
    }
    uint32_t magic;
    fread_one_chk(magic, f);
    fseek_chk(f, -4, SEEK_CUR, error);
    
    int res;
    if (magic == MH_MAGIC) {
        res = core_open_macho32(f, core);
    } else if (magic == MH_MAGIC_64) {
        res = core_open_macho64(f, core);
    } else {
        errfn = EINVAL;
        goto error;
    }
    
    if (core_open_vm(core) < 0) {
        goto error;
    }
    
    return res;
    
error:
    return -1;
}

static struct load_command *macho_parse_lc(FILE *f) {
    struct load_command cmd;
    struct load_command *cmdp = NULL;
    fread_one_chk(cmd, f);
    if ((cmdp = malloc(cmd.cmdsize)) == NULL) {
        errfn = "malloc";
        goto error;
    }
    memcpy(cmdp, &cmd, sizeof(cmd));
    fread_chk((char *) (cmdp + 1), cmd.cmdsize - sizeof(cmd), f);
    return cmdp;
    
error:
    free(cmdp);
    return NULL;
}

static int core_open_macho32(FILE *f, struct core *core) {
    struct mach_header hdr;
    fread_one_chk(hdr, f);
    
    if (hdr.filetype != MH_CORE) {
        errfn = __FUNCTION__;
        errno = EINVAL;
        goto error;
    }
    
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
    
    if (hdr.filetype != MH_CORE) {
        errfn = __FUNCTION__;
        errno = EINVAL;
        goto error;
    }
    
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

static int core_vm_read(struct core *core, char *buf, int size) {
    FILE *vm_f = core->vm.f;
    FILE *core_f = core->f;
    fpos_t *vmaddr = &core->vm.pos;
    
    int total = 0;
    while (size > 0) {
        /* find segment containing vm_addr */
        struct core_segment *seg;
        if ((seg = core_find_vmaddr(core, *vmaddr)) == NULL) {
            break;
        }
        const uint64_t offset = *vmaddr - seg->vmbase;
        if (offset >= seg->filesize) {
            fprintf(stderr, "%s: internal error: filesize < vmsize\n", __FUNCTION__);
            abort();
        }
        const uint64_t fileoff = seg->filebase + offset;
        fseek_chk(core->f, fileoff, SEEK_SET, error);
        const int bytes_read = min(seg->filesize - offset, size);
        fread_chk(buf, bytes_read, core->f);
        
        size -= bytes_read;
        total += bytes_read;
        *vmaddr += bytes_read;
    }
    
    return total;
    
error:
    return -1;
}

static fpos_t core_vm_seek(struct core *core, fpos_t pos, int whence) {
    switch (whence) {
        case SEEK_SET:
            core->vm.pos = pos;
            break;
            
        case SEEK_CUR:
            core->vm.pos += pos;
            break;
            
        case SEEK_END:
        default:
            errno = EINVAL;
            goto error;
    }
    
    return core->vm.pos;
    
error:
    return -1;
}

static int core_open_vm(struct core *core) {
    if ((core->vm.f = funopen(core, (core_vm_read_t *) &core_vm_read, NULL, (core_vm_seek_t *) &core_vm_seek, NULL)) == NULL) {
        errfn = "funopen";
        goto error;
    }
    
    return 0;

error:
    return -1;
}
