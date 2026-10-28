# V03 field surround

Visual authoring only; no server map, navigation or collision behavior is added.

- `Zone1Meadow`: Unity built-in Plane, fixed world center (50, -0.035, 50), 240 × 240 m, bounds [-70,170] on X/Z. Existing Zone 1 movement coordinates remain [-5,105].
- `Meadow.mat`: clone of the project's self-authored V02 EarthAndGrass material/texture, with the same 64 m texture period and world phase. The old short world ground renderer is hidden to avoid overlapping surfaces. The login diorama keeps its original ground.
- 48 outer pines, 24 rocks and four distant ridges reuse existing self-authored V02 meshes and materials. Tree trunks lie outside the playable square. Decorations have no Collider. The old short world ridge is hidden because it crossed playable coordinates; the diorama remains unchanged.
- `DistantRidge.mat`: clone of the existing V02 MountainHaze material. No new shader/package or project fog setting.
- Central landmark around (50,50): three short arcs totaling 12 small buried stones, seven small scattered paving remnants, two broken wall fragments up to 1.3 m high, and a grass-edged trail from the original gate. About 80% of the circumference is empty. These are decorative remnants, not a skill range ring, server encounter boundary, collision arena or preplaced boss.
- 23 sparse field clusters reuse five small V02 grass clumps and one ankle-height stone each. Tall trees remain outside the playable square. `OldArenaStone.mat` and `WornTrail.mat` reuse the existing stone/earth textures; no new bitmap, shader or package.
- `C02SceneBuilder.ApplyV03()` rebuilds only `C02World/V03FieldSurround` and the two original world renderer visibility settings. Existing own-character and C04 prefabs, UI and runtime bindings are retained.

Source/license: existing repository-original V02 assets plus Unity's built-in Plane mesh. Layout/material variations authored for this project through Unity Editor APIs; no third-party download, purchased asset or external image/3D generation service. Existing V02 source and license notes continue to apply. No animation/rig is created.

Static evidence: `UnityClient/Captures/v03-static/` contains Editmode QuarterViewCamera renders. The latest landmark revision is shown at (39,39), (50,50), (105,105); the other corner PNGs are from the initial coverage revision. These are synthetic camera-position checks, not evidence of login, input or server movement. The Coordinator's C05 batch owns runtime validation.
