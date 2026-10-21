# JGJGs Transmog System

**In development. There is no installable transmog release yet.** Target: Skyrim Steam **1.6.1170**.

## Download packages

[Actions](https://github.com/bitsbb01/JGJGs-Transmog-System/actions) builds source ZIPs and a developer-only Windows component after successful checks. GitHub may require sign-in to download Actions artifacts.

Version tags beginning with `v` publish a public **source prerelease** on [Releases](https://github.com/bitsbb01/JGJGs-Transmog-System/releases), after both build jobs succeed. These source downloads do not require a mod manager and are not playable packages.

The runtime packager supports a Data-relative ZIP suitable for MO2, Vortex and manual installation, plus SHA-256 checksums. It deliberately refuses to package unfinished releases or missing/invalid DLL, PEX and ESP files. The separate **Package playable mod** workflow builds, checks and publishes installable prerelease ZIPs, but stops immediately until those components exist and are marked verified. Current source/developer artifacts are not offered as installable mods.

## Agreed behaviour

- Standard SkyUI MCM interface, without PrismaUI or MCM Helper.
- Same-type weapon appearance swaps (including distinct battleaxe/warhammer and bow/crossbow categories).
- Armour compatibility filtered by equipment slots and player race/sex model availability.
- Preserve original equipment, stats, tempering and enchantments.
- Browse compatible installed items without owning them, with alphabetical sorting, name/plugin filters and pages of 25 (maximum 50).
- Temporary on-character appearance preview before Apply; Cancel changes nothing.
- Ten named outfit favourites covering armour, shield and both hands. Skip and report incompatible/missing slots.
- Save/load restoration and restore-original controls.

## What exists now

| Component | Status |
|---|---|
| Catalogue, filtering and pagination | Implemented and tested with synthetic data |
| Preset planning and staged selection | Implemented and tested; in-memory only |
| Catalogue and compatibility filters | Implemented; portable tests cover the core and Windows CI builds the SKSE adapter |
| Player armor appearance hook | Implemented for Steam 1.6.1170; deferred same-instance refresh added, in-game validation pending |
| Player weapon appearance hook | Same-type swaps confirmed in game after sheathe/draw; automatic refresh still under test |
| Apply, restore and temporary on-character preview | Apply is confirmed for weapons; armor, temporary preview and cancel restoration still need in-game validation |
| Armor and weapon save/load | Implemented through SKSE serialization; confirmed working in game |
| MCM test assets | Compiled in CI and attached to an ESL-flagged test ESP; confirmed working in game |
| Ten combined outfit favorite slots | Armor and weapon appearance choices; confirmed working in game |
| Side-by-side 3D preview | Disabled; a separate isolated UI render scene is needed |
| Complete playable package | Not ready; armor and preview validation and the separate 3D viewer remain outstanding |

The catalogue adapter snapshots form metadata on the game task queue, then sorts copied data on a worker. Appearance mappings are stored without replacing the equipped items. A development-status page now reports refresh and renderer-hook counters to diagnose why models are not changing. Synthetic core tests are not game benchmarks.

## Dependencies

Native component: matching SKSE64 plus **Address Library for SKSE Plugins** for 1.6.1170. MCM integration additionally needs SkyUI SE. Address Library is a native address database, not a UI framework. It is an explicit additional dependency of this initial implementation.

Build dependencies: MSVC x64, CMake, vcpkg (spdlog, rapidcsv), pinned CommonLibSSE-NG. No proprietary game files are included. The final MCM build also needs a Papyrus compiler and appropriate script interfaces.

## Build

Portable core: `cmake -S . -B build`, `cmake --build build`, `ctest --test-dir build`.

Windows component: use the Windows workflow, or configure with `-DJGJG_BUILD_SKSE=ON` and your vcpkg toolchain. It refuses runtimes other than 1.6.1170 before initializing the engine interface.

Packaging: `python tools/package.py --mode source`; developer component: `--mode developer --stage build/Release`; future verified playable package: `--mode runtime --stage path/to/Data`.

See [installation guidance](docs/INSTALL.md), [development decisions](docs/DEVELOPMENT.md), and `release.json` for the explicit release gate. A successful build alone does not assert game compatibility.
