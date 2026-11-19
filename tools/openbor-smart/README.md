# OpenBOR-SMART

**System, Media, Automated Rewrite Technology**

OpenBOR-SMART is a standalone Go tool for converting legacy C-style OpenBOR script functions into Lua.

## Build

From this directory:

```sh
go build -o openbor-smart .
go test ./...
```

## Convert one script

```sh
./openbor-smart -in data/scripts/boss.c
```

This creates `data/scripts/boss.lua` and leaves the original file untouched.

## Convert a folder

Convert every `.c` file in a folder and its subfolders:

```sh
./openbor-smart -in /path/to/game/data/scripts -recursive
```

By default, each `.lua` file is written beside its source `.c` file. To write into a separate directory while preserving the folder structure:

```sh
./openbor-smart -in /path/to/game/data/scripts -recursive -out /path/to/converted-scripts
```

Existing output files are never overwritten unless `-force` is supplied. Batch conversion reports each failed file and exits with an error if any file could not be converted.

## Current conversion support

The converter currently handles C-style function definitions, typed parameters, local scalar and array declarations, assignments, arithmetic and comparison expressions, logical operators, function calls, array indexing, member access, unary not/sign operators, the conditional operator, and common control flow: `if/else`, `while`, `do/while`, `for`, non-fallthrough `switch/case/default`, `break`, and `return`.

C array indices are shifted by one for Lua's 1-based tables. C truth testing is preserved for conditions so numeric zero is false, as it is in C. Scalar C casts are removed because Lua does not need C's static type casts.

The converter deliberately reports unsupported constructs instead of silently emitting potentially incorrect Lua. Current limitations include pointer operations/declarations, `sizeof`, comma expressions, postfix increments used inside larger expressions, `continue`, switch fallthrough/grouped empty labels, array initializer lists, typedef-based declarations, preprocessor directives, global declarations, and some less-common C declarators. Comments are currently omitted from generated Lua. Real game scripts may use constructs outside this subset, so inspect conversion errors and test generated scripts in the target OpenBOR build before using them in a release.

## Animation files and @cmd

Animation text is currently preserved verbatim by the animation-text helper. In particular, **all `@cmd` lines remain unchanged**. The Lua-enabled OpenBOR engine is responsible for interpreting legacy `@cmd` syntax; the converter must not rewrite those commands.

## Scope

This is a script converter, not a complete C compiler. PAK extraction/packing, sound conversion, and automatic asset-path rewriting are outside its scope. Compatibility must be checked against the target-era OpenBOR API; newer engine APIs must not be assumed to exist.
