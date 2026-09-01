/* typew - terminal write-only typewriter.
   Copyright (c) 2026 Josef Patoprsty.
   SPDX-License-Identifier: MIT */

#include "args.h"

#include <string.h>

static int any_flag(int argc, char **argv, const char *short_f,
                    const char *long_f) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], short_f) == 0 || strcmp(argv[i], long_f) == 0)
            return 1;
    }
    return 0;
}

int wants_help(int argc, char **argv) {
    return any_flag(argc, argv, "-h", "--help");
}

int wants_version(int argc, char **argv) {
    return any_flag(argc, argv, "-v", "--version");
}

int parse_args(int argc, char **argv, const char **config_path,
               const char **filename) {
    *config_path = NULL;
    *filename = NULL;

    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        if (strcmp(a, "-c") == 0 || strcmp(a, "--config") == 0) {
            if (i + 1 < argc)
                *config_path = argv[++i];
        } else if (strncmp(a, "--config=", 9) == 0) {
            *config_path = a + 9;
        } else if (strncmp(a, "-c=", 3) == 0) {
            *config_path = a + 3;
        } else if (*filename == NULL) {
            *filename = a;
        }
    }

    return *filename == NULL ? -1 : 0;
}