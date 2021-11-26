#pragma once

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

struct load_command *macho_parse_lc(FILE *f);

#ifdef __cplusplus
}
#endif
