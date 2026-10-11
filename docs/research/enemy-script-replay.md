# Enemy Map Script functions under Shared Callback replay

Findings for #84, part of the Shared Enemies map (`amnesia-spelos/amnesia-multiplayer#51`). The question: when a Shared Callback (ADR 0006 in `amnesia-multiplayer`) replays a Map Script callback in the other player's game, how does each enemy-related script function behave? Does it refer to "the player" implicitly, and does running it in both games duplicate, conflict with, or agree with a Shared Enemy?

Engine paths are relative to `src/amnesia/src/game/`. Chapter 1 Map Scripts are not in this repository. They were read from a Steam install, `Amnesia The Dark Descent/maps/main/ch01/*.hps` (cited as `ch01/<file>.hps:line`). Enemy names come from each map's `.map` file. The Brute (`servant_brute.ent`) is an `Enemy_Grunt` with different values, so Grunt findings cover it too. The Kaernk is `Enemy_WaterLurker` (`LuxBase.cpp:1508-1509`). ManPig-only functions (`ChangeManPigPose`, `SetTeslaPig*`, `ForceTeslaPigSighting`, `LuxScriptHandler.cpp:647-652`) are left out.

## Short answer

- **Every enemy function is a plain setter on a named enemy, with two exceptions.** It runs on the game's own copy of the enemy. When both games run the same callback, both copies get the same setting.
- **`AddEnemyPatrolNode` is the only one that duplicates.** It appends to a list. Running it twice gives the route twice, unless the same callback calls `ClearEnemyPatrolNodes` first, which most chapter 1 call sites do.
- **`ShowEnemyPlayerPosition` and `AlertEnemyOfPlayerPresence` are the only functions that read "the player".** They read the local player's position. Under replay, each game aims its enemy at its own player, not at the player who triggered the callback. Chapter 1 never calls them.
- **The real gaps are around the functions, not in them.** In chapter 1, enemies are set up by callbacks that ADR 0006 keeps local:
  - pickup callbacks (`05_wine_cellar`, `12_storage`)
  - checkpoint callbacks
  - collide callbacks with the enemy as parent
  - the `OnAutoDisabled` entity callback

  The engine also activates, routes, deactivates, and fades an enemy based on the local player alone.

## How an enemy sees "the player"

All player references go through `gpBase->mpPlayer`, the local player. An Avatar is never one of them.

- `DistToPlayer` measures to the local player's body, and returns 100000 when that player is dead (`LuxEnemy.cpp:2115-2120`).
- `UpdatePlayerInRange` sends `PlayerInRange` or `PlayerOutOfRange` against `ActivationDistance` (`LuxEnemy.cpp:1504-1519`). It is 40 for the Grunt and the Brute (`servant_grunt.ent`, `servant_brute.ent`).
- A Grunt in Idle starts its patrol only on `PlayerInRange` (`LuxEnemy_Grunt.cpp:176-200`). It deactivates itself on `PlayerOutOfRange` (`:164-165`). An activated enemy therefore stands at its spawn point in a game whose player is more than 40 m away, while the triggering player's copy walks its route.
- At its last patrol node, a Grunt removes itself and runs the `OnAutoDisabled` entity callback, but only if the local player has not seen it and is more than 10 m away (`LuxEnemy_Grunt.cpp:980-990`). "Seen" is set by the local player's sanity check (`LuxPlayerHelpers.cpp:1399-1438`).
- A hallucination fades to smoke when the local player comes within `HallucinationEndDist` (`LuxEnemy.cpp:1719-1727`).
- `TriggersDisabled` also returns true during the local player's flashback (`LuxEnemy.cpp:1779-1782`).
- Hunt, Alert, and attack states path to `mvLastKnownPlayerPos` or the local player's feet (for example `LuxEnemy_Grunt.cpp:372`, `:712`, `:818`; `LuxEnemy_WaterLurker.cpp:412`).

## The functions

Registrations are at `LuxScriptHandler.cpp:636-653`. Each one runs over every enemy that matches the name (`BEGIN_SET_PROPERTY`, `:362-369`).

| Function | What it changes | Implicit player? | Both games run it | Against a Shared Enemy |
|---|---|---|---|---|
| `SetEnemyDisabled(name, b)` | `mbDisabled`. When true, it stops the pathfinder (`LuxEnemy.cpp:1139-1148`) and `OnUpdate` returns early (`:627`). The enemy freezes in place, still active and visible. | No | Agree | Agree. It must stop the simulating copy too. |
| `SetEnemyIsHallucination(name, b)` | `mbHallucination` only (`LuxEnemy.h:488`). | No, but its effect uses local distance (`LuxEnemy.cpp:1719-1727`) | Agree on the flag. Each copy fades when its own player gets close, so the two copies vanish at different times. | Conflict on the fade unless one game decides it |
| `FadeEnemyToSmoke(name, sound)` | Bone particles, mesh dissolve, `SetActive(false)`, optional GUI sound (`LuxEnemy.cpp:1041-1075`) | No | Agree. Each game shows its own effect. | Agree, as for deactivation |
| `SetEnemyDisableTriggers(name, b)` | `mbDisableTriggers`. When true, a Grunt that is not Idle drops back to Patrol (`LuxEnemy.cpp:1127-1135`; `LuxEnemy_Grunt.cpp:927-932`). It filters senses and messages (`LuxEnemy.cpp:858-865`). | No | Agree | Agree on the flag. The "back to Patrol" is a state change the simulating copy must make. |
| `SetEnemySanityDecreaseActive(name, b)` | `mbCausesSanityDecrease` (`LuxEnemy.h:486`). When false, the player's sight check also never marks the enemy seen (`LuxPlayerHelpers.cpp:1399-1402`), so it can auto-remove in plain view. | No | Agree | Agree. Sanity is each player's own. |
| `ShowEnemyPlayerPosition(name)` | Sets `mvLastKnownPlayerPos` to the **local player's** feet (`LuxEnemy.cpp:825-828`). Then the state goes to Hunt. The guard uses `\|\|`, so it is always true, and the call even interrupts an attack or a door break (`LuxScriptHandler.cpp:3089-3106`). | **Yes** | Conflict: each game's copy hunts its own player | Conflict: it names the wrong player in the non-triggering game |
| `AlertEnemyOfPlayerPresence(name)` | Same position write. Then the state goes to Search unless Hunt, an attack, or BreakDoor (`LuxScriptHandler.cpp:3108-3125`). It skips `iLuxEnemy::AlertOfPlayerPresence` and its `TriggersDisabled` check (`LuxEnemy.cpp:830-839`). | **Yes** | Conflict, as above | Conflict, as above |
| `AddEnemyPatrolNode(name, node, wait, anim)` | Appends to `mvPatrolNodes` (`LuxScriptHandler.cpp:3142-3165`; `LuxEnemy.cpp:1079-1089`) | No | **Duplicates** unless a `ClearEnemyPatrolNodes` runs first in the same callback | Duplicates likewise in the simulating game, if it receives both its own trigger and the replay |
| `ClearEnemyPatrolNodes(name)` | Empties the list and resets the current node to 0 (`LuxEnemy.cpp:1091-1096`) | No | Agree (idempotent) | Agree |
| `TeleportEnemyToNode` / `TeleportEnemyToEntity` | Sets the body's feet position (`LuxScriptHandler.cpp:3187-3235`) | No | Agree | Agree in the simulating game. The other game's position is overwritten by whatever shows the enemy. |
| `GetEnemyStateName(name)` | Read only (`LuxScriptHandler.cpp:3314-3325`) | No | Each game reads its own copy, so branches on it diverge | Only the simulating game's answer is meaningful |
| `SetEntityActive` on an enemy | `iLuxEntity::SetActive` returns early if unchanged (`LuxEntity.cpp:145-152`). `OnSetActive(false)` drops the enemy from attack and search music and from terror, and sets Idle (`LuxEnemy.cpp:2247-2271`). | No | Agree (idempotent) | Agree. This is how chapter 1 activates enemies. |
| `SetEntityPos` / `SetEntityPosRot` on an enemy | Feet position, yaw, and pitch (`LuxScriptHandler.cpp:2276-2310`) | No | Agree | As for the teleports |

Two general points:

- **One-shot behaviour.** ADR 0006 notes that two players triggering the same one-shot is harmless. For these functions that holds except for `AddEnemyPatrolNode` without a preceding clear.
- **Effect of a doubled route.** A Grunt with a doubled route walks it twice. Repeating the same node twice in a row adds an extra Wait (`LuxEnemy_Grunt.cpp:980-1005`). The auto-remove check runs only at the final node.

## Chapter 1 usage

Only three of these functions appear: `AddEnemyPatrolNode` (44 calls), `ClearEnemyPatrolNodes` (11), and `SetEnemyDisabled` (2). No chapter 1 script calls `ShowEnemyPlayerPosition`, `AlertEnemyOfPlayerPresence`, `SetEnemyIsHallucination`, `FadeEnemyToSmoke`, `SetEnemyDisableTriggers`, `SetEnemySanityDecreaseActive`, the teleports, or `GetEnemyStateName`. Activation and deactivation use `SetEntityActive`. The "kind" column says how the enclosing callback runs under ADR 0006.

| Map | Enemies | Enemy calls and the callback that makes them | Kind |
|---|---|---|---|
| `03_archives` | `servant_grunt_1..3` | Patrol nodes in `OnStart` (`ch01/03_archives.hps:1354-1356`) | Local, runs in each game |
| | | `CollideActivateLiveGrunt` activates 1 or 2 (`:1227-1234`, registered `:1338-1339`) | Shared (`"Player"` collide) |
| | | `CollideDisableLiveGrunt` deactivates (`:1263-1265`, registered `:1345-1346`) | Enemy-parent collide: not a shared kind |
| | | `LookAtGrunt` activates 3 (`:1278-1280`) | Look-at: local |
| `05_wine_cellar` | `grunt_normal_1` | `PickEnemy` activates (`ch01/05_wine_cellar.hps:643-668`). It is the `CallbackFunc` of a pickup in `05_wine_cellar.map`. | Pickup entity callback: local |
| | | `PickEnemy` also registers `CollideActivateEnemy` (`:650`). That callback and `LookAtEnemy` each add `PathNodeArea_3` with no clear (`:677`, `:688`). | Shared, but the registration exists only in the picker's game |
| | | `CollideDeactivateEnemy` deactivates (`:727-732`, registered `:805`) | Enemy-parent collide |
| `06_distillery` | `grunt_normal_1` | Patrol node in `OnStart` (`ch01/06_distillery.hps:472`) | Local, each game |
| | | `EnterEnemyGlimpse` activates (`:355-357`, registered `:456`) | Shared |
| | | `EnemyDisabled` is the grunt's `OnAutoDisabled` callback (`:386-395`) | Entity callback, runs where that game's copy auto-removes |
| `07_archives_cellar` | `waterlurker_1..4` | `SetEntityActive` only, from `"Player"` collides and a timer they start (`ch01/07_archives_cellar.hps:7`, `:16`, `:157-159`, `:176-194`) | Shared |
| | | `CheckPoint02` reactivates `waterlurker_2` (`:275-287`) | Checkpoint: local |
| `08_cellar_maze` | `waterlurker_1..4` | `SetEnemyDisabled` in `CollideStopLurker01/02` (`ch01/08_cellar_maze.hps:3-24`, registered `:223-227`), guarded by `GetSwingDoorClosed`, the receiving game's door state | Shared |
| | | Activation from shared collides and a timer they start (`:93-169`, `:218`) | Shared |
| `10_daniels_room` | `grunt_normal_1` | `CollideAreaGrunt` (`:72-83`, registered `:347`) starts timers. `TimerActivateGrunt` activates, then clears and adds four nodes (`ch01/10_daniels_room.hps:93-110`). | Shared, idempotent |
| | | `CollideAreaGrunt02` clears and adds (`:59-71`, registered `:109`). It is skipped when local var `GruntLeftCloset` is set by the enemy-parent collide `CollideAreaGruntInCloset` (`:42-47`, registered `:250`, `:351`). | Shared, but its guard depends on each game's own copy of the enemy |
| | | `CollideAreaGruntDeactivate` (`:124-138`, registered `:114`) deactivates or reroutes on local var `PlayerInArea`. That var is set by the `"Player"` collide `CollideAreaCheckPlayer`, which is shared, so a replay writes the other player's in/out state. | Enemy-parent collide |
| | | `CheckPoint01` reactivates and reroutes (`:228-249`) | Checkpoint: local |
| `12_storage` | `grunt_normal_1..3`, `*_extra`, `grunt_in_room` | `TimerDrillEnemy` activates and adds nodes with no clear (`ch01/12_storage.hps:18-28`), from `PickDrill` (`:5-16`) | Pickup: local |
| | | `TimerRodGrunt` activates and adds nodes (`:574-579`), from `PickRod` (`:525-548`) | Pickup: local |
| | | `PickRod` also registers `CollideEnemy1` (`:546`) → `TimerEnemy1`, which clears and adds (`:500-521`; `TimerEnemy1` at `:507`) | Shared, but the registration exists only in the picker's game |
| | | `CollideEnemy1Extra/2Extra/3Extra` clear and add (`:617-664`). They are registered only in checkpoint callbacks (`:674-704`). | Shared, but the registration exists only in the dead player's game |
| | | `CollideAreaCloseDoor` adds two nodes with no clear on each door opening (`:882-901`, registered `:1114`, repeating). The parent is the door `cellar_wood01_7`. | Peer-Driven Entity collide while the door is Held. Otherwise local in each game. |
| | | `StartBangDoorEvent` / `StopBangDoorEvent` clear, then add (`:933-960`) | Called from the above |

## Implications for the Shared Enemies spec

1. **Agreement holds.** Enemy script functions agree under replay, except for the two that read the player's position and the unguarded `AddEnemyPatrolNode`. The setup the map counts on ("Enemy activation and patrol setup reach both games through Shared Callbacks") works where the setup callback is a shared kind.
2. **Whose position?** `ShowEnemyPlayerPosition` and `AlertEnemyOfPlayerPresence` need a decision: "the player" in a Shared Callback should mean the triggering player. Otherwise only the simulating game applies them. Chapter 1 does not exercise them, but custom stories and later chapters may. `ShowEnemyPlayerPosition` also always forces Hunt (the `||` bug above). That is vanilla behaviour to keep, not to fix.
3. **Possible ADR 0006 conflict.** Pickup callbacks are kept local, but they start the `05_wine_cellar` encounter and two of the three `12_storage` encounters. In the other game the enemy never activates, and the shared collide they register replays as a missing-registration no-op. A Shared Enemy started this way exists in one game only.
4. **Not shared kinds.** Collide callbacks with an enemy as parent (`03`, `05`, `10`) and the `OnAutoDisabled` entity callback (`06`, `12`) are not shared kinds. Each runs when that game's own copy of the enemy reaches an area or auto-removes. With a Shared Enemy, they should follow the simulating copy.
5. **Checkpoints.** Checkpoint callbacks re-arm enemies in the dying player's game only (`10:228-249`, `12:674-704`, `07:275-287`). This is the "enemy kills one player" question in the map's Not yet specified list.
6. **Vanilla rules read only the local player.** The engine still activates, routes, deactivates, and fades the enemy against the local player alone: activation range, out-of-range self-deactivation, auto-remove at path end, and hallucination fade. "Vanilla targeting for every player" therefore has to extend these checks to every player in the simulating game. Otherwise the two copies drift even when every script call agrees.
