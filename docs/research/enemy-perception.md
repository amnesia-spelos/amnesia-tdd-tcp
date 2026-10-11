# Enemy perception: how the Grunt, Brute, and Kaernk perceive, choose, and chase the player

Findings for the Shared Enemies map (`amnesia-spelos/amnesia-multiplayer#51`), ticket `amnesia-spelos/amnesia-tdd-tcp#81`. The question: how each in-scope enemy perceives, chooses, and chases the player, and where the engine assumes there is exactly one player. Terms follow both `CONTEXT.md` files (Avatar, Pose, Peer-Driven Entity, Map Visit, Shared Enemy, Hold).

Paths are relative to `src/` and line numbers are those at commit `a6dcdbd`. Values marked *(.ent)* come from the shipped Steam install (`entities/enemy/*/*.ent`, `sounds/EnemySounds.dat`, `materials.cfg`, `config/game.cfg`), which is not in this repository.

## Which classes

- **Grunt and Brute are one class.** Both `.ent` files say `EntityType="Enemy_Grunt"` *(.ent)*, so both run `cLuxEnemy_Grunt` (`amnesia/src/game/LuxEnemy_Grunt.cpp`). They differ only in variables: the Brute has `SightRange` 13 against the Grunt's 20, a wider body (1.5 against 1.2), `AlertToInstantHuntDistance` 3.5 against 2.5, and shorter hunt pauses *(.ent)*.
- **Kaernk is `cLuxEnemy_WaterLurker`** (`EntityType="Enemy_WaterLurker"` *(.ent)*; `LuxEnemy_WaterLurker.cpp`). Its mesh is always hidden (`:605-611`).
- **There is exactly one player.** Every query reads `gpBase->mpPlayer`. An enemy's senses produce one set of latches (`mbCanSeePlayer`, `mbPlayerDetected`, `mbPlayerInRange`, `mvLastKnownPlayerPos`; `LuxEnemy.h:629-664`), and no code path names any other character as a target.

## The update loop

`iLuxEnemy::OnUpdate` (`LuxEnemy.cpp:625-708`) runs only for active entities (`LuxMap.cpp:395`) that are not disabled (`:627`). While health is above zero it runs the senses in this order: `UpdateCanSeePlayer`, `UpdatePlayerDetected`, `UpdatePlayerInRange`, `UpdateCheckStuckAtDoor`, `UpdateCheckLastPlayerPos` (`:643-659`). Then come the state machine, the class-specific update, the body, animation, sound, glow, regen, and hallucination.

The senses never act directly. They queue a `cLuxStateMessage` (`SendMessage`, `:856-883`), and the state machine delivers it (`UpdateStateMachine`, `:1232-1272`). The class's handler for the current state runs first, then the global handler (state `-1`) if the state has none (`StateEvent`, `:1221-1228`; macros at `LuxEnemy.h:315-323`). A state "overloads global" by declaring an empty handler, which is how Hunt and the attack states ignore `PlayerDetected` and `HelpMe`. While `TriggersDisabled()` is true (script `SetEnemyDisableTriggers`, or the *local* player's flashback, `:1779-1782`), every message after `EndOfPath` except `PlayerInRange` and `PlayerOutOfRange` is dropped (`:859-866`).

## Sight

`iLuxEnemy::UpdateCanSeePlayer` (`LuxEnemy.cpp:1379-1480`):

- Returns at once if the player is dead (`:1383`). Checks every 0.3 s (`:1393-1395`).
- **Range.** `fMaxRange` starts at `SightRange` (`:1419`): 20 m Grunt, 13 m Brute, 10 m Kaernk *(.ent)*, ×1.15 in hard mode (`:278-282`).
  - **Darkness:** if the lantern is off, the player's light level (`GetHelperLightLevel()->GetNormalLightLevel()`) is below 0.35 (`mfPlayerInDarknessLightLevel`, `:501`), and the player's average speed is below 0.05, the range drops to `DarknessSightRange`: 5 m, or 4 m for the Kaernk *(.ent)* (`:1423-1425`). Standing still in the dark is what counts. A moving player in the dark is seen at full range.
  - **Crouch:** with the lantern off, crouching multiplies the range by 0.5 (`mfCrouchVisibleRangeMul`, `:502`, `:1427-1428`). Crouch is read from the local player's `cLuxMoveState_Normal` (`:1405-1414`).
  - **Lantern:** `if(bLanternOn) mfSightRange *= 1.5f;` (`:1421`) multiplies the *member*, not `fMaxRange`. Each 0.3 s check with the lantern lit raises that enemy's sight range by half again, and nothing resets it (`ResetProperties`, `:999-1037`, does not touch it). It returns to the `.ent` value only when the map is loaded again. A lantern lit for a second or two makes every active enemy's sight range effectively unlimited. This is in the source as imported (`4033aa8`). The lantern also adds 1.0 to the player's light level (`LuxPlayerHelpers.cpp:2783-2789`), so the darkness rule cannot apply while it is lit.
- **Field of view.** `InFOV` (`:2218-2238`) compares the angle to the target with the body's forward vector against `FOV × mfFOVMul`: 120° for Grunt and Brute, 180° for the Kaernk *(.ent)*. Investigate, Alert, Hunt, HuntWander, HuntPause, BreakDoor, the attacks, and Hurt set `mfFOVMul = 4` (for example `LuxEnemy_Grunt.cpp:312`, `:379`, `:545`), which takes the angle past 360°. **Once alerted, the enemy sees all round.** The FOV test is also skipped when the two bodies nearly touch (`LuxEnemy.cpp:1438-1442`).
- **Line of sight.** `LineOfSight` (`:1917-1972`) casts up to 9 rays from the enemy's eye to the player's body, spread over 40% of the body's size, and succeeds when 2 rays are clear. `cLuxLineOfSightCallback::BeforeIntersect` (`LuxMapHelper.cpp:44-87`) ignores character bodies, non-colliding bodies, and entities with only translucent materials. Walls, static geometry, and solid props block.
- **Latching.** Three consecutive successful checks (0.6 s or more) set `mbCanSeePlayer` and send `PlayerSeen` (`:1449-1459`). One failed check clears it (`:1463-1475`). Neither Grunt nor Kaernk handles `PlayerSeen`/`PlayerUnseen`; their state machines read `CanSeePlayer()` and `PlayerIsDetected()` instead.
- **Light on the enemy itself plays no part.** Light enters only through the *player's* light level. (`UpdateDarknessGlow`, `:1633-1704`, measures light at the enemy, but only to draw a glow for the local camera.)

## Detection and proximity

- **Grunt/Brute `PlayerIsDetected`** (`LuxEnemy_Grunt.cpp:913-924`): sight, or the player closer than the enemy's body width (1.2 m or 1.5 m *(.ent)*) and inside its FOV. `UpdatePlayerDetected` (`LuxEnemy.cpp:1484-1500`) sends `PlayerDetected`/`PlayerUndetected` on the edges, and the global `PlayerDetected` handler switches to **Alert** (`LuxEnemy_Grunt.cpp:152-153`). Idle overrides it with nothing (`:201-202`).
- **Kaernk `PlayerIsDetected`** (`LuxEnemy_WaterLurker.cpp:598-601`) uses **no sight and no distance limit**: the player is detected whenever their feet are within `PlayerDetectionHeight` (0.68 m *(.ent)*) of the Kaernk's feet height, which in practice means standing in its water. The global `PlayerDetected` handler goes straight to **Hunt** (`:151-152`). While eating, it hunts only if the player is within 2 m horizontally (`:359-364`).
- **Activation range.** `UpdatePlayerInRange` (`LuxEnemy.cpp:1504-1521`) sends `PlayerInRange` within `ActivationDistance` (40 m Grunt and Brute, 20 m Kaernk *(.ent)*) and `PlayerOutOfRange` beyond `1.25 × ActivationDistance + 2`. Idle waits for `PlayerInRange` before it starts patrolling (`LuxEnemy_Grunt.cpp:181-199`). The Grunt's global `PlayerOutOfRange` handler **deactivates the enemy** (`:164-165`).
- **Dead player.** `DistToPlayer` and its variants return 100000 when the player is dead (`LuxEnemy.cpp:2115-2139`), and the senses stop updating (`:1383`, `:1486`, `:1506`).

## Hearing

All hearing ends in `cLuxMap::BroadcastEnemySoundMessage` (`LuxMap.cpp:720-748`). It sends `SoundHeard` with a position and a volume to every active enemy whose body lies within the sound's max distance, skipping sounds closer than 2 m to the enemy. There is no occlusion: walls do not muffle a sound. Two paths feed it (also recorded in `docs/research/avatars.md`, "How enemies hear sounds"):

1. **World sound entities, by name.** `cMapHandlerSoundCallback::OnStart` (`LuxMapHandler.cpp:73-102`) is a global callback on every `cSoundEntity` start (registered at `:205-206`). It broadcasts a sound whose sound-data name starts with a prefix in `sounds/EnemySounds.dat`: `impact_`, `break_`, `explosion_`, `weapon_`, `player_step`, `player_scare`, `door_`, `interact_`, `item_dynamite`, `item_generic_throw`, `item_flare_loop`, `item_flashlight`, `item_radio_noise`, `metal_`, `plastic_`, `stall_`, `unbreak_`, `wood_`, `water_` *(.ent)*. It passes `GetMaxDistance()` as both min and max distance (`:95-96`), so any enemy within range hears the sound's full volume. Physics impacts become `impact_*` sound entities (`materials.cfg` *(.ent)*), created from Newton's contact callback for any two bodies that use surface effects (`HPL2/core/sources/impl/PhysicsMaterialNewton.cpp:361-366`; `physics/SurfaceData.cpp:121`).
2. **The player's footsteps**, which are GUI sounds and not sound entities. `cLuxMoveState_Normal` broadcasts them explicitly at the local player's feet with the step sound's volume and distances (`LuxMoveState_Normal.cpp:853-870`). In hard mode, running doubles the volume and adds 25% to the distance (`:860-867`).

The listener side:

- An enemy reacts when the received volume exceeds `HearVolume`: 0.55 for all three *(.ent)*, ×0.6 in hard mode (`LuxEnemy.cpp:283`).
- **Grunt/Brute** react in GoHome, Wait, Patrol, and Search by switching to **Investigate** at the sound's position (`LuxEnemy_Grunt.cpp:214-220`, `:262-268`, `:291-297`, `:517-523`). Inside Investigate, a louder sound re-targets the walk (`:353-362`; note that it re-paths to the *old* `mvTempPos`). **Alert, Hunt, and the attack states ignore sound**, and so does Idle. Investigate does not reveal the player. The enemy walks to the noise, and sight or proximity must take over from there (`:301-362`).
- **Kaernk** reacts in Idle, GoHome, and Investigate the same way (`LuxEnemy_WaterLurker.cpp:183-189`, `:222-228`, `:278-287`). Its own splashes are `water_*` sound entities, but the 2 m rule stops it from hearing itself.
- **Food.** A prop marked as food sends `FoodInRange` every 2 s to any enemy whose `InRangeOfFood` accepts it (`LuxProp_Object.cpp:810-830`). Only the Kaernk uses it: food within its detection height pulls it to Search, then Eat (`LuxEnemy_WaterLurker.cpp:160-172`, `:291-371`).

### Could an Avatar or a Peer-Driven Entity be heard?

- **Avatar: no, today.** `LuxAvatarHandler.cpp` plays no sounds and broadcasts nothing. Avatar footsteps would need either a matching sound-entity name or an explicit broadcast (see `avatars.md`).
- **Peer-Driven Entity: yes, its impacts can be heard.** A driven body stays in the Newton world with its streamed velocity (`LuxPeerDrivenEntityHandler.cpp:137-144`), and nothing turns off its surface effects. Its contacts therefore create `impact_*` sound entities in the receiving game, at the prop's position, which that game's enemies hear. Inference, not tested: a prop the other player throws makes noise in both games, and enemies in either game may investigate it. Breaks reach the receiving game through `BreakFrom` (`:124-133`). Whether the engine break plays a `break_*` sound entity there was not checked.

## The Grunt/Brute state machine

`cLuxEnemy_Grunt::StateEventImplement` (`LuxEnemy_Grunt.cpp:138-890`). An enemy starts in **Idle** (`LuxEnemy.cpp:445-446`).

| State | Entered from | What it does | Leaves to |
|---|---|---|---|
| **Idle** (`:176-202`) | start; `PlayerDead` (global); deactivation | resets `mbPlayerInRange` | `PlayerInRange` → Patrol (with patrol nodes) or Wait; plays the "enabled" sound once per global cooldown |
| **Wait** (`:224-269`) | Patrol end of path | waits the patrol node's time, plays idle extras | timeout → Patrol (or back to Search if it came from Search); sound → Investigate |
| **Patrol** (`:273-297`) | Idle, Wait, Search timeout, Investigate end | walks the script-added patrol nodes (`PatrolUpdateGoal`, `:952-976`) | door → BreakDoor; sound → Investigate; end → Wait. At the last node, with `mbAutoRemoveAtPathEnd`, not seen by the player, and the player more than 10 m away, it deactivates and runs `OnAutoDisabled` (`:980-990`) |
| **Investigate** (`:301-362`) | sound above `HearVolume` | Notice animation, then walks to a node near the sound | end of path → Search if it came from Search, else Patrol |
| **Alert** (`:367-468`) | `PlayerDetected` (global); BreakDoor or HuntPause when the player is still detected; Hurt | walks to `mvLastKnownPlayerPos` at 1.2× speed, re-paths every 0.3 s while detected, and adds itself to the player's **terror** set | see below |
| **Search** (`:472-523`) | Alert when the player is no longer detected and terror is below 1; Hunt after 1.5 s without detection | for `PlayerSearchTime` (10 s *(.ent)*) it repeatedly paths to a random node **4–12 m from the player's true position** (`:505`), not the last known one; plays search music | timeout → Patrol; sound → Investigate |
| **Hunt** (`:528-651`) | Alert; `HelpMe` (global); HuntPause loop; Hurt when detected | runs to `mvLastKnownPlayerPos`, re-paths every 0.2 s, and every 0.2 s broadcasts `HelpMe` to enemies within `0.5 × ActivationDistance` (`:627-628`), which puts them into Hunt at this player's position (`:170-172`); attack music; terror | sight within `NormalAttackDistance` (1.9 m *(.ent)*) → AttackMeleeShort; sight between 1.9 and 3.8 m, checked every 0.6 s → AttackMeleeLong; door → BreakDoor; stuck → HuntWander; 1.5 s without detection → Search. **After any landed attack** (`mlAttackHitCounter >= 1`), entering Hunt goes to HuntPause instead (`:532-539`) |
| **HuntPause** (`:688-727`) | Hunt after a hit | walks slowly towards the player's **true** feet position for `HuntPause{Min,Max}Time` | timeout → Alert |
| **HuntWander** (`:655-683`) | Hunt stuck while detected | walks to a nearby reachable node | end of path → Hunt |
| **BreakDoor** (`:732-772`) | any moving state when `mbStuckAtDoor` (Alert only at terror ≥ 1) | BreakDoor animation; its special event runs `Attack` with door damage | the player is detected → Alert; the door is broken → previous state; otherwise swing again |
| **AttackMeleeShort / AttackMeleeLong** (`:776-833`) | Hunt; Hurt | swing animation; its special event calls `Attack()`. The lunge steers to the player's **true** feet position until the hit frame (`:815-819`) | animation over → previous state |
| **Hurt** (`:837-866`) | damage above `GroggyDamageCount` (global `TakeHit`, `:155-161`) | flinch, 50% chance of a counter-swing when within 1.3 × attack distance | detected → Hunt, else Alert |
| **Dead** (`:870-886`) | health ≤ 0 (`LuxEnemy.cpp:808-814`) | deactivates the body, runs the `OnDeath` callback | — |

**Alert to Hunt is gated by the player's terror** (`:420-440`). The player's `mfTerror` rises at 0.33/s while any enemy is in their terror set and falls at 0.2/s otherwise (`LuxPlayer.cpp:1319-1332`; *(.ent)* `config/game.cfg`). Alert moves to Hunt and pins terror at 1 when the player is detected and either terror has reached 1 (about 3 s of Alert) and the player is farther than `AlertToHuntDistance` (9 m), or the player has kept running towards the enemy (`mfAlertRunTowardsCount` above 30 for the Grunt or 20 for the Brute). It moves to Hunt at once when the player is within `AlertToInstantHuntDistance` (2.5 m or 3.5 m). Losing detection drops to Search only while terror is below 1, and breaking a door from Alert requires terror at 1. **Terror is a value on the local `cLuxPlayer`, so the AI's decision depends on a per-player state that only the local game holds.**

**`PlayerDead` sends every Grunt to Idle** (`:167-168`), whichever state it is in.

## The Kaernk state machine

`cLuxEnemy_WaterLurker::StateEventImplement` (`LuxEnemy_WaterLurker.cpp:145-536`). It has no patrol (`PatrolUpdateGoal`/`PatrolEndOfPath` are empty, `:630-640`) and no Alert or Search for the player.

- **Idle** → sound → **Investigate** (walks to the sound, then back to Idle) or `FoodInRange` → **Search** (walks to the food) → **Eat** (bites the nearest food within 3.5 m every 1.2 s, then **GoHome**).
- `PlayerDetected` (the height rule) → **Hunt**: re-paths to the player's **true** feet position every 0.4 s (`:410-418`); within 1.9 m it goes to **AttackMeleeShort**, which needs no sight, then **HuntPause** (1.8 s *(.ent)*) and back to Hunt. Losing detection (the player leaves the water) → **GoHome**, which paths to a node within 6 m **of the player's feet** (`:193-208`), or else to the start position.
- `TakeHit` is ignored (`:154-155`). `PlayerDead` → Idle (`:157-158`).
- `mbCausesSanityDecrease` is true only while hunting (`:377-394`, `:414-418`).

## Attacks and damage

`iLuxEnemy::Attack` (`LuxEnemy.cpp:1786-1803`) calls `cLuxMapHelper::ShapeDamage` (`LuxMapHelper.cpp:265-445`) with `abCheckEnemies=false`, the attack's `mbCheckPlayer` and `mbCheckProps`, and `abLethalForPlayer=true`.

- It overlaps the attack shape with physics bodies and requires a clear ray from the enemy's centre that ignores characters (`cLuxAttackRayCallback`, `:202-209`).
- A character body counts as "the player" only if it is `gpBase->mpPlayer`'s current body (`:308-319`). Every other character body counts as an enemy, and enemies are skipped. **So an Avatar's body is never hit.**
- On the player it adds a knockback velocity (`:359-371`) and calls `gpBase->mpPlayer->GiveDamage(..., abLethal=true)` (`:426-436`). `bHitPlayer` comes back to the caller, but the Grunt counts any hit, on a prop or a door as well, in `mlAttackHitCounter` (`LuxEnemy.cpp:1798-1801`).
- Props, including Peer-Driven Entities, take damage and impulses (`:359-371`, `:426-431`). An impulse on a driven body is overwritten by the next streamed sample.
- Enemy bodies pass through Avatars (`LuxEnemy.cpp:187-188`), so an Avatar can neither block an enemy nor be pushed by one.

## When the local player dies

- `cLuxPlayerDeath::Start` broadcasts `PlayerDead` to every enemy on the map (`LuxPlayerHelpers.cpp:2004-2006`), so every Grunt and Kaernk goes to Idle.
- When the death sequence ends, `ResetGame` (`:2302-2324`) calls `cLuxMap::LoadCheckPoint`. That puts the player at the checkpoint, and every enemy on the map is **deactivated and reset to its start position** (`LuxMap.cpp:509-539`: `SetActive(false)` + `ResetProperties`). Terror enemies and music are cleared, and the map script's checkpoint callback runs. (`cLuxPlayerDeath::Start` itself begins at `:1948`.)
- In vanilla, the death of either player in their own game therefore removes every enemy from that game's map until the script re-activates it. The other player's game sees nothing of this unless it is shared.

## Other global triggers that assume the player

- Insanity collapse calls `AlertOfPlayerPresence` on every active enemy (`LuxPlayerHelpers.cpp:140-148`), which sends Idle and Patrol enemies to Search with the player's position (`LuxEnemy.cpp:830-839`).
- A flashback broadcasts `Reset` (`LuxPlayerHelpers.cpp:880`), which clears terror and music and sends Grunts to Wait (`LuxEnemy_Grunt.cpp:144-150`). While it lasts, `TriggersDisabled` mutes the senses (`LuxEnemy.cpp:1779-1782`).
- Script functions `ShowEnemyPlayerPosition` and `AlertEnemyOfPlayerPresence` (`LuxScriptHandler.cpp:3089-3125`) copy the local player's feet position into `mvLastKnownPlayerPos`. The first always forces Hunt, because its state test uses `||` and is always true (`:3097-3104`). The second sends the enemy to Search unless it is already hunting or attacking.
- `SetIsSeenByPlayer` is written each update by the local player's sanity helper, which also drains sanity when the player sees an enemy (`LuxPlayerHelpers.cpp:1393-1459`). The Grunt reads it only for auto-removal at the end of its patrol (`LuxEnemy_Grunt.cpp:984`).

## Every use of the player, grouped by what "a target among several" needs

Uses in `LuxEnemy.cpp`, `LuxEnemy_Grunt.cpp`, and `LuxEnemy_WaterLurker.cpp`. `LuxEnemyMover.cpp` and `LuxEnemyPathfinder.cpp` have none.

### 1. Target position only: swap in "the current target's character body"

These need only a feet or body position, and an Avatar already has an `iCharacterBody` driven from its Pose.

- `GetPlayerFeetPos` (`LuxEnemy.cpp:2068-2071`), `DistToPlayer`, `DistToPlayer2D` (both overloads), `AbsHeightDistToPlayer`, `GetDirection2DToPlayer` (`:2115-2144`), `PlayerInFOV` (`:2240-2243`).
- `ShowPlayerPosition` (`:825-828`), `UpdateCheckLastPlayerPos` (`:1612-1629`), `GetSearchForPlayerNode` (`:1830`), `GetPatrolAroundPlayerNode` (`:1863`), `UpdateHallucination` (`:1723`).
- Grunt: Search node near the player (`LuxEnemy_Grunt.cpp:505`), HuntPause re-path (`:711-712`), lunge steering (`:818`), and the `DistToPlayer`/`DistToPlayer2D` calls at `:392`, `:445`, `:568`, `:585`, `:595`, `:711`, `:858`, `:921`, `:984`.
- Kaernk: Hunt re-path (`LuxEnemy_WaterLurker.cpp:412`), GoHome (`:197`), `DistToPlayer2D` (`:360`), `DistToPlayer` (`:423`), `AbsHeightDistToPlayer` (`:600`).
- **Caveat:** `GetPlayerMovementTowardEnemyAmount` (`LuxEnemy.cpp:2190-2205`) reads body velocity, which is always zero for an Avatar (`avatars.md`, Animations pitfall). It needs speed from the Pose model.

### 2. Target state that the Pose does not fully carry

- **Lantern lit** (`:1403`): carried by the Pose's raised-lantern flag.
- **Crouching** (`:1405-1414`): in the Pose, but `cLuxAvatarHandler` drops it today (`avatars.md`).
- **Light level at the player** (`:1423`): `cLuxPlayerLightLevel` exists only for the local player. An Avatar would need `cLuxMapHelper::GetLightLevelAtPos` (`LuxMapHelper.cpp:522`) at its position, plus 1.0 when the lantern is raised.
- **Average speed** (`:1423`): from the Pose model, as above.
- **Dead** (`:1383`, `:1486`, `:1506`, `:2117`, `:2124`, `:2136`): not in the Pose.
- **Flashback active** (`:1781`): local only.
- **Body shape for line of sight and the minimum distance** (`:1399`, `:1435-1436`, `:1445`): available from the Avatar's body.

### 3. Single-target sense latches and messages: per-target state, or a "current target" with re-latching

- `mbCanSeePlayer`, `mlPlayerInLOSCount`, `mfLookForPlayerCount` (`:1379-1480`); `mbPlayerDetected` (`:1484-1500`); `mbPlayerInRange` (`:1504-1521`); `mvLastKnownPlayerPos` and `mfLastPlayerPosCount` (`:1612-1629`). All are saved (`SaveToSaveData`, `:2413+`).
- The message types `PlayerSeen`/`Unseen`/`Detected`/`Undetected`/`InRange`/`OutOfRange`/`PlayerDead` carry no target. Choosing between two players means evaluating the senses per candidate and deciding which one drives these latches.
- Range-based deactivation (`LuxEnemy_Grunt.cpp:164-165`) and auto-removal (`:984`) must count *all* players, or one player walking away switches the enemy off for both.

### 4. The decision depends on the local player's feedback state

- **Terror:** `AddTerrorEnemy`/`RemoveTerrorEnemy` (`LuxEnemy_Grunt.cpp:147`, `:374`, `:388`, `:549`, `:639`, `:875`; `LuxEnemy_WaterLurker.cpp:522`; `LuxEnemy.cpp:2267`), with `GetTerror`/`SetTerror` gating Alert → Hunt and Alert → BreakDoor (`LuxEnemy_Grunt.cpp:420`, `:429`, `:435-438`). A simulating game has no terror value for an Avatar.
- **Music** (`gpBase->mpMusicHandler`, Search and Attack music throughout the Grunt and Kaernk) and the enemy-activate sound: presentation for the local player.
- **`IsSeenByPlayer`, `IsInPlayerFovAtFeetPos`, `IsVisibleToPlayerAtFeetPos`** (`LuxEnemy.cpp:2148-2186`) use the local camera. The Grunt and Kaernk do not call them, beyond the `mbIsSeenByPlayer` flag above.
- **Darkness glow** (`:1653-1670`) uses the local camera and sanity: purely local rendering.

### 5. Damage: only the local player can be hit

- `ShapeDamage` compares the body against `gpBase->mpPlayer` (`LuxMapHelper.cpp:308-319`, `:426-436`). Hitting an Avatar needs a new branch that reports the hit, rather than applying damage locally.

### 6. Player lifecycle broadcasts that hit every enemy on the map

- `PlayerDead` (`LuxPlayerHelpers.cpp:2006`), checkpoint reset (`LuxMap.cpp:509`), `Reset` on flashback (`LuxPlayerHelpers.cpp:880`), insanity-collapse alert (`:147`), and the two script functions (`LuxScriptHandler.cpp:3089-3125`). Each assumes that what happens to the one player should happen to every enemy.

## What this means for a Shared Enemy

- **Each game's AI perceives only that game's own player.** Avatars are invisible to sight (characters are skipped by line-of-sight rays, and there is no target branch), silent (no sounds), untouchable (no collision with enemy bodies), and unhittable (`ShapeDamage`). In vanilla, each game runs its own copy of the enemy, chasing its own player.
- **Vanilla targeting reduces to per-player predicates that are cheap to evaluate for an Avatar:** a range from `SightRange` with the darkness, crouch, and lantern modifiers; FOV (ignored once alerted); 2-of-9-ray line of sight against solid geometry; proximity under body width; and, for the Kaernk, feet height only. Those predicates need the Avatar's body, its lantern, crouch, and speed from the Pose, and a light-level sample at its position. Hearing is position-only and already works for anything that makes a hearable sound entity in the simulating game.
- **One player wins at a time.** The machine has a single target. Search, HuntPause, the lunge, and the Kaernk's chase read the target's *true* position, not the last known one. A switch of target is therefore a switch of whose position those reads return, plus re-latching sight and detection.
- **Terror is the one decision input that is per-player and not perceptual.** For the Grunt and Brute, a Shared Enemy needs a terror value for each candidate in the simulating game, or a rule that replaces it.
- **The enemy hears the other player's noise only if that noise exists in the simulating game.** Impacts of Peer-Driven Entities already exist there. Avatar footsteps do not.
- **The vanilla death path deactivates every enemy in the dying player's game** (`PlayerDead`, then `LoadCheckPoint`). With one simulating game, the death of the simulating player would switch off the enemy for both unless that path is changed.
- **Watch the lantern sight-range growth.** Two players means twice the chances to light a lantern near an active enemy, and the enemy's `mfSightRange` keeps the growth for the rest of the Map Visit.
