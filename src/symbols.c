#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include <mach-o/loader.h>
#include <mach-o/nlist.h>
#include <mach-o/stab.h>

#include "symbols.h"
#include "macho.h"
#include "util.h"

static int symbols_open_macho32(FILE *f, struct symbols *syms);

extern _Thread_local const char *errfn;

static int fread_peek(void *restrict ptr, size_t size, size_t nitems, FILE *restrict stream) {
    if (fread(ptr, size, nitems, stream) != nitems) {
        if (feof(stream)) {
            errno = EINVAL;
            errfn = __FUNCTION__;
            goto error;
        } else {
            errfn = "fread";
            goto error;
        }
    }
    
    fseek_chk(stream, -size * nitems, SEEK_CUR);

    return 0;
    
error:
    return -1;
}

void symbols_perror(const char *s) {
    fprintf(stderr, "%s: %s: %s\n", s, errfn, strerror(errno));
}

static void symbol_init(struct symbol *sym) {
    sym->vmaddr = 0;
    sym->name = NULL;
}

static void symbols_init(struct symbols *syms) {
    syms->f = NULL;
    syms->symc = 0;
    syms->symv = NULL;
}

static int symbols_reserve(struct symbols *syms, size_t count) {
    if ((syms->symv = calloc(count, sizeof(struct symbol))) == NULL) {
        return -1;
    }
    return 0;
}

static void symbols_add(struct symbols *syms, const struct symbol *sym) {
    syms->symv[syms->symc++] = *sym;
}

static int symbols_sort_cmp(const struct symbol *a, const struct symbol *b) {
    return a->vmaddr - b->vmaddr;
}

static void symbols_sort(struct symbols *syms) {
    qsort(syms->symv, syms->symc, sizeof(struct symbol), (int (*)(const void *, const void *)) &symbols_sort_cmp);
}

// returns containing function

static int symbols_find_cmp(const uint64_t *vmaddr, const struct symbol *sym) {
    return *vmaddr - sym->vmaddr;
}

const struct symbol *symbols_find(const struct symbols *syms, uint64_t vmaddr) {
    const struct symbol *prev = NULL;
    size_t i;
    for (i = 0; i < syms->symc && syms->symv[i].vmaddr <= vmaddr; ++i) {}

    if (i == 0) {
        return NULL;
    } else {
        return &syms->symv[i - 1];
    }
}

int symbols_open(FILE *f, struct symbols *syms) {
    symbols_init(syms);

    fseek_chk(f, 0, SEEK_SET);

    syms->f = f;
    
    uint32_t magic;
    

    if (fread_peek(&magic, sizeof(magic), 1, f) < 0) {
        goto error;
    }

    if (magic == MH_MAGIC) {
        if (symbols_open_macho32(f, syms) < 0) {
            goto error;
        }
    } else {
        errno = EINVAL;
        errfn = __FUNCTION__;
        goto error;
    }
    
    symbols_sort(syms);
    
    return 0;
    
error:
    return -1;
}

static int symbols_macho32_handle_symtab(FILE *f, struct symbols *syms, struct symtab_command *symtab);
static int symbols_open_macho32(FILE *f, struct symbols *syms) {
    struct mach_header hdr;
    fread_one_chk(hdr, f);
    assert(hdr.magic == MH_MAGIC);
    
    for (size_t i = 0; i < hdr.ncmds; ++i) {
        struct load_command *cmd;
        if ((cmd = macho_parse_lc(f)) == NULL) {
            goto error;
        }
        
        switch (cmd->cmd) {
            case LC_SYMTAB: {
                struct symtab_command *symtab = (struct symtab_command *) cmd;
                fpos_t pos;
                if (fgetpos(f, &pos) < 0) {
                    errfn = "fgetpos";
                    goto error;
                }
                if (symbols_macho32_handle_symtab(f, syms, symtab) < 0) {
                    goto error;
                }
                if (fsetpos(f, &pos) < 0) {
                    errfn = "fsetpos";
                    goto error;
                }
                break;
            }
        }
        
        free(cmd);
    }
    
    return 0;
    
error:
    return -1;
}

static int symbols_macho32_handle_symtab(FILE *f, struct symbols *syms, struct symtab_command *symtab) {
    /* read string table */
    fseek_chk(f, symtab->stroff, SEEK_SET);
    char *strtab;
    if ((strtab = malloc(symtab->strsize)) == NULL) {
        errfn = "malloc";
        goto error;
    }
    fread_chk(strtab, symtab->strsize, f);
    
    fseek_chk(f, symtab->symoff, SEEK_SET);
    if (symbols_reserve(syms, symtab->nsyms) < 0) {
        goto error;
    }
    
    for (size_t i = 0; i < symtab->nsyms; ++i) {
        struct nlist sym;
        fread_one_chk(sym, f);

        // check symbol type
        if ((sym.n_type & N_EXT)) {
            // external, so ignore
            continue;
        }
        switch (sym.n_type & N_TYPE) {
            case N_SECT:
                break;
            default:
                continue;
        }
        
        /* check if stroff is in bounds */
        const size_t strx = sym.n_un.n_strx;
        if (strx >= symtab->strsize) {
            continue;
        }
        
        const char *s = strtab + strx;
        if (strx == 0 || *s == '\0') {
            continue;
        }
        
        if ((sym.n_type & N_STAB)) {
            continue;
        }
        
        // fprintf(stderr, "name=%s desc=%hx sect=%hhx pext=%hhx\n", s, sym.n_desc, sym.n_sect, sym.n_type & N_PEXT);
        
        struct symbol sym_;
        if ((sym_.name = strdup(s)) == NULL) {
            errfn = "strdup";
            goto error;
        }
        sym_.vmaddr = sym.n_value;
        symbols_add(syms, &sym_);
    }
    
    return 0;
    
error:
    return -1;
}
