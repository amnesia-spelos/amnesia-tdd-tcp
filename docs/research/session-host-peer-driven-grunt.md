# A peer-driven Grunt in the Session Host's game, without a Map Script

Research for #125, part of the Monster Mode map (amnesia-spelos/amnesia-multiplayer#90). The question: how can the Session Host's game show the Monster Player as a Grunt driven from a peer stream, when no Map Script placed it? Terms follow `GLOSSARY.md` and amnesia-multiplayer's `CONTEXT.md` (Monster Player, Embodied, Drifting, Shared Enemy, Enemy Pose, Avatar, Session Host). Line numbers are from `origin/main` at `7f618df`. Paths are relative to `src/amnesia/src/game/` unless they start with `HPL2/` (meaning `src/HPL2/core/sources/`). Paths to installed game data start with `<game>/`.

## Answer in brief

- **Spawn a real `iLuxEnemy` at runtime and keep it in the existing driven mode.** `cLuxMap::CreateEntity("servant_grunt.ent")` is the path `CreateEntityAtArea` uses. It runs the normal enemy loader, so the Monster Player's Grunt gets the same attack boxes, damage data, sounds, music names, and sanity flag as a map-placed one. `SetEnemyDriven(true)` then turns off its AI. `UpdateEnemyCopy` and `ResolveEnemyAttack` work on it unchanged.
- **Don't reuse the Avatar for this.** An Avatar has no attack shapes, doesn't drain sanity, isn't in the map's enemy list for music, and its clip model only knows idle/walk/crouch. Copy two things from it instead: Session ownership (ADR 0002) and the head-pitch bone technique.
- **Reusing the Shared Enemy path is cheap, but the "enemy" label needs guards.** Today, every place that loops over a map's enemies would also pick up the Monster Grunt. Six of those places must skip it: the Enemy Pose sampler, release at Session end, release at map leave, both save paths, and the "other player is on this map" check. Without these fixes, the Session Host's game would stream the Grunt back as a Shared Enemy, release it into AI, or save it as a real Grunt.
- **Hitting the Session Host already works.** `ResolveEnemyAttack` tests the reported box against `gpBase->mpPlayer` only. In the Session Host's game, that player is exactly the victim.
- **Map-placed enemies already ignore it, with two flags.** Their senses consider only the local player and Avatars, and a driven enemy drops every message. To make their bodies pass through it, give its character body the Avatar collide flag.
- **Sanity drain comes free. Music, terror, and growls need a state from the Monster Player.** Footsteps and attack sounds don't play on any driven copy today, including Shared Enemy copies in a Joining Player's game.
- **The sight test has two engine helpers already**, written for a monster that must not appear in the player's view: `IsInPlayerFovAtFeetPos` and `IsVisibleToPlayerAtFeetPos`.

## 1. Spawning

### What exists

- **Runtime creation.** `cLuxMap::CreateEntity` (`LuxMap.cpp:571-585`) calls `cWorld::CreateEntity` with a free entity ID. It also marks the result `CreatedAtRuntime` through `AddEntity` (`LuxMap.cpp:610-632`). `cWorld::CreateEntity` resolves the `.ent` through the resource manager and dispatches on its `EntityType` (`HPL2/scene/World.cpp:379-419`), so a bare `servant_grunt.ent` works. The script function `CreateEntityAtArea` is a thin wrapper around it (`LuxScriptHandler.cpp:2452-2471`). No Map Script is needed. The game can call `CreateEntity` directly.
- **The enemy loader works at runtime.** `iLuxEnemyLoader::AfterLoad` reads the map from `gpBase->mpCurrentMapLoading`, which `CreateEntity` sets (`LuxEnemy.cpp:119-123`). It creates the character body, loads attack damage and size data (`:323-326`), and runs `SetupAfterLoad`, which creates the mover and pathfinder (`:358`, `:599-619`). It then calls `AddEntity`, which puts the Grunt in `mlstEnemies` (`LuxMap.cpp:619-623`). It reads instance variables only when present (`LuxEnemy.cpp:366-377`), so a runtime Grunt is not a hallucination, has no callback, and uses the biped pose.
- **What doesn't run.** `AfterWorldLoad` runs only during a map load (`LuxMap.cpp:207`, `:326-334`). On a runtime Grunt, the pathfinder therefore never gets an AI node container or an A* handler (`LuxEnemyPathfinder.cpp:68-81`). That is harmless while the Grunt is driven. But **releasing it into its own AI would leave a Grunt that can't path**, so it must never be released (see §1 "Guards").
- **The ragdoll pitfall in `docs/research/avatars.md` doesn't apply here.** The Grunt `.ent` has animations (`<game>/entities/enemy/servant_grunt/servant_grunt.ent:71-110`), so the loader plays one instead of enabling skeleton physics.

### Hiding, showing, removing

- **Drifting: `SetActive(false)`.** `iLuxEnemy::OnSetActive` (`LuxEnemy.cpp:2502-2528`) deactivates the body (no collision), hides the mesh, and removes the Grunt from attack/search music and from the player's terror set. The sanity check skips inactive enemies (`LuxPlayerHelpers.cpp:1402`). Becoming Embodied is `SetActive(true)` plus Enemy Poses. A copy's lifecycle model treats activation as ending any disappearance (`:2505`).
- **Leaving or Session end: `cLuxMap::DestroyEntity`.** This is a deferred destroy (`LuxMap.cpp:634-645`).
- **Lifetime.** A map entity dies with its map, so the Grunt belongs to one Map Visit. It should be owned like an Avatar (ADR 0002): a Session-owned handler creates it on the first Embodied pose for the current map and destroys it on Session end, on map leave, and on a return to the menu.

### Guards needed because the Grunt is in `mlstEnemies`

Each of these loops over all enemies on the map:

| Site | What it would do to the Monster Grunt | Needed |
|---|---|---|
| `GetEnemyPoses` (`LuxSocketServer.cpp:198-210`) | Samples every active Grunt-type enemy, driven or not. The Session Host's Game Peer would stream the Monster Grunt to the Monster Player as a Shared Enemy. | Skip it. `IsSharedEnemy()` (`LuxEnemy.cpp:3108-3112`) already excludes driven enemies but isn't used here. |
| `ReleaseDrivenEnemies` at Session end (`LuxSocketServer.cpp:288-293`) | `SetEnemyDriven(false)` hands it to its own AI, which has no path nodes. | Destroy it instead. |
| `DestroyWorldEntities` at map leave (`LuxSocketServer.cpp:561-565`, sent from `LuxMapHandler.cpp:452-460`) | Releases it a moment before the map is destroyed. | Skip it, or destroy it first. |
| Save-game map save, `cLuxSavedMap::FromMap` (`LuxSavedMap.cpp:156-166`, reload `:337-347`) | Saves every entity with `IsSaved()`, runtime ones included. A driven enemy is saved as a resumable vanilla Grunt (`LuxEnemy.cpp:2822-2836`). **Loading that save would spawn a real AI Grunt where the Monster Player stood.** | `SetIsSaved(false)` on creation (`LuxEntity.h:165`). |
| Map-leave save, `cLuxSavedGameMap::FromMap` (`LuxSavedGame.cpp:481-490`, `:476`), which runs before the leave (`LuxMapHandler.cpp:645`) | Records it by ID as an enemy and possibly as a disabled entity. On a return, it logs "Saved enemy with ID %d does not exist" (`LuxSavedGame.cpp:697-707`). The disabled-ID list could hit a later runtime entity that reuses the free ID. | Skip `!IsSaved()` entities there too. |
| `LoadCheckPoint` (`LuxMap.cpp:528-541`) | A driven enemy is left alone, except that a coordinated Shared Death calls `ResetEnemyCopy` (deactivates it). | Acceptable. The Session Host's death forces Drifting anyway. |

Command lookup refuses runtime entities: `FindEnemy` returns `EntityWrongType` for anything `IsCreatedAtRuntime()` (`LuxSocketServer.cpp:404-414`). This is correct, because the runtime ID is just a free gap in the map file's IDs and means nothing to the other game. The Monster Grunt therefore needs its own addressing: one Grunt per Session, named by the Session rather than by an entity ID. The existing `enemydrive`/`enemypose`/`enemyattack` lines can't reach it as they stand.

### Cost

The first `CreateEntity` of `servant_grunt.ent` on a map with no Grunts loads its mesh, 12 animations, and sounds synchronously. That is a visible hitch at the first Embodied switch. Preloading the `.ent` when Monster Mode starts, or creating the Grunt inactive at map enter, moves the hitch to the load screen. This is an inference from the loader; it hasn't been measured.

## 2. Driving

The driven-copy machinery works on the Monster Grunt as it is:

- `SetEnemyDriven(true)` (`LuxEnemy.cpp:3043-3080`) ends any state, stops the pathfinder, zeroes forces, turns gravity and collision testing off, and removes music and terror.
- `iLuxEnemy::OnUpdate` then runs only `UpdateEnemyCopy` (`:640`).
- `UpdateEnemyCopy` (`:3167-3199`) samples the interpolated Enemy Pose (feet, yaw, clip, clip time, loop) on the ADR 0003 delay, sets the body, and shows exactly one clip frozen at the streamed time (speed 0). It then runs `UpdateEnemyCopySpecific`, `UpdateSoundState`, and `UpdateDarknessGlow`.
- `AddEnemyPose` (`:3143-3158`) rejects a clip the mesh doesn't have.
- `GetEnemyPose` (`:3125-3141`) shows what a sample is made of. The Monster Player's game can produce the same tuple from its own local Grunt mesh.

What's missing:

- **Attack animations: nothing to add.** `SwingClaws01`, `SwingClaws02`, `SwingLaunch`, and `BreakDoor` are clips in the `.ent` (`servant_grunt.ent:80-93`). The Monster Player's game puts the clip name and time in the Enemy Pose, and the copy shows it.
- **Head and look direction.** The Enemy Pose carries body yaw only. Camera pitch has to be added to the Monster Player's stream. To show it, reuse the Avatar technique: set a pre-transform on bone states (`LuxAvatarHandler.cpp:615-630`), which persists across the animation update. The Grunt `.ent` has no `MoveHeadBones` variable, which is where the Avatar reads its bones (`LuxAvatarHandler.cpp:215-260`). Its neck and head bones are `servant_grunt_A_CenterNeck` and `servant_grunt_A_CenterHead` (`servant_grunt.ent:23-24`), so they need a hard-coded or configured list.
- **Sound state.** A runtime Grunt starts `Silent` (`LuxEnemy.cpp:459`). Driving it keeps `Silent` (`:3060`), and `UpdateSoundState` returns at once while silent (`:1355`). So the copy makes no ambient growls until something calls `ChangeSoundState` (see §5).

## 3. Hitting the Session Host

The existing `enemyattack` resolution can be used as is, called on the Monster Grunt through a Monster-specific command.

- `iLuxEnemy::ResolveEnemyAttack` (`LuxEnemy.cpp:3216-3240`) finds the attack shape whose box size matches the report and picks the Normal or BreakDoor damage data. It then calls `ShapeDamage` from the copy's shown position and yaw (`GetDamageShapeMatrix`, `:2219-2232`), restricted by `apOnlyBody` to `gpBase->mpPlayer`'s body. On a hit it plays the `.ent`'s hit sound.
- `ShapeDamage` (`LuxMapHelper.cpp:266-320`) applies the random damage roll, knockback, and lethal damage to the player. That is the vanilla death the map asks for.
- In the Session Host's game, `gpBase->mpPlayer` *is* the Session Host. This matches ADR 0009's victim rule: the hit is judged from what the Session Host's game shows, about 100 ms behind the Monster Player.
- The box values come from the Monster Player's game. The protocol type `cGameInteractionEnemyAttack` (kind, box size, box offset) is filled from the `.ent` by `GetEnemyAttack` (`LuxEnemy.cpp:3201-3214`). The Monster Player's game can fill it the same way from its own Grunt, which has the same `NormalDamageSize`/`NormalDamageOffset`.
- `iLuxEnemy::Attack` returns early on a driven enemy (`:1995`). The copy never runs its own attack, never publishes one, and never damages props.
- Because the Grunt is an `iLuxEnemy`, `GetAttackingEnemy()` and door code can identify it. A Monster door break would still need its own path (the map lists this as not yet specified). `ResolveEnemyAttack` passes `abCheckProps=false`, so the Monster Grunt's attacks never touch doors in the Session Host's game.

## 4. Map-placed enemies ignore it

- **Sight and targeting.** Every sense reads the local player plus `cLuxAvatarHandler::GetEnemyTargets()` (`LuxEnemy.cpp:1450-1516`, `:2371-2392`). The Monster Grunt is neither, so it is never perceived or targeted.
- **Hearing.** Its ambient and animation sounds are `grunt/*` (`servant_grunt.ent:73-110`, `:179-185`). No `EnemySounds.dat` prefix matches them (`<game>/sounds/EnemySounds.dat`), so map-placed enemies don't hear it.
- **Messages.** A driven enemy drops all messages (`SendMessage`, `LuxEnemy.cpp:896-898`), including `HelpMe`, `SoundHeard`, `PlayerDead`, and food. It also ignores damage (`GiveDamage`, `:811-813`), so it is invulnerable as the map requires. Enemy attacks skip non-player characters (`ShapeDamage` with `abCheckEnemies=false`).
- **Bodies.** This needs one change. Enemy bodies are created with `eFlagBit_All & ~kCollideFlag` (`LuxEnemy.cpp:189-191`, flag in `LuxAvatarHandler.h:57-58`), and Newton drops a contact when the two bodies' flags don't overlap (`HPL2/impl/PhysicsMaterialNewton.cpp:241`). A runtime Grunt gets the same enemy flags, so map-placed Grunts would collide with it. Setting its body flags to `cLuxAvatarHandler::kCollideFlag` alone, as Avatars do (`LuxAvatarHandler.cpp:360-361`), makes enemies pass through it. The Session Host's player body has all flags, so the player still collides with it.
  - Decide whether the Session Host should be blocked by the Monster Grunt, as by a real Grunt. If not, the body can simply stay inactive. Its mass must also be raised, as for Avatars (`LuxAvatarHandler.cpp:362-364`), or the player could shove it off its pose.
- **"Other player on this map" is an Avatar today, and Monster Mode breaks that proxy.** `HasAwakeAvatar()` (`LuxAvatarHandler.cpp:109-116`) is what makes enemies Shared (`IsSharedEnemy`, `LuxEnemy.cpp:3108-3112`). It also drives Shared Callback enemy reporting (`LuxSharedCallbackHandler.cpp:49`, `:164`) and the Shared Death policy (`LuxSocketServer.cpp:487`). Hallucinations end near any awake Avatar (`LuxEnemy.cpp:1916-1922`).
  - If the Session Host's game shows the Monster Player as the Grunt and *not* as an Avatar, these checks see nobody on the map. The Monster Player's game then gets no Shared Enemy copies.
  - If it keeps an Avatar as well, the Session Host sees a human figure, map-placed enemies may target it (unless `avatarenemysight` is off), and hallucinations poof near the Monster Player.
  - **Monster Mode needs a "Monster Player present on this map" input to these four checks**, separate from the Avatar. This is a game change, and it borders ADR 0008's rule that the game decides deaths from what it can see.

## 5. Acting like a real enemy without AI

| Effect | Where it comes from | Driven Monster Grunt today | How the Monster Player's input could drive it |
|---|---|---|---|
| **Sanity drain** on sight | `cLuxPlayerSanity::UpdateCheckEnemySeen` loops over map enemies: active, `CausesSanityDecrease`, within 35 m, in the camera frustum, 2 of 5 rays clear. It then calls `LowerSanity` and the radial blur (`LuxPlayerHelpers.cpp:1396-1460`). | **Works with no change.** Independent of AI. The Grunt drains by default (`LuxEnemy.cpp:452-453`). | Nothing needed. Drifting (inactive) stops it. |
| **Danger ("close") music** | `cLuxMusicHandler` plays `DangerMusic` while any active enemy has `GetPlayerInRange()` (`LuxMusicHandler.cpp:266-310`). `mbPlayerInRange` is set by the AI sense `UpdatePlayerInRange`. | Off. Senses don't run while driven. | Set the copy's in-range flag from a distance rule in the Session Host's game, or from a streamed state. |
| **Search and attack (chase) music** | AI states call `mpMusicHandler->AddEnemy(Search\|Attack, this)` (`LuxEnemy_Grunt.cpp:482`, `:549`). `UpdateEnemyMusic` plays the highest-priority track (`LuxMusicHandler.cpp:324-380`, `AddEnemy` at `:193-203`). | Off. Driving removes the Grunt from both sets (`LuxEnemy.cpp:3070-3071`). | Call `AddEnemy`/`RemoveEnemy` on a Hunt/Search state change in the Monster stream. |
| **Terror** (heartbeat, sanity effects) | AI Alert and Hunt call `mpPlayer->AddTerrorEnemy(this)` (`LuxEnemy_Grunt.cpp:375`, `:550`). The player reacts while the set isn't empty (`LuxPlayer.cpp:961-969`, `:1339`). | Off (`LuxEnemy.cpp:3072`). | Same state change as the chase music. |
| **Growls** (ambient voice) | `UpdateSoundState` plays `AmbientSound_<Idle\|Alert\|Hunt>` at random intervals (`LuxEnemy.cpp:1353-1374`, data in `servant_grunt.ent:179-185`). | Runs, but the state stays `Silent` (§2). | `ChangeSoundState(Idle/Alert/Hunt)` from the streamed state. |
| **Footsteps, claw swings, notice sounds** | `PlaySound` animation events in the `.ent` (`servant_grunt.ent:71-110`), fired by `cMeshEntity` when an event time falls between the clip's previous and current time (`HPL2/scene/MeshEntity.cpp:670-688`, `:1443-1467`). | **Don't fire.** `UpdateEnemyCopy` sets the time with speed 0, and `AddTimePosition` copies the time into the previous time before adding 0 (`HPL2/scene/AnimationState.cpp:274-280`; `SetTimePosition`, `:186-196`, doesn't touch the previous time). So the window is always empty. This is an inference from the code. **It applies to every Shared Enemy copy in a Joining Player's game too.** | In `UpdateEnemyCopy`, fire the clip's `PlaySound` events between the previously shown and the newly shown clip time, handling a loop wrap. This is generic, so it would also fix Shared Enemy copies. |

All five AI-driven effects collapse to one small enum the Monster Player's game could stream: `Idle` / `Alert` / `Hunt`, which mirrors `eLuxEnemySoundState`. Setting it on the copy in the Session Host's game would:

- change the sound state;
- add or remove the Grunt in the Attack and Search music sets;
- add or remove it as a terror enemy;
- set its in-range flag.

Which input selects the state is a design question that the map lists as "not yet specified". Options include "running = Hunt", an explicit hunt/roar input, or a rule the Session Host's game computes (Embodied, within N m, mutual line of sight). `OnSetActive(false)` already clears music and terror (`LuxEnemy.cpp:2518-2523`), so Drifting cleans up after itself.

## 6. Sight test (no becoming Embodied in the Session Host's sight)

What the engine has:

- **`iLuxEnemy::IsInPlayerFovAtFeetPos(feet)`** (`LuxEnemy.cpp:2414-2423`): tests the local camera frustum against the enemy's bounding volume moved to a candidate feet position.
- **`iLuxEnemy::IsVisibleToPlayerAtFeetPos(feet)`** (`:2427-2436`): runs `LineOfSight` from the player's eye to that position, with the body's size and several rays (`:2125-2200`).
  - The ManPig (Justine's Suitors) uses exactly these two to avoid appearing in the player's view (`LuxEnemy_ManPig.cpp:1145-1146`, `:1217-1218`, `:2008`, `:2094`). That is the same semantics as "no appearing out of thin air".
  - Both also return true when an Avatar sees the point, through `AvatarSeesPosition`. That must be bypassed in Monster Mode.
- **`cLuxPlayerSanity::UpdateCheckEnemySeen`**: the frustum test plus 5 rays, 2 of which must be clear (`LuxPlayerHelpers.cpp:1407-1442`). This is the "Session Host sees a monster" rule that already drains sanity. It is the most consistent choice: the Monster Player may become Embodied only where the Session Host's sanity wouldn't react.
- **`iLuxEnemy::AvatarSeesPosition`** (`LuxEnemy.cpp:2371-2392`): builds a camera from an Avatar's eye, yaw, and pitch, with the local FOV and aspect. The Monster Player's game could use this against the Session Host's Avatar to give instant UI feedback. It is about 100 ms stale, and it uses the Monster Player's FOV and aspect, not the Session Host's.
- **The enemy's own sight** (`UpdateCanSeePlayer`, `LuxEnemy.cpp:1560-1664`) is the wrong tool. It is the enemy's FOV and range, latched over three checks.

Recommendation: the Session Host's game decides, using the sanity-check rule at the candidate feet position with the Grunt's body size. It is the only game with the Session Host's real camera, FOV, and aspect. That makes the switch a request and reply on the wire: the Monster Player asks to become Embodied at a position, and the Session Host's game accepts or refuses. Optionally, the Monster Player's game pre-checks with `AvatarSeesPosition` for the HUD. None of the helpers consider light, so a Grunt in total darkness inside the frustum counts as "in sight". That matches the sanity drain.

## Recommended shape (for slicing)

1. **A Session-owned Monster handler in the game**, parallel to `cLuxAvatarHandler`. It holds at most one runtime Grunt on the current Map Visit and:
   - creates it with `CreateEntity`, `SetIsSaved(false)`, the Avatar collide flag, raised mass, and `SetEnemyDriven(true)`;
   - toggles `SetActive` for Embodied/Drifting;
   - destroys it on Session end, map leave, and return to menu.
2. **Guards** at the six sites in §1, plus a "Monster Player present" input for the four Avatar-presence checks in §4.
3. **Commands** addressed by the Session, not by entity ID: create/remove, pose (Enemy Pose fields plus camera pitch), state (`Idle`/`Alert`/`Hunt`), and attack (box as in `enemyattack`). Two events go back to the Monster Player: hit or miss, and the Embodied request result.
4. **Generic improvement** to `UpdateEnemyCopy`: fire `PlaySound` animation events between the shown times, and apply head pitch.

## Risks and unknowns

- **Footsteps on driven copies** are silent according to the code (§5), and this also affects today's Shared Enemies. Confirm by listening to a Grunt copy in a Joining Player's game before relying on it.
- **Losing the Avatar as the "other player present" signal** (§4) is the largest design change. It touches the Shared Enemy, Shared Callback, Shared Death, and hallucination decisions, and it needs a decision on whether the Session Host's game also keeps a hidden Avatar for the Monster Player.
- **A save taken with the Grunt present** would resurrect it as an AI Grunt without `SetIsSaved(false)`. Map-leave saves log an error without the extra filter.
- **Releasing the Grunt into AI** (Session end, map leave) would leave a Grunt with no path nodes. It must always be destroyed, never released.
- **The first-spawn hitch** on maps without Grunts hasn't been measured.
- **Collision policy** between the Session Host's player and the Monster Grunt is undecided (§4). Props pushed by the driven body (it carries the Avatar flag, which props share) haven't been checked in play.
- **Brute later**: the same class and path (`Enemy_Grunt`, other `.ent`). **Kaernk later**: no mesh or animations (`WaterLurker`), so most of §2 and §5 differs.

## Sources

- Game source at `7f618df`: `LuxEnemy.cpp/.h`, `LuxEnemy_Grunt.cpp`, `LuxEnemyPathfinder.cpp`, `LuxMap.cpp`, `LuxMapHandler.cpp`, `LuxSavedGame.cpp`, `LuxSavedMap.cpp`, `LuxEntity.h`, `LuxSocketServer.cpp`, `GameInteractionGateway.cpp`, `LuxAvatarHandler.cpp/.h`, `LuxPlayerHelpers.cpp`, `LuxPlayer.cpp`, `LuxMusicHandler.cpp`, `LuxMapHelper.cpp`, `LuxScriptHandler.cpp`, `LuxEnemy_ManPig.cpp`. Engine: `HPL2/scene/World.cpp`, `HPL2/scene/MeshEntity.cpp`, `HPL2/scene/AnimationState.cpp`, `HPL2/impl/PhysicsMaterialNewton.cpp`.
- Installed game data: `<game>/entities/enemy/servant_grunt/servant_grunt.ent`, `<game>/sounds/EnemySounds.dat`.
- Earlier research: `docs/research/avatars.md` (sounds, possession), #81 (`enemy-perception.md`), #82 (`enemy-motion.md`). amnesia-multiplayer ADR 0009 and `docs/research/shared-enemy-death-and-hallucinations.md`. ADRs 0002, 0003, 0005, and 0008 here.
