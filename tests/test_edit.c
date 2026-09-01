/* typew - terminal write-only typewriter.
   Copyright (c) 2026 Josef Patoprsty.
   SPDX-License-Identifier: MIT */

#define _XOPEN_SOURCE 700

#include "../src/config.h"
#include "../src/edit.h"
#include "tests.h"

static Config *new_cfg(void) {
    Config *c = calloc(1, sizeof(Config));
    config_defaults(c);
    return c;
}

static KeyResult type(Editor *e, Buffer *b, const Config *c, const char *s) {
    KeyResult kr;
    for (const char *p = s; *p; p++) {
        kr = editor_key(e, b, c, (unsigned char)*p);
    }
    return kr;
}

/* ------------------------------------------------------------------ */
/* caret position on open                                              */
/* ------------------------------------------------------------------ */
static void test_open_new_file_start_position(void) {
    Editor e;
    Buffer b;
    char *path = test_tmp_path("newfile.txt");
    editor_init(&e);
    buffer_init(&b);
    editor_open(&e, &b, path);
    CHECK_EQ_INT(e.row, 0);
    CHECK_EQ_INT(e.col, 0);
    buffer_destroy(&b);
    free(path);
}

static void test_open_existing_file_below_last(void) {
    /* caret should go one line below the last at the far left */
    char *path = test_tmp_path("open.txt");
    FILE *f = fopen(path, "w");
    fputs("aa\nbb\n", f);
    fclose(f);
    Editor e;
    Buffer b;
    editor_init(&e);
    buffer_init(&b);
    editor_open(&e, &b, path);
    CHECK_EQ_INT(b.count, 2);
    CHECK_EQ_INT(e.row, 2);   /* one below the last */
    CHECK_EQ_INT(e.col, 0);
    buffer_destroy(&b);
    remove(path);
    free(path);
}

static void test_open_existing_empty_file_start_position(void) {
    /* an existing, empty file must behave like a new one (row 0, col 0) */
    char *path = test_tmp_path("open_empty.txt");
    FILE *f = fopen(path, "w");
    fclose(f);
    Editor e;
    Buffer b;
    editor_init(&e);
    buffer_init(&b);
    editor_open(&e, &b, path);
    CHECK_EQ_INT(b.count, 0);
    CHECK_EQ_INT(e.row, 0);
    CHECK_EQ_INT(e.col, 0);
    CHECK_EQ_INT(e.dirty, 0);
    buffer_destroy(&b);
    remove(path);
    free(path);
}

/* ------------------------------------------------------------------ */
/* typing + overwrite rule                                             */
/* ------------------------------------------------------------------ */
static void test_typing_appends(void) {
    Editor e;
    Buffer b;
    editor_init(&e);
    buffer_init(&b);
    Config *c = new_cfg();
    type(&e, &b, c, "hello");
    CHECK_STR(buffer_line(&b, 0), "hello");
    CHECK_EQ_INT(e.col, 5);
    CHECK_EQ_INT(e.dirty, 1);
    config_destroy(c);
    buffer_destroy(&b);
}

static void test_typing_at_blank_column_inserts_spaces(void) {
    Editor e;
    Buffer b;
    editor_init(&e);
    buffer_init(&b);
    Config *c = new_cfg();
    /* move right 10 columns into empty text, then type */
    for (int i = 0; i < 10; i++)
        editor_key(&e, &b, c, EDIT_KEY_RIGHT);
    editor_key(&e, &b, c, 'z');
    CHECK_STR(buffer_line(&b, 0), "          z");   /* 10 spaces + z */
    config_destroy(c);
    buffer_destroy(&b);
}

static void test_overwrite_non_space_uses_marker(void) {
    Editor e;
    Buffer b;
    editor_init(&e);
    buffer_init(&b);
    Config *c = new_cfg();          /* overwrite = '#' */
    type(&e, &b, c, "ab cd");
    /* col 5 -> move left 4 to col 1 ('b', a real char) */
    for (int i = 0; i < 4; i++)
        editor_key(&e, &b, c, EDIT_KEY_LEFT);
    editor_key(&e, &b, c, 'Z');
    CHECK_STR(buffer_line(&b, 0), "a# cd");
    config_destroy(c);
    buffer_destroy(&b);
}

static void test_overwrite_space_prints_typed_char(void) {
    Editor e;
    Buffer b;
    editor_init(&e);
    buffer_init(&b);
    Config *c = new_cfg();
    type(&e, &b, c, "ab cd");
    /* col 5 -> move left 3 to col 2 (the space) */
    for (int i = 0; i < 3; i++)
        editor_key(&e, &b, c, EDIT_KEY_LEFT);
    editor_key(&e, &b, c, 'W');
    CHECK_STR(buffer_line(&b, 0), "abWcd");
    config_destroy(c);
    buffer_destroy(&b);
}

static void test_overwrite_configurable_marker(void) {
    Editor e;
    Buffer b;
    editor_init(&e);
    buffer_init(&b);
    Config c;
    config_defaults(&c);
    c.overwrite = 'X';
    type(&e, &b, &c, "abcde");
    for (int i = 0; i < 5; i++)
        editor_key(&e, &b, &c, EDIT_KEY_LEFT);   /* to col 0 'a' */
    editor_key(&e, &b, &c, 'Q');
    CHECK_STR(buffer_line(&b, 0), "Xbcde");
    config_destroy(&c);
    buffer_destroy(&b);
}

/* ------------------------------------------------------------------ */
/* end-of-line lock: no chars past margin, only bell                  */
/* ------------------------------------------------------------------ */
static void test_lock_stops_past_line_length(void) {
    Editor e;
    Buffer b;
    editor_init(&e);
    buffer_init(&b);
    Config c;
    config_defaults(&c);       /* line_length 80, lock always on */
    c.eol_bell = 0;            /* isolate the lock's own beep */
    int bells = 0;
    for (int i = 0; i < 85; i++) {
        KeyResult kr = editor_key(&e, &b, &c, 'a');
        if (kr.bell)
            bells++;
    }
    /* exactly 80 chars land, no overwrite marker, nothing past margin */
    CHECK_EQ_INT(buffer_line_len(&b, 0), 80);
    CHECK(buffer_line(&b, 0)[79] == 'a');          /* no '#' at the end */
    CHECK_EQ_INT(bells, 5);                        /* 5 locked attempts */
    config_destroy(&c);
    buffer_destroy(&b);
}

/* ------------------------------------------------------------------ */
/* eol warning bell: rings exactly once per line at bell_count         */
/* before the end                                                      */
/* ------------------------------------------------------------------ */
static void test_bell_once_per_line(void) {
    Editor e;
    Buffer b;
    editor_init(&e);
    buffer_init(&b);
    Config c;
    config_defaults(&c);       /* length 80, bell_count 8, eol_bell on */
    int bells = 0;
    for (int i = 0; i < 80; i++) {
        KeyResult kr = editor_key(&e, &b, &c, 'a');
        if (kr.bell)
            bells++;
    }
    /* bell fires once at col 72 (8 before the last col 79), not per key */
    CHECK_EQ_INT(bells, 1);
    config_destroy(&c);
    buffer_destroy(&b);
}

static void test_bell_reset_on_new_line(void) {
    Editor e;
    Buffer b;
    editor_init(&e);
    buffer_init(&b);
    Config c;
    config_defaults(&c);
    int bells = 0;
    for (int i = 0; i < 80; i++) {
        KeyResult kr = editor_key(&e, &b, &c, 'a');
        if (kr.bell)
            bells++;
    }
    editor_key(&e, &b, &c, EDIT_KEY_ENTER);   /* new line, resets bell */
    for (int i = 0; i < 80; i++) {
        KeyResult kr = editor_key(&e, &b, &c, 'a');
        if (kr.bell)
            bells++;
    }
    CHECK_EQ_INT(bells, 2);   /* one per line */
    config_destroy(&c);
    buffer_destroy(&b);
}

static void test_bell_disabled_when_off(void) {
    Editor e;
    Buffer b;
    editor_init(&e);
    buffer_init(&b);
    Config c;
    config_defaults(&c);
    c.eol_bell = 0;
    int bells = 0;
    for (int i = 0; i < 80; i++) {
        KeyResult kr = editor_key(&e, &b, &c, 'a');
        if (kr.bell)
            bells++;
    }
    CHECK_EQ_INT(bells, 0);
    config_destroy(&c);
    buffer_destroy(&b);
}

static void test_movement_resets_bell(void) {
    /* navigating away from a line (up/down/left/backspace) resets
       bell_fired_row so re-entering the zone rings again */
    Editor e;
    Buffer b;
    editor_init(&e);
    buffer_init(&b);
    Config c;
    config_defaults(&c);   /* length 80, bell_count 8 -> zone at col 72 */

    /* each helper moves down a line and back to col 0, then types 72 chars */
    int bells = 0;
    for (int i = 0; i < 72; i++) {
        KeyResult kr = editor_key(&e, &b, &c, 'a');
        if (kr.bell)
            bells++;
    }
    CHECK_EQ_INT(bells, 1);

    editor_key(&e, &b, &c, EDIT_KEY_DOWN);   /* resets */
    for (int i = 0; i < 72; i++)
        editor_key(&e, &b, &c, EDIT_KEY_LEFT);
    for (int i = 0; i < 72; i++) {
        KeyResult kr = editor_key(&e, &b, &c, 'a');
        if (kr.bell)
            bells++;
    }
    CHECK_EQ_INT(bells, 2);

    editor_key(&e, &b, &c, EDIT_KEY_UP);     /* resets */
    for (int i = 0; i < 72; i++)
        editor_key(&e, &b, &c, EDIT_KEY_LEFT);
    for (int i = 0; i < 72; i++) {
        KeyResult kr = editor_key(&e, &b, &c, 'a');
        if (kr.bell)
            bells++;
    }
    CHECK_EQ_INT(bells, 3);

    editor_key(&e, &b, &c, EDIT_KEY_LEFT);   /* col 71 < zone -> resets */
    KeyResult kr = editor_key(&e, &b, &c, 'a');   /* back into zone */
    CHECK_EQ_INT(kr.bell, 1);
    if (kr.bell)
        bells++;
    CHECK_EQ_INT(bells, 4);

    editor_key(&e, &b, &c, EDIT_KEY_BACKSPACE);  /* col 71 < zone -> resets */
    kr = editor_key(&e, &b, &c, 'a');
    CHECK_EQ_INT(kr.bell, 1);
    if (kr.bell)
        bells++;
    CHECK_EQ_INT(bells, 5);

    config_destroy(&c);
    buffer_destroy(&b);
}

/* ------------------------------------------------------------------ */
/* movement / return / line space                                      */
/* ------------------------------------------------------------------ */
static void test_arrow_bounds(void) {
    Editor e;
    Buffer b;
    editor_init(&e);
    buffer_init(&b);
    Config *c = new_cfg();
    /* cannot go above line 1 (row 0) */
    editor_key(&e, &b, c, EDIT_KEY_UP);
    CHECK_EQ_INT(e.row, 0);
    /* can go beyond the last line */
    editor_key(&e, &b, c, EDIT_KEY_DOWN);
    editor_key(&e, &b, c, EDIT_KEY_DOWN);
    CHECK_EQ_INT(e.row, 2);
    /* cannot go past left margin */
    editor_key(&e, &b, c, EDIT_KEY_LEFT);
    CHECK_EQ_INT(e.col, 0);
    /* cannot go past right margin (line_length) */
    for (int i = 0; i < 90; i++)
        editor_key(&e, &b, c, EDIT_KEY_RIGHT);
    CHECK_EQ_INT(e.col, 80);
    config_destroy(c);
    buffer_destroy(&b);
}

static void test_backspace_moves_left_no_delete(void) {
    Editor e;
    Buffer b;
    editor_init(&e);
    buffer_init(&b);
    Config *c = new_cfg();
    type(&e, &b, c, "abcd");
    for (int i = 0; i < 2; i++)
        editor_key(&e, &b, c, EDIT_KEY_BACKSPACE);
    CHECK_EQ_INT(e.col, 2);
    CHECK_STR(buffer_line(&b, 0), "abcd");   /* nothing deleted */
    config_destroy(c);
    buffer_destroy(&b);
}

static void test_return_line_space(void) {
    Editor e;
    Buffer b;
    editor_init(&e);
    buffer_init(&b);
    Config c;
    config_defaults(&c);
    c.line_space = 2;
    type(&e, &b, &c, "line");
    editor_key(&e, &b, &c, EDIT_KEY_ENTER);
    CHECK_EQ_INT(e.row, 2);     /* jumped 2 lines */
    CHECK_EQ_INT(e.col, 0);
    editor_key(&e, &b, &c, 'q');
    CHECK_STR(buffer_line(&b, 2), "q");
    config_destroy(&c);
    buffer_destroy(&b);
}

static void test_return_default_line_space(void) {
    Editor e;
    Buffer b;
    editor_init(&e);
    buffer_init(&b);
    Config *c = new_cfg();      /* line_space 1 */
    editor_key(&e, &b, c, EDIT_KEY_ENTER);
    CHECK_EQ_INT(e.row, 1);
    config_destroy(c);
    buffer_destroy(&b);
}

static void test_non_printable_keys_ignored(void) {
    /* raw control/non-printable values (other than the mapped specials)
       must be ignored without changing the buffer, caret, or dirty flag */
    Editor e;
    Buffer b;
    editor_init(&e);
    buffer_init(&b);
    Config *c = new_cfg();
    type(&e, &b, c, "ab");
    CHECK_EQ_INT(e.col, 2);
    CHECK_EQ_INT(e.dirty, 1);

    KeyResult kr = editor_key(&e, &b, c, 0);
    CHECK_EQ_INT(kr.bell, 0);
    CHECK_EQ_INT(kr.quit, 0);
    editor_key(&e, &b, c, 1);
    editor_key(&e, &b, c, 31);
    editor_key(&e, &b, c, 127);
    editor_key(&e, &b, c, 255);

    CHECK_STR(buffer_line(&b, 0), "ab");
    CHECK_EQ_INT(e.col, 2);
    CHECK_EQ_INT(e.dirty, 1);
    config_destroy(c);
    buffer_destroy(&b);
}

static void test_resize_key_noop(void) {
    Editor e;
    Buffer b;
    editor_init(&e);
    buffer_init(&b);
    Config *c = new_cfg();
    KeyResult kr = editor_key(&e, &b, c, EDIT_KEY_RESIZE);
    CHECK_EQ_INT(kr.bell, 0);
    CHECK_EQ_INT(kr.quit, 0);
    CHECK_EQ_INT(e.row, 0);
    CHECK_EQ_INT(e.col, 0);
    CHECK_EQ_INT(e.dirty, 0);
    config_destroy(c);
    buffer_destroy(&b);
}

static void test_escape_sets_quit(void) {
    Editor e;
    Buffer b;
    editor_init(&e);
    buffer_init(&b);
    Config *c = new_cfg();
    KeyResult kr = editor_key(&e, &b, c, EDIT_KEY_ESC);
    CHECK_EQ_INT(kr.quit, 1);
    config_destroy(c);
    buffer_destroy(&b);
}

/* ------------------------------------------------------------------ */
/* line 31: arrows always move 1 even with a line_space selector > 1  */
/* ------------------------------------------------------------------ */
static void test_arrow_line_7_return_to_9(void) {
    /* doc: move via arrows to line 7, with line_space 2, return -> line 9 */
    Editor e;
    Buffer b;
    editor_init(&e);
    buffer_init(&b);
    Config c;
    config_defaults(&c);
    c.line_space = 2;

    /* arrows move 1 per press regardless of line_space */
    for (int i = 0; i < 7; i++)
        editor_key(&e, &b, &c, EDIT_KEY_DOWN);
    CHECK_EQ_INT(e.row, 7);          /* arrows stepped 1, not 2 */

    editor_key(&e, &b, &c, EDIT_KEY_ENTER);
    CHECK_EQ_INT(e.row, 9);          /* return honors line_space */
    CHECK_EQ_INT(e.col, 0);

    config_destroy(&c);
    buffer_destroy(&b);
}

/* ------------------------------------------------------------------ */
/* line 35: buffer not considered changed by navigation                */
/* ------------------------------------------------------------------ */
static void test_navigation_does_not_dirty(void) {
    Editor e;
    Buffer b;
    editor_init(&e);
    buffer_init(&b);
    Config *c = new_cfg();

    /* arrows, backspace, return all move without marking dirty */
    for (int i = 0; i < 10; i++)
        editor_key(&e, &b, c, EDIT_KEY_RIGHT);
    for (int i = 0; i < 5; i++)
        editor_key(&e, &b, c, EDIT_KEY_LEFT);
    editor_key(&e, &b, c, EDIT_KEY_BACKSPACE);
    for (int i = 0; i < 3; i++)
        editor_key(&e, &b, c, EDIT_KEY_DOWN);
    CHECK_EQ_INT(e.dirty, 0);
    CHECK_EQ_INT(e.row, 3);
    CHECK_EQ_INT(e.col, 4);

    /* inserting the first character marks it dirty */
    editor_key(&e, &b, c, 'x');
    CHECK_EQ_INT(e.dirty, 1);

    config_destroy(c);
    buffer_destroy(&b);
}

/* ------------------------------------------------------------------ */
/* line 38: navigating into empty space, then save, leaves a newline   */
/* ------------------------------------------------------------------ */
static void test_navigate_empty_save_truncates_to_newline(void) {
    char *path = test_tmp_path("nav.txt");
    Editor e;
    Buffer b;
    editor_init(&e);
    buffer_init(&b);
    Config *c = new_cfg();

    editor_open(&e, &b, path);
    CHECK_EQ_INT(e.dirty, 0);
    /* move into empty space, no data created */
    for (int i = 0; i < 10; i++)
        editor_key(&e, &b, c, EDIT_KEY_RIGHT);
    CHECK_EQ_INT(buffer_empty(&b), 1);
    CHECK_EQ_INT(e.dirty, 0);

    /* save: everything is spaces/empty, so only a bare newline is written */
    CHECK_EQ_INT(buffer_save(&b, path, "\n"), 1);
    FILE *f = fopen(path, "r");
    int cbyte = fgetc(f);
    CHECK_EQ_INT(cbyte, '\n');
    fclose(f);
    remove(path);
    free(path);

    config_destroy(c);
    buffer_destroy(&b);
}

void run_edit_tests(void) {
    test_open_new_file_start_position();
    test_open_existing_file_below_last();
    test_open_existing_empty_file_start_position();
    test_typing_appends();
    test_typing_at_blank_column_inserts_spaces();
    test_overwrite_non_space_uses_marker();
    test_overwrite_space_prints_typed_char();
    test_overwrite_configurable_marker();
    test_lock_stops_past_line_length();
    test_bell_once_per_line();
    test_bell_reset_on_new_line();
    test_bell_disabled_when_off();
    test_movement_resets_bell();
    test_arrow_bounds();
    test_backspace_moves_left_no_delete();
    test_return_line_space();
    test_return_default_line_space();
    test_arrow_line_7_return_to_9();
    test_navigation_does_not_dirty();
    test_navigate_empty_save_truncates_to_newline();
    test_non_printable_keys_ignored();
    test_resize_key_noop();
    test_escape_sets_quit();
}

int main(void) {
    TEST_MAIN_SUMMARY(run_edit_tests);
}
