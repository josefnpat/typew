/* typew - terminal write-only typewriter.
   Copyright (c) 2026 Josef Patoprsty.
   SPDX-License-Identifier: MIT */

#define _XOPEN_SOURCE 700

#include "config.h"
#include "default_ini.h"
#include "util.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static int make_parent_dirs(const char *path) {
    char *p = strdup(path);
    if (!p)
        return -1;
    int rc = 0;
    for (char *s = p + 1; *s; s++) {
        if (*s == '/') {
            *s = '\0';
            if (mkdir(p, 0755) != 0 && errno != EEXIST) {
                rc = -1;
                break;
            }
            *s = '/';
        }
    }
    free(p);
    return rc;
}

/* Sensible caps so a broken config can't cause pathological behaviour
   (e.g. bell_count >= line_length makes the bell ring on every keystroke). */
#define MAX_LINE_LENGTH 4096
#define MAX_LINE_SPACE  4096

/* Enforces invariants across fields, run after parsing a config file. */
static void config_normalize(Config *cfg) {
    if (cfg->line_length < 1)
        cfg->line_length = 1;
    if (cfg->line_length > MAX_LINE_LENGTH)
        cfg->line_length = MAX_LINE_LENGTH;
    if (cfg->bell_count < 0)
        cfg->bell_count = 0;
    if (cfg->bell_count >= cfg->line_length)
        cfg->bell_count = cfg->line_length - 1;
    if (cfg->line_space < 1)
        cfg->line_space = 1;
    if (cfg->line_space > MAX_LINE_SPACE)
        cfg->line_space = MAX_LINE_SPACE;
}

void config_defaults(Config *cfg) {
    cfg->save_delay = 1.5;
    cfg->line_endings = xstrdup("\n");
    cfg->eol_bell = 1;
    cfg->bell_count = 8;
    cfg->line_length = 80;
    cfg->line_space = 1;
    cfg->overwrite = '#';
    config_normalize(cfg);
}

void config_destroy(Config *cfg) {
    free(cfg->line_endings);
    cfg->line_endings = NULL;
}

static char *trim(char *s) {
    while (*s && isspace((unsigned char)*s))
        s++;
    char *end = s + strlen(s);
    while (end > s && isspace((unsigned char)end[-1]))
        end--;
    *end = '\0';
    return s;
}

static void set_double(Config *cfg, const char *value) {
    char *end = NULL;
    double v = strtod(value, &end);
    if (end && *end == '\0' && v > 0)
        cfg->save_delay = v;
}

static void set_int(Config *cfg, const char *key, const char *value) {
    errno = 0;
    char *end = NULL;
    long v = strtol(value, &end, 10);
    if (end == value || (end && *end != '\0') || errno == ERANGE)
        return;
    if (v < INT_MIN)
        v = INT_MIN;
    if (v > INT_MAX)
        v = INT_MAX;
    if (strcmp(key, "bell_count") == 0)
        cfg->bell_count = (int)v;
    else if (strcmp(key, "line_length") == 0)
        cfg->line_length = (int)v;
    else if (strcmp(key, "line_space") == 0)
        cfg->line_space = (int)v;
}

static void set_bool(int *dst, const char *value) {
    char *own = strdup(value);
    if (!own)
        return;
    char *t = trim(own);
    if (strcmp(t, "true") == 0 || strcmp(t, "1") == 0 || strcmp(t, "yes") == 0)
        *dst = 1;
    else if (strcmp(t, "false") == 0 || strcmp(t, "0") == 0 || strcmp(t, "no") == 0)
        *dst = 0;
    free(own);
}

static void set_line_endings(Config *cfg, const char *value) {
    char *own = strdup(value);
    if (!own)
        return;
    char *t = trim(own);
    if (strcmp(t, "\\n") == 0) {
        free(cfg->line_endings);
        cfg->line_endings = xstrdup("\n");
    } else if (strcmp(t, "\\r\\n") == 0) {
        free(cfg->line_endings);
        cfg->line_endings = xstrdup("\r\n");
    } else if (strcmp(t, "\\r") == 0) {
        free(cfg->line_endings);
        cfg->line_endings = xstrdup("\r");
    } else if (t[0] != '\0') {
        free(cfg->line_endings);
        cfg->line_endings = xstrdup(t);
    }
    free(own);
}

static void set_overwrite(Config *cfg, const char *value) {
    char *own = strdup(value);
    if (!own)
        return;
    char *t = trim(own);
    if (t[0] != '\0')
        cfg->overwrite = t[0];
    free(own);
}

static void apply_key(Config *cfg, const char *key, const char *value) {
    if (strcmp(key, "save_delay") == 0)
        set_double(cfg, value);
    else if (strcmp(key, "line_endings") == 0)
        set_line_endings(cfg, value);
    else if (strcmp(key, "eol_bell") == 0)
        set_bool(&cfg->eol_bell, value);
    else if (strcmp(key, "bell_count") == 0)
        set_int(cfg, key, value);
    else if (strcmp(key, "line_length") == 0)
        set_int(cfg, key, value);
    else if (strcmp(key, "line_space") == 0)
        set_int(cfg, key, value);
    else if (strcmp(key, "overwrite") == 0)
        set_overwrite(cfg, value);
}

int config_load(Config *cfg, const char *path) {
    /* "rb": explicit binary mode (Cygwin/Windows text mode would translate
       \r\n -> \n, but read_line already strips a lone \r). */
    FILE *f = fopen(path, "rb");
    if (!f)
        return 0;

    char *line;
    while ((line = read_line(f)) != NULL) {
        char *s = trim(line);
        if (*s == '\0' || *s == '#' || *s == ';' || *s == '[') {
            free(line);
            continue;
        }

        char *eq = strchr(s, '=');
        if (!eq) {
            free(line);
            continue;
        }
        *eq = '\0';
        char *key = trim(s);
        char *value = trim(eq + 1);
        apply_key(cfg, key, value);
        free(line);
    }
    fclose(f);
    config_normalize(cfg);
    return 1;
}

char *config_user_path(void) {
    const char *home = getenv("HOME");
    const char *xdg = getenv("XDG_CONFIG_HOME");

    /* Prefer the XDG Base Directory location. */
    const char *dir;
    if (xdg && *xdg)
        dir = xdg;
    else if (home && *home) {
        static const char fallback[] = "/.config";
        size_t n = strlen(home) + strlen(fallback) + strlen("/typew/config") + 1;
        char *p = malloc(n);
        if (!p)
            return NULL;
        snprintf(p, n, "%s%s/typew/config", home, fallback);
        return p;
    } else {
        return NULL;
    }

    size_t n = strlen(dir) + strlen("/typew/config") + 1;
    char *p = malloc(n);
    if (!p)
        return NULL;
    snprintf(p, n, "%s/typew/config", dir);
    return p;
}

int config_write_default_if_missing(const char *path) {
    FILE *probe = fopen(path, "rb");
    if (probe) {
        fclose(probe);
        return 0;
    }
    if (make_parent_dirs(path) != 0)
        return 0;
    FILE *f = fopen(path, "wb");
    if (!f)
        return 0;
    fputs(DEFAULT_CONFIG, f);
    return fclose(f) == 0;
}
