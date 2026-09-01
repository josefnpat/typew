# typew

typew - terminal write-only typewriter

typew is a minimal, distraction-free typewriter for the terminal. It loads a
single file, lets you type freely with a blinking cursor, and writes the
buffer back to disk automatically on a timer. There are no save/load
commands — press **Escape** to save and quit.

## Requirements

- A C99 compiler (`cc`, `clang`, or `gcc`)
- [GNU Make](https://www.gnu.org/software/make/)
- ncurses development files

## Build

```sh
git clone https://github.com/josefnpat/typew
cd typew
make
```

This produces the `typew` binary in the current directory. To install it
system-wide:

```sh
sudo make install
```

This installs the binary to `/usr/local/bin` and the man page to
`/usr/local/share/man/man1`. Override the prefix with `make install PREFIX=/usr`
(and use `DESTDIR` for package staging).

## Test

```sh
make test
```

Runs the unit test suites. `make check` is an alias for `make test`.

## Usage

```sh
typew paper.txt
```

Or, with a custom config:

```sh
typew [--config CONFIGFILE | -c CONFIGFILE] FILENAME
```

`FILENAME` is required. If the file doesn't exist, it's created. Press **Escape** to save and quit.

## Keys

| Key | Action |
|---|---|
| Return | Next line, cursor to start |
| Backspace / Delete | Move cursor left |
| Arrow keys | Move cursor |
| Escape | Save and quit |

## Configuration

Configuration is read from `$XDG_CONFIG_HOME/typew/config` (or
`~/.config/typew/config` if `XDG_CONFIG_HOME` is unset) on first run, or the
file given with `--config`. Format is INI-style `key = value`.

| Key | Default | Description |
|---|---|---|
| `save_delay` | `1.5` | Seconds before writing buffer to disk |
| `line_endings` | `\n` | Line ending character(s) |
| `eol_bell` | `true` | Bell near end of writable line |
| `bell_count` | `8` | Characters from end to ring bell |
| `line_length` | `80` | Maximum line length |
| `line_space` | `1` | Lines advanced by Return key |
| `overwrite` | `#` | Character marking overwritten positions |

## Documentation

The man page (`docs/typew.1`) documents options, configuration, keys, and
exit status in detail.

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for how to report bugs and submit
patches. Bugs and feature requests are tracked in the
[issue tracker](https://github.com/josefnpat/typew/issues).

## License

[MIT](LICENSE)
