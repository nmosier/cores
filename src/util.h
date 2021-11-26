#pragma once

#include <errno.h>

#define fread_chk(ptr, nitems, file) do { \
if (fread(ptr, sizeof(*ptr), nitems, file) != nitems) { \
if (feof(file)) { \
errfn = __FUNCTION__; \
errno = EINVAL; \
} else { \
errfn = "fread"; \
} \
goto error; \
} \
} while (0)

#define fread_one_chk(var, file) fread_chk(&var, 1, file)

#define ftell_chk(var, file) do { \
if ((var = ftell(file)) < 0) { \
errfn = "ftell"; \
goto error; \
} \
} while (0)

#define fseek_chk(file, offset, whence) do { \
if (fseek(file, offset, whence) < 0) { \
errfn = "fseek"; \
goto error; \
} \
} while (0)

#define fgetpos_chk(file, posp) do { \
if (fgetpos(file, posp) < 0) { \
errfn = "fgetpos"; \
goto error; \
} \
} while (0)

#define fsetpos_chk(file, posp) do { \
if (fsetpos(file, posp) < 0) { \
errfn = "fgetpos"; \
goto error; \
} \
} while (0)

#define malloc_chk(ptr, size) do { \
if ((ptr = malloc(size)) == NULL) { \
errfn = "malloc"; \
goto error; \
} \
} while (0)

#define strdup_chk(ptr, s) do { \
if ((ptr = strdup(s)) == NULL) { \
errfn = "strdup"; \
goto error; \
} \
} while (0)

#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))

extern _Thread_local const char *errfn;
