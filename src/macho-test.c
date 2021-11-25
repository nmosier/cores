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
}
