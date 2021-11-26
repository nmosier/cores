#include <stdlib.h>
#include <mach-o/loader.h>
#include <limits.h>
#include <assert.h>
#include <string.h>

#include "core.h"
#include "symbols.h"
#include "util.h"
#include "bound.h"

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "usage: %s <corepath>\n", argv[0]);
        return EXIT_FAILURE;
    }
    
    const char *path = argv[1];

#if 1
    struct core core;
    if (core_fopen(path, &core) < 0) {
        core_perror("core_fopen");
        return EXIT_FAILURE;
    }
    
    /* try to open executable mapped into core's memory */
    const char *errfn = NULL;
    
    for (size_t i = 0; i < core.segc; ++i) {
        const struct core_segment *seg = &core.segv[i];
        if (seg->prot != (VM_PROT_READ | VM_PROT_EXECUTE)) { continue; }
        
        /* is this an executable? */
        fseek_chk(core.vm.f, seg->vmbase, SEEK_SET);
        uint32_t magic;
        fread_one_chk(magic, core.vm.f);
        if (magic != MH_MAGIC) {
            continue;
        }

        FILE *seg_f;
        if ((seg_f = lbound_open(core.vm.f, seg->vmbase)) == NULL) {
            goto error;
        }
        
        
#if 0
        fseek_chk(seg_f, 0, SEEK_SET);
        fread_one_chk(magic, seg_f);
        
        assert(magic == MH_MAGIC);
        printf("opened executable\n");
        
        struct symbols syms;
        if (symbols_open(seg_f, &syms) < 0) {
            symbols_perror("symbols_open");
            goto error;
        }
        
        for (size_t i = 0; i < min(5, syms.symc); ++i) {
            printf("%s %llx\n", syms.symv[i].name, syms.symv[i].vmaddr);
        }
#else
      
        struct core incore;
        if (core_open(seg_f, &incore) < 0) {
            core_perror("core_open");
            continue;
        }
        
        printf("core opened\n");
        printf("core segs: %d\n", incore.segc);
        
        // print segmetn vmaddrs
        for (size_t i = 0; i < incore.segc; ++i) {
            printf("segment name=%s vmaddr=%08llx vmsize=%08llx\n", incore.segv[i].name, incore.segv[i].vmbase, incore.segv[i].vmsize);
        }
        
#if 0
        struct symbols syms;
        if (symbols_open(incore.vm.f, &syms) < 0) {
            symbols_perror("symbols_open");
            continue;
        }
        
        printf("symbols opened\n");
        printf("syms: %d\n", syms.symc);
        
        for (size_t i = 0; i < syms.symc; ++i) {
            printf("symbol: %s %08llx\n", syms.symv[i].name, syms.symv[i].vmaddr);
        }
#endif
        
#endif
    }
    
    return EXIT_SUCCESS;
    
error:
    perror(errfn);
    return EXIT_FAILURE;
    
#else
    FILE *f;
    if ((f = fopen(path, "r")) == NULL) {
        perror("fopen");
        return EXIT_FAILURE;
    }
    struct symbols syms;
    if (symbols_open(f, &syms) < 0) {
        symbols_perror("symbols_open");
        return EXIT_FAILURE;
    }
    fprintf(stderr, "%zu\n", syms.symc);
    
    for (size_t i = 0; i < syms.symc; ++i) {
        struct symbol *sym = &syms.symv[i];
        printf("name=%s addr=%llx\n", sym->name, sym->vmaddr);
    }
    
    while (1) {
        uint64_t addr;
        if (scanf("%llx", &addr) < 1) {
            break;
        }
        const struct symbol *sym = symbols_find(&syms, addr);
        printf("name=%s addr=%llx\n", sym->name, sym->vmaddr);
    }
    
#endif
    
}
