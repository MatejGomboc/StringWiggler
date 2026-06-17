Review all changed or specified files for StringWiggler style compliance.

Check against these rules (from `STYLE.md`, `.clang-format` and `.editorconfig`):

## C++ Formatting

- Allman braces for functions and namespaces, attached for classes/structs/enums
- 4-space indentation (no tabs)
- 170-column limit
- Left-aligned pointers (`int* ptr`, not `int *ptr`) and references (`int& ref`)
- No short forms on single lines (no single-line if/for/while/functions)
- Braces inserted on all control statements (`InsertBraces`)
- Every source file begins with the GPLv3 header block (see `LICENCE`)
- Doxygen `//!` comments for documentation

Run `clang-format --dry-run --Werror` (or `clang-format -i` to fix) to confirm formatting.

## C++ Naming

- Library namespaces: `PascalCase` with a `Lib` suffix (`SignalsLib`, `TestingLib`, `LoggingLib`, `MathLib`, `WindowLib`)
- Application namespace: `Engine`
- Types/Classes: `PascalCase`
- Functions: `camelCase`
- Constants: `SCREAMING_SNAKE_CASE`
- Macros: `SCREAMING_SNAKE_CASE`
- Member variables: `m_snake_case` (leading `m_`, then `snake_case`)
- Local variables: `snake_case`

## YAML (GitHub Actions)

- 4-space structure indentation
- 2-space continuation from list items
- Blank lines between top-level keys and between jobs
- No inline comments

## JSON

- 4-space indentation

## Markdown

- ATX-style headings
- Dash lists (`-`), numeric ordered lists (`1.`)
- Fenced code blocks with language specified

Report every violation with file path, line number, and what is wrong. If no files are specified via $ARGUMENTS, check all files modified since the last commit.
