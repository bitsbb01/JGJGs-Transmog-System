# Development decisions and remaining work

The game patcher/debug-menu concept is explicitly a separate future mod.

## Equipment integrity

Do not implement transmog by changing shared base forms or removing/replacing player inventory items. That could affect NPCs or lose per-instance tempering, charge, custom enchantments, ownership and favourite state. The read-only prototype cannot apply skins. The intended backend should replace visual inputs during player model construction while leaving gameplay records unchanged.

The public okan1100/Skyrim-Transmog-System code was reviewed, but is not copied into this project. Its dynamic inventory duplication approach is not adopted. Fitting Room's source was inspected for rendering feasibility; no code from it is included. If code is reused later, its GPL obligations and attribution must be handled explicitly.

## Runtime work still required

1. Implement and verify player-only armour/weapon model hooks on the actual 1.6.1170 runtime. Check the displaced instructions, ownership, first/third-person models, draw/sheath, race/sex, and safe refresh.
2. Resolve staged selections into an atomic visual override snapshot. Revalidate target equipment and compatibility at Apply. Preserve the old snapshot on failure.
3. Persist active overrides through SKSE serialization and favourites through a versioned format using origin plugin plus local form ID. Handle removed plugins, light plugins and quick-load/new-game resets.
4. Compile the Papyrus scripts, create a verified startup quest/VMAD plugin, and test registration, navigation, searchable pages and preset actions in SkyUI. The current .psc files are only a catalogue draft.
5. Prototype a bundled Scaleform comparison view. MCM does not itself supply two independent 3D inventory renderers. Do not imply text labels constitute the requested live preview; do not silently change the actual player equipment for a preview.
6. Collect game tests before marking any feature ready. Required cases: named enchanted/tempered items, two identical weapons with different extras, dual wielding, unequip/re-equip, selling/dropping, save/reload, transformation, custom races/body meshes, missing preset mods, head slots, shields, bows/crossbows and common animation/display mods.

## Build and release

Source and developer packages are intentionally labelled. `release.json` is not evidence by itself: set a feature true only after its implementation and validation notes are committed. The runtime packager validates file presence and signatures, but cannot prove that an ESP quest or compiled script works in game.

A future runtime release job must consume the verified runtime stage, call `package.py --mode runtime`, and attach that ZIP and checksums to a prerelease. Do not point ordinary users at the current developer DLL.
