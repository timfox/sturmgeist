# ET: Legacy — Sturmgeist

[![CI](https://github.com/timfox/sturmgeist/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/timfox/sturmgeist/actions/workflows/ci.yml?query=branch%3Amain)
[![ETLBuild](https://github.com/timfox/sturmgeist/actions/workflows/build.yml/badge.svg?branch=main)](https://github.com/timfox/sturmgeist/actions/workflows/build.yml?query=branch%3Amain)
[![Snap](https://snapcraft.io/etlegacy/badge.svg)](https://snapcraft.io/etlegacy)
[![Coverity](https://scan.coverity.com/projects/1160/badge.svg)](https://scan.coverity.com/projects/1160)
[![Discord](https://img.shields.io/discord/260750790203932672.svg?logo=discord)](https://discord.gg/UBAZFys)

*A second breath of life for Wolfenstein: Enemy Territory*

## About this repository

This is **[Sturmgeist](https://github.com/timfox/sturmgeist)** — a fork of **[ET: Legacy](https://github.com/etlegacy/etlegacy)**. Upstream docs, wiki, and releases remain the reference for the wider project; use this repo for fork-specific changes and CI.

**Quick links**

| | |
|--|--|
| Website | [etlegacy.com](https://www.etlegacy.com) |
| Downloads | [etlegacy.com/download](https://www.etlegacy.com/download) |
| Wiki / FAQ | [github.com/etlegacy/etlegacy/wiki](https://github.com/etlegacy/etlegacy/wiki) |
| Docs | [etlegacy.readthedocs.io](https://etlegacy.readthedocs.io/) |
| Lua API | [etlegacy-lua-docs.readthedocs.io](https://etlegacy-lua-docs.readthedocs.io) |
| Translation | [Transifex — etlegacy](https://app.transifex.com/etlegacy/etlegacy) |
| Chat | [#etlegacy](https://web.libera.chat/?channels=#etlegacy) (IRC), [Discord](https://discordapp.com/channels/260750790203932672/346956915814957067) |
| Windows signing | [SignPath — etlegacy](https://signpath.org/projects/etlegacy/) |

## Introduction

ET: Legacy is based on [Wolfenstein: Enemy Territory](https://www.splashdamage.com/games/wolfenstein-enemy-territory/) ([GPL release, 2010](https://www.splashdamage.com/news/wolfenstein-enemy-territory-goes-open-source/)).

- **Engine** — Fixes, security hardening, fewer legacy deps, optional modern renderers, compatibility with ET 2.60b and many mods.
- **Legacy mod** — Gameplay-close improvements, lightweight, [Lua](https://etlegacy-lua-docs.readthedocs.io)-extensible.

More context: [ET: Legacy wiki](https://github.com/etlegacy/etlegacy/wiki).

## Contributing and security

- **[CONTRIBUTING.md](CONTRIBUTING.md)** — workflow, upstream fetch, validation.
- **[SECURITY.md](SECURITY.md)** — reporting issues.

## General notes

### Game data

The tree contains **engine and mod code only**, not retail game data (still under the original EULA).

You need **`pak0.pk3`** under `etmain/`. Some mods also need **`pak1.pk3`** / **`pak2.pk3`**. [Splash Damage — W:ET](https://www.splashdamage.com/games/wolfenstein-enemy-territory/) hosts the free base game.

### Compatibility

- **Not** compatible with PunkBuster servers or **ETPro** servers.
- **64-bit Linux clients** need a 64-bit mod on the server; 32-bit-only mods require a 32-bit build or cross-compile. See [Compatible mods](https://github.com/etlegacy/etlegacy/wiki/Compatible-Mods).

### Bundled libraries (`libs/`)

If system packages are missing, use the **`etlegacy-libs`** submodule:

```sh
git submodule update --init --recursive
```

Then tune **`BUNDLED_LIBS`** and per-library **`BUNDLED_*`** flags in CMake. Version notes: [Libs changelog (wiki)](https://github.com/etlegacy/etlegacy/wiki/Libs-Changelog).

## Dependencies

**Required (typical desktop build)**

- **CMake**
- **OpenGL** + **GLEW** (OpenGL renderers)
- **SDL2**
- **Zlib**, **MiniZip**
- **libjpeg-turbo** or **libjpeg**

**Common optional features (often on by default in CMake)**

- **libcurl**, **WolfSSL** or **OpenSSL**
- **Lua**, **Ogg Vorbis**, **Theora**, **Freetype**, **libpng**, **SQLite**, **OpenAL**

**Vulkan renderer** (`-DFEATURE_RENDERER_VULKAN=ON`)

- **Vulkan loader** — CMake `find_package(Vulkan)` (e.g. LunarG SDK or distro `libvulkan-dev`).

## Clone and sync (this fork)

```sh
git clone https://github.com/timfox/sturmgeist.git
cd sturmgeist
git submodule update --init --recursive
```

That initializes the **`libs/`** submodule (bundled native libraries). The **Android JoyStick** sources live under **`app/libs/joystick/`** in this fork (vendored in-tree, not a submodule). Use **`BUNDLED_LIBS`** and **`BUNDLED_*`** in CMake to prefer bundled vs system libraries.

Sync from upstream without merging:

```sh
pixi run upstream-fetch
# then: git log --oneline HEAD..FETCH_HEAD
```

See [Development helpers](#development-helpers) and [CONTRIBUTING.md](CONTRIBUTING.md).

## Development helpers

[Pixi](https://pixi.sh/) workspace: **`pixi.toml`**.

| Command | Purpose |
|---------|---------|
| `pixi install` then `pixi run -e validation check-changes` | Format / workflow checks on changed files (merge base from `.upstream-remote-branch` or `origin/main` / `origin/master`) |
| `pixi run -e validation autoformat` | Apply formatters |
| `pixi run -e validation lint-workflows` | [actionlint](https://github.com/rhysd/actionlint) on `.github/workflows` |
| `pixi run upstream-fetch` | `git fetch` upstream `master` into `FETCH_HEAD` only (`ETL_UPSTREAM`, `ETL_UPSTREAM_BRANCH` override) |

Local builds: **`./easybuild.sh help`**.

## Compile and install

Install layout is controlled in CMake, notably:

| Variable | Role |
|----------|------|
| **`INSTALL_DEFAULT_BASEDIR`** | Default `fs_basepath` (empty → cwd when not installing system-wide) |
| **`INSTALL_DEFAULT_BINDIR`** | Executables (default `bin` under prefix) |
| **`INSTALL_DEFAULT_SHAREDIR`** | Shared data (default `share`) |
| **`INSTALL_DEFAULT_MODDIR`** | Libraries / paks (default `lib/etlegacy` + `legacy`) |
| **`DOCDIR`** | Docs (default `INSTALL_DEFAULT_SHAREDIR/doc/etlegacy`) |

### Linux

**Option A — easybuild**

```sh
./easybuild.sh      # 32-bit build, or
./easybuild.sh -64  # 64-bit
```

Default install: `~/etlegacy`.

**Option B — CMake**

```sh
mkdir build && cd build
cmake ..
make
sudo make install   # optional; set install vars first
```

**Notes**

- 32-bit builds may need **multilib / `-devel`** packages.
- **nasm** is needed for bundled jpeg-turbo.
- If CMake picks wrong lib bitness: `CC="gcc -m32" CXX="g++ -m32" cmake ..`
- X11: missing audio → `libpulse-dev` or `libasound2-dev`; odd mouse → `libxi-dev`.

### Cross-compile — MinGW (Linux host)

```sh
mkdir build && cd build
cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/Toolchain-cross-mingw-linux.cmake ..
make
```

Adjust the MinGW prefix in **`cmake/Toolchain-cross-mingw-linux.cmake`** if needed (default **`i686-w64-mingw32`**).

### Windows

1. [Visual Studio](https://visualstudio.microsoft.com/) — *Desktop development with C++*
2. [CMake](https://cmake.org/) on `PATH`

**easybuild:** run **`easybuild.bat`** → install under `My Documents\ETLegacy-Build`.

**CLI / VS:** from a `build/` folder:

```sh
cmake -G "NMake Makefiles" -DBUNDLED_LIBS=YES .. && nmake
```

or a VS generator, e.g. **`cmake -G "Visual Studio 17" -A Win32 -DBUNDLED_LIBS=YES ..`**.

**Notes**

- Failed bundled libs: in **`libs/`**, `git clean -df && git reset --hard HEAD`, then rebuild.
- libcurl + missing **sed**: [GnuWin sed](http://gnuwin32.sourceforge.net/packages/sed.htm) or use Git’s tools on `PATH`.

### macOS

- Xcode CLT (`xcode-select --install`) or full Xcode
- [Homebrew](https://brew.sh/)

```sh
brew install cmake autoconf nasm automake libtool
# system libs if not using BUNDLED_LIBS:
brew install glew sdl2 minizip jpeg-turbo curl lua libogg libvorbis theora freetype libpng sqlite openal-soft
# or: brew bundle && brew bundle --file=misc/macos/libs.Brewfile
```

**easybuild:** Mojave and older: `./easybuild.sh` or `./easybuild.sh -64`. Catalina+: `./easybuild.sh -64 --osx=10.15`. See **`easybuild.sh`** for flags.

**CMake:** `mkdir build && cd build && cmake .. && make` (set install variables before `make install` if needed).

### Raspberry Pi / aarch64

OpenGL and GLES are supported. See **`easybuild.sh`** (`RPI` section): e.g. `./easybuild.sh -RPI -j4` and toggle **`FEATURE_RENDERER_GLES`** as documented there. Dependencies vary by distro; see script comments and wiki for current package names.

### Snap

Snap packaging: [etlegacy-snap](https://github.com/etlegacy/etlegacy-snap).

## License

See **[COPYING.txt](COPYING.txt)** (GPLv3) and file headers. Wolfenstein: Enemy Territory is subject to **additional terms** from id Software; see COPYING and upstream notices.

### MD4 (RSA Data Security)

Copyright (C) 1991–1992 RSA Data Security, Inc. Use and derivative works must retain the RSA MD4 identification where referenced. Provided *as is*.

### MD5 (Colin Plumb, 1993)

Public-domain C implementation; no copyright claimed. Use freely *as is*.
