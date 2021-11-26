#pragma once

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <mach/vm_prot.h>

#ifdef __cplusplus
extern "C" {
#endif

struct core_segment {
    uint64_t filebase;
    uint64_t filesize;
    uint64_t vmbase;
    uint64_t vmsize;
    vm_prot_t prot;
    char *name;
};

enum core_format {
    CORE_INVALID,
    CORE_MACHO32,
    CORE_MACHO64,
};

struct core {
    FILE *f; // backing file
    enum core_format fmt; // format of core
    size_t segc;
    struct core_segment *segv;
    FILE *vm;
};

int core_fopen(const char *path, struct core *core);
int core_open(FILE *f, struct core *core, FILE *vm); // vm may be null

void core_perror(const char *s);

off_t core_ftovm(const struct core *core, off_t fileoff);

#ifdef __cplusplus
}
#endif
