#pragma once

#include <stdio.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct core;

struct symbol {
    uint64_t vmaddr;
    char *name;
};

struct symbols {
    size_t symc;
    struct symbol *symv;
};

int symbols_open(struct core *core, struct symbols *syms);

#if 0
void symbols_perror(const char *s);
#endif

const struct symbol *symbols_find(const struct symbols *syms, uint64_t vmaddr);

#ifdef __cplusplus
}
#endif
