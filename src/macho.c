#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <mach-o/loader.h>

#include "macho.h"
#include "util.h"

struct load_command *macho_parse_lc(FILE *f) {
    struct load_command cmd;
    struct load_command *cmdp = NULL;
    fread_one_chk(cmd, f);
    if ((cmdp = malloc(cmd.cmdsize)) == NULL) {
        errfn = "malloc";
        goto error;
    }
    memcpy(cmdp, &cmd, sizeof(cmd));
    fread_chk((char *) (cmdp + 1), cmd.cmdsize - sizeof(cmd), f);
    return cmdp;
    
error:
    free(cmdp);
    return NULL;
}
