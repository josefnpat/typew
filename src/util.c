/* typew - terminal write-only typewriter.
   Copyright (c) 2026 Josef Patoprsty.
   SPDX-License-Identifier: MIT */

#define _XOPEN_SOURCE 700

#include "util.h"

#include <stdlib.h>
#include <string.h>

char *xstrdup(const char *s) {
    char *p = strdup(s);
    if (!p)
        exit(EXIT_FAILURE);
    return p;
}

char *read_line(FILE *f) {
    size_t cap = 0, len = 0;
    char *buf = NULL;
    int c;
    while ((c = fgetc(f)) != EOF && c != '\n') {
        if (len + 1 >= cap) {
            cap = cap == 0 ? 64 : cap * 2;
            char *nbuf = realloc(buf, cap);
            if (!nbuf) {
                free(buf);
                return NULL;
            }
            buf = nbuf;
        }
        buf[len++] = (char)c;
    }
    if (c == EOF && len == 0) {
        free(buf);
        return NULL;
    }
    if (buf)
        buf[len] = '\0';
    char *out = xstrdup(buf ? buf : "");
    free(buf);
    if (len > 0 && out[len - 1] == '\r')
        out[len - 1] = '\0';
    return out;
}
