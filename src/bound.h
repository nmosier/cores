#pragma once

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

FILE *bound_open(FILE *f, off_t begin, off_t end);
FILE *lbound_open(FILE *f, off_t begin);

#ifdef __cplusplus
}
#endif
