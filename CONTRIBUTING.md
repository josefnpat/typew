# Contributing to typew

Thanks for helping out. This file covers the basics; keep it simple.

## Reporting bugs

Open an issue at the [issue tracker](https://github.com/josefnpat/typew/issues).
Include:

- What you ran (the full command line)
- What you expected to happen
- What actually happened
- Your OS and terminal emulator, if relevant

## Requesting features

Open an issue describing the problem you're trying to solve. typew is
deliberately minimal, so a concrete use case helps.

## Submitting patches

1. Fork the repository.
2. Create a branch for your change.
3. Make your change, keeping the style of the surrounding code.
4. Run `make` and `make test` — everything must pass.
5. Open a pull request.

Small, focused changes are easier to review than large rewrites.

## Code style

- C99, compiled with `-Wall -Wextra -Wpedantic`
- Keep functions small and focused
- Follow the existing naming and formatting conventions

## License

Contributions are licensed under the MIT License (see [LICENSE](LICENSE)).
