/* typew - terminal write-only typewriter.
   Copyright (c) 2026 Josef Patoprsty.
   SPDX-License-Identifier: MIT */

#ifndef TYPEW_UTIL_H
#define TYPEW_UTIL_H

#include <stddef.h>
#include <stdio.h>

/* Reads the next line of `f` into a freshly allocated, caller-owned string.
   The trailing newline (and a preceding \r) is stripped. Returns NULL at
   EOF or on error. */
char *read_line(FILE *f);

/* strdup that never returns NULL; exits on allocation failure. */
char *xstrdup(const char *s);

#endif
