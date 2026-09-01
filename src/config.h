/* typew - terminal write-only typewriter.
   Copyright (c) 2026 Josef Patoprsty.
   SPDX-License-Identifier: MIT */

#ifndef TYPEW_CONFIG_H
#define TYPEW_CONFIG_H

/* Baked-in default config text, generated from src/default.ini (see
   src/default_ini.h, produced by the Makefile). */
extern const char DEFAULT_CONFIG[];

typedef struct {
    double save_delay;
    char *line_endings;
    int eol_bell;
    int bell_count;
    int eol_lock;
    int line_length;
    int line_space;
    char overwrite;
} Config;

void config_defaults(Config *cfg);

/* Loads config from `path`, applying overrides on top of the defaults.
   Returns 1 on success, 0 if the file could not be opened. */
int config_load(Config *cfg, const char *path);
void config_destroy(Config *cfg);

/* Returns a malloc'd path to the per-user configuration file, following the
   XDG Base Directory spec ($XDG_CONFIG_HOME/typew/config, falling back to
   ~/.config/typew/config). Returns NULL if neither the XDG var nor HOME is
   set. Caller frees. */
char *config_user_path(void);

/* Writes the baked-in default config to `path` if it does not already
   exist. Returns 1 if it wrote a new file, 0 otherwise (file already
   present, or write failed). */
int config_write_default_if_missing(const char *path);

#endif
