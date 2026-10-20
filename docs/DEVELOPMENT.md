# Development decisions and remaining work

The game patcher/debug-menu concept is explicitly a separate future mod.

## Equipment integrity

Do not implement transmog by changing shared base forms or removing/replacing player inventory items. That could affect NPCs or lose per-instance tempering, charge, custom enchantments, ownership and favourite state. The user confirmed the MCM and catalogue appear in game on Steam 1.6.1170. The latest appearance build reported success, but Apply did not change armor or weapons, Preview did nothing, and the previous 3D scene clone crashed. Do not describe visual appearance swapping or preview as working until verified in game.

The public okan1100/Skyrim-Transmog-System code was reviewed, but is not copied into this project. Its dynamic inventory duplication approach is not adopted. Fitting Room's source was inspected for rendering feasibility; no code from it is included. If code is reused later, its GPL obligations and attribution must be handled explicitly.

## Runtime validation still required

1. Use the MCM Development status page's refresh, render-call, and swap counters to locate the broken model update path. Validate armor and weapon Apply, temporary Preview, Cancel, and restoration in game.
2. Add a separate preview menu backed by an isolated UI render scene; do not clone and attach player scene graphs to the live world scene or label an empty MCM column as a viewer.
3. Complete broader compatibility testing before release: named enchanted/tempered items, two identical weapons with different extras, dual wielding, unequip/re-equip, selling/dropping, save/reload, transformation, custom races/body meshes, missing preset mods, head slots, shields, bows/crossbows and common animation/display mods.

## Build and release

Source and developer packages are intentionally labelled. `release.json` is not evidence by itself: set a feature true only after its implementation and validation notes are committed. The runtime packager validates file presence and signatures, but cannot prove that an ESP quest or compiled script works in game.

The readiness-gated runtime release job consumes `package/Data`, calls `package.py --mode runtime`, and attaches that ZIP and checksums to a prerelease. Runtime asset compilation/staging remains to be implemented. Do not point ordinary users at the test build as a release.
