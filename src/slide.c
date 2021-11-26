#include <errno.h>
#include <stdlib.h>

#include "slide.h"
#include "util.h"

struct bound {
    FILE *f;
    off_t begin;
    off_t end;
};

static int bound_rem(const struct bound *bnd, int size) {
    long pos;
    ftell_chk(pos, bnd->f);
    size = min(bnd->end - pos, size);
    return size;
    
error:
    return -1;
}

static fpos_t bound_clamp(const struct bound *bnd, fpos_t pos) {
    return max(min(pos, bnd->end), bnd->begin);
}

static int bound_read(struct bound *bnd, char *buf, int size) {
    int rem;
    if ((rem = bound_rem(bnd, size)) < 0) {
        goto error;
    }
    return fread(buf, 1, rem, bnd->f);
    
error:
    return -1;
}

static int bound_write(struct bound *bnd, char *buf, int size) {
    int rem;
    if ((rem = bound_rem(bnd, size)) < 0) {
        goto error;
    }
    return fwrite(buf, 1, rem, bnd->f);
    
error:
    return -1;
}

static fpos_t bound_seek(struct bound *bnd, fpos_t pos, int whence) {
    fpos_t res;
    switch (whence) {
        case SEEK_SET:
            res = pos;
            break;
        case SEEK_CUR:
            fgetpos_chk(bnd->f, &res);
            res += pos;
            break;
        case SEEK_END:
            res = bnd->end - pos;
            break;
        default:
            errno = EINVAL;
            errfn = "bound_seek";
            return -1;
    }
    res = bound_clamp(bnd, res);
    fsetpos_chk(bnd->f, &res);
    return res;
    
error:
    return -1;
}

static int bound_close(struct bound *bnd) {
    free(bnd);
    return 0;
}

FILE *bound_open(FILE *f, off_t begin, off_t end) {
    struct bound *bnd;
    malloc_chk(bnd, sizeof(struct bound));
    bnd->f     = f;
    bnd->begin = begin;
    bnd->end   = end;
    FILE *res = funopen(bnd, (int (*)(void *, char *, int)) bound_read, (int (*)(void *, const char *, int)) bound_write, (fpos_t (*)(void *, fpos_t, int)) bound_seek, (int (*)(void *)) bound_close);
    return res;
    
error:
    return NULL;
}
