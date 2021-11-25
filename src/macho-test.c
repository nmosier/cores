#include <stdlib.h>

#include "core.h"

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "usage: %s <corepath>\n", argv[0]);
        return EXIT_FAILURE;
    }
    
    const char *path = argv[1];
    struct core core;
    if (core_fopen(path, &core) < 0) {
        core_perror("core_fopen");
        return EXIT_FAILURE;
    }
    
    FILE *f = core.vm.f;
    if (fseek(f, 0xbff80fbc, SEEK_SET) < 0) {
        perror("fseek");
        return EXIT_FAILURE;
    }
    
    uint32_t val;
    if (fread(&val, sizeof(val), 1, f) != 1) {
        if (feof(f)) {
            fprintf(stderr, "unexpected EOF\n");
        } else {
            perror("fread");
        }
        return EXIT_FAILURE;
    }
    
    printf("%x\n", val);
}
