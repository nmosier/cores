#pragma once

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

FILE *bound_open(FILE *f, off_t begin, off_t end);

#ifdef __cplusplus
}
#endif
