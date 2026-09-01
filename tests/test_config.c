/* typew - terminal write-only typewriter.
   Copyright (c) 2026 Josef Patoprsty.
   SPDX-License-Identifier: MIT */

#define _XOPEN_SOURCE 700

#include "../src/config.h"
#include "tests.h"

static int write_file(const char *path, const char *content) {
    FILE *f = fopen(path, "wb");
    if (!f)
        return 0;
    fputs(content, f);
    fclose(f);
    return 1;
}

static void test_defaults(void) {
    Config c;
    config_defaults(&c);
    CHECK_EQ_INT(c.save_delay >= 1.5 && c.save_delay <= 1.5, 1);
    CHECK_STR(c.line_endings, "\n");
    CHECK_EQ_INT(c.eol_bell, 1);
    CHECK_EQ_INT(c.bell_count, 8);
    CHECK_EQ_INT(c.line_length, 80);
    CHECK_EQ_INT(c.line_space, 1);
    CHECK_EQ_INT(c.overwrite, '#');
    config_destroy(&c);
}

static void test_load_all_keys(void) {
    char *path = test_tmp_path("all.ini");
    write_file(path,
               "; comment line\n"
               "# another comment\n"
               "[section]\n"
               "save_delay = 2.5\n"
               "line_endings = \\r\\n\n"
               "eol_bell = false\n"
               "bell_count = 4\n"
               "line_length = 100\n"
               "line_space = 3\n"
               "overwrite = X\n");
    Config c;
    config_defaults(&c);
    config_load(&c, path);
    CHECK_EQ_INT(c.save_delay >= 2.5 && c.save_delay <= 2.5, 1);
    CHECK_STR(c.line_endings, "\r\n");
    CHECK_EQ_INT(c.eol_bell, 0);
    CHECK_EQ_INT(c.bell_count, 4);
    CHECK_EQ_INT(c.line_length, 100);
    CHECK_EQ_INT(c.line_space, 3);
    CHECK_EQ_INT(c.overwrite, 'X');
    config_destroy(&c);
    remove(path);
    free(path);
}

static void test_missing_file_keeps_defaults(void) {
    Config c;
    config_defaults(&c);
    config_load(&c, "no_such_file.ini");
    CHECK_EQ_INT(c.line_length, 80);
    CHECK_EQ_INT(c.overwrite, '#');
    config_destroy(&c);
}

static void test_no_space_value_forms(void) {
    /* INI with no spaces around '=' and numeric booleans (1/0) */
    char *path = test_tmp_path("tight.ini");
    write_file(path,
               "save_delay=3.0\n"
               "eol_bell=0\n"
               "bell_count=2\n"
               "line_length=120\n"
               "line_space=4\n"
               "overwrite=Z\n");
    Config c;
    config_defaults(&c);
    config_load(&c, path);
    CHECK_EQ_INT(c.save_delay >= 3.0 && c.save_delay <= 3.0, 1);
    CHECK_EQ_INT(c.eol_bell, 0);
    CHECK_EQ_INT(c.bell_count, 2);
    CHECK_EQ_INT(c.line_length, 120);
    CHECK_EQ_INT(c.line_space, 4);
    CHECK_EQ_INT(c.overwrite, 'Z');
    config_destroy(&c);
    remove(path);
    free(path);
}

static void test_bare_cr_line_ending(void) {
    char *path = test_tmp_path("cr.ini");
    write_file(path, "line_endings = \\r\n");
    Config c;
    config_defaults(&c);
    config_load(&c, path);
    CHECK_STR(c.line_endings, "\r");
    config_destroy(&c);
    remove(path);
    free(path);
}

static void test_value_clamping(void) {
    char *path = test_tmp_path("clamp.ini");
    write_file(path,
               "save_delay = 0\n"     /* <=0 -> keep default 1.5 */
               "bell_count = -5\n"    /* clamped to 0 */
               "line_length = 0\n"    /* clamped to 1 */
               "line_space = -2\n");  /* clamped to 1 */
    Config c;
    config_defaults(&c);
    config_load(&c, path);
    CHECK_EQ_INT(c.save_delay >= 1.5 && c.save_delay <= 1.5, 1);
    CHECK_EQ_INT(c.bell_count, 0);
    CHECK_EQ_INT(c.line_length, 1);
    CHECK_EQ_INT(c.line_space, 1);
    config_destroy(&c);
    remove(path);
    free(path);
}

static void test_bell_count_clamped_to_line_length(void) {
    /* bell_count >= line_length would make the bell ring every keystroke. */
    char *path = test_tmp_path("bell.ini");
    write_file(path, "line_length = 20\nbell_count = 50\n");
    Config c;
    config_defaults(&c);
    config_load(&c, path);
    CHECK_EQ_INT(c.line_length, 20);
    CHECK_EQ_INT(c.bell_count, 19);   /* < line_length */
    config_destroy(&c);
    remove(path);
    free(path);
}

static void test_upper_bounds_capped(void) {
    char *path = test_tmp_path("cap.ini");
    write_file(path,
               "line_length = 999999\n"
               "line_space = 999999\n");
    Config c;
    config_defaults(&c);
    config_load(&c, path);
    CHECK_EQ_INT(c.line_length, 4096);
    CHECK_EQ_INT(c.line_space, 4096);
    config_destroy(&c);
    remove(path);
    free(path);
}

static void test_invalid_int_keeps_previous_value(void) {
    /* Non-numeric or overflowing int values must be ignored, not turned
       into 0 (strtol, not atoi). */
    char *path = test_tmp_path("badint.ini");
    write_file(path,
               "line_length = notanumber\n"
               "bell_count = 99999999999999999999\n"
               "line_space = 12abc\n");
    Config c;
    config_defaults(&c);
    config_load(&c, path);
    CHECK_EQ_INT(c.line_length, 80);   /* non-numeric -> default kept */
    CHECK_EQ_INT(c.bell_count, 8);     /* overflow -> default kept */
    CHECK_EQ_INT(c.line_space, 1);     /* trailing junk -> default kept */
    config_destroy(&c);
    remove(path);
    free(path);
}

/* Regression for the memory-safety fix: values with surrounding whitespace
   must be parsed and applied correctly without leaked/corrupted state. */
static void test_whitespace_padded_values(void) {
    char *path = test_tmp_path("pad.ini");
    write_file(path,
               "eol_bell =  true \n"
               "overwrite =  X  \n"
               "line_endings =  \\r\\n \n");
    Config c;
    config_defaults(&c);
    config_load(&c, path);
    CHECK_EQ_INT(c.eol_bell, 1);
    CHECK_EQ_INT(c.overwrite, 'X');
    CHECK_STR(c.line_endings, "\r\n");
    config_destroy(&c);
    remove(path);
    free(path);
}

/* Long lines beyond an old fixed 1024-byte buffer must load correctly. */
static void test_long_line_not_truncated(void) {
    char *path = test_tmp_path("long.ini");
    size_t n = 5000;
    char *content = malloc(n + 1);
    for (size_t i = 0; i < n; i++)
        content[i] = 'a';
    content[n] = '\0';
    FILE *f = fopen(path, "w");
    fprintf(f, "line_length = 10\n");
    fprintf(f, "overwrite = %s\n", content);
    fclose(f);
    Config c;
    config_defaults(&c);
    config_load(&c, path);
    CHECK_EQ_INT(c.line_length, 10);
    config_destroy(&c);
    free(content);
    remove(path);
    free(path);
}

static void test_config_load_returns_status(void) {
    Config c;
    config_defaults(&c);
    char *good = test_tmp_path("ok.ini");
    write_file(good, "line_length = 50\n");
    CHECK_EQ_INT(config_load(&c, good), 1);
    remove(good);
    free(good);
    CHECK_EQ_INT(config_load(&c, "does_not_exist.ini"), 0);
    config_destroy(&c);
}

static void test_defaults_destroyed(void) {
    Config c;
    config_defaults(&c);
    config_destroy(&c);
    CHECK_EQ_INT(c.line_endings == NULL, 1);
}

/* ------------------------------------------------------------------ */
/* config_user_path: XDG base directory resolution                    */
/* ------------------------------------------------------------------ */
static void test_user_path_uses_xdg(void) {
    setenv("XDG_CONFIG_HOME", "/xdg/root", 1);
    unsetenv("HOME");
    char *p = config_user_path();
    CHECK_STR(p, "/xdg/root/typew/config");
    free(p);
}

static void test_user_path_falls_back_to_home(void) {
    unsetenv("XDG_CONFIG_HOME");
    setenv("HOME", "/home/tester", 1);
    char *p = config_user_path();
    CHECK_STR(p, "/home/tester/.config/typew/config");
    free(p);
}

static void test_user_path_returns_null_without_home(void) {
    unsetenv("XDG_CONFIG_HOME");
    unsetenv("HOME");
    char *p = config_user_path();
    CHECK_EQ_INT(p == NULL, 1);
}

/* ------------------------------------------------------------------ */
/* config_write_default_if_missing                                    */
/* ------------------------------------------------------------------ */
static void test_write_default_creates_file(void) {
    char *path = test_tmp_path("sub/dir/config");
    CHECK_EQ_INT(config_write_default_if_missing(path), 1);
    /* second call: file exists, nothing written */
    CHECK_EQ_INT(config_write_default_if_missing(path), 0);
    /* parent dirs were created */
    char dir[512];
    snprintf(dir, sizeof dir, "%s/sub/dir", test_tmpdir());
    struct stat st;
    CHECK_EQ_INT(stat(dir, &st) == 0 && S_ISDIR(st.st_mode), 1);
    /* content is the baked-in default */
    char *content = malloc(1);
    content[0] = '\0';
    FILE *f = fopen(path, "rb");
    if (f) {
        fseek(f, 0, SEEK_END);
        long n = ftell(f);
        fseek(f, 0, SEEK_SET);
        free(content);
        content = malloc((size_t)n + 1);
        size_t rd = fread(content, 1, (size_t)n, f);
        content[rd] = '\0';
        fclose(f);
    }
    CHECK_STR(content, DEFAULT_CONFIG);
    free(content);
    remove(path);
    free(path);
}

static void test_write_default_fails_when_parent_is_file(void) {
    /* A path whose parent is a regular file (not a dir) must fail. */
    char *blocker = test_tmp_path("afile");
    FILE *f = fopen(blocker, "wb");
    fputs("x", f);
    fclose(f);
    char *path = test_tmp_path("afile/config");
    CHECK_EQ_INT(config_write_default_if_missing(path), 0);
    remove(blocker);
    free(blocker);
    free(path);
}

/* ------------------------------------------------------------------ */
/* Unknown and duplicate keys                                         */
/* ------------------------------------------------------------------ */
static void test_unknown_key_ignored(void) {
    char *path = test_tmp_path("unknown.ini");
    write_file(path, "line_length = 42\nnot_a_real_key = 999\n");
    Config c;
    config_defaults(&c);
    config_load(&c, path);
    CHECK_EQ_INT(c.line_length, 42);
    config_destroy(&c);
    remove(path);
    free(path);
}

static void test_duplicate_key_last_wins(void) {
    char *path = test_tmp_path("dup.ini");
    write_file(path, "line_length = 10\nline_length = 20\n");
    Config c;
    config_defaults(&c);
    config_load(&c, path);
    CHECK_EQ_INT(c.line_length, 20);
    config_destroy(&c);
    remove(path);
    free(path);
}

static void test_line_without_equals_skipped(void) {
    /* a line with no '=' must be ignored without disturbing the rest */
    char *path = test_tmp_path("noeq.ini");
    write_file(path, "save_delay\nline_length = 42\n");
    Config c;
    config_defaults(&c);
    config_load(&c, path);
    CHECK_EQ_INT(c.save_delay >= 1.5 && c.save_delay <= 1.5, 1);
    CHECK_EQ_INT(c.line_length, 42);
    config_destroy(&c);
    remove(path);
    free(path);
}

static void test_literal_line_endings(void) {
    /* a literal (non-escape) value is used verbatim as the line ending */
    char *path = test_tmp_path("lit.ini");
    write_file(path, "line_endings = |\n");
    Config c;
    config_defaults(&c);
    config_load(&c, path);
    CHECK_STR(c.line_endings, "|");
    config_destroy(&c);
    remove(path);
    free(path);
}

static void test_bool_word_forms(void) {
    char *path = test_tmp_path("yes.ini");
    write_file(path, "eol_bell = yes\n");
    Config c;
    config_defaults(&c);
    config_load(&c, path);
    CHECK_EQ_INT(c.eol_bell, 1);
    config_destroy(&c);
    remove(path);

    path = test_tmp_path("no.ini");
    write_file(path, "eol_bell = no\n");
    config_defaults(&c);
    config_load(&c, path);
    CHECK_EQ_INT(c.eol_bell, 0);
    config_destroy(&c);
    remove(path);
    free(path);
}

static void test_save_delay_partial_number_rejected(void) {
    /* "2.5.5" parses as 2.5 with trailing junk, so the value is ignored
       and the default is kept (full-string parse, not atof) */
    char *path = test_tmp_path("junk.ini");
    write_file(path, "save_delay = 2.5.5\n");
    Config c;
    config_defaults(&c);
    config_load(&c, path);
    CHECK_EQ_INT(c.save_delay >= 1.5 && c.save_delay <= 1.5, 1);
    config_destroy(&c);
    remove(path);
    free(path);
}

static void test_user_path_empty_xdg_falls_back_to_home(void) {
    /* an empty XDG_CONFIG_HOME is treated as unset */
    setenv("XDG_CONFIG_HOME", "", 1);
    setenv("HOME", "/home/tester", 1);
    char *p = config_user_path();
    CHECK_STR(p, "/home/tester/.config/typew/config");
    free(p);
}

void run_config_tests(void) {
    test_defaults();
    test_load_all_keys();
    test_missing_file_keeps_defaults();
    test_no_space_value_forms();
    test_bare_cr_line_ending();
    test_value_clamping();
    test_bell_count_clamped_to_line_length();
    test_upper_bounds_capped();
    test_invalid_int_keeps_previous_value();
    test_whitespace_padded_values();
    test_long_line_not_truncated();
    test_config_load_returns_status();
    test_defaults_destroyed();
    test_user_path_uses_xdg();
    test_user_path_falls_back_to_home();
    test_user_path_returns_null_without_home();
    test_write_default_creates_file();
    test_write_default_fails_when_parent_is_file();
    test_unknown_key_ignored();
    test_duplicate_key_last_wins();
    test_line_without_equals_skipped();
    test_literal_line_endings();
    test_bool_word_forms();
    test_save_delay_partial_number_rejected();
    test_user_path_empty_xdg_falls_back_to_home();
}

int main(void) {
    TEST_MAIN_SUMMARY(run_config_tests);
}
