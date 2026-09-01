/* typew - terminal write-only typewriter.
   Copyright (c) 2026 Josef Patoprsty.
   SPDX-License-Identifier: MIT */

#ifndef TYPEW_BUFFER_H
#define TYPEW_BUFFER_H

typedef struct {
    char **lines;
    int count;
    int capacity;
} Buffer;

void buffer_init(Buffer *buf);
void buffer_destroy(Buffer *buf);

void buffer_ensure_line(Buffer *buf, int line);

int buffer_line_len(const Buffer *buf, int line);

void buffer_set_char(Buffer *buf, int line, int col, char c);

const char *buffer_line(const Buffer *buf, int line);

int buffer_load(Buffer *buf, const char *path);

int buffer_save(const Buffer *buf, const char *path, const char *line_ending);

int buffer_empty(const Buffer *buf);

#endif
