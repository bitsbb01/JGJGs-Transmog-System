# Development decisions and remaining work

The game patcher/debug-menu concept is explicitly a separate future mod.

## Equipment integrity

Do not implement transmog by changing shared base forms or removing/replacing player inventory items. That could affect NPCs or lose per-instance tempering, charge, custom enchantments, ownership and favourite state. The current armor prototype swaps the visual armor input during player model construction and leaves gameplay records unchanged. Weapon rendering and outfit favorites are not implemented.

The public okan1100/Skyrim-Transmog-System code was reviewed, but is not copied into this project. Its dynamic inventory duplication approach is not adopted. Fitting Room's source was inspected for rendering feasibility; no code from it is included. If code is reused later, its GPL obligations and attribution must be handled explicitly.

## Runtime work still required

1. Validate the existing player-only armor hook on the actual 1.6.1170 runtime. Check the displaced instructions, first/third-person models, race/sex, safe refresh, and whether visual swaps preserve the equipped item instance.
2. Implement and validate weapon rendering for left/right hands, dual wielding, bows/crossbows, draw/sheath, and equipment refresh. Do not ship an unverified hook that can affect NPCs or corrupt the render tree.
3. Extend the current ten armor-only favorite slots into editable named, multi-slot outfit favorites that include weapons. Revalidate target equipment and compatibility at Apply; preserve the previous visual state if an operation fails.
4. Extend versioned SKSE serialization from the current armor overrides and fixed-slot armor favorites to weapon overrides and complete outfit favorites. Use origin plugin plus local form ID and handle removed plugins, light plugins and quick-load/new-game resets.
5. The Papyrus scripts and ESL-flagged MCM test ESP compile in CI. Test script registration, quest startup, search, navigation, apply/restore, and save/load in SkyUI before release.
6. Build a side-by-side 3D comparison view. The current temporary armor preview changes the player's visible armor and is not a comparison renderer. MCM does not itself supply two independent 3D inventory renderers; do not imply text labels are a live preview.
7. Collect game tests before marking any feature ready. Required cases: named enchanted/tempered items, two identical weapons with different extras, dual wielding, unequip/re-equip, selling/dropping, save/reload, transformation, custom races/body meshes, missing preset mods, head slots, shields, bows/crossbows and common animation/display mods.

## Build and release

Source and developer packages are intentionally labelled. `release.json` is not evidence by itself: set a feature true only after its implementation and validation notes are committed. The runtime packager validates file presence and signatures, but cannot prove that an ESP quest or compiled script works in game.

The readiness-gated runtime release job consumes `package/Data`, calls `package.py --mode runtime`, and attaches that ZIP and checksums to a prerelease. Runtime asset compilation/staging remains to be implemented. Do not point ordinary users at the current developer DLL.
