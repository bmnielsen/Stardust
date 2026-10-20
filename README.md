# ShushanKing — AIIDE 2026 StarCraft AI Competition Submission

- Entry Name: ShushanKing
- Contact: Stephen Howsiny <howsiny@mail.sdu.edu.cn>
- Affiliation: iFlytek Digital Technology (Hefei)
- Race: Protoss
- Bot type: native BWAPI DLL (no proxy/client required)
- BWAPI version: **4.4.0** (official release; vendored headers under `3rdparty/BWAPILIB/include`, `BWAPI_VER "4.4.0"`)
- GPU compute required: No
- Description: A Protoss bot derived from Stardust by B. M. Nielsen (which is
  itself based on the UAlbertaBot framework), with rewritten strategy
  selection, opponent learning and micro modules. The original Stardust
  license is included verbatim (see LICENSE).

## Package contents

```
ShushanKing/
  ShushanKing.dll        compiled binary (ready to run)
  params.json            runtime configuration (loaded from this folder)
  strategies/*.json      opening strategy library (5 files)
  recognizers.json       enemy opening recognizer thresholds (optional)
  fingerprints.json      strategy label vocabulary (optional)
  README.md              this file
  README_zh.md           Chinese version of this README
  LICENSE                the Stardust License (upstream project terms, verbatim)
  src/                   full C++ source
  data/                  reference copy of runtime data (params/strategies snapshot)
  tests/                 offline test harnesses (developer tooling, not part of
                         the build; scripts use local default paths overridable
                         by arguments)
  vs/                    Visual Studio 2022 solution + project files
  3rdparty/              vendored dependencies (BWAPILIB headers, BWEM, FAP, zstd,
                         zstdstream, nlohmann, bitsery, cppcrc) — no BWAPI binaries
```

## Installation

Copy the folder to `bwapi-data/AI/ShushanKing/` and point the BWAPI `ai` line at
`bwapi-data/AI/ShushanKing/ShushanKing.dll`. The DLL reads `params.json`,
`strategies/`, `recognizers.json` and `fingerprints.json` from its own folder
(`bwapi-data/AI/ShushanKing/`) first, then falls back to `bwapi-data/read/` and
`bwapi-data/write/`. All optional files degrade gracefully to built-in defaults
if missing; `params.json` is recommended (selects the tuned configuration).

Learning archives are written to `bwapi-data/write/shushanking_<opponent_name>.json`
(one cumulative file per opponent name, capped at the most recent 500 games).
Diagnostic logs (`shushanking_framestats.txt`, `shushanking_htpipe.txt`,
`shushanking_dt_suspect.txt`) are also cumulative, uniquely named, and written
to `bwapi-data/write/`.

## Compilation

Requirements: Visual Studio 2022 (toolset v143), Windows SDK (any 10.x — the
project targets 10.0.26100.0 and redirects automatically), C++20, Win32 (x86)
Release configuration only. No environment variables needed — all include/library
paths are relative to the solution directory, and no third-party packages beyond
the vendored `3rdparty/` tree are required. BWAPI itself must NOT be bundled;
the vendored `3rdparty/BWAPILIB/include` contains only the BWAPI 4.4.0 public
headers the bot compiles against.

Command line:

```
msbuild vs\ShushanKing.sln /p:Configuration=Release /p:Platform=Win32
```

Output: `vs\src\Release\ShushanKing.dll` (~2.0 MB). The four dependency
projects (BWAPILIB, BWEM, zstd, zstdstream) build from the same solution; a
known MSB9008 warning about a missing `libzstd_static.vcxproj` project reference
is harmless (the zstd static library builds via the same solution and links
correctly). No prebuilt binaries or intermediate files are included; a clean
build produces all dependency libraries under `vs\3rdparty\*\Release\` itself.

## Source ↔ binary correspondence

The source in this package is the exact tree of the validated competition
build (108-game validation set, 92.6% overall) plus submission-only naming
unification: runtime file-name prefixes (`shushanking_*`), the AI module class
name (`ShushanKingAIModule`), project/solution file names, and config search
roots — the bot first looks for `params.json`/`strategies/` in the DLL's own
folder (so it works no matter what directory name the installer uses, e.g.
the alias-based AIIDE deployment), then `bwapi-data/AI/ShushanKing/`,
`bwapi-data/`, `bwapi-data/read/`, `bwapi-data/write/`. These changes touch
string constants, identifiers and file names only — no gameplay, strategy or
decision logic differs from the validated build.

The shipped `ShushanKing.dll` was produced by building this exact source tree
(sha256 first 8 hex: `C1D7C8A3`; 2,017,280 bytes). A string-set audit between
this and the pre-rename binary shows the only real string differences are the
expected renamed file names and the RTTI class name.

## Notes

- Frame budget: the bot self-monitors `onFrame` duration with a reactive ladder
  governor (`frame_governor.mode=ladder, limit_ms=55, skip_max=3` in
  `params.json`); measured averages are <1 ms per frame with no frames over
  35 ms in 1000+ frame samples, well inside the 42 ms / 55 ms competition limits.
- No cheat flags: the bot never enables perfect information, never issues chat
  spam, and writes only to `bwapi-data/write/`.
