#pragma once

#include <stdio.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct symbol {
    uint64_t vmaddr;
    char *name;
};

struct symbols {
    FILE *f;
    size_t symc;
    struct symbol *symv;
};

int symbols_open(FILE *f, struct symbols *syms);

void symbols_perror(const char *s);

const struct symbol *symbols_find(const struct symbols *syms, uint64_t vmaddr);

#ifdef __cplusplus
}
#endif
