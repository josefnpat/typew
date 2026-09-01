/* typew - terminal write-only typewriter.
   Copyright (c) 2026 Josef Patoprsty.
   SPDX-License-Identifier: MIT */

#ifndef TYPEW_EDIT_H
#define TYPEW_EDIT_H

#include "buffer.h"
#include "config.h"

enum {
    EDIT_KEY_ESC = 27
};
#define EDIT_KEY_UP       1000
#define EDIT_KEY_DOWN     1001
#define EDIT_KEY_LEFT     1002
#define EDIT_KEY_RIGHT    1003
#define EDIT_KEY_BACKSPACE 1004
#define EDIT_KEY_ENTER    1005
#define EDIT_KEY_RESIZE   1006

typedef struct {
    int row;
    int col;
    int dirty;
    int bell_fired_row;
} Editor;

typedef struct {
    int bell;
    int quit;
} KeyResult;

void editor_init(Editor *e);

void editor_open(Editor *e, Buffer *buf, const char *path);

KeyResult editor_key(Editor *e, Buffer *buf, const Config *cfg, int key);

#endif
