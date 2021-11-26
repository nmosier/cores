#include <errno.h>
#include <stdlib.h>
#include <assert.h>

#include "bound.h"
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
    const size_t res = fread(buf, 1, rem, bnd->f);
    if (res < rem) {
        if (ferror(bnd->f)) {
            errfn = "fread";
            goto error;
        }
    }
    return res;
    
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
            res = bnd->begin;
            break;
        case SEEK_CUR:
            fgetpos_chk(bnd->f, &res);
            break;
        case SEEK_END:
            res = bnd->end;
            break;
        default:
            errno = EINVAL;
            errfn = "bound_seek";
            goto error;
    }
    res += pos;
    res = bound_clamp(bnd, res);
    fsetpos_chk(bnd->f, &res);
    return res - bnd->begin;
    
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
    FILE *res;
    if ((res = funopen(bnd, (int (*)(void *, char *, int)) bound_read, (int (*)(void *, const char *, int)) bound_write, (fpos_t (*)(void *, fpos_t, int)) bound_seek, (int (*)(void *)) bound_close)) == NULL) {
        goto error;
    }
    return res;
    
error:
    return NULL;
}



struct lbound {
    FILE *f;
    off_t begin;
};

static int lbound_read(struct lbound *lb, char *buf, int size) {
    return fread(buf, 1, size, lb->f);
}

static int lbound_write(struct lbound *lb, const char *buf, int size) {
    return fwrite(buf, 1, size, lb->f);
}

static fpos_t lbound_seek(struct lbound *lb, fpos_t pos, int whence) {
    switch (whence) {
        case SEEK_SET:
            fseek_chk(lb->f, lb->begin + pos, SEEK_SET);
            break;
            
        case SEEK_CUR: {
            fseek_chk(lb->f, pos, SEEK_CUR);
            break;
        }
            
        case SEEK_END: {
            fseek_chk(lb->f, pos, SEEK_END);
            break;
        }
            
        default:
            errno = EINVAL;
            goto error;
    }
    
    long newpos;
    ftell_chk(newpos, lb->f);
    if (newpos < lb->begin) {
        fseek(lb->f, lb->begin, SEEK_SET);
        newpos = lb->begin;
    }
    
    newpos -= lb->begin;
    assert(newpos >= 0);
    return newpos;
    
error:
    return -1;
}

int lbound_close(struct lbound *lb) {
    free(lb);
    return 0;
}

FILE *lbound_open(FILE *f, off_t begin) {
    struct lbound *lb;
    malloc_chk(lb, sizeof(struct lbound));
    
    lb->f = f;
    lb->begin = begin;
    
    FILE *res;
    if ((res = funopen(lb, (int (* _Nullable)(void *, char *, int)) lbound_read, (int (* _Nullable)(void *, const char *, int)) lbound_write, (fpos_t (* _Nullable)(void *, fpos_t, int)) lbound_seek, (int (* _Nullable)(void *)) lbound_close)) == NULL) {
        errfn = "funopen";
        goto error;
    }
    
    return res;
    
error:
    return NULL;
}
