# Dependency audit — Sturmgeist / ET: Legacy

Scope: pinned third-party dependencies in this tree and its **`libs`** submodule
(bundled C/C++ under `libs/`, `ExternalProject_Add` entries in `libs/CMakeLists.txt`,
Android/Gradle in `app/build.gradle` and `app/libs/joystick`, Gradle wrapper,
GitHub Actions, Pixi, and Docker images under `misc/docker/`).

> Last reviewed: 2026-04-18. Version targets reflect advisories and changelogs
> current to the reporting date.

## Rollup status

| Phase | Status |
|---|---|
| 1 — P0 bundled C/C++ security (FreeType, curl, OpenSSL, wolfSSL, libpng, cJSON) | **Partially prepared:** the checked-out **`libs`** submodule still lists curl **8.5.0**, OpenSSL **3.2.0**, wolfSSL **5.6.6** in `libs/CMakeLists.txt`. Target bumps (**8.12.1** / **3.2.6** / **5.7.6-stable**) are in `misc/patches/0001-libs-p0-curl-openssl-wolfssl.patch` — apply on the **`libs`** remote, merge, then bump the submodule pointer here. FreeType / libpng / cJSON remain vendored overlays in **`libs`**. |
| 2 — Android hygiene | **Done:** AndroidX test stack, Kotlin BOM **2.0.21**, `TestETL` / manifest updates (see `app/build.gradle` and related paths). |
| 3 — CI / Docker hygiene | **Done:** third-party and first-party Actions pinned to SHAs; Dependabot for actions, Gradle, Docker; Rocky **9** `lnx-build` image; **Debian 12.13-slim** aarch64 and dedicated images; Android SDK base image **digest**-pinned. |
| 4 — Lua / Theora | **Deferred** — sources live in the **`libs`** submodule (see Phase 4 plan). |

---

## 1. Executive summary

| Priority | Dependency | Current | Recommended | Reason |
|---|---|---|---|---|
| P0 (security) | FreeType (`libs/freetype`) | 2.10.2 | 2.13.3+ | **CVE-2025-27363** (heap OOB write in TTF/variable fonts, CVSS 8.1, exploited in the wild). Affects ≤ 2.13.0. |
| P0 (security) | libcurl (`libs/CMakeLists.txt`) | **8.5.0** (submodule) | **8.12.1** | CVE backlog through 8.12.x; mailbox patch ready under `misc/patches/`. |
| P0 (security) | OpenSSL (`libs/CMakeLists.txt`) | **3.2.0** (submodule) | **3.2.6** | Stay on 3.2.x LTS; patch bumps tarball + hash. |
| P0 (security) | wolfSSL (`libs/CMakeLists.txt`) | **5.6.6** (submodule) | **5.7.6-stable** | CVE fixes; patch switches to GitHub archive + SHA256 and drops dead patch wiring. |
| P0 (security) | libpng (`libs/libpng`) | 1.6.47 | 1.6.50+ | Latent read-beyond-end-of-malloc in `png_write_iCCP` fixed in 1.6.47, but 1.6.47 still predates the iCCP/iTXt fixes shipped in 1.6.48–1.6.50. |
| P0 (security) | cJSON (`libs/cjson`) | 1.7.15 | 1.7.18 | CVE-2023-53154 (heap buffer over-read in `parse_string`), CVE-2024-31755 (NULL deref), and a heap-buffer-overflow fix all land in 1.7.18. |
| P1 (stability) | libjpeg-turbo (`libs/jpegturbo`) | 2.0.4 | 3.0.x (latest 3.0.4) | 2.0.4 is EOL. CVE-2020-17541, CVE-2021-20205. 3.0.x is a drop-in API match; 2.1.x is an intermediate safe stop. |
| P1 (security) | SQLite amalgamation (`libs/sqlite3/src/sqlite3.c`) | 3.36.0 (2021-06) | 3.46.x | Many non-CVE correctness fixes since 3.36.0 and several "engine" CVEs in the `concat_ws`/aggregate paths. Drop-in amalgamation swap. |
| P1 | zlib (`libs/zlib`) | 1.3.1 | 1.3.1 (no action) | Only known 1.3.1 CVE (CVE-2026-22184) is in `contrib/untgz`, which we do not build or ship. No fix needed. |
| P1 | GLEW (`libs/glew`) | 2.1.0 | 2.2.0 | No CVEs, but 2.2.0 is current and adds OpenGL 4.6 ext tables. |
| P1 (Android) | Gradle wrapper | 8.13 | 8.13 | Matches AGP 8.13 requirement; keep. |
| P1 (Android) | AGP (`build.gradle`, `app/libs/joystick/build.gradle`) | **8.13.0** (both) | keep | JoyStick is vendored under `app/libs/joystick/` (no longer a submodule) and uses the same AGP/Gradle major as the root project. |
| P2 | Android `compileSdk` 36 / `targetSdk` 36 | 36 | 36 | AGP 8.13 supports API 36; no change. |
| P2 | `androidx.appcompat` | 1.7.1 | 1.7.1 | current |
| P2 | `androidx.recyclerview` | 1.4.0 | 1.4.0 | current |
| P2 | `com.google.code.gson:gson` | 2.13.2 | 2.13.2 | current |
| P2 | `org.jetbrains.kotlin:kotlin-bom` | **2.0.21** | keep | Aligned with AGP 8.13 in `app/build.gradle`. |
| P2 | `com.jayway.android.robotium:robotium-solo` | 5.6.3 | **deprecated/abandoned** | Last release 2017; still listed in `app/build.gradle` for instrumented tests — consider Espresso or removal. |
| P2 | `com.android.support.test:runner` | — | **removed** | Replaced with **`androidx.test:runner:1.6.2`** (+ rules / junit) in `app/build.gradle`. |
| P2 (CI) | `Ilshidur/action-discord` | 0.3.2 | keep | Pinned to SHA in workflows. |
| P2 (CI) | `addnab/docker-run-action` | v3 | keep | Pinned to SHA in workflows. |
| P2 (CI) | `canonical/snapcraft-multiarch-action` | 1.10.1 | keep | Pinned to SHA in workflows. |
| P3 | Docker `misc/docker/build.Dockerfile` | ~~centos:7~~ → **rockylinux/9** | **Migrated** — Rocky Linux 9 with multilib, CRB, EPEL; same CMake / Wayland / Ninja bootstrap as before. Rebuild `etlegacy/lnx-build` via LinuxBuildMachine workflow after merge. |
| P3 | Docker `misc/docker/lnx-aarch64.Dockerfile` | **debian:12.13-slim** | keep | Bumped from Debian 11 base. |
| P3 | Docker `misc/docker/android.Dockerfile` | **thyrlian/android-sdk@sha256:…** | keep | Base image pinned by digest. |
| P3 | Docker `misc/docker/dedicated.Dockerfile` | **debian:12.13-slim** (builder) | keep | Pinned point release instead of floating `stable-slim`. |
| P4 | Lua (`libs/lua`) | 5.4.3 | 5.4.7 | Bugfixes only; same API; mods unaffected. |
| P4 | libogg (`libs/ogg`) | 1.3.5 | 1.3.5 | current |
| P4 | libvorbis (`libs/vorbis`) | 1.3.7 | 1.3.7 | current |
| P4 | libtheora (`libs/theora`) | 1.1.1 (2009) | **no upstream** | Upstream unmaintained; 1.1.1 is the last release. Impacted by CVE-2024-56431 and CVE-2026-5673 (both out-of-bounds reads on malformed streams). Theora is a client-only codec used by the cinematic decoder; decisions below. |
| P4 | OpenAL Soft (`libs/openal`) | 1.19.1 | 1.23.1+ | Many platform fixes; API-compatible. |
| P4 | findlocale (`libs/findlocale`) | 0.46 (2005) | no upstream | vendored, tiny, no network input — leave as-is. |
| P4 | Pixi toolchain (`pixi.toml`) | git 2.49, python 3.13.3, requests 2.32.4, etc. | keep | `dependabot.yml` covers actions / Gradle / Docker; Pixi deps stay manual. |

### What "high-confidence" upgrade means here

A "high-confidence" upgrade is one where:
1. The new release is a drop-in API/ABI match for how this repository consumes the library (checked against `src/` usages).
2. The release notes contain no new soname bump or configure-flag rename that would flow into our `ExternalProject_Add` or patch files.
3. The library is built via `ExternalProject_Add` from a tarball URL we control (so the upgrade is a one-line URL + hash change that CI can exercise end-to-end).

Most P0 items in §4 follow this rule; large vendored overlays (e.g. FreeType) deserve their own PR and extra CI time.

### What is NOT a high-confidence upgrade

- SDL2 2.30.9 → SDL3: SDL3 renames `SDL_main.h`, removes `SDL_CreateRGBSurface`, and breaks `SDL_WINDOW_*` flags we rely on. **Do not bundle SDL3 yet.** Keep 2.30.9 (current, receives bugfixes via sdl2-compat downstream).
- OpenSSL 3.2 → 3.4: 3.4 removes legacy ENGINE plumbing and low-level `EVP_*_meth_*` symbols; curl builds fine, but our `./Configure` recipe and the Windows NMake build step need retesting on all 5 CI hosts. Stay on 3.2.6 for the patch train.
- ~~AGP 8.2.2 → 8.13 inside `app/libs/joystick`~~ **Done in-tree**: JoyStick sources live under `app/libs/joystick/` and track the root AGP line.

---

## 2. Evidence and citations

| Finding | Source (file and exact location) |
|---|---|
| FreeType 2.10.2 in tree | `libs/freetype/include/freetype/freetype.h` defines `FREETYPE_MAJOR 2 / MINOR 10 / PATCH 2`. |
| libcurl 8.5.0 URL (current submodule) | `libs/CMakeLists.txt` → `curl-8_5_0/curl-8.5.0.tar.gz`. After patch: `curl-8_12_1/curl-8.12.1.tar.gz`. |
| OpenSSL 3.2.0 URL (current submodule) | `libs/CMakeLists.txt` → `openssl-3.2.0.tar.gz` (Win + Unix). After patch: **3.2.6**. |
| wolfSSL 5.6.6 URL (current submodule) | `libs/CMakeLists.txt` → `v5.6.6-stable/wolfssl-5.6.6.tar.gz`. After patch: **`v5.7.6-stable` archive**. |
| SDL2 2.30.9 URL | `libs/CMakeLists.txt` → `URL https://github.com/libsdl-org/SDL/releases/download/release-2.30.9/SDL2-2.30.9.tar.gz`. |
| libpng 1.6.47 in tree | `libs/libpng/png.h` → `PNG_LIBPNG_VER_STRING "1.6.47"`. |
| cJSON 1.7.15 in tree | `libs/cjson/cJSON.h` → `CJSON_VERSION_MAJOR 1 / MINOR 7 / PATCH 15`. |
| libjpeg-turbo 2.0.4 in tree | `libs/jpegturbo/CMakeLists.txt` → `set(VERSION 2.0.4)`. |
| SQLite 3.36.0 in tree | `libs/sqlite3/src/sqlite3.h` → `#define SQLITE_VERSION "3.36.0"`. |
| zlib 1.3.1 in tree | `libs/zlib/zlib.h` → `ZLIB_VERSION "1.3.1"`. |
| GLEW 2.1.0 in tree | `libs/glew/CMakeLists.txt` → `VERSION 2.1.0`. |
| Lua 5.4.3 in tree | `libs/lua/src/lua.h` → `LUA_VERSION_RELEASE "3"`. |
| libogg 1.3.5 in tree | `libs/ogg/configure.ac` → `AC_INIT([libogg],[1.3.5],…)`. |
| libvorbis 1.3.7 in tree | `libs/vorbis/configure.ac` → `AC_INIT([libvorbis],[1.3.7],…)`. |
| libtheora 1.1.1 in tree | `libs/theora/configure.ac` → `AC_INIT(libtheora,[1.1.1])`. |
| OpenAL-Soft 1.19.1 in tree | `libs/openal/CMakeLists.txt` → `LIB_MAJOR_VERSION "1" / LIB_MINOR_VERSION "19" / LIB_REVISION "1"`. |
| findlocale 0.46 in tree | `libs/findlocale/VERSION` → `v0.46 -- 2005-04-06`. |
| AGP 8.13.0 (root and vendored JoyStick) | `build.gradle` line 9 and `app/libs/joystick/build.gradle` line 9. |
| Gradle wrapper 8.13 | `gradle/wrapper/gradle-wrapper.properties` → `gradle-8.13-bin.zip`. |
| Android deps | `app/build.gradle` lines 160–171. |
| CI actions | `.github/workflows/*.yml` — all `uses:` lines inventoried above. |
| Rocky Linux 9 build image | `misc/docker/build.Dockerfile` → `FROM rockylinux/9`. |
| Debian 12.13 aarch64 image | `misc/docker/lnx-aarch64.Dockerfile` → `FROM debian:12.13-slim`. |
| Android SDK image digest | `misc/docker/android.Dockerfile` → `FROM thyrlian/android-sdk@sha256:…`. |

---

## 3. Upgrade notes (risk / confidence)

Short notes for the **remaining** work in §4. Items already merged (Android stack, CI pins, Docker bases) are omitted here.

| Item | Notes |
|------|--------|
| **FreeType 2.10 → 2.13** | Public API used in `src/renderercommon/tr_font.c` is stable; bundled build uses `-DFT_WITH_*=OFF` / `CMAKE_DISABLE_FIND_PACKAGE_*` — **high** confidence. |
| **curl → 8.12** (patch ready) | Same `ExternalProject` flags; use **`-DCURL_USE_GSSAPI=OFF`** on 8.12+. Static link — **high**. |
| **OpenSSL 3.2.0 → 3.2.6** (patch ready) | Same `./Configure` targets and `install_sw`; **high**. |
| **wolfSSL → 5.7.6** (patch ready) | CMake options preserved; full matrix CI after merge — **high** for the build path. |
| **libpng / cJSON** | Vendored overlays; patch-level — **high**. |
| **libjpeg-turbo → 3.x** | Prefer **2.1.x** first, then 3.x; review `src/renderercommon/tr_image_jpg.c` — **medium**. |
| **SQLite amalgamation** | Drop-in; mod SQL uses single-quoted literals — **high**. |
| **GLEW / OpenAL** | Vendored / static; OpenAL may need desktop CI smoke — **high** with CI. |
| **libtheora 1.1.1** | Unmaintained; CVEs via malformed cinematics — options: document risk, vendor patches, or remove Theora path (see Phase 4). |

---

## 4. Smallest safe update plan

A "minimal" first PR means every change is:

- One-line URL + hash swap in `libs/CMakeLists.txt`, OR
- A verbatim tarball overlay inside `libs/<lib>/`, OR
- A one-line version pin in `app/build.gradle` / `build.gradle`.

No patches, no build-system refactors, no API code touched.

### Phase 1 — P0 security (single PR, one commit per lib)

1. **FreeType 2.10.2 → 2.13.3** — overlay `libs/freetype` from the upstream 2.13.3 tarball. (This is the biggest overlay. Consider making this its own PR if the submodule stores freetype there.)
2. **libcurl 8.5.0 → 8.12.1** — **prepared** in `misc/patches/0001-libs-p0-curl-openssl-wolfssl.patch`; merge on **`libs`** then bump submodule.
3. **OpenSSL 3.2.0 → 3.2.6** — same patch / same workflow.
4. **wolfSSL 5.6.6 → 5.7.6-stable** — same patch (archive URL + SHA256; drops unused `Patch` / `WolfSSL.patch` wiring).
5. **libpng 1.6.47 → 1.6.50** — overlay `libs/libpng`.
6. **cJSON 1.7.15 → 1.7.18** — overwrite `libs/cjson/cJSON.{c,h}` and `libs/cjson/cJSON_Utils.{c,h}`.

### Phase 2 — P1 stability (second PR)

7. **libjpeg-turbo 2.0.4 → 2.1.5.1** — overlay `libs/jpegturbo`.
8. **SQLite 3.36.0 → 3.46.x** — overlay `libs/sqlite3/src/{sqlite3.c,sqlite3.h,sqlite3ext.h,shell.c}`.
9. **GLEW 2.1.0 → 2.2.0** — overlay `libs/glew`.
10. **OpenAL-Soft 1.19.1 → 1.23.1** — overlay `libs/openal`.
11. **Gradle: JoyStick AGP** — sources are vendored in `app/libs/joystick/`; keep its `classpath 'com.android.tools.build:gradle:…'` aligned with the root `build.gradle` when upgrading AGP.
12. ~~**Kotlin BOM → 2.0.21**~~ — **done** in `app/build.gradle`.
13. ~~**AndroidX instrumented tests**~~ — **done** (`androidx.test:*` in `app/build.gradle`).

### Phase 3 — P2/P3 hygiene (third PR)

14. ~~Pin third-party GitHub Actions + add Dependabot~~ — **done** (`.github/workflows`, `.github/dependabot.yml`).
15. ~~Migrate `misc/docker/build.Dockerfile` off CentOS 7~~ — **done** (Rocky Linux 9).
16. ~~Bump `misc/docker/lnx-aarch64.Dockerfile` to Debian 12~~ — **done** (`debian:12.13-slim`).
17. ~~Pin Android SDK and dedicated server base images~~ — **done** (Android digest; dedicated **12.13-slim**).

### Phase 4 — P4 cleanup

18. Lua 5.4.3 → 5.4.7 (overlay `libs/lua`).
19. libtheora: choose option 2 (patch) or 3 (feature removal).

---

## 5. Verification plan (per phase, minimum bar)

For each phase, run:

- Linux x86_64 client + dedicated: `./easybuild.sh -64`
- Linux aarch64 build: `docker run etlegacy/lnx-aarch64-build …`
- macOS 64-bit: `./easybuild.sh -64 --osx=10.15`
- Windows VS 2022: `cmake -G "Visual Studio 17" -A x64 -DBUNDLED_LIBS=YES ..`
- Android APK via `./gradlew :app:assembleDebug`
- `misc/collect-and-check-gh-build-logs.py` on the resulting CI run
- Smoke: launch `etl.x86_64`, connect to a local `etlded` server, fire the Legacy mod, play one Theora cinematic, render one PNG/JPG map screenshot, run one curl-based file download from the update channel — exercises every upgraded library exactly once.

---

## 6. Document maintenance

- Keep **§1** and **§2** aligned with the **checked-out `libs` commit** (run `grep` on `libs/CMakeLists.txt` and headers after submodule bumps).
- **§4** is the actionable checklist; prefer **one logical change per commit** (large overlays like FreeType may be their own PR).
- When a phase is finished, strike it through in **§4** and adjust the **Rollup** table so readers are not misled.
