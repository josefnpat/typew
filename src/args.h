/* typew - terminal write-only typewriter.
   Copyright (c) 2026 Josef Patoprsty.
   SPDX-License-Identifier: MIT */

#ifndef TYPEW_ARGS_H
#define TYPEW_ARGS_H

/* Parses command line arguments. On success sets *config_path (may be NULL)
   and *filename. Returns 0 on success, -1 if no FILENAME given. */
int parse_args(int argc, char **argv, const char **config_path,
               const char **filename);

/* Returns nonzero if --help/-h was requested. */
int wants_help(int argc, char **argv);

/* Returns nonzero if --version/-v was requested. */
int wants_version(int argc, char **argv);

#endif