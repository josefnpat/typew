CC      ?= cc

# User-supplied CFLAGS/CPPFLAGS/LDFLAGS from the environment are never
# overwritten: CFLAGS is left to the user (empty by default) and the
# project's mandatory flags always go into PROJECT_CFLAGS, which is
# appended after the user's flags on every compile.
CFLAGS  ?=
PROJECT_CFLAGS = -std=c99 -Wall -Wextra -Wpedantic -O2
DEPFLAGS = -MMD -MP

# Locate ncurses via pkg-config when available; fall back to the default
# linker flags otherwise. Override with NCURSES_CFLAGS / NCURSES_LDFLAGS.
#
# Only the include path (-I...) is taken from pkg-config. Debian/Ubuntu's
# ncurses.pc also emits -D_DEFAULT_SOURCE -D_XOPEN_SOURCE=600, which would
# redefine the feature-test macros each translation unit defines itself and
# break -Werror builds.
NCURSES_CFLAGS ?= $(shell pkg-config --cflags-only-I ncurses 2>/dev/null || true)
NCURSES_LDFLAGS ?= $(if $(shell pkg-config --exists ncurses 2>/dev/null && echo yes),$(shell pkg-config --libs ncurses),-lncurses)

CPPFLAGS += $(NCURSES_CFLAGS)
LDLIBS   = $(NCURSES_LDFLAGS)

PREFIX  ?= /usr/local
BINDIR  ?= $(PREFIX)/bin
MANDIR  ?= $(PREFIX)/share/man
MAN1DIR ?= $(MANDIR)/man1
DESTDIR ?=

MAN     = docs/typew.1
INSTALL = install

SRC     = src/main.c src/edit.c src/config.c src/buffer.c src/util.c src/args.c
OBJ     = $(SRC:.c=.o)
DEP     = $(OBJ:.o=.d)

GEN_HDR = src/default_ini.h

typew: check_deps $(OBJ)
	$(CC) $(CFLAGS) $(PROJECT_CFLAGS) -o $@ $(OBJ) $(LDFLAGS) $(LDLIBS)

# Fails fast with a helpful message if ncurses development headers/libs are
# missing, instead of surfacing as an opaque compile/link error.
.PHONY: check_deps
check_deps:
	@rm -f .deps_check.$$$$ .deps_check.$$$$.dSYM 2>/dev/null; \
	if ! printf 'int main(void){return 0;}' | $(CC) -x c - $(CFLAGS) $(PROJECT_CFLAGS) $(LDFLAGS) $(LDLIBS) -o .deps_check.$$$$ 2>/dev/null; then \
		rm -rf .deps_check.$$$$ .deps_check.$$$$.dSYM; \
		echo "error: ncurses development files not found."; \
		echo "Install the ncurses development package for your distribution"; \
		echo "(e.g. 'libncurses-dev' on Debian/Ubuntu, 'ncurses' on Homebrew,"; \
		echo "'ncurses-devel' on Fedora), or set NCURSES_CFLAGS/NCURSES_LDFLAGS."; \
		exit 1; \
	fi; \
	rm -rf .deps_check.$$$$ .deps_check.$$$$.dSYM

%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(PROJECT_CFLAGS) $(DEPFLAGS) -c -o $@ $<

$(GEN_HDR): src/default.ini
	{ printf 'const char DEFAULT_CONFIG[] =\n'; \
	  tr -d '\r' < $< | sed -e 's/\\/\\\\/g' -e 's/"/\\"/g' -e 's/^/  "/' -e 's/$$/\\n"/'; \
	  printf '  ;\n'; \
	} > $@

src/config.o: $(GEN_HDR)

-include $(DEP)

TEST_SRC = $(wildcard tests/test_*.c)
TEST_OBJ = $(TEST_SRC:.c=.o)
TEST_BIN = $(TEST_SRC:.c=)
CORE_OBJ = src/edit.o src/config.o src/buffer.o src/util.o src/args.o

check: test

test: $(TEST_BIN)
	@pass=0; fail=0; \
	for t in $(TEST_BIN); do \
		printf '%-40s' "$${t#tests/}: "; \
		if ./$$t; then pass=$$((pass+1)); else fail=$$((fail+1)); fi; \
	done; \
	echo "======================"; \
	echo "test suites passed: $$pass, failed: $$fail"; \
	[ $$fail -eq 0 ]

tests/test_%: tests/test_%.o $(CORE_OBJ)
	$(CC) $(CFLAGS) $(PROJECT_CFLAGS) -o $@ $^ $(LDFLAGS)

install: typew
	$(INSTALL) -d "$(DESTDIR)$(BINDIR)" "$(DESTDIR)$(MAN1DIR)"
	$(INSTALL) -m 0755 typew "$(DESTDIR)$(BINDIR)/typew"
	$(INSTALL) -m 0644 $(MAN) "$(DESTDIR)$(MAN1DIR)/typew.1"

uninstall:
	rm -f "$(DESTDIR)$(BINDIR)/typew" "$(DESTDIR)$(MAN1DIR)/typew.1"

clean:
	rm -f typew $(OBJ) $(DEP) $(GEN_HDR) $(TEST_OBJ) $(TEST_BIN)

.PHONY: clean test check install uninstall check_deps
