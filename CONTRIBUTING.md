# Contributing to StringWiggler

Thanks for your interest in StringWiggler! It is a deliberately-pointless little
Vulkan toy, but contributions are still welcome. This document covers the basics.

StringWiggler is in its early stages: it opens a window, initialises Vulkan
(instance, surface, device selection) and then idles. Mouse tracking, the string,
the physics and the rendering are still to come — see [TODO.md](TODO.md).

## Code of Conduct

This project follows the Contributor Covenant. By participating you agree to
uphold it — see [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md).

## Reporting Bugs

Before opening an issue, please make sure you are on the latest revision and
gather the following so the problem can be reproduced:

- Operating system and version
- GPU (and driver version)
- Vulkan SDK version
- Compiler
- CMake preset used
- Steps to reproduce, plus expected vs actual behaviour
- Any Vulkan validation-layer output, if relevant

## Pull Requests

Fork the repository, branch off `main`, make your changes following the
conventions below, then open a PR against `main`.

### PR checklist

- [ ] Compiles warning-free on the relevant presets (warnings are errors)
- [ ] No Vulkan validation-layer errors in a debug build
- [ ] `clang-format` applied (`.clang-format` is enforced)
- [ ] Tests pass via CTest (e.g. `ctest --preset windows-msvc-debug`)
- [ ] A journal note added to [TODO.md](TODO.md) for non-trivial changes
- [ ] Commit messages follow Conventional Commits (see below)

## Development Setup

Prerequisites:

- C++20 compiler (MSVC 19.30+, GCC 12+, or Clang 15+)
- CMake 3.16+
- Ninja
- Vulkan SDK 1.4.335.0+
- XCB development headers on Linux (`libxcb1-dev`)

See [docs/DEV_ENV_SETUP.md](docs/DEV_ENV_SETUP.md) for the full walkthrough.
Quick start once the prerequisites are installed:

```bash
git clone https://github.com/MatejGomboc/StringWiggler.git
cd StringWiggler

# Windows
cmake --preset windows-msvc
cmake --build build/windows-msvc --config Debug

# Linux
sudo apt-get install libxcb1-dev
cmake --preset linux-gcc
cmake --build build/linux-gcc --config Debug
```

## Coding Standards

- Follow `.clang-format` (Allman braces for functions/namespaces, attached for
  classes/structs/enums, 4-space indent, 170-column limit, left pointer alignment).
- C++20. No exceptions in production code — return `bool` and fill a
  `std::string& out_error_message`. Test code may use exceptions.
- Naming: PascalCase types, camelCase functions, `m_snake_case` members,
  SCREAMING_SNAKE_CASE constants, snake_case locals.
- Doxygen `//!` comments for non-obvious logic.
- See [STYLE.md](STYLE.md) for the full guide.

### British spelling

Use British spelling in all prose, comments and user-facing strings:

| American | British |
|----------|---------|
| color | colour |
| behavior | behaviour |
| center | centre |
| license (noun) | licence |
| initialize | initialise |
| optimize | optimise |
| recognize | recognise |
| analyze | analyse |
| synchronize | synchronise |

**Note:** code identifiers may keep American spelling where it matches a
library/API convention (e.g. Vulkan names, `GNU General Public License`).

## Commit Messages

We use [Conventional Commits](https://www.conventionalcommits.org/):

```text
<type>(<scope>): <description>

[optional body]

[optional footer(s)]
```

### Types

| Type | Description |
|------|-------------|
| `feat` | New feature |
| `fix` | Bug fix |
| `docs` | Documentation only |
| `style` | Formatting, no code change |
| `refactor` | Code change that neither fixes a bug nor adds a feature |
| `perf` | Performance improvement |
| `test` | Adding or updating tests |
| `chore` | Maintenance tasks |
| `ci` | CI changes |

### Examples

```text
feat(window): add XCB pointer-motion events

fix(device): prefer discrete GPU when scoring physical devices

docs: update README with build instructions

chore: bump Vulkan SDK to 1.4.335.0
```

### Rules

- Imperative mood ("add feature", not "added feature")
- Don't capitalise the first letter of the description
- No full stop at the end of the subject line
- Keep the subject under 72 characters
- Reference issues in the footer: `Fixes #123`

## Security

Please do not file security issues in public. See [SECURITY.md](SECURITY.md) for
how to report them privately.

Thanks for contributing!
