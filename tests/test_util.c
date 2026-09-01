/* typew - terminal write-only typewriter.
   Copyright (c) 2026 Josef Patoprsty.
   SPDX-License-Identifier: MIT */

#define _XOPEN_SOURCE 700

#include "../src/util.h"
#include "tests.h"

static int write_file(const char *path, const char *content) {
    FILE *f = fopen(path, "wb");
    if (!f)
        return 0;
    fputs(content, f);
    fclose(f);
    return 1;
}

static void test_read_line_basic(void) {
    char *path = test_tmp_path("rl.txt");
    write_file(path, "one\ntwo\nthree");
    FILE *f = fopen(path, "rb");
    char *l1 = read_line(f);
    char *l2 = read_line(f);
    char *l3 = read_line(f);
    char *l4 = read_line(f);   /* EOF */
    CHECK_STR(l1, "one");
    CHECK_STR(l2, "two");
    CHECK_STR(l3, "three");
    CHECK_EQ_INT(l4 == NULL, 1);
    free(l1);
    free(l2);
    free(l3);
    fclose(f);
    remove(path);
    free(path);
}

static void test_read_line_strips_cr(void) {
    char *path = test_tmp_path("crlf.txt");
    write_file(path, "a\r\nb\n");
    FILE *f = fopen(path, "rb");
    char *l1 = read_line(f);
    char *l2 = read_line(f);
    CHECK_STR(l1, "a");
    CHECK_STR(l2, "b");
    free(l1);
    free(l2);
    fclose(f);
    remove(path);
    free(path);
}

static void test_read_line_long(void) {
    char *path = test_tmp_path("longline.txt");
    size_t n = 100000;
    char *content = malloc(n + 1);
    for (size_t i = 0; i < n; i++)
        content[i] = (char)('a' + (i % 26));
    content[n] = '\0';
    write_file(path, content);
    FILE *f = fopen(path, "rb");
    char *l = read_line(f);
    CHECK_EQ_INT(l != NULL, 1);
    CHECK_EQ_INT((int)strlen(l), (int)n);
    CHECK_EQ_INT(strcmp(l, content), 0);
    free(l);
    fclose(f);
    free(content);
    remove(path);
    free(path);
}

static void test_xstrdup_returns_copy(void) {
    char *p = xstrdup("hello");
    CHECK_STR(p, "hello");
    CHECK_EQ_INT(p != NULL, 1);
    free(p);
}

static void test_xstrdup_empty(void) {
    char *p = xstrdup("");
    CHECK_STR(p, "");
    free(p);
}

static void test_read_line_empty_file(void) {
    char *path = test_tmp_path("rl_empty.txt");
    write_file(path, "");
    FILE *f = fopen(path, "rb");
    CHECK_EQ_INT(read_line(f) == NULL, 1);
    fclose(f);
    remove(path);
    free(path);
}

static void test_read_line_only_newline(void) {
    char *path = test_tmp_path("rl_nl.txt");
    write_file(path, "\n\n");
    FILE *f = fopen(path, "rb");
    char *l1 = read_line(f);
    char *l2 = read_line(f);
    char *l3 = read_line(f);
    CHECK_STR(l1, "");
    CHECK_STR(l2, "");
    CHECK_EQ_INT(l3 == NULL, 1);
    free(l1);
    free(l2);
    fclose(f);
    remove(path);
    free(path);
}

static void test_read_line_embedded_cr_preserved(void) {
    /* An interior \r (not at end of line) must be preserved. */
    char *path = test_tmp_path("rl_cr.txt");
    write_file(path, "a\rb\n");
    FILE *f = fopen(path, "rb");
    char *l1 = read_line(f);
    CHECK_STR(l1, "a\rb");
    free(l1);
    fclose(f);
    remove(path);
    free(path);
}

static void test_read_line_cr_at_eof_stripped(void) {
    /* A trailing \r at EOF (no newline) is stripped like a normal CRLF. */
    char *path = test_tmp_path("rl_cr_eof.txt");
    write_file(path, "a\r");
    FILE *f = fopen(path, "rb");
    char *l1 = read_line(f);
    CHECK_STR(l1, "a");
    free(l1);
    CHECK_EQ_INT(read_line(f) == NULL, 1);
    fclose(f);
    remove(path);
    free(path);
}

void run_util_tests(void) {
    test_read_line_basic();
    test_read_line_strips_cr();
    test_read_line_long();
    test_xstrdup_returns_copy();
    test_xstrdup_empty();
    test_read_line_empty_file();
    test_read_line_only_newline();
    test_read_line_embedded_cr_preserved();
    test_read_line_cr_at_eof_stripped();
}

int main(void) {
    TEST_MAIN_SUMMARY(run_util_tests);
}