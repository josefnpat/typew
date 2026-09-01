/* typew - terminal write-only typewriter.
   Copyright (c) 2026 Josef Patoprsty.
   SPDX-License-Identifier: MIT */

#ifndef TYPEW_TESTS_H
#define TYPEW_TESTS_H

#include <dirent.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <unistd.h>

static int tests_run = 0;
static int tests_failed = 0;

#define CHECK(cond)                                                     \
    do {                                                                \
        tests_run++;                                                    \
        if (!(cond)) {                                                  \
            tests_failed++;                                             \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        }                                                               \
    } while (0)

#define CHECK_EQ_INT(actual, expected)                                  \
    do {                                                                \
        int _a = (actual), _e = (expected);                             \
        tests_run++;                                                    \
        if (_a != _e) {                                                 \
            tests_failed++;                                             \
            fprintf(stderr, "FAIL %s:%d: %s == %d, expected %d\n",      \
                    __FILE__, __LINE__, #actual, _a, _e);               \
        }                                                               \
    } while (0)

#define CHECK_STR(actual, expected)                                     \
    do {                                                                \
        const char *_a = (actual);                                      \
        const char *_e = (expected);                                    \
        tests_run++;                                                    \
        if (strcmp(_a, _e) != 0) {                                      \
            tests_failed++;                                             \
            fprintf(stderr, "FAIL %s:%d: %s == \"%s\", expected \"%s\"\n", \
                    __FILE__, __LINE__, #actual, _a, _e);               \
        }                                                               \
    } while (0)

/* A single per-binary temporary directory, created lazily under the OS temp
   dir. Test artifacts live here so a crash can't pollute the repo. */
static char _test_tmpdir[512];

static const char *test_tmpdir(void) __attribute__((unused));
static char *test_tmp_path(const char *name) __attribute__((unused));
static void test_cleanup(void) __attribute__((unused));

/* Creates a uniquely named temp directory with mkdir(2), avoiding mkdtemp
   (BSD-only, hidden by strict POSIX feature-test macros on some systems). */
static const char *test_tmpdir(void) {
    if (_test_tmpdir[0] == '\0') {
        const char *t = getenv("TMPDIR");
        const char *base = t && *t ? t : "/tmp";
        for (int i = 0; i < 10000; i++) {
            snprintf(_test_tmpdir, sizeof _test_tmpdir, "%s/typewtest%d_%d",
                     base, (int)getpid(), i);
            if (mkdir(_test_tmpdir, 0700) == 0)
                break;
        }
        if (access(_test_tmpdir, W_OK | X_OK) != 0) {
            perror("mkdir");
            exit(1);
        }
    }
    return _test_tmpdir;
}

/* Returns the path of `name` under the shared temp dir. */
static char *test_tmp_path(const char *name) {
    const char *dir = test_tmpdir();
    size_t n = strlen(dir) + strlen(name) + 2;
    char *p = malloc(n);
    snprintf(p, n, "%s/%s", dir, name);
    return p;
}

/* Removes a directory tree recursively. */
static void test_rm_rf(const char *path) {
    DIR *d = opendir(path);
    if (d) {
        struct dirent *ent;
        while ((ent = readdir(d)) != NULL) {
            if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
                continue;
            char child[PATH_MAX];
            snprintf(child, sizeof child, "%s/%s", path, ent->d_name);
            struct stat st;
            if (lstat(child, &st) == 0 && S_ISDIR(st.st_mode))
                test_rm_rf(child);
            else
                remove(child);
        }
        closedir(d);
    }
    rmdir(path);
}

/* Removes the shared temp dir and any leftover test artifacts. */
static void test_cleanup(void) {
    if (_test_tmpdir[0] == '\0')
        return;
    test_rm_rf(_test_tmpdir);
    _test_tmpdir[0] = '\0';
}

#define TEST_MAIN_SUMMARY(func)                                         \
    func();                                                             \
    test_cleanup();                                                     \
    if (tests_failed == 0) {                                            \
        printf("OK: %d tests passed\n", tests_run);                     \
        return 0;                                                       \
    } else {                                                            \
        printf("FAILED: %d of %d tests failed\n", tests_failed, tests_run); \
        return 1;                                                       \
    }

#endif
