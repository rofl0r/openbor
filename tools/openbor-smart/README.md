# OpenBOR-SMART

**System, Media, Automated Rewrite Technology**

OpenBOR-SMART is a standalone Go-based toolkit for helping developers migrate legacy OpenBOR scripts to Lua while preserving compatibility with the target-era engine.

## Current implementation

The lexer tokenizes legacy C-style source while preserving whitespace, comments, literals, and source positions. It reports unterminated comments and quoted literals with line and column information.

The parser recognises C-style function definitions, typed parameters, local variable declarations, pointer and array declarators, initialisers, return statements, expression statements, nested blocks, `if`/`else` (including `else if`), `while`, `do`/`while`, `for`, `break`, `continue`, and empty statements. Expressions and loop clauses are retained as source text; expression parsing and Lua code generation are not implemented yet.

Animation text is currently preserved verbatim. In particular, **all `@cmd` lines are copied unchanged**. The Lua-enabled OpenBOR engine is responsible for executing legacy `@cmd` syntax, so the converter must not rewrite, parse, or otherwise alter those commands.

The remaining conversion work must support standalone legacy script files and C-style `@script` blocks embedded in animation text, while preserving legacy function names, callback entry points, and engine API calls. Compatibility checks must use the target-era OpenBOR source (around builds 3000–4000); newer APIs must not be assumed to exist.

## Build and use

Build from this directory:

```sh
go build -o openbor-smart .
```

Run the current command-line tool:

```sh
./openbor-smart -in path/to/input-file
```

The current CLI still writes the input unchanged beside the original file, with its extension replaced by `.lua`. The parser is not yet connected to Lua code generation or the CLI. The original file is left untouched.

Run the tests:

```sh
go test ./...
```

## Planned next steps

- Add full expression parsing with precedence, unary/binary operators, calls, indexing, member access, casts, and assignments.
- Add Lua generation for the parsed legacy C script dialect.
- Extract and translate embedded `@script` blocks while leaving surrounding animation data and every `@cmd` line unchanged.
- Preserve legacy OpenBOR API names and callback entry points; check API compatibility against the target-era engine.
- Add safe folder/batch conversion and collision checks.

PAK extraction/packing, sound conversion, and automatic asset-path rewriting are outside the initial script-conversion scope.
