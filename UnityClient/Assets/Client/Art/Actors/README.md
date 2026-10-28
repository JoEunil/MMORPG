# C04 observation visuals

Four pure visual Prefabs support only the existing snapshot kinds. The cosmetic goblin/orc/golem descriptions do not add server names, abilities, equipment effects or monster definitions.

| Prefab | Runtime mapping | Appearance |
| --- | --- | --- |
| ObservedPlayer | Other player Full | Existing V02 adult guard visual, cloned with the same shared meshes/materials and its existing1.2 display scale |
| MonsterType0 | monsterId0 | Lean, small ash-olive goblin; long ears, ragged umber cloth, leather straps and narrow dagger |
| MonsterType1 | monsterId1 | Broader adult orc; weathered olive skin, leather bracers/baldric, one forged shoulder plate and axe |
| MonsterType2 | monsterId2 | Larger slate golem; irregular segmented stone masses, moss at shoulder seams, recessed stone face |

All roots are identity scale1, pivot at groundy0 and face local-Z. No colliders, UI/raycast targets, MonoBehaviours, AI, movement or animation are included. No names, health values or other game-stat labels are baked into these assets. OwnCharacter's hierarchy, shared V02 meshes/materials and server coordinate mapping stay unchanged. Runtime instantiation/position/direction/lifetime belong to C04ObservedWorld, not the Prefabs.

Source/tool: original C04 geometry and color/material values are authored by C02SceneBuilder.BuildC04Assets with the Unity Mesh/Material/PrefabUtility APIs, driven through the assigned live Unity CLI Editor connection. No external image/model generator, downloaded resources, paid service or asset purchase was used. The player reuses original project V02 content; the three monster meshes are original procedural profile surfaces, small explicit face/ear/weapon surfaces and irregular rock surfaces. Existing Unity primitives used for tiny details follow Unity's engine-resource license. New content was authored for this repository; existing V02/license conditions are unchanged. No third-party monster art or new textures/fonts were imported.

Rendering: URP/Lit. Nonmetallic skin/leather/cloth/stone use smoothness0.14; DarkForgedIron uses metallic0.45/smoothness0.28. Distinct muted green, brown and slate silhouettes are intended to read at the fixed quarterview distance. Monster heights target approximately1.5/2.2/3.2 Unity units; exact renderer bounds and triangle observations are recorded in Captures/c04-art-freeze.txt.

Editor integration: C04ObservedWorld sits on the active C02Client. displayRoot is the empty C02World/C04Observations child; playerPrefab and monsterPrefabs[0..2] bind these exact assets. ApplyC04 creates assets/binds/saves without rebuilding the V02 environment. Existing ApplyV01/Build regeneration finishes with the same binding, preserving C04 references. The existing ten-row character selection and top-stretch scroll content are retained.

Validation boundary: Captures/c04-art-quarterview.png is a separate synthetic Editor art preview, left-to-right player/type0/type1/type2. It was directly inspected for size, silhouette and material distinction. No Play, live input, server observation or injected runtime snapshot is used as art validation. Actual C04 functionality and snapshot/visual matching require the Coordinator's frozen integration test.
