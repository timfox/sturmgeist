# Dependency Audit — ET: Legacy

Scope: every explicitly pinned third-party dependency in this repository
(bundled C/C++ libraries in `libs/`, sources downloaded via `ExternalProject_Add`
in `libs/CMakeLists.txt`, Android/Gradle dependencies in `app/build.gradle` and
`app/libs/joystick`, the Gradle wrapper, GitHub Actions, the Pixi toolchain, and
Docker base images).

> Reporting date: 2026-04-17. "Latest" reflects releases verified via
> upstream changelogs / advisories on that date.

## Status in this PR

| Phase | Status |
|---|---|
| 1 — P0 bundled C/C++ security bumps (FreeType, curl, OpenSSL, wolfSSL, libpng, cJSON) | **deferred — `libs/` is the `etlegacy/etlegacy-libs` submodule** and the actual source lives in another repo. Findings stay in this doc as the tracked plan. |
| 2 — Android hygiene | **landed in this PR** (see commits): deprecated `com.android.support.test` removed; migrated to `androidx.test:runner 1.6.2` + `rules 1.6.1` + `junit 1.2.1`; `TestETL.java` and `AndroidManifest.xml` ported; `org.jetbrains.kotlin:kotlin-bom` 1.8.0 → 2.0.21. |
| 3 — CI/Docker hygiene | **landed in this PR**: every third-party GitHub Action SHA-pinned (`addnab/docker-run-action`, `geekyeggo/delete-artifact`, `signpath/github-action-submit-signing-request`, `prefix-dev/setup-pixi`, `canonical/snapcraft-multiarch-action`, `snapcore/action-publish`, `Ilshidur/action-discord` bumped 0.3.0 → 0.3.2); first-party `actions/checkout`, `actions/upload-artifact`, `actions/download-artifact`, and `actions/setup-java` v4 refs pinned to commit SHAs; `misc/docker/lnx-aarch64.Dockerfile` debian:11.8-slim → debian:12.13-slim; `misc/docker/dedicated.Dockerfile` now pins to a concrete 12.13 instead of floating `stable-slim`; `misc/docker/android.Dockerfile` pins `thyrlian/android-sdk` by digest; `.github/dependabot.yml` added covering `github-actions`, `gradle`, and `docker`. |
| 4 — Lua 5.4.3 → 5.4.7 and libtheora remediation | deferred (lives in `libs/` submodule). |

---

## 1. Executive summary

| Priority | Dependency | Current | Recommended | Reason |
|---|---|---|---|---|
| P0 (security) | FreeType (`libs/freetype`) | 2.10.2 | 2.13.3+ | **CVE-2025-27363** (heap OOB write in TTF/variable fonts, CVSS 8.1, exploited in the wild). Affects ≤ 2.13.0. |
| P0 (security) | libcurl (`libs/CMakeLists.txt`) | 8.5.0 | 8.12.1+ | 8 CVEs fixed between 8.5.0 and 8.12.0 (netrc credential leaks, HSTS subdomain, gzip integer overflow, ASN.1 overreads, …). |
| P0 (security) | OpenSSL (`libs/CMakeLists.txt`) | 3.2.0 | 3.2.6 (LTS track) | 3.2.0 is ~2½ years old; 3.2.6 (Sep 2025) is the latest security patch of the 3.2 series. Preferable to a jump to 3.4.x for stability on this consumer. |
| P0 (security) | wolfSSL (`libs/CMakeLists.txt`) | 5.6.6 | 5.7.6+ (latest stable on the 5.7 track) | CVE-2023-6935 (Marvin/RSA timing) + 6936/6937 affect 5.6.6. |
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
| P2 | `org.jetbrains.kotlin:kotlin-bom` | 1.8.0 | 2.0.x (align with AGP 8.13 default) | Not a CVE, but 1.8.0 is ~2 years behind and forces older `kotlin-stdlib` transitively. |
| P2 | `com.jayway.android.robotium:robotium-solo` | 5.6.3 | **deprecated/abandoned** | Last release 2017. Consider removing (the instrumented tests do not appear to be wired into CI) or migrating to Espresso. |
| P2 | `com.android.support.test:runner:1.0.2` | 1.0.2 | `androidx.test:runner:1.6.2` | `com.android.support.*` is deprecated. Must move to AndroidX equivalents for newer AGP/androidx transitive chain. |
| P2 (CI) | `Ilshidur/action-discord@0.3.0` | 0.3.0 | 0.3.2 (pinned SHA) | Pin to a SHA (Dependabot best practice for third-party Actions). |
| P2 (CI) | `addnab/docker-run-action@v3` | floating `v3` | pin to SHA | supply-chain hygiene. |
| P2 (CI) | `canonical/snapcraft-multiarch-action@v1.10.1` | 1.10.1 | latest on `v1` | tag is fine but pinning to SHA preferred. |
| P3 | Docker `misc/docker/build.Dockerfile` → `centos:7` | centos:7 | replace | **CentOS 7 reached EOL on 2024-06-30**. CI still succeeds, but no security updates; should migrate to Rocky 9 / Alma 9 or Debian 12. |
| P3 | Docker `misc/docker/lnx-aarch64.Dockerfile` → `debian:11.8-slim` | 11.8 | `debian:12-slim` | Debian 11 LTS ends 2026-08-31. Easy swap. |
| P3 | Docker `misc/docker/android.Dockerfile` → `thyrlian/android-sdk:latest` | `:latest` | pin digest | `:latest` is non-reproducible. |
| P3 | Docker `misc/docker/dedicated.Dockerfile` → `debian:stable-slim` | floating | pin to `debian:12.x-slim` | reproducibility. |
| P4 | Lua (`libs/lua`) | 5.4.3 | 5.4.7 | Bugfixes only; same API; mods unaffected. |
| P4 | libogg (`libs/ogg`) | 1.3.5 | 1.3.5 | current |
| P4 | libvorbis (`libs/vorbis`) | 1.3.7 | 1.3.7 | current |
| P4 | libtheora (`libs/theora`) | 1.1.1 (2009) | **no upstream** | Upstream unmaintained; 1.1.1 is the last release. Impacted by CVE-2024-56431 and CVE-2026-5673 (both out-of-bounds reads on malformed streams). Theora is a client-only codec used by the cinematic decoder; decisions below. |
| P4 | OpenAL Soft (`libs/openal`) | 1.19.1 | 1.23.1+ | Many platform fixes; API-compatible. |
| P4 | findlocale (`libs/findlocale`) | 0.46 (2005) | no upstream | vendored, tiny, no network input — leave as-is. |
| P4 | Pixi toolchain (`pixi.toml`) | git 2.49, python 3.13.3, requests 2.32.4, etc. | keep; add `dependabot.yml` or equivalent | already near-current. |

### What "high-confidence" upgrade means here

A "high-confidence" upgrade is one where:
1. The new release is a drop-in API/ABI match for how this repository consumes the library (checked against `src/` usages).
2. The release notes contain no new soname bump or configure-flag rename that would flow into our `ExternalProject_Add` or patch files.
3. The library is built via `ExternalProject_Add` from a tarball URL we control (so the upgrade is a one-line URL + hash change that CI can exercise end-to-end).

All P0 items listed below are "high-confidence" by that rule.

### What is NOT a high-confidence upgrade

- SDL2 2.30.9 → SDL3: SDL3 renames `SDL_main.h`, removes `SDL_CreateRGBSurface`, and breaks `SDL_WINDOW_*` flags we rely on. **Do not bundle SDL3 yet.** Keep 2.30.9 (current, receives bugfixes via sdl2-compat downstream).
- OpenSSL 3.2 → 3.4: 3.4 removes legacy ENGINE plumbing and low-level `EVP_*_meth_*` symbols; curl builds fine, but our `./Configure` recipe and the Windows NMake build step need retesting on all 5 CI hosts. Stay on 3.2.6 for the patch train.
- ~~AGP 8.2.2 → 8.13 inside `app/libs/joystick`~~ **Done in-tree**: JoyStick sources live under `app/libs/joystick/` and track the root AGP line.

---

## 2. Evidence and citations

| Finding | Source (file and exact location) |
|---|---|
| FreeType 2.10.2 in tree | `libs/freetype/include/freetype/freetype.h` defines `FREETYPE_MAJOR 2 / MINOR 10 / PATCH 2`. |
| libcurl 8.5.0 URL | `libs/CMakeLists.txt` → `CURL_DOWNLOAD_URL "https://github.com/curl/curl/releases/download/curl-8_5_0/curl-8.5.0.tar.gz"`. |
| OpenSSL 3.2.0 URL | `libs/CMakeLists.txt` → two `ExternalProject_Add(bundled_openssl ... openssl-3.2.0.tar.gz)` blocks (Win/Unix). |
| wolfSSL 5.6.6 URL | `libs/CMakeLists.txt` → `URL https://github.com/wolfSSL/wolfssl/releases/download/v5.6.6-stable/wolfssl-5.6.6.tar.gz`. |
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
| CentOS 7 base image | `misc/docker/build.Dockerfile` → `FROM centos:7`. |
| Debian 11.8 | `misc/docker/lnx-aarch64.Dockerfile` → `FROM debian:11.8-slim`. |
| android-sdk:latest | `misc/docker/android.Dockerfile` → `FROM thyrlian/android-sdk:latest`. |

---

## 3. Likely breaking changes per P0/P1 upgrade

### FreeType 2.10.2 → 2.13.3

- No public-header breakage between 2.10 and 2.13. `FT_Init_FreeType`, `FT_Load_Char`, `FT_Get_Kerning`, `FT_Bitmap_Convert`, `FT_Get_Advance` (used by this repo in `src/renderercommon/tr_font.c`) are unchanged.
- CMake build: `DISABLE_FORCE_DEBUG_POSTFIX=ON` (already passed from `libs/CMakeLists.txt`) is still accepted.
- ABI: `libfreetype.a` filename unchanged on all platforms.
- Risk: 2.11 added mandatory default-include of `zlib` / `bzip2` / `png` *detection*, but because we pass `-DCMAKE_DISABLE_FIND_PACKAGE_*` and `-DFT_WITH_*=OFF`, the existing flags cover it.
- **Confidence:** high.

### libcurl 8.5.0 → 8.12.1

- We pass `-DCURL_DISABLE_*` flags that still exist in 8.12. 8.11 renamed `CMAKE_USE_GSSAPI` → `CURL_USE_GSSAPI`; we pass `-DCMAKE_USE_GSSAPI=OFF` but `CURL_USE_GSSAPI` defaults to OFF, so behavior is preserved — the old flag is silently ignored. Consider renaming during the upgrade.
- `-DENABLE_MANUAL=OFF`, `-DBUILD_CURL_EXE=OFF`, `-DENABLE_ARES=OFF`, `-DCURL_WINDOWS_SSPI=OFF`, `-DCURL_USE_LIBPSL=OFF`, all still recognized through 8.12.
- ABI: linked statically, soname not a concern.
- Runtime API usage in `src/client/cl_main.c` (easy-perform), `src/qcommon/dl_main_curl.c`, and `src/server/sv_update.c` uses only `curl_easy_*` / `curl_multi_*` / `curl_slist_*` — all stable.
- **Confidence:** high.

### OpenSSL 3.2.0 → 3.2.6

- Same series, patch only.
- Our `./Configure linux-x86_64 / darwin64-x86_64 / darwin64-arm64 / linux-x86 / linux-aarch64 / VC-WIN64 / VC-WIN32` targets are unchanged in 3.2.6.
- NMake / make targets `install_sw` still exist.
- **Confidence:** high.

### wolfSSL 5.6.6 → 5.7.6

- `WOLFSSL_CURL=yes` / `WOLFSSL_OPENSSLEXTRA=yes` / `WOLFSSL_ASIO=yes` / `WOLFSSL_CRL=yes` still present.
- 5.7.0 added `--enable-secure-renegotiation` by default on the CMake side — we do not test against it and it is off by default.
- The `WolfSSL.patch` in `libs/patches/WolfSSL.patch` was written against 5.6.x — **needs verification** against the 5.7 CMakeLists. This is the only non-trivial risk in the P0 batch.
- **Confidence:** medium (patch refresh likely required).

### libpng 1.6.47 → 1.6.50

- Patch-level change inside 1.6.x. No API or ABI delta. Already uses our `-DPNG_SHARED=OFF -DPNG_STATIC=ON -DPNG_TOOLS=OFF -DPNG_TESTS=OFF`.
- **Confidence:** high.

### cJSON 1.7.15 → 1.7.18

- `libs/cjson` is vendored sources (`cJSON.c`/`cJSON.h`), built by `BUNDLED_CJSON` block. Overwrite source files.
- API unchanged. Consumer usages in `src/game/g_cmds.c`, `src/qcommon/json.c` rely on `cJSON_Parse`, `cJSON_GetObjectItem`, `cJSON_Print`, `cJSON_AddStringToObject` — all stable.
- **Confidence:** high.

### libjpeg-turbo 2.0.4 → 3.0.4

- 3.0 dropped the non-TurboJPEG API subset `jpeg_mem_*` renaming; we use the static `turbojpeg`/`libjpeg` API through `tjInitCompress`/`tj3*`. Need to check `src/renderercommon/tr_image_jpg.c` for `tjEncodeJPEG` vs. new `tj3CompressFromYUV` signatures.
- Safer intermediate: 2.1.5.1 (no API break from 2.0.x).
- ABI: we link the static library, so soname drift does not matter.
- **Confidence:** medium. Suggest a two-step: 2.0.4 → 2.1.5.1 (quick), then 2.1.5.1 → 3.0.4 (separate PR).

### SQLite 3.36.0 → 3.46.x

- Amalgamation drop-in. No API break. Only subtlety is that 3.37 made `STRICT` tables valid SQL syntax, and 3.45 changed the default of `SQLITE_DQS` to `0` (double-quoted string literals reject) — the mod's SQL in `src/game/sqlite_helpers.c` uses only single-quoted literals, so this is safe.
- **Confidence:** high.

### GLEW 2.1.0 → 2.2.0

- No API break. We ship GLEW as vendored sources; the upgrade is an overwrite of `libs/glew/{src,include}` from the 2.2.0 tarball.
- **Confidence:** high.

### OpenAL-Soft 1.19.1 → 1.23.1

- CMake variables used (`LIBTYPE=STATIC`, `ALSOFT_*=OFF`) all still exist.
- Public header path (`AL/al.h`, `AL/alc.h`) unchanged. Consumer is `src/client/snd_openal.c` using only `alSourcef`, `alSourcei`, `alBufferData`, `alGenSources`, `alDeleteSources`, `alcOpenDevice`, `alcCloseDevice` — stable since 1.19.
- 1.22 made `ALSOFT_BACKEND_SNDIO=OFF` redundant (still accepted). 1.23 dropped direct SSE2 unless `CMAKE_POLICY_VERSION_MINIMUM` is set — this repo already sets `CMAKE_POLICY_VERSION_MINIMUM` in `etl_setup_cmake_args`, so it works.
- **Confidence:** high, but needs a CI smoke on all three desktop targets.

### libtheora 1.1.1

- Upstream is dead. The two known CVEs (both heap OOB reads) are triggered by malformed *input cinematics*. ET: Legacy ships signed cinematics shipped inside `pak0.pk3`; they are the only realistic consumer. Because we never consume untrusted Theora streams over the network, the CVE risk is limited to maliciously crafted demo files shipped by third parties.
- Options:
  1. **Keep 1.1.1** and add a one-line note in `SECURITY.md` about the untrusted-demo risk.
  2. **Vendor one of the Debian / Xiph-master patch sets** that fixes the CVE without a full rebase.
  3. **Drop Theora entirely** (`-DFEATURE_THEORA=OFF` already the Android path) and remove `src/client/cl_cin.c` Theora backend.
- Recommendation: **option 2** for the next minor; option 3 is a long-horizon discussion.

---

## 4. Smallest safe update plan

A "minimal" first PR means every change is:

- One-line URL + hash swap in `libs/CMakeLists.txt`, OR
- A verbatim tarball overlay inside `libs/<lib>/`, OR
- A one-line version pin in `app/build.gradle` / `build.gradle`.

No patches, no build-system refactors, no API code touched.

### Phase 1 — P0 security (single PR, one commit per lib)

1. **FreeType 2.10.2 → 2.13.3** — overlay `libs/freetype` from the upstream 2.13.3 tarball. (This is the biggest overlay. Consider making this its own PR if the submodule stores freetype there.)
2. **libcurl 8.5.0 → 8.12.1** — edit `libs/CMakeLists.txt`:
   - `CURL_DOWNLOAD_URL` → `.../curl-8_12_1/curl-8.12.1.tar.gz`
   - `CURL_DOWNLOAD_HASH` → MD5 from https://curl.se/download.html
   - rename `-DCMAKE_USE_GSSAPI=OFF` → `-DCURL_USE_GSSAPI=OFF`
3. **OpenSSL 3.2.0 → 3.2.6** — edit both `ExternalProject_Add(bundled_openssl …)` blocks:
   - `URL …/openssl-3.2.6.tar.gz` / `URL_HASH MD5=<from openssl.org/source/old>`.
4. **wolfSSL 5.6.6 → 5.7.6** — edit `ExternalProject_Add(bundled_wolfssl …)`:
   - `URL …/v5.7.6-stable/wolfssl-5.7.6.tar.gz` / hash.
   - Verify `libs/patches/WolfSSL.patch` still applies (`git apply --check` at build time is already enforced via `PATCH_COMMAND`); if not, regenerate against the 5.7.6 tree.
5. **libpng 1.6.47 → 1.6.50** — overlay `libs/libpng`.
6. **cJSON 1.7.15 → 1.7.18** — overwrite `libs/cjson/cJSON.{c,h}` and `libs/cjson/cJSON_Utils.{c,h}`.

### Phase 2 — P1 stability (second PR)

7. **libjpeg-turbo 2.0.4 → 2.1.5.1** — overlay `libs/jpegturbo`.
8. **SQLite 3.36.0 → 3.46.x** — overlay `libs/sqlite3/src/{sqlite3.c,sqlite3.h,sqlite3ext.h,shell.c}`.
9. **GLEW 2.1.0 → 2.2.0** — overlay `libs/glew`.
10. **OpenAL-Soft 1.19.1 → 1.23.1** — overlay `libs/openal`.
11. **Gradle: JoyStick AGP** — sources are vendored in `app/libs/joystick/`; keep its `classpath 'com.android.tools.build:gradle:…'` aligned with the root `build.gradle` when upgrading AGP.
12. **Kotlin BOM 1.8.0 → 2.0.21** in `app/build.gradle`.
13. **Replace `com.android.support.test:runner:1.0.2` with `androidx.test:runner:1.6.2`** (or drop the instrumented test stanza entirely if unused).

### Phase 3 — P2/P3 hygiene (third PR)

14. Pin all third-party GitHub Actions (`Ilshidur/action-discord`, `addnab/docker-run-action`, `canonical/snapcraft-multiarch-action`, `snapcore/action-publish`, `geekyeggo/delete-artifact`, `signpath/github-action-submit-signing-request`) to explicit commit SHAs and add `dependabot.yml` or `renovate.json`.
15. Migrate `misc/docker/build.Dockerfile` off CentOS 7 (EOL since 2024-06-30). Suggested target: Rocky Linux 9 or Debian 12.
16. Bump `misc/docker/lnx-aarch64.Dockerfile` `debian:11.8-slim` → `debian:12-slim`.
17. Pin `misc/docker/android.Dockerfile` and `misc/docker/dedicated.Dockerfile` to image digests.

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

## 6. What I intentionally did NOT change yet

- No `CMakeLists.txt` edits.
- No `libs/*` version bumps.
- No `app/build.gradle` edits.
- No CI pinning.

This document is the "plan before editing code" the task asked for. Each numbered item above is intended to become its own commit (or small PR when the diff is large, e.g. FreeType).
