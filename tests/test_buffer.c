/* typew - terminal write-only typewriter.
   Copyright (c) 2026 Josef Patoprsty.
   SPDX-License-Identifier: MIT */

#define _XOPEN_SOURCE 700

#include "../src/buffer.h"
#include "tests.h"

static int write_file(const char *path, const char *content) {
    FILE *f = fopen(path, "wb");
    if (!f)
        return 0;
    fputs(content, f);
    fclose(f);
    return 1;
}

static char *read_file(const char *path) {
    /* "rb": explicit binary mode so CRLF round-trips identically on
       Cygwin/Windows (where "r" would translate \r\n -> \n). */
    FILE *f = fopen(path, "rb");
    if (!f)
        return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *s = malloc((size_t)n + 1);
    size_t rd = fread(s, 1, (size_t)n, f);
    s[rd] = '\0';
    fclose(f);
    return s;
}

static void test_load_lines(void) {
    char *path = test_tmp_path("load.txt");
    write_file(path, "aa\nbb\ncc\n");
    Buffer b;
    buffer_init(&b);
    CHECK_EQ_INT(buffer_load(&b, path), 1);
    CHECK_EQ_INT(b.count, 3);
    CHECK_STR(buffer_line(&b, 0), "aa");
    CHECK_STR(buffer_line(&b, 1), "bb");
    CHECK_STR(buffer_line(&b, 2), "cc");
    buffer_destroy(&b);
    remove(path);
    free(path);
}

static void test_load_no_trailing_newline(void) {
    char *path = test_tmp_path("nonl.txt");
    write_file(path, "hello");
    Buffer b;
    buffer_init(&b);
    CHECK_EQ_INT(buffer_load(&b, path), 1);
    CHECK_EQ_INT(b.count, 1);
    CHECK_STR(buffer_line(&b, 0), "hello");
    buffer_destroy(&b);
    remove(path);
    free(path);
}

static void test_save_trims_trailing_spaces(void) {
    char *path = test_tmp_path("save.txt");
    Buffer b;
    buffer_init(&b);
    buffer_ensure_line(&b, 0);
    buffer_ensure_line(&b, 1);
    buffer_set_char(&b, 0, 0, 'a');
    buffer_set_char(&b, 0, 1, ' ');
    buffer_set_char(&b, 1, 0, 'b');
    buffer_set_char(&b, 1, 1, ' ');
    buffer_set_char(&b, 1, 2, 'c');
    CHECK_EQ_INT(buffer_save(&b, path, "\n"), 1);
    char *s = read_file(path);
    /* "a\nbc\n": line 0 "a " loses trailing space -> "a";
       line 1 "b c" keeps internal space -> "b c" */
    CHECK_STR(s, "a\nb c\n");
    free(s);
    buffer_destroy(&b);
    remove(path);
    free(path);
}

static void test_save_crlf_line_endings(void) {
    char *path = test_tmp_path("crlf.txt");
    Buffer b;
    buffer_init(&b);
    buffer_ensure_line(&b, 0);
    buffer_set_char(&b, 0, 0, 'x');
    CHECK_EQ_INT(buffer_save(&b, path, "\r\n"), 1);
    char *s = read_file(path);
    CHECK_STR(s, "x\r\n");
    free(s);
    buffer_destroy(&b);
    remove(path);
    free(path);
}

static void test_set_char_grows_with_spaces(void) {
    Buffer b;
    buffer_init(&b);
    buffer_set_char(&b, 0, 9, 'z');   /* col 10 -> 9 spaces then z */
    CHECK_EQ_INT(buffer_line_len(&b, 0), 10);
    CHECK_STR(buffer_line(&b, 0), "         z");
    buffer_destroy(&b);
}

static void test_empty(void) {
    Buffer b;
    buffer_init(&b);
    CHECK_EQ_INT(buffer_empty(&b), 1);
    buffer_ensure_line(&b, 0);
    buffer_set_char(&b, 0, 0, 'a');
    CHECK_EQ_INT(buffer_empty(&b), 0);
    buffer_destroy(&b);
}

static void test_spaces_line_not_empty(void) {
    /* buffer_empty only considers lines whose first byte is NUL as empty;
       a line holding spaces is treated as content. */
    Buffer b;
    buffer_init(&b);
    buffer_set_char(&b, 0, 4, ' ');
    CHECK_EQ_INT(buffer_empty(&b), 0);
    buffer_destroy(&b);
}

static void test_line_len_out_of_range(void) {
    Buffer b;
    buffer_init(&b);
    CHECK_EQ_INT(buffer_line_len(&b, 0), 0);
    CHECK_EQ_INT(buffer_line_len(&b, -1), 0);
    CHECK_STR(buffer_line(&b, 0), "");
    buffer_destroy(&b);
}

static void test_save_is_atomic_no_stray_tmp(void) {
    /* Saving must not leave a temp file behind. */
    char *path = test_tmp_path("atomic.txt");
    Buffer b;
    buffer_init(&b);
    buffer_ensure_line(&b, 0);
    buffer_set_char(&b, 0, 0, 'x');
    CHECK_EQ_INT(buffer_save(&b, path, "\n"), 1);
    char tmp[512];
    snprintf(tmp, sizeof tmp, "%s.typew.%d", path, (int)getpid());
    CHECK_EQ_INT(access(tmp, F_OK), -1);   /* temp cleaned up */
    char *s = read_file(path);
    CHECK_STR(s, "x\n");
    free(s);
    buffer_destroy(&b);
    remove(path);
    free(path);
}

static void test_save_stale_temp_uses_suffix(void) {
    /* A stale temp file from a crashed run must not wedge saves; a new
       suffixed temp is used instead. */
    char *path = test_tmp_path("stale.txt");
    char stale[512];
    snprintf(stale, sizeof stale, "%s.typew.%d", path, (int)getpid());
    FILE *f = fopen(stale, "wb");
    fputs("junk", f);
    fclose(f);

    Buffer b;
    buffer_init(&b);
    buffer_ensure_line(&b, 0);
    buffer_set_char(&b, 0, 0, 'x');
    CHECK_EQ_INT(buffer_save(&b, path, "\n"), 1);

    char *s = read_file(path);
    CHECK_STR(s, "x\n");
    free(s);
    buffer_destroy(&b);
    remove(path);
    remove(stale);
    free(path);
}

static void test_save_overwrites_existing(void) {
    /* Saving over an existing file atomically replaces its contents. */
    char *path = test_tmp_path("over.txt");
    write_file(path, "old content\n");
    Buffer b;
    buffer_init(&b);
    buffer_ensure_line(&b, 0);
    buffer_set_char(&b, 0, 0, 'n');
    buffer_set_char(&b, 0, 1, 'e');
    buffer_set_char(&b, 0, 2, 'w');
    CHECK_EQ_INT(buffer_save(&b, path, "\n"), 1);
    char *s = read_file(path);
    CHECK_STR(s, "new\n");
    free(s);
    buffer_destroy(&b);
    remove(path);
    free(path);
}

static void test_load_empty_file(void) {
    char *path = test_tmp_path("empty.txt");
    write_file(path, "");
    Buffer b;
    buffer_init(&b);
    CHECK_EQ_INT(buffer_load(&b, path), 1);
    CHECK_EQ_INT(b.count, 0);
    buffer_destroy(&b);
    remove(path);
    free(path);
}

static void test_load_missing_file_fails(void) {
    char *path = test_tmp_path("missing.txt");
    remove(path);
    Buffer b;
    buffer_init(&b);
    CHECK_EQ_INT(buffer_load(&b, path), 0);
    CHECK_EQ_INT(b.count, 0);
    buffer_destroy(&b);
    free(path);
}

static void test_save_empty_buffer(void) {
    /* an empty buffer (count 0) saves as an empty file */
    char *path = test_tmp_path("save_empty.txt");
    Buffer b;
    buffer_init(&b);
    CHECK_EQ_INT(buffer_save(&b, path, "\n"), 1);
    char *s = read_file(path);
    CHECK_STR(s, "");
    free(s);
    buffer_destroy(&b);
    remove(path);
    free(path);
}

static void test_set_char_negative_line_and_col(void) {
    /* negative line/col must be ignored, not crash (buffer_set_char is
       reached with e->row/e->col which are never negative, but the API
       should be defensive) */
    Buffer b;
    buffer_init(&b);
    buffer_ensure_line(&b, 0);
    buffer_set_char(&b, 0, 0, 'x');
    buffer_set_char(&b, -1, 0, 'y');
    buffer_set_char(&b, 0, -1, 'y');
    CHECK_EQ_INT(b.count, 1);
    CHECK_STR(buffer_line(&b, 0), "x");
    buffer_destroy(&b);
}

static void test_save_failure_reports_error(void) {
    /* Saving into a nonexistent directory must return 0 (not crash). */
    char *path = test_tmp_path("nodir/out.txt");
    Buffer b;
    buffer_init(&b);
    buffer_ensure_line(&b, 0);
    buffer_set_char(&b, 0, 0, 'x');
    CHECK_EQ_INT(buffer_save(&b, path, "\n"), 0);
    buffer_destroy(&b);
    free(path);
}

static void test_save_failure_readonly_dir(void) {
    /* On a read-only directory, save must fail cleanly. */
    char *dir = test_tmp_path("ro");
    mkdir(dir, 0500);
    char *path = test_tmp_path("ro/out.txt");
    Buffer b;
    buffer_init(&b);
    buffer_ensure_line(&b, 0);
    buffer_set_char(&b, 0, 0, 'x');
    int failed = 0;
    if (access(dir, W_OK) != 0)   /* skip if running as root/owner bypass */
        failed = 1;
    if (failed)
        CHECK_EQ_INT(buffer_save(&b, path, "\n"), 0);
    buffer_destroy(&b);
    free(path);
    free(dir);
}

void run_buffer_tests(void) {
    test_load_lines();
    test_load_no_trailing_newline();
    test_save_trims_trailing_spaces();
    test_save_crlf_line_endings();
    test_set_char_grows_with_spaces();
    test_empty();
    test_spaces_line_not_empty();
    test_line_len_out_of_range();
    test_save_is_atomic_no_stray_tmp();
    test_save_stale_temp_uses_suffix();
    test_save_overwrites_existing();
    test_save_failure_reports_error();
    test_save_failure_readonly_dir();
    test_load_empty_file();
    test_load_missing_file_fails();
    test_save_empty_buffer();
    test_set_char_negative_line_and_col();
}

int main(void) {
    TEST_MAIN_SUMMARY(run_buffer_tests);
}
