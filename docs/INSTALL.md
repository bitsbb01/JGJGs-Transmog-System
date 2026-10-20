# Installation — future playable packages

There is currently no release-ready mod package. The `APPEARANCE-TEST` Actions artifact is an in-game test build. SOURCE and DEVELOPER-ONLY downloads are not ready to install into Skyrim.

The `APPEARANCE-TEST` Actions artifact includes an ESL-flagged ESP-FE, compiled MCM scripts, and the native SKSE plugin. It adds player armor and same-type weapon appearance selection, apply/restore, save/load serialization, ten fixed-slot outfit favorites, and a temporary on-character preview. Apply and Preview queue a player mesh refresh so the chosen appearance can be seen immediately. The experimental side-by-side scene clone was removed after it crashed in game. Re-test armor, weapons, apply, preview, cancel, and save/load in a disposable MO2/Vortex profile and save. Requires Skyrim Steam 1.6.1170, matching SKSE64, SkyUI SE, and the matching Address Library database.

When a playable prerelease is published:

1. Use Skyrim Steam 1.6.1170 with the matching SKSE64 build, SkyUI SE and Address Library's matching Anniversary Edition database.
2. Download the ZIP explicitly labelled Steam-1.6.1170 from GitHub Releases. Do not use GitHub's automatic Source code download as an installable mod.
3. In Mod Organizer 2, use Install a new mod from an archive. In Vortex, use Install From File. Enable the mod and its ESP.
4. For manual installation, extract the ZIP's Data-relative contents into Skyrim's Data directory.
5. Launch via SKSE. Open Mod Configuration > JGJGs Transmog System.

First tests must use a disposable test save/profile. Test changes, restoring originals, save/reload, dual wielding, armour slots, custom enchantments and tempering. Report the log and exact steps; do not assume successful compilation means in-game validation.

The custom 3D original/selected comparison remains under development.
