#pragma once

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>


struct core_segment {
    uint64_t filebase;
    uint64_t filesize;
    uint64_t vmbase;
    uint64_t vmsize;
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
};

int core_fopen(const char *path, struct core *core);
int core_open(FILE *f, struct core *core);

void core_perror(const char *s);
