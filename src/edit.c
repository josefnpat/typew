/* typew - terminal write-only typewriter.
   Copyright (c) 2026 Josef Patoprsty.
   SPDX-License-Identifier: MIT */

#include "edit.h"

#include <string.h>

void editor_init(Editor *e) {
    e->row = 0;
    e->col = 0;
    e->dirty = 0;
    e->bell_fired_row = -1;
}

void editor_open(Editor *e, Buffer *buf, const char *path) {
    if (buffer_load(buf, path)) {
        e->row = buf->count;
        e->col = 0;
    } else {
        buffer_ensure_line(buf, 0);
    }
    e->dirty = 0;
    e->bell_fired_row = -1;
}

KeyResult editor_key(Editor *e, Buffer *buf, const Config *cfg, int key) {
    KeyResult r = {0, 0};
    int margin = cfg->line_length;
    int bell_zone = cfg->line_length - cfg->bell_count;

    if (key == EDIT_KEY_ESC) {
        r.quit = 1;
        return r;
    }

    if (key == EDIT_KEY_UP) {
        if (e->row > 0) {
            e->row--;
            e->bell_fired_row = -1;
        }
        return r;
    }

    if (key == EDIT_KEY_DOWN) {
        e->row++;
        e->bell_fired_row = -1;
        return r;
    }

    if (key == EDIT_KEY_LEFT || key == EDIT_KEY_BACKSPACE) {
        if (e->col > 0) {
            e->col--;
            if (e->col < bell_zone)
                e->bell_fired_row = -1;
        }
        return r;
    }

    if (key == EDIT_KEY_RIGHT) {
        if (e->col < margin)
            e->col++;
        return r;
    }

    if (key == EDIT_KEY_ENTER) {
        e->col = 0;
        e->row += cfg->line_space;
        buffer_ensure_line(buf, e->row);
        e->bell_fired_row = -1;
        return r;
    }

    if (key == EDIT_KEY_RESIZE)
        return r;

    if (key >= 32 && key <= 126) {
        if (cfg->eol_lock && e->col >= cfg->line_length) {
            r.bell = 1;
            return r;
        }

        int len = buffer_line_len(buf, e->row);
        char existing = e->col < len ? buffer_line(buf, e->row)[e->col] : ' ';

        if (existing != ' ') {
            buffer_set_char(buf, e->row, e->col, cfg->overwrite);
        } else {
            buffer_set_char(buf, e->row, e->col, (char)key);
        }
        e->dirty = 1;

        if (e->col < margin)
            e->col++;

        if (cfg->eol_bell && e->col >= bell_zone &&
            e->bell_fired_row != e->row) {
            r.bell = 1;
            e->bell_fired_row = e->row;
        }
    }

    return r;
}
