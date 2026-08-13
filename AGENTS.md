# AGENTS.md

ScummVM is a large C++ codebase (~146 game engines) targeting many platforms.
It is a real-time game interpreter, not a library: most work happens in one
engine under `engines/<name>/`, using shared libraries in `common/`,
`graphics/`, `audio/`, `gui/`, `video/`.

**Read `AI-GUIDELINES.md` first.** AI-assisted contributions MUST be disclosed
in the commit message via an `Assisted-by: AGENT:MODEL` trailer, must not be
authored by the agent, and AI is never allowed for art assets.

## Build

The build system is the custom `./configure` + GNU Make (NOT CMake; CMake is
only used via `devtools/create_project` to generate IDE projects):

```
./configure --enable-all-engines   # or --enable-engine=mads for one engine
make -j$(nproc)                    # requires configure to have run first
make test                          # build + run CxxTest unit tests
make devtools                      # build devtools/
```

- `make` auto-reruns `./configure` when an `engines/*/configure.engine` or
  `engines.awk` changes.
- `./configure` generates `config.h`, `config.mk`,
  `engines/plugins_table.h`, `engines/detection_table.h`,
  `engines/engines.mk`. Never edit these — they are produced from
  `engines/*/configure.engine` by `engines.awk`.
- Engines build as dynamic plugins by default. `--enable-engine-static=<name>`
  for static.
- CI (`.github/workflows/ci.yml`) builds `--enable-all-engines`, then runs
  `make test` and `make devtools`.

## Engines

- New engine: copy the templates in `devtools/create_engine/files/`
  (`configure.engine`, `module.mk`, `detection.h/cpp`, `detection_tables.h`,
  `metaengine.h/cpp`, `console.h/cpp`, `credits.pl`, `POTFILES`), then re-run
  `./configure`.
- `engines/<name>/module.mk` lists every `.o` in `MODULE_OBJS` explicitly.
  New `.cpp` files must be added there or they silently won't compile in.
  `devtools/make_class.py` scaffolds a class and updates `module.mk` for you.
- Each engine has a `configure.engine` with an `add_engine` line; that's where
  `--enable-engine` names and subengines come from.

## Code style

- `.clang-format` (tabs, 4-width). Format changed files with
  `clang-format -i`; pre-commit enforces it.
- Every source file keeps the GPLv3+ license header block — never remove it.
- Naming: CamelCase classes, camelBack methods (enforced via `.clang-tidy`).
- No STL (`<vector>`, `<string>`, ...) anywhere in engines or core — use
  `Common::Array`, `Common::String`, `Common::HashMap` from `common/`.
- No C++ exceptions (built with `-fno-exceptions`) and no RTTI — propagate
  errors via return values or `error()`/`warning()`.
- Use `nullptr`, not `NULL`/`0`.
- Byte order is handled explicitly for portability: use
  `READ_BE_UINT16`, `WRITE_LE_UINT32`, etc. from `common/endian.h`.
- Prefer `Common::String` and its `format()`; format numbers with `%d`, not
  `%i`.

## Tests

- Unit tests are CxxTest suites written as `.h` headers under `test/`
  (see `test/module.mk` for the `TESTS` list); `make test` regenerates
  `test/runner` from them and runs everything.
- Engine test suites (e.g. `test/engines/wintermute`, `ultima`) only build when
  that engine is enabled as a static plugin.
- `test-games` target runs the game-recording regression suite
  (`devtools/run_event_recorder_tests.py`); it needs real game data and a
  prebuilt binary.

## Conventions

- Commit subjects must start with an all-caps tag: `ENGINE:`, `ALL:`, `NEWS:`,
  `DOCS:`, `VIDEO:`, `GUI:`, `JANITORIAL:` ... enforced by
  `devtools/check-commit-msg.py` (pre-commit hook).
- `pre-commit install` gives you the repo hooks (check-yaml,
  end-of-file-fixer, trailing-whitespace, clang-format, check-commit-msg).
- Portability is a hard requirement: keep platform-specific code in
  `backends/` and use the `common/` abstractions elsewhere. LF line endings are
  enforced via `.gitattributes`.
