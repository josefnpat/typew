/* typew - terminal write-only typewriter.
   Copyright (c) 2026 Josef Patoprsty.
   SPDX-License-Identifier: MIT */

#define _XOPEN_SOURCE 700

#include "../src/args.h"
#include "tests.h"

static void test_config_then_filename(void) {
    const char *cfgpath = NULL, *filename = NULL;
    char *argv[] = {"typew", "-c", "a.ini", "f.txt", NULL};
    CHECK_EQ_INT(parse_args(4, argv, &cfgpath, &filename), 0);
    CHECK_STR(cfgpath, "a.ini");
    CHECK_STR(filename, "f.txt");
}

static void test_long_config_separated(void) {
    const char *cfgpath = NULL, *filename = NULL;
    char *argv[] = {"typew", "--config", "b.ini", "g.txt", NULL};
    CHECK_EQ_INT(parse_args(4, argv, &cfgpath, &filename), 0);
    CHECK_STR(cfgpath, "b.ini");
    CHECK_STR(filename, "g.txt");
}

static void test_eq_forms(void) {
    const char *cfgpath = NULL, *filename = NULL;
    char *argv[] = {"typew", "--config=c.ini", "-c=d.ini", "h.txt", NULL};
    CHECK_EQ_INT(parse_args(4, argv, &cfgpath, &filename), 0);
    CHECK_STR(cfgpath, "d.ini");
    CHECK_STR(filename, "h.txt");
}

static void test_missing_filename(void) {
    const char *cfgpath = NULL, *filename = NULL;
    char *argv[] = {"typew", NULL};
    CHECK_EQ_INT(parse_args(1, argv, &cfgpath, &filename), -1);
}

static void test_dash_c_without_value_and_no_filename(void) {
    const char *cfgpath = NULL, *filename = NULL;
    char *argv[] = {"typew", "-c", NULL};
    CHECK_EQ_INT(parse_args(2, argv, &cfgpath, &filename), -1);
}

static void test_empty_eq_forms(void) {
    const char *cfgpath = NULL, *filename = NULL;
    char *argv[] = {"typew", "--config=", "f.txt", NULL};
    CHECK_EQ_INT(parse_args(3, argv, &cfgpath, &filename), 0);
    CHECK_STR(cfgpath, "");
    CHECK_STR(filename, "f.txt");

    const char *cfg2 = NULL, *fn2 = NULL;
    char *argv2[] = {"typew", "-c=", "g.txt", NULL};
    CHECK_EQ_INT(parse_args(3, argv2, &cfg2, &fn2), 0);
    CHECK_STR(cfg2, "");
    CHECK_STR(fn2, "g.txt");
}

static void test_unknown_flag_treated_as_filename(void) {
    const char *cfgpath = NULL, *filename = NULL;
    char *argv[] = {"typew", "-x", "f.txt", NULL};
    CHECK_EQ_INT(parse_args(3, argv, &cfgpath, &filename), 0);
    CHECK_EQ_INT(cfgpath == NULL, 1);
    CHECK_STR(filename, "-x");   /* first non-option token */
}

static void test_extra_positionals_ignored(void) {
    const char *cfgpath = NULL, *filename = NULL;
    char *argv[] = {"typew", "one.txt", "two.txt", NULL};
    CHECK_EQ_INT(parse_args(3, argv, &cfgpath, &filename), 0);
    CHECK_STR(filename, "one.txt");
}

static void test_config_after_filename(void) {
    const char *cfgpath = NULL, *filename = NULL;
    char *argv[] = {"typew", "f.txt", "-c", "a.ini", NULL};
    CHECK_EQ_INT(parse_args(4, argv, &cfgpath, &filename), 0);
    CHECK_STR(cfgpath, "a.ini");
    CHECK_STR(filename, "f.txt");
}

static void test_multiple_config_last_wins(void) {
    const char *cfgpath = NULL, *filename = NULL;
    char *argv[] = {"typew", "-c", "a.ini", "--config", "b.ini", "f.txt", NULL};
    CHECK_EQ_INT(parse_args(6, argv, &cfgpath, &filename), 0);
    CHECK_STR(cfgpath, "b.ini");
    CHECK_STR(filename, "f.txt");
}

static void test_dash_c_consumes_flag_like_token(void) {
    /* the value after -c is taken verbatim, even if it looks like a flag */
    const char *cfgpath = NULL, *filename = NULL;
    char *argv[] = {"typew", "-c", "-v", "f.txt", NULL};
    CHECK_EQ_INT(parse_args(4, argv, &cfgpath, &filename), 0);
    CHECK_STR(cfgpath, "-v");
    CHECK_STR(filename, "f.txt");
}

static void test_wants_help(void) {
    char *h[] = {"typew", "--help", NULL};
    CHECK_EQ_INT(wants_help(2, h), 1);
    char *h2[] = {"typew", "-h", "file.txt", NULL};
    CHECK_EQ_INT(wants_help(3, h2), 1);
    char *h3[] = {"typew", "file.txt", NULL};
    CHECK_EQ_INT(wants_help(2, h3), 0);
    char *h4[] = {"typew", "--version", NULL};
    CHECK_EQ_INT(wants_help(2, h4), 0);
}

static void test_wants_version(void) {
    char *v[] = {"typew", "--version", NULL};
    CHECK_EQ_INT(wants_version(2, v), 1);
    char *v2[] = {"typew", "-v", "file.txt", NULL};
    CHECK_EQ_INT(wants_version(3, v2), 1);
    char *v3[] = {"typew", "file.txt", NULL};
    CHECK_EQ_INT(wants_version(2, v3), 0);
    char *v4[] = {"typew", "--help", NULL};
    CHECK_EQ_INT(wants_version(2, v4), 0);
}

void run_args_tests(void) {
    test_config_then_filename();
    test_long_config_separated();
    test_eq_forms();
    test_missing_filename();
    test_dash_c_without_value_and_no_filename();
    test_empty_eq_forms();
    test_unknown_flag_treated_as_filename();
    test_extra_positionals_ignored();
    test_config_after_filename();
    test_multiple_config_last_wins();
    test_dash_c_consumes_flag_like_token();
    test_wants_help();
    test_wants_version();
}

int main(void) {
    TEST_MAIN_SUMMARY(run_args_tests);
}