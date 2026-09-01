/* typew - terminal write-only typewriter.
   Copyright (c) 2026 Josef Patoprsty.
   SPDX-License-Identifier: MIT */

#define _XOPEN_SOURCE 700

#include "buffer.h"
#include "util.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void buffer_init(Buffer *buf) {
    buf->lines = NULL;
    buf->count = 0;
    buf->capacity = 0;
}

void buffer_destroy(Buffer *buf) {
    for (int i = 0; i < buf->count; i++)
        free(buf->lines[i]);
    free(buf->lines);
    buf->lines = NULL;
    buf->count = 0;
    buf->capacity = 0;
}

void buffer_ensure_line(Buffer *buf, int line) {
    while (buf->count <= line) {
        if (buf->count == buf->capacity) {
            int ncap = buf->capacity == 0 ? 64 : buf->capacity * 2;
            char **nl = realloc(buf->lines, (size_t)ncap * sizeof(char *));
            if (!nl)
                exit(EXIT_FAILURE);
            buf->lines = nl;
            buf->capacity = ncap;
        }
        buf->lines[buf->count] = xstrdup("");
        buf->count++;
    }
}

int buffer_line_len(const Buffer *buf, int line) {
    if (line < 0 || line >= buf->count)
        return 0;
    return (int)strlen(buf->lines[line]);
}

void buffer_set_char(Buffer *buf, int line, int col, char c) {
    buffer_ensure_line(buf, line);
    int len = buffer_line_len(buf, line);
    if (col < len) {
        buf->lines[line][col] = c;
    } else {
        char *nl = realloc(buf->lines[line], (size_t)(col + 2));
        if (!nl)
            exit(EXIT_FAILURE);
        buf->lines[line] = nl;
        for (int i = len; i < col; i++)
            buf->lines[line][i] = ' ';
        buf->lines[line][col] = c;
        buf->lines[line][col + 1] = '\0';
    }
}

const char *buffer_line(const Buffer *buf, int line) {
    if (line < 0 || line >= buf->count)
        return "";
    return buf->lines[line];
}

static char *trim_trailing(char *s) {
    int len = (int)strlen(s);
    while (len > 0 && s[len - 1] == ' ')
        len--;
    s[len] = '\0';
    return s;
}

int buffer_load(Buffer *buf, const char *path) {
    /* "rb": explicit binary mode so Cygwin/Windows text-mode CRLF
       translation can't interfere; read_line strips \r itself. */
    FILE *f = fopen(path, "rb");
    if (!f)
        return 0;

    char *line;
    while ((line = read_line(f)) != NULL) {
        buffer_ensure_line(buf, buf->count);
        free(buf->lines[buf->count - 1]);
        buf->lines[buf->count - 1] = line;
    }
    fclose(f);
    return 1;
}

int buffer_save(const Buffer *buf, const char *path, const char *line_ending) {
    /* Write to a temporary file in the same directory, then rename over the
       target. This keeps the save atomic: a crash mid-write can never leave
       a truncated/corrupt file, and the previous good copy survives. */
    size_t pathlen = strlen(path);
    char *tmp = malloc(pathlen + 32);
    if (!tmp)
        return 0;

    int fd = -1;
    for (int i = 0; i < 100 && fd < 0; i++) {
        /* Include a counter so a stale temp file from a crashed run can't
           permanently wedge saves. */
        if (i == 0)
            snprintf(tmp, pathlen + 32, "%s.typew.%d", path, (int)getpid());
        else
            snprintf(tmp, pathlen + 32, "%s.typew.%d.%d",
                     path, (int)getpid(), i);
        fd = open(tmp, O_WRONLY | O_CREAT | O_EXCL, 0600);
    }
    if (fd < 0) {
        free(tmp);
        return 0;
    }
    FILE *f = fdopen(fd, "wb");
    if (!f) {
        close(fd);
        unlink(tmp);
        free(tmp);
        return 0;
    }

    int ok = 1;
    for (int i = 0; i < buf->count; i++) {
        char *writebuf = xstrdup(buf->lines[i]);
        char *t = trim_trailing(writebuf);
        if (fputs(t, f) == EOF || fputs(line_ending, f) == EOF)
            ok = 0;
        free(writebuf);
        if (!ok)
            break;
    }

    if (ok && fflush(f) != 0)
        ok = 0;
    if (ok && fsync(fd) != 0)
        ok = 0;
    if (fclose(f) != 0)
        ok = 0;

    if (ok) {
        if (rename(tmp, path) != 0)
            ok = 0;
    }
    if (!ok)
        unlink(tmp);
    free(tmp);
    return ok;
}

int buffer_empty(const Buffer *buf) {
    for (int i = 0; i < buf->count; i++) {
        if (buf->lines[i][0] != '\0')
            return 0;
    }
    return 1;
}
