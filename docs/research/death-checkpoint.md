# Death and checkpoints: what a player's death does to enemies and the map

Findings for issue #83, part of the Shared Enemies map (`amnesia-spelos/amnesia-multiplayer#51`). The question was what a death does to enemies and to the map in Amnesia, and what that means when the other player in a Multiplayer Session is still alive. Terms follow `CONTEXT.md` here (Avatar, Pose, Peer-Driven Entity, Map Visit, Map Script) and in `amnesia-multiplayer` (Hold, Settling, Shared Callback, Shared Enemy).

Paths are relative to `src/amnesia/src/game/` unless they say otherwise. Line numbers are those at commit `a6dcdbd`. Map Script examples come from the shipped Main Story (`maps/main/ch01/*.hps` in the game's install folder), which is not in this repository. `config/game.cfg` is also from the install folder.

## Short answer

A death does not reload the map. The engine plays a death screen of about 7 to 12 seconds, then restores the player in place ("loads the checkpoint"): it teleports the player to the checkpoint's start position, **deactivates every enemy on the map and teleports it to its start position**, resets music, and runs the checkpoint's Map Script callback. The callback is what brings enemies back, usually reactivating them, rewriting patrol nodes, and resetting props. Entities, variables, timers, and Map Script globals all carry on. The same Map Visit continues, so Avatars and Peer-Driven Entities are untouched.

Every step runs only in the dying player's game. With a second player alive, the enemy that killed one player is reset in the killer's game and keeps hunting in the other game. The checkpoint callback's prop resets and enemy setup also happen in one game only, so the two maps drift apart.

## The death flow

1. **Death starts when health reaches 0.** `cLuxPlayer::SetHealth` (`LuxPlayer.cpp:859-868`), `AddHealth` (`:894-903`), and `GiveDamage` (`:587-605`) call `cLuxPlayerDeath::Start` when health falls to 0 or below. `IsDead()` is just `mfHealth <= 0` (`LuxPlayer.h:198`). Other causes go through the same path: falling below the auto-kill height (`LuxPlayer.cpp:381`), a script's `SetPlayerHealth(0)` (`LuxScriptHandler.cpp:1448`), and hard mode's sanity at 0 (`LuxPlayer.cpp:871-883`). The flow is the same whether or not an enemy did the killing.
2. **`cLuxPlayerDeath::Start`** (`LuxPlayerHelpers.cpp:1948-2025`):
   - It keeps input in the game state and forces the player state to `Normal` (`:1960-1962`). Leaving an interact state calls `ResetInteractVars` (`LuxPlayerState_Interact.cpp:113-118`, for example from `LuxPlayerState_InteractGrab.cpp:270`), so **a death ends the local interaction and reports `interactionend`**.
   - It lowers the lantern, holsters the hand object, and stops any insanity collapse (`LuxPlayerHelpers.cpp:1963-1966`).
   - It picks the death hint text: the one the last `CheckPoint` or `SetDeathHint` call set, else `Hints/DefaultDeath` (`:1970-1981`).
   - It stops all music, plays the body-fall sound, and fades out world volume and speed (`:1990-2002`).
   - It **broadcasts `eLuxEnemyMessage_PlayerDead` to every active enemy on the map** (`:2006`), not only the killer. `cLuxMap::BroadcastEnemyMessage` skips inactive enemies (`LuxMap.cpp:692-711`).
3. **The death screen** (`cLuxPlayerDeath::Update`, `LuxPlayerHelpers.cpp:2029-2177`). The head drops and rolls, then the screen fades to black over `Death_FadeTime = 3` seconds (`config/game.cfg:96`). The hint fades in. Once it is nearly opaque and the screen is black, it stays for 5.5 seconds (`:2094`), or less if the player presses a button or has turned off death hints (`OnPressButton`, `:2261-2265`; input while dead goes only there, `LuxInputHandler.cpp:982-993`). Then `ResetGame` runs (`:2098`) and the screen fades back in. Inference from the constants: about 7 seconds if skipped, about 12 seconds if the hint is read through. **The game is not paused**: the map, its timers, its enemies, and physics keep updating throughout.
4. **`ResetGame`** (`:2302-2327`) resets the camera roll and pitch and the head offset, stops the body, sets health to 50, raises sanity to at least 40, restores world volume, and calls `cLuxMap::LoadCheckPoint`.
5. **Other endings.** With perma-death the game exits after a voice line (`:2067-2077`, `:2158-2165`). In hard mode the game loads the latest save (`AutoLoad(true)`, `:2143-2152`), which starts a new Map Visit, or returns to the main menu (`:2180-2205`).

## Checkpoints

- **Setting one.** `CheckPoint(name, startPos, callback, hintCat, hintEntry)` (`LuxScriptHandler.cpp:1026-1030`) calls `cLuxMap::SetCheckPoint` and sets the death hint. `SetCheckPoint` (`LuxMap.cpp:469-503`) **returns early if the name is unchanged**. Otherwise it stores the start position and callback, resets the per-checkpoint death count `mlCheckPointCount` to 0, and records the highest-priority looping music playing at that moment. Every map entry sets an automatic checkpoint `"_auto"` at the entry start position with no callback (`LuxMapHandler.cpp:476`).
- **Where it is called.** Shipped maps call `CheckPoint` from the Map Script callback that starts an encounter. The callbacks are usually `"Player"` collide callbacks, for example `EnterEnemyGlimpse` in `06_distillery.hps:355-366`, registered at `:456`. Under ADR 0006 these are Shared Callbacks, so each game sets its own checkpoint when the encounter starts.
- **Loading one** (`cLuxMap::LoadCheckPoint`, `LuxMap.cpp:509-548`), in order:
  1. Place the player at the start position (`:516`, via `PlacePlayerAtStartPos`, `:452-465`). `cLuxPlayer::PlaceAtStartNode` sets the feet, yaw, and pitch, and calls `CountTeleport()` (`LuxPlayer.cpp:822-836`).
  2. **For every enemy on the map: `SetActive(false)`, then `ResetProperties()`** (`LuxMap.cpp:520-526`).
  3. Clear the player's terror enemies (`:530`) and fade in.
  4. Reset music and replay the music recorded when the checkpoint was set (`:538-540`).
  5. Run the callback as `Callback("name", count)`, last of all (`:545`), then increment the count (`:547`). `count` is how many times this checkpoint has been loaded in this game, so scripts branch on it (see below).
- **What is not reset.** Props, doors, levers, items, Map Script variables and AngelScript globals, timers, collide callbacks, and Peer-Driven Entities are left as they are unless the callback resets them. Nothing is destroyed or rebuilt, so no `DestroyWorldEntities` or `CreateWorldEntities` message is sent. This matches `CONTEXT.md`: "A death ... continues the same Map Visit."

## What happens to enemies

- **At the moment of death.** The Grunt class (also used by the Brute: both `servant_grunt.ent` and `servant_brute.ent` declare `EntityType="Enemy_Grunt"`) and the WaterLurker class (the Kaernk) answer `PlayerDead` by changing to `Idle` (`LuxEnemy_Grunt.cpp:167-168`; `LuxEnemy_WaterLurker.cpp:157-158`). While the player is dead, the senses return early: `UpdateCanSeePlayer` (`LuxEnemy.cpp:1383`), `UpdatePlayerDetected` (`:1486`), and `UpdatePlayerInRange` (`:1506`). `DistToPlayer*` returns 100000 (`:2117`, `:2124`, `:2136`). So for the 7 to 12 seconds of the death screen the enemy is idle, active, and blind to the player. A Grunt's `Idle` leaves only on `PlayerInRange` (`LuxEnemy_Grunt.cpp:176-200`), and that message is not sent while the player is dead, so in practice it stands where it was. `cLuxPlayer::GiveDamage` returns at once while dead (`LuxPlayer.cpp:589`), so a swing already under way does no more damage.
- **At the checkpoint load.** `iLuxEntity::SetActive` is a no-op if the value is unchanged (`LuxEntity.cpp:145-152`). For an active enemy, `iLuxEnemy::OnSetActive(false)` (`LuxEnemy.cpp:2247-2273`) deactivates its character body and mesh and hides the mesh. It also removes the enemy from the attack and search music and from the player's terror set, and changes its state to `Idle`. The WaterLurker keeps its mesh invisible either way (`LuxEnemy_WaterLurker.cpp:605-610`).
- **`ResetProperties` runs for every enemy, active or not** (`LuxEnemy.cpp:999-1037`):
  - It teleports the enemy to its map start position (`SetPositionAtStartPos`, `:992-995`) and sets its state to `Idle`.
  - It clears what the enemy perceived and remembers (sight, detection, range, last known player position, door-stuck state, damage count) and resets the patrol index to 0.
  - It also **sets `mbDisabled = false`** (`:1006`). This undoes `SetEnemyDisabled`, the hook `avatars.md` names for driving an enemy from a Peer.
  - It does not clear the patrol node list (`ClearPatrolNodes`, `:1091-1094`, runs only from script), the enemy's health, or `mbDisableTriggers`.
- **Enemies stay inactive after a checkpoint load unless the callback reactivates them.** Shipped callbacks do this, often only on the first reload. In `10_daniels_room.hps:228-249`, when `alCount < 1` the callback calls `SetEntityActive("grunt_normal_1", true)`, replaces its patrol nodes, and registers a collide callback on it. In `07_archives_cellar.hps:256-290` the Kaernk is reactivated directly on one count and by a fresh `"Player"` collide callback on another. In `06_distillery.hps:397-400` the callback is empty, so the glimpse Grunt stays gone. **On a later death the enemy may not return at all**: the encounter changes with each death.

## What the checkpoint callback does to the map

Shipped callbacks restore the encounter's set-up in the dying player's own game. In `07_archives_cellar.hps:256-290`:

- `SetPlayerHealth(RandFloat(80, 95))`;
- removing and restarting a timer (`RemoveTimer("Gate1Down")`) and adding to a local variable;
- `SetWheelAngle("valve_iron_1", 0)` and `SetMoveObjectState("castle_portcullis_2", 0)`, which resets a puzzle;
- `ResetProp("cellar_wood01_2")` and recreating body parts;
- registering new `"Player"` collide callbacks that start the next attack.

`10_daniels_room.hps:228-262` also uses `ResetProp` on a door and cabinets, a timer that re-enables an area, and **a nested `CheckPoint`** that switches the map to a second checkpoint with its own callback (`CPCall02`, which calls `StartPlayerLookAt`).

ADR 0006 keeps checkpoint callbacks local: they are never Shared Callbacks.

## Avatars and Peer-Driven Entities across a death (today)

- **No game code reacts to a death.** `LuxAvatarHandler.cpp`, `LuxPeerDrivenEntityHandler.cpp`, `PeerDrivenEntityModel.cpp`, and the interaction report handler have no death or checkpoint path. Avatar world objects and Peer-Driven Entities are released only on `DestroyWorldEntities` (map left or save loaded; `LuxAvatarHandler.cpp:129-136`, `LuxPeerDrivenEntityHandler.cpp:265-270`), on a game reset (`:139`, `:258-262`), or at the end of a Session (`LuxSocketServer.cpp:192`). A death does none of these.
- **Pose during and after a death.** The local Pose stays `Live` because input stays in the game state (`LuxSocketServer.cpp:100-109`; `LuxPlayerHelpers.cpp:1960`). The Pose has feet, yaw, pitch, crouch, and lantern (`:113-130`), but not the death's head drop or roll. The other player therefore sees the dead player's Avatar standing at the death spot with its lantern lowered for the whole death screen. It then **snaps** to the checkpoint start, because `PlaceAtStartNode` increments the teleport counter (`LuxPlayer.cpp:835`; snapping in `AvatarPoseModel.cpp:41`).
- **Holds.** The death ends the dying player's interaction (`interactionend`, released), so under ADR 0004 any Hold of theirs goes on Settling, and its bodies keep streaming. The teleport puts the dying player's body somewhere else, so it stops touching the props it touched.
- **The living player's Holds in the dying player's game.** Peer-Driven Entities stay driven through the death. A checkpoint callback's `ResetProp` on one is not refused (`LuxScriptHandler.cpp:3385-3393`; only health changes are, `:3357`, `:3369`). The next streamed sample overwrites the reset, as ADR 0005 expects.

## Where one death leaves the two games' maps or enemies different

Assume both players are on the map and each game runs its own copy of the enemy, as today. When player A dies:

1. **The killing enemy.** In A's game it goes idle and blind (`PlayerDead`), then is deactivated and teleported to its start position. In B's game the same enemy keeps hunting, or starts to; B's game never learns that A died.
2. **Every other enemy on the map.** `PlayerDead` goes to all active enemies and `LoadCheckPoint` deactivates and resets all of them, so A's death also resets enemies that were never chasing A.
3. **Reactivation.** Only A's checkpoint callback reactivates enemies and rewrites patrol nodes, and only on the counts it chooses. B's copy was never deactivated and gets nothing.
4. **The checkpoint count.** `mlCheckPointCount` counts deaths per game, and `SetCheckPoint` resets it only when the name changes. The two games' counts differ as soon as one player dies, so a later death by B takes a different `alCount` branch than A's did. That means a different reactivation, or none.
5. **Map resets in the callback.** `ResetProp`, `SetWheelAngle`, `SetMoveObjectState`, `RemoveTimer`/`AddTimer`, local variables, and nested `CheckPoint` calls change A's map only. A puzzle B half-solved can be undone in A's game and not in B's.
6. **Collide callbacks the callback registers.** They exist only in A's game. Under ADR 0006, A triggering one is replayed into B, which has no such registration (a no-op with one log line). B walking into the same area triggers nothing, because B's game never registered it.
7. **The death hint and checkpoint music** are taken from each game's own last `CheckPoint`/`SetDeathHint` call and from the music each game played when its checkpoint was set.
8. **The Avatar.** It stands still for 7 to 12 seconds while an enemy in B's game may still be swinging at that spot, then jumps to the checkpoint start.
9. **Hard mode and perma-death.** A's game loads a save (a new Map Visit in A only; Holds and Peer-Driven Entities are released) or exits (a departure).
10. **A Peer-driven enemy.** If a Shared Enemy is driven in one game by `SetEnemyDisabled` plus streamed motion, a death in that game clears `mbDisabled`, deactivates the enemy, and teleports it, all through `ResetProperties`.

## What this means for a Shared Enemy (inference)

The vanilla design assumes one player: a death "rewinds" the encounter by hiding every enemy and letting the checkpoint callback restage it. With a living second player, that rewind cannot simply be applied to a single shared threat. Applying it locally (today's behavior) splits the enemy in two. Applying it in both games teleports the enemy away from a player who is still being chased, and it runs a callback ADR 0006 keeps local. Not applying it means the dead player respawns into an active hunt, at a start position the enemy may be standing beside.

The spec needs a rule for a death while the other player lives. Two candidates are to defer the checkpoint enemy reset until both players are dead or out of reach, or to treat the respawning player as merely out of play while the enemy keeps its target on the survivor. The rule must also settle whether the checkpoint callback, or only its enemy part, becomes shared. The engine's seams for either choice are the `PlayerDead` broadcast (`LuxPlayerHelpers.cpp:2006`), the enemy loop in `LoadCheckPoint` (`LuxMap.cpp:520-526`), and the callback call (`:545`).
