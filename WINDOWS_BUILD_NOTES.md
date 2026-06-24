# Windows Build Notes (this fork)

Notes for building `custom_infill_slicer` on Windows with a **modern toolchain**
(Visual Studio 2026 / MSVC v14.5x and the CMake 4.x bundled with it). The upstream
instructions in [`doc/How to build - Windows.md`](doc/How%20to%20build%20-%20Windows.md)
still apply; this file records the extra fixes that were needed to get a clean build
on VS 2026 + CMake 4.x, plus how to do fast incremental rebuilds and how to package a
shareable bundle.

> **TL;DR for a returning developer:** the dependencies are already compiled at
> `C:\src\cis-deps`. Do **not** rebuild them from scratch (~2 h). To rebuild the app:
> ```
> build_win.bat -d C:\src\cis-deps -s app-dirty
> ```

---

## 1. Environment that worked

| Tool | Version |
|------|---------|
| Visual Studio | 2026 Community (major version **18**), MSVC toolset `14.51` (v145) |
| CMake | 4.2.3 (bundled with VS 2026) |
| Git | any recent |
| Windows | 11, with **Long Paths enabled** (`HKLM\SYSTEM\CurrentControlSet\Control\FileSystem\LongPathsEnabled = 1`) |

No standalone CMake or older VS is required — VS 2026 also ships the v142 (2019) and
v143 (2022) toolsets if you ever need an older compiler (`-T v143` to CMake).

## 2. Fixes applied to `build_win.bat`

The stock script targets VS 2019/2022 + CMake 3.x. Three changes were made so it works
with VS 2026 + CMake 4.x. If you ever pull a fresh upstream `build_win.bat`, re-apply these:

1. **Accept VS major version 18.** The supported range was `[16,18)`; bumped the upper
   bound so 18 is included:
   ```bat
   SET PS_VERSION_SUPPORTED=16
   SET PS_VERSION_EXCEEDED=19    REM was 18
   ```

2. **CMake 4.x policy shim.** CMake 4.x removed compatibility with
   `cmake_minimum_required(VERSION <3.5)`, which several bundled/downloaded deps still
   declare. An env var (inherited by every child `cmake`, including the deps
   ExternalProject sub-builds) fixes it. Added near the top, after `PS_CONFIG_LIST`:
   ```bat
   SET CMAKE_POLICY_VERSION_MINIMUM=3.5
   ```

3. **Correct the CMake generator year.** The script builds the generator name as
   `Visual Studio <major> <year>`, taking `<year>` from
   `vswhere -property catalog_productLineVersion`. For VS 2019/2022 that returns the
   calendar year (`2019`/`2022`), but for **VS 2026 it returns `18`**, yielding the
   invalid generator `Visual Studio 18 18` → CMake errors out and configure aborts.
   Map the major version to the right year instead (added after the `vswhere` query):
   ```bat
   IF "%PS_VERSION%"=="16" SET PS_PRODUCT_VERSION=2019
   IF "%PS_VERSION%"=="17" SET PS_PRODUCT_VERSION=2022
   IF "%PS_VERSION%"=="18" SET PS_PRODUCT_VERSION=2026
   ```

## 3. First-time / clean build

From the repo root (a VS Developer prompt is **not** required — the script sets up its
own environment via `vsdevcmd`):

```bat
build_win.bat -d C:\src\cis-deps -s all
```

- `-d` is the dependency **destdir** — keep it short and outside the source tree.
- `-s all` cleans and builds **deps then app**. The default config is `RelWithDebInfo`
  (optimized like Release, with symbols — fine for distribution).


## 4. Incremental app rebuilds (the common case)

After the one-time deps build, day-to-day rebuilds are fast and don't touch deps:

```bat
build_win.bat -d C:\src\cis-deps -s app-dirty
```

`app-dirty` = build the app **without cleaning**, so only changed files recompile.

## 5. Unit tests are disabled

The `tests` targets are turned **off** in the build cache:

```bat
cmake -S . -B build -DSLIC3R_BUILD_TESTS=OFF
```

Reason: `libseqarrange_tests` fails to link under `RelWithDebInfo` — it pulls in the
**Debug** Catch2 (`Catch2d.lib`, `MDd`) against **Release** code (`MD`), an
`LNK2038 _ITERATOR_DEBUG_LEVEL / RuntimeLibrary` mismatch. The application itself is
unaffected; only that test executable was failing and it was breaking `ALL_BUILD`
(hence `build_win.bat` reporting failure). With the flag cached OFF, `-s app-dirty`
builds green. If you ever do a clean app build (`-s app`) or delete `build\`, re-set the
flag with the `cmake` line above before building. (To actually *run* the tests you'd
need to fix the RelWithDebInfo→Debug Catch2 config mapping in the deps.)

## 6. Packaging a shareable bundle

[`package.ps1`](package.ps1) assembles a **portable, no-install** zip for sharing. Run it after a successful build:

```powershell
.\package.ps1
# -> ...cis-dist\custom_infill_slicer-<version>-win64.zip
```

It stages: the three launcher `.exe`s + `PrusaSlicer.dll` (the real ~92 MB binary) +
`OCCTWrapper.dll` + `libgmp-10`/`libmpfr-4`/`WebView2Loader` + the **MSVC C++ runtime**
(`vcruntime140*`, `msvcp140*`, `concrt140` — sourced from `C:\src\cis-deps\usr\local\bin`,
shipped **app-local** so recipients need no VC++ redistributable) + the full
`resources\` tree, and a `printer-profile\` folder. It excludes dev artifacts (no PDBs).

Recipients just unzip and double-click `prusa-slicer.exe`. The only soft requirement is
the Edge **WebView2 Runtime**, which is preinstalled on Win10/11.

To confirm a bundle is self-contained, extract it and run
`prusa-slicer-console.exe --help` with Visual Studio removed from `PATH` — it should
exit 0 and print the version banner using only its app-local DLLs.

## 8. Gotchas seen

- **`'vswhere.exe' is not recognized`** printed during `vsdevcmd` — cosmetic, internal to
  VS 2026's `vsdevcmd.bat`. The environment still sets up correctly (exit code 0).
- **`CMake Error: CMAKE_GENERATOR ... doesn't exist`** then `Configuring incomplete` — the
  generator-year bug (s2.3). CMake falls back to its default but still exits non-zero.
- **`prusa-slicer.exe` is tiny (~0.25 MB)** — expected. It's a launcher stub; the real
  application code is in `PrusaSlicer.dll`.
