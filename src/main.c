/* typew - terminal write-only typewriter.
   Copyright (c) 2026 Josef Patoprsty.
   SPDX-License-Identifier: MIT */

#define _XOPEN_SOURCE 700

#include <curses.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "args.h"
#include "buffer.h"
#include "config.h"
#include "edit.h"

#define VERSION "0.1.0"
#define COPYRIGHT "Copyright (C) 2026 Josef Patoprsty."
#define LICENSE \
    "License MIT: <https://opensource.org/licenses/MIT>.\n" \
    "This is free software: you are free to change and redistribute it.\n" \
    "THE SOFTWARE IS PROVIDED AS IS, WITHOUT WARRANTY OF ANY KIND, EXPRESS\n" \
    "OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF\n" \
    "MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT."

static volatile sig_atomic_t g_sig_end = 0;

/* Set when the last auto-save failed, so the user can see and retry. */
static int g_save_failed = 0;

static void on_signal(int sig) {
    (void)sig;
    g_sig_end = 1;
}

static double now_seconds(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1000000000.0;
}

static int mid_row(void) {
    return (LINES - 1) / 2;
}

static char caret_char(const Editor *e, const Buffer *buf) {
    int len = buffer_line_len(buf, e->row);
    if (e->col >= len)
        return ' ';
    return buffer_line(buf, e->row)[e->col];
}

static void render(const Editor *e, const Buffer *buf, int show_caret) {
    erase();
    int center = mid_row();
    int start = e->row - center;

    for (int r = 0; r < LINES; r++) {
        int brow = start + r;
        if (brow >= 0) {
            mvprintw(r, 0, "%.*s", COLS, buffer_line(buf, brow));
        }
    }

    int screen_row = e->row - start;
    if (screen_row >= 0 && screen_row < LINES) {
        if (show_caret) {
            attron(A_REVERSE);
            mvaddch(screen_row, e->col, caret_char(e, buf));
            attroff(A_REVERSE);
        }
        move(screen_row, e->col);
    }

    if (g_save_failed) {
        attron(A_REVERSE);
        const char *msg = "SAVE FAILED - check disk space/permissions";
        mvaddstr(0, 0, msg);
        attroff(A_REVERSE);
    }

    refresh();
}

static void render_too_narrow(int required) {
    erase();
    char msg[128];
    snprintf(msg, sizeof msg,
             "Terminal too narrow, please resize to %d columns.", required);
    int c = (COLS - (int)strlen(msg)) / 2;
    if (c < 0)
        c = 0;
    mvprintw(mid_row(), c, "%s", msg);
    move(mid_row(), c);
    refresh();
}

static int edit_key(int ch) {
    if (ch == KEY_UP) return EDIT_KEY_UP;
    if (ch == KEY_DOWN) return EDIT_KEY_DOWN;
    if (ch == KEY_LEFT) return EDIT_KEY_LEFT;
    if (ch == KEY_RIGHT) return EDIT_KEY_RIGHT;
    if (ch == KEY_BACKSPACE || ch == 127 || ch == 8) return EDIT_KEY_BACKSPACE;
    if (ch == '\n' || ch == '\r' || ch == KEY_ENTER) return EDIT_KEY_ENTER;
    if (ch == KEY_RESIZE) return EDIT_KEY_RESIZE;
    return ch;
}

static void usage(void) {
    fprintf(stderr,
            "Usage: typew [--config CONFIGFILE | -c CONFIGFILE] FILENAME\n");
}

static void print_version(void) {
    printf("typew %s\n", VERSION);
    printf("%s\n", COPYRIGHT);
    printf("%s\n\n", LICENSE);
    printf("Written by Josef Patoprsty.\n");
}

static void print_help(void) {
    printf(
        "Usage: typew [--config CONFIGFILE | -c CONFIGFILE] FILENAME\n"
        "\n"
        "typew - terminal write-only typewriter\n"
        "\n"
        "Options:\n"
        "  -c, --config FILE   use FILE as the configuration\n"
        "  -h, --help          show this help and exit\n"
        "  -v, --version       show version information and exit\n");
}

static int load_config(Config *cfg, const char *config_path) {
    config_defaults(cfg);
    if (config_path) {
        if (config_load(cfg, config_path) == 0) {
            fprintf(stderr, "typew: cannot open config file: %s\n",
                    config_path);
            return -1;
        }
    } else {
        char *up = config_user_path();
        if (up) {
            config_write_default_if_missing(up);
            config_load(cfg, up);
            free(up);
        }
    }
    return 0;
}

static void save_and_quit(const Buffer *buf, const Config *cfg,
                          const char *path) {
    int has = !buffer_empty(buf);
    int ok = 1;
    if (has)
        ok = buffer_save(buf, path, cfg->line_endings);
    config_destroy((Config *)cfg);
    endwin();
    if (has && !ok) {
        fprintf(stderr, "typew: save failed: %s\n", path);
        exit(1);
    }
    printf("%s\n", has ? "Ding." : "Nothing to save. Not saved.");
    exit(0);
}

static int install_signal_handlers(void) {
    struct sigaction sa = {0};
    sa.sa_handler = on_signal;
    sigemptyset(&sa.sa_mask);
    return sigaction(SIGINT, &sa, NULL) < 0 ||
           sigaction(SIGTERM, &sa, NULL) < 0 ||
           sigaction(SIGHUP, &sa, NULL) < 0;
}

/* Sleeps in small slices. Returning early when a signal arrives lets the
   main loop notice pending SIGINT/SIGTERM/SIGHUP promptly. */
static void sleep_ms(long ms) {
    struct timespec ts = {ms / 1000, (ms % 1000) * 1000000L};
    int done = 0;
    while (!done && nanosleep(&ts, &ts) != 0) {
        if (errno == EINTR) {
            if (g_sig_end)
                done = 1;
        } else {
            break;
        }
    }
}

int main(int argc, char **argv) {
    if (wants_help(argc, argv)) {
        print_help();
        return 0;
    }
    if (wants_version(argc, argv)) {
        print_version();
        return 0;
    }

    const char *config_path = NULL;
    const char *filename = NULL;

    if (parse_args(argc, argv, &config_path, &filename) != 0) {
        usage();
        return 1;
    }

    Config cfg;
    if (load_config(&cfg, config_path) != 0)
        return 1;

    Buffer buf;
    buffer_init(&buf);
    Editor ed;
    editor_init(&ed);
    editor_open(&ed, &buf, filename);

    if (install_signal_handlers() != 0)
        fprintf(stderr,
                "typew: warning: could not install signal handlers; "
                "interrupts may exit without saving.\n");

    set_escdelay(25);
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);
    nodelay(stdscr, TRUE);

    double last_save = now_seconds();
    double last_toggle = now_seconds();
    int caret_on = 1;
    int needs_render = 1;

    for (;;) {
        double now = now_seconds();

        if (g_sig_end)
            save_and_quit(&buf, &cfg, filename);

        if (COLS < cfg.line_length) {
            render_too_narrow(cfg.line_length);
            int ch = getch();
            if (ch == 27 || g_sig_end) {
                save_and_quit(&buf, &cfg, filename);
            } else if (ch == KEY_RESIZE) {
                resize_term(0, 0);
            }
            needs_render = 1;
            sleep_ms(20);
            continue;
        }

        if (now - last_toggle >= 0.5) {
            caret_on = !caret_on;
            last_toggle = now;
            needs_render = 1;
        }

        int ch = getch();
        if (g_sig_end)
            save_and_quit(&buf, &cfg, filename);

        if (ch == ERR) {
            if (ed.dirty && now - last_save >= cfg.save_delay) {
                if (buffer_save(&buf, filename, cfg.line_endings)) {
                    ed.dirty = 0;
                    g_save_failed = 0;
                } else {
                    g_save_failed = 1;
                    beep();
                }
                last_save = now;
                needs_render = 1;
            }
            sleep_ms(20);
        } else {
            if (ch == KEY_RESIZE)
                resize_term(0, 0);
            KeyResult kr = editor_key(&ed, &buf, &cfg, edit_key(ch));
            if (kr.bell)
                beep();
            if (kr.quit) {
                save_and_quit(&buf, &cfg, filename);
            }
            needs_render = 1;
        }

        if (needs_render) {
            render(&ed, &buf, caret_on);
            needs_render = 0;
        }
    }
}