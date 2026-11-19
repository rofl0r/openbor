# OpenBOR-SMART

**System, Media, Automated Rewrite Technology**

OpenBOR-SMART is a standalone Go-based toolkit for helping developers migrate legacy OpenBOR scripts to Lua while preserving compatibility with the target-era engine.

## Current implementation

The lexer tokenizes legacy C-style source while preserving whitespace, comments, literals, and source positions. It reports unterminated comments and quoted literals with line and column information.

Animation text is currently preserved verbatim. In particular, **all `@cmd` lines are copied unchanged**. The Lua-enabled OpenBOR engine is responsible for executing legacy `@cmd` syntax, so the converter must not rewrite, parse, or otherwise alter those commands.

The C-style script-to-Lua parser and generator are the next implementation task. They must support standalone legacy script files and C-style `@script` blocks embedded in animation text, while preserving legacy function names, callback entry points, and engine API calls. Compatibility checks must use the target-era OpenBOR source (around builds 3000–4000); newer APIs must not be assumed to exist.

## Build and use

Build from this directory:

```sh
go build -o openbor-smart .
```

Run the current command-line tool:

```sh
./openbor-smart -in path/to/input-file
```

The current CLI writes the input unchanged beside the original file, with its extension replaced by `.lua`. This is only the current pass-through behaviour; C-to-Lua conversion is not implemented yet. The original file is left untouched.

Run the tests:

```sh
go test ./...
```

## Planned next steps

- Implement a parser and Lua generator for the legacy C script dialect, building on the existing lexer.
- Recognize function definitions, parameters, declarations, and statements, including multiple declarators, pointers, and arrays.
- Add control-flow and expression parsing, with explicit diagnostics for unsupported syntax rather than silently producing incorrect Lua.
- Extract and translate embedded `@script` blocks while leaving surrounding animation data and every `@cmd` line unchanged.
- Preserve legacy OpenBOR API names and callback entry points; check API compatibility against the target-era engine.
- Add safe folder/batch conversion and collision checks.

PAK extraction/packing, sound conversion, and automatic asset-path rewriting are outside the initial script-conversion scope.
