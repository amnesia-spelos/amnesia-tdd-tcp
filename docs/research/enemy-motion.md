# Enemy motion and attacks, and driving a copy from streamed state

Findings for the Shared Enemies map (`amnesia-spelos/amnesia-multiplayer#51`, ticket #82). The questions: what an enemy's motion and attack are made of, and whether another machine can reproduce an enemy from streamed state instead of simulating it. This covers the Grunt, the Brute, and the Kaernk. Perception (sight, hearing, targeting) has its own ticket and is touched here only where motion depends on it. Terms follow `CONTEXT.md` here and in `amnesia-multiplayer`. ADR 0003 and ADR 0005 describe the existing Avatar and Peer-Driven Entity machinery.

Paths are relative to `src/`, and line numbers are those at commit `a6dcdbd`. Paths under `entities/` are the installed game's shipped data, which is not in this repository. "Inference" marks reasoning that has not been checked directly.

## Which code each enemy runs

- Three enemy loaders are registered: `Enemy_Grunt`, `Enemy_WaterLurker`, and `Enemy_ManPig` (`amnesia/src/game/LuxBase.cpp:1508-1510`).
- **Grunt and Brute run the same class.** `servant_grunt.ent:114` and `servant_brute.ent:150` both say `Type="Enemy_Grunt"`. Only their variables differ: the Brute has a wider body (`Body_Size` 1.5 rather than 1.2) and hits harder (`NormalAttackDamage` 50–120 rather than 30–60). The rest of this document says "Grunt" for both.
- **The Kaernk is `cLuxEnemy_WaterLurker`** (`waterlurker.ent:10`). It sets `mbUseAnimations = false` (`amnesia/src/game/LuxEnemy_WaterLurker.cpp:103`) and hides its mesh at setup and on every activation (`:126`, `:605-610`). Its `.ent` has no `<Animation>` entries. What the player sees of it is only its splashes (`SplashWater`, `:572-594`) and sounds.

## What an enemy is made of

### Character body

- The loader creates an `iCharacterBody` sized by `Body_Size` (`amnesia/src/game/LuxEnemy.cpp:185`). It sets the body's mass, push mass and force, character push mass and force, step size, and slope (`:189-198`). It makes the body carry the mesh with `SetEntity`, sets the offset from `Body_OffsetRot`/`Body_OffsetTrans`, and turns on Y smoothing over 20 samples (`:200-206`).
- **The enemy already passes through Avatars.** Its collide flags exclude `cLuxAvatarHandler::kCollideFlag` (`:187-188`).
- The body moves itself. It is not a Newton dynamic body. `iCharacterBody::Update` (`HPL2/core/sources/physics/CharacterBody.cpp:950-1029`) stores the last position, moves the body by its move speed (`UpdatePostionFromCharSpeed`), resolves collision, applies external forces and gravity, and then places the mesh (`UpdateEntity`, `:2063-2093`).
- **The enemy pushes props only while it moves by itself.** `CheckMoveCollision` attaches the push callback only when the position change is non-zero (`CharacterBody.cpp:1561-1562`), and the force pass does the same (`:1787-1793`). The callback pushes a prop no heavier than `MaxPushMass` with `PushForce` at the contact points, and pushes a character with `CharacterPushForce` (`:129-198`). For the Grunt these are 40 kg and 200 (`servant_grunt.ent:143-144`).

### Mover: turning, speed, and the move animation

`cLuxEnemyMover::OnUpdate` runs four steps each tick (`amnesia/src/game/LuxEnemyMover.cpp:72-78`):

1. **Stuck counter** (`:168-196`). It compares the speed the body wanted with the distance it actually covered. The pathfinder gives up on a path when the counter reaches its maximum, and the door check reads it.
2. **Turning** (`:200-224`). `TurnToPos` sets a goal yaw (`:90-104`). Each tick the body turns `min(TurnSpeedMul·|Δ|, TurnMaxSpeed)` towards that yaw with `AddYaw`. `MoveToPos` turns and then calls `Move(eCharDir_Forward, 1)` (`:82-86`), which is the only way the AI moves the body.
3. **Move animation** (`:227-340`). This is a state machine over backward, stopped, walking, and running, with hysteresis thresholds from the `.ent` (Grunt: 0.15, 0.1, 3.2, and 2.8 m/s, `servant_grunt.ent:164-167`). It plays `Backward`, `Idle`, `Walk`, or `Run` through `iLuxEnemy::PlayAnim` and keeps the walk/run phase in step across a switch (`:310-321`). It sets the clip's playback speed to `|speed|·MoveSpeedAnimMul`. When the enemy turns in place, it uses the turn speed instead (`:327-339`). It reads speed from `GetVelocity` (`:142-147`), which is `(position − lastPosition)/dt` (`CharacterBody.cpp:595-600`).
4. **Step effects** (`:344-402`). In water, every `Step` animation event creates a splash particle system and a sound.

`iLuxEnemy::UpdateCharBody` sets the forward speed cap from the current move speed, times a brake factor during sharp turns (`LuxEnemy.cpp:1367-1375`, `LuxEnemyMover.cpp:118-138`). `SetMoveSpeed` picks the walk or run speed for the current pose (`LuxEnemy.cpp:1807-1814`). Hard mode raises those speeds (`:298-306`).

### Pathfinder

- At map load, the pathfinder builds an AI node container named after the enemy's `.ent` file and sized to its body, plus an A* handler (`amnesia/src/game/LuxEnemyPathfinder.cpp:68-81`).
- `MoveTo(goal)` runs A* to fill a node list (`:85-128`). `UpdateMoving` steers the mover towards the next node and pops it when the node falls inside an enlarged bounding box (`:334-403`). It also pops a node when the distance to it has kept growing over 150 samples (`:366-387`). At the end of the path it sends `EndOfPath`. When the mover is stuck, it sends `EndOfPath` with a custom value of 1 (`:406-414`).
- `GetNodeAtPos` uses `cMath::RandRectl` both for its random start index and for its final pick (`:150`, `:222`).
- None of this is needed on a copy that only shows the enemy. The node list is transient, and only the goal and node ids are saved (`:428-466`).

### State machine and animation choice

- `iLuxEnemy::OnUpdate` runs the following in order: pathfinder, mover, the senses (sight, detection, range, stuck-at-door, and last player position), the state machine, the type-specific update, the character-body speed caps, the special-animation check, ambient sound, darkness glow, regeneration, hallucination, and ground alignment (`LuxEnemy.cpp:625-712`). It skips the whole update when the step is shorter than 0.8 of a logic step (`:631-634`), and it does nothing while `mbDisabled` is set (`:627`).
- States change through `ChangeState`, which sets `mNextState` (`:843-852`). The state machine runs Leave and Enter events (`:1274-1286`) and delivers queued messages whose delay has run out (`:1232-1270`). Timers such as `TimeOut` and `TimeOut_2` are delayed messages to the enemy itself.
- **Clips come from two places:**
  - **Move clips** come from the mover's speed state machine, as above.
  - **Action clips** are chosen by states: `Notice1/2`, `IdleExtra1..3`, `SwingClaws01/02`, `SwingLaunch`, `BreakDoor`, `Flinch`, and `Dead` (`amnesia/src/game/LuxEnemy_Grunt.cpp:257, 307, 735, 760, 780, 807, 840, 874`). `PlayAnim` cross-fades the new clip in and the previous one out. It can override the move state and hand control back to it when the clip ends (`LuxEnemy.cpp:887-949`).
- `UpdateAnimation` runs only while an action clip overrides the move state (`LuxEnemy.cpp:1332-1363`). It sends `AnimationSpecialEvent` when the clip's time crosses its `SpecialEventTime`, which is in seconds and is read from the `.ent` (`HPL2/core/sources/resources/EntityLoader_Object.cpp:447-457`). It sends `AnimationOver` when the clip ends.
- **Animation events play their own sounds.** A `PlaySound` event in an `.ent` animation, such as the Grunt's footsteps and claw whooshes (`servant_grunt.ent:72-83`), is played by `cMeshEntity` as the clip advances (`HPL2/core/sources/scene/MeshEntity.cpp:1449-1461`). The enemy's AI is not involved. **A copy that plays the same clip gets these sounds for free.**
- Ambient growls are separate. `UpdateSoundState` plays `AmbientSound_<Idle|Alert|Hunt>` at random intervals (`LuxEnemy.cpp:1290-1328`).

### Randomness in enemy code

Every call draws from the one unseeded global `rand()` stream (`docs/research/physics-determinism.md`), so the two machines cannot reproduce these choices:

| Where | What it picks |
|---|---|
| `LuxEnemyPathfinder.cpp:150, 222` | which nearby AI node a search, wander, or flee goes to |
| `LuxEnemy.cpp:1827, 1838, 1870, 1880` | the angle and distance of a search or patrol-around-player node |
| `LuxEnemy.cpp:1302, 1322` | when the next ambient growl plays |
| `LuxEnemy_Grunt.cpp:227, 232, 252, 260, 478, 500, 692` | wait, idle-extra, search, and hunt-pause timer lengths |
| `LuxEnemy_Grunt.cpp:257, 307, 779` | which `IdleExtra`, `Notice`, or `SwingClaws` clip plays |
| `LuxEnemy_Grunt.cpp:858` | whether a hurt Grunt counter-attacks |
| `LuxEnemy_WaterLurker.cpp:467` | the pause between the Kaernk's door hits |
| `LuxMapHelper.cpp:427` | the damage of each hit, between min and max |
| `LuxPlayer.cpp:1365`; `LuxPlayerHelpers.cpp:2545, 2549` | the hit's head-spin direction and the damage-splash image and position |

A Grunt hunt also reads the player's position many times per second (`LuxEnemy_Grunt.cpp:568-632`). **Running the same AI on both machines and expecting it to agree is not feasible.** Motion depends on this randomness, on player positions read at slightly different ticks, and on character-body collisions with props that Newton moves differently on each machine (`docs/research/physics-determinism.md`).

## How an attack lands

### The hit test

- Each attack state calls `iLuxEnemy::Attack(sizeData, damageData)` once:
  - The Grunt calls it on the clip's `AnimationSpecialEvent`, for `SwingClaws` at 0.5 s and `SwingLaunch` at 0.45 s (`LuxEnemy_Grunt.cpp:792-793`, `:823-825`; `servant_grunt.ent:80-86`).
  - The Kaernk calls it on a 0.3 s timer, since it has no clips (`LuxEnemy_WaterLurker.cpp:494-505`).
- `Attack` calls `cLuxMapHelper::ShapeDamage` (`LuxEnemy.cpp:1786-1803`). The attack shape is a box sized by `NormalDamageSize`. It is placed `NormalDamageOffset` in front of the body and turned to the body's yaw (`:2006-2019`, `:418-423`). For the Grunt it is 1 × 1.6 × 1.75 at 1.2 m forward (`servant_grunt.ent:189-190`).
- `ShapeDamage` (`amnesia/src/game/LuxMapHelper.cpp:265-446`) works in four steps:
  1. It collects every body in the shape's bounding box and skips any that is inactive or does not collide (`:282-303`).
  2. It skips a character body unless it is the local player's (`abCheckPlayer`) or `abCheckEnemies` is set (`:308-318`). `Attack` passes `abCheckEnemies = false` (`LuxEnemy.cpp:1794`).
  3. It needs an exact shape-to-shape collision (`:327-329`) and a clear ray from the enemy's centre to the body (`:333-336`).
  4. For each body that passes, it applies knockback, plays a hit sound and particle system from the body's surface material, and deals damage (`:358-436`).
- **An Avatar can never be hit.** It is a character body that is not the local player's, and enemy attacks do not check other characters (`LuxMapHelper.cpp:308-318`). So **in the simulating game, the enemy's attack cannot reach the other player** unless the attack itself changes. This is consistent with the enemy passing through Avatars (`LuxEnemy.cpp:187-188`).
- **One swing hits everything in the box,** including props and doors (`abCheckProps` defaults to true, `LuxEnemy.h:331-333`). Any hit raises `mlAttackHitCounter` and plays `msHitSound` (`LuxEnemy.cpp:1796-1800`). The Grunt's next `Hunt` Enter turns into `HuntPause` after any hit (`LuxEnemy_Grunt.cpp:532-539`), even when only a prop was hit.

### Damage, knockback, and the player's hit effects

- **Damage** is `RandRectf(min, max)` (`LuxMapHelper.cpp:427`). The player gets `cLuxPlayer::GiveDamage(damage, strength, type, spinHead = true, lethal = true)` (`:432-435`). Enemy attacks are lethal (`LuxEnemy.cpp:1794`). Hard mode doubles the damage range (`LuxEnemy.cpp:408-413`).
- **Hit effects on the player** (`amnesia/src/game/LuxPlayer.cpp:587-606`) are a damage splash on the HUD for the damage type, a 0.25 s red flash, and a head spin in a random direction (`:1363-1371`). At zero health, `mpDeath->Start()` runs. All of this is local to the player's own game, so it needs only "you were hit for N with type T" to reproduce.
- **Knockback on the player** is `AddForceVelocity(dir · force · 0.1)` on the player's character body (`LuxMapHelper.cpp:364-367`; `CharacterBody.h:207`). The direction runs from the enemy's centre to the contact point. With the Grunt's `NormalAttackForce` of 30 (`servant_grunt.ent:193`) this is a 3 m/s shove. On a prop, the knockback is an impulse of `min(force/mass, maxImpulse)` at the hit point (`:369-372`).
- The engine also runs the player's per-enemy terror and music (`AddTerrorEnemy`, `mpMusicHandler->AddEnemy`, for example `LuxEnemy_Grunt.cpp:547-549`), plus sanity loss from seeing an enemy. That check is in the player's own helper, which looks at every active enemy's character body from the player's camera (`amnesia/src/game/LuxPlayerHelpers.cpp:1395-1440`). Inference: the sanity check needs only an active enemy body in the right place, so it would work on a driven copy unchanged. Terror and music are called from the AI's states, so a copy whose AI is not running would need them driven separately.

## How an enemy breaks doors and props

- **Detecting a door.** While the stuck counter is at 0.1 or above, every 0.1 s `UpdateCheckStuckAtDoor` looks for a swing-door body with mass whose bounding box overlaps the enemy's. The door must lie within about 78° of the direction the enemy is heading (dot product > 0.2). When it finds one, it sends `StuckAtDoor` (`LuxEnemy.cpp:1525-1608`).
- **Grunt.** From Patrol, Investigate, Search, or Hunt (and from Alert only at full terror), the Grunt changes to `BreakDoor` (`LuxEnemy_Grunt.cpp:281-285`, `341-345`, `420-424`, `490-494`, `573-577`). It stops, turns towards the door, and plays `BreakDoor`. At the clip's special event (0.5 s) it calls `Attack` with the `BreakDoor` damage, and it repeats the clip until `DoorIsBroken` (`:732-766`).
- **Kaernk.** It calls `Attack` on a timer instead, then returns to `Hunt` after a random 0.5–1.5 s whether the door broke or not (`LuxEnemy_WaterLurker.cpp:457-481`).
- **The door breaks through ordinary prop damage.** `ShapeDamage` calls the door's `GiveDamage` (`LuxMapHelper.cpp:428-431`), which lowers its health by toughness (`amnesia/src/game/LuxProp.cpp:567-575`). `cLuxProp_SwingDoor::OnHealthChange` (`amnesia/src/game/LuxProp_SwingDoor.cpp:409-548`) has two damage-mesh stages. At zero health it disables the door's bodies, **creates a runtime `<name>_broken` entity with an impulse on each piece**, plays the break sound and particle system, and runs the `Break` callback. The hit also pushes the door's body (`LuxMapHelper.cpp:369-372`).
- Any other breakable prop in the swing box is damaged the same way. The Kaernk also damages and drags food props (`LuxEnemy_WaterLurker.cpp:328-338`).
- **The existing break relay does not cover doors.** A Peer-Driven Entity break reaches the other game only for a `cLuxProp_Object` (`amnesia/src/game/LuxPeerDrivenEntityHandler.cpp:122-136`, ADR 0005). Any other prop is handed back to local physics. A door broken by an enemy in one game therefore needs its own path to the other game: its damage stage, its break, and the runtime debris. Debris is runtime-created, so it is not a candidate for a Peer-Driven Entity (`LuxPeerDrivenEntityHandler.cpp:45`).

## What a driven copy needs

### Driving the body: use the Avatar recipe, not the Peer-Driven Entity

- A **Peer-Driven Entity cannot carry an enemy.** The handler accepts only map-placed props (`LuxPeerDrivenEntityHandler.cpp:41-51`). It drives `iPhysicsBody` matrices and velocities (`:138-145`), and an enemy is a character body.
- The **Avatar recipe fits.** Each update, `cLuxAvatarHandler::Update` zeroes the force and the force velocity, then sets the feet position and yaw from a Pose interpolated on the ADR 0003 delay (`amnesia/src/game/LuxAvatarHandler.cpp:107-112`). It uses `cAvatarPoseModel` for the sender's clock, the map, the teleport counter, and holding the last Pose. That model now also reports horizontal and forward speed for clip choice (`:531-532`). The same model could buffer an enemy's feet position and yaw unchanged (inference).
- Adjustments for an enemy body:
  - **Stop its own motion.** Call `StopMovement()` once, which zeroes move speed and acceleration (`CharacterBody.cpp:800-810`). Turn gravity off, as Avatars do (`LuxAvatarHandler.cpp:297`). A body that is not moving by itself will not push props or the player (`CharacterBody.cpp:1561-1562`), so the copy does not disturb local props. The simulating game's pushes are not reproduced either. They are part of the knock-on problem in `physics-determinism.md`.
  - **Keep the Y smoothing.** `SetFeetPosition` defaults to `abSmooth = false`, which clears the 20-sample Y-smoothing list on every call (`CharacterBody.h:181`; `CharacterBody.cpp:604-617`). The mesh then follows the streamed Y exactly. Pass `true` except at a snap to keep stair smoothing. This corrects the remark in `avatars.md` that the smoothed Y "will lag behind snaps".
  - **Speed.** A body placed by `SetFeetPosition` reports zero velocity (`CharacterBody.cpp:604-609`). The mover's clip choice and the speed-dependent clip rate would see zero, so speed must come from the Pose model or be streamed.

### Keeping the copy's AI from fighting the stream

- `SetDisabled(true)` stops the whole `OnUpdate` and every message to the enemy (`LuxEnemy.cpp:627`, `:856-858`, `:1139-1147`). It does not stop the world from advancing the active clip or the character body's own `Update`. `avatars.md` § "Enemy possession" lists everything it stops and the save and script hazards. Those hazards apply unchanged: `mbDisabled` is saved, and a map script may call `SetEnemyDisabled(name, false)`.
- Disabling also stops what the copy still wants locally: ambient growls (`UpdateSoundState`), the darkness glow drawn for the local camera (`UpdateDarknessGlow`, `LuxEnemy.cpp:1633-1660`), and ground alignment. The likely shape is a dedicated "driven" mode that skips the pathfinder, mover, senses, state machine, and `UpdateCharBody`, but keeps those local effects (inference). It is not the existing `mbDisabled`.

### The minimal stream

| State | Why the copy needs it | Rate |
|---|---|---|
| feet position, yaw | where the body and mesh are, and the hit box's frame | Pose rate, interpolated (ADR 0003) |
| teleport counter, map | snap rather than glide after `SetPositionAtStartPos`, `FadeToSmoke`, a script teleport, or a Map Visit change | with each sample |
| current clip name, loop flag, start time (or relative time) | action clips are random (`SwingClaws01/02`, `Notice1/2`, `IdleExtraN`) and set by state, so the copy cannot infer them from speed. The time keeps the swing in step with the hit | on change, sender-timestamped |
| clip playback speed, or let the copy use interpolated speed | move clips scale with speed (`LuxEnemyMover.cpp:327-339`) | per sample, or derived |
| active, alive/dead, sound state (Silent/Idle/Alert/Hunt) | visibility and sanity check; ambient growl set; death clip | on change |
| target player | which player is being hunted, for per-player terror and music, and for attack resolution | on change |
| hit events | "player P hit for N, damage type T, knockback direction D" and prop or door damage, since `ShapeDamage` is random and decides against one machine's state | per hit |

Animation-event sounds and the mover's water splashes follow from the clip, so they need no messages. The Kaernk has no mesh and no clips. Its copy needs position, active state, sound state, and its four `SplashWater` kinds as events (`LuxEnemy_WaterLurker.cpp:572-594`), because those come from timers, not clips.

### What this settles, and what it leaves to the spec

- **The enemy can be shown from streamed state** with the Avatar's driving recipe and Pose model plus a clip channel. The Peer-Driven Entity machinery does not apply to it.
- **The hit cannot come from the simulating game's attack as it is**, because `ShapeDamage` never tests an Avatar's body (`LuxMapHelper.cpp:308-318`). Two options follow from the code:
  - **The victim's game resolves the hit.** It runs the same `ShapeDamage` box against its own player from the driven copy's position and yaw at the special-event time. The hit then matches what that player sees and keeps the ray and shape tests local.
  - **The simulating game resolves the hit.** It tests the attack box against the Avatar as well and sends the hit.

  This choice belongs to the authority-model ticket.
- **Door and prop breaks caused by an enemy need a path that ADR 0005 does not yet provide.** It covers only an Object's break, and only when the Object is Peer-driven.

## Sources

- Engine and game code at `a6dcdbd`: `amnesia/src/game/LuxEnemy.cpp`, `LuxEnemyMover.cpp`, `LuxEnemyPathfinder.cpp`, `LuxEnemy_Grunt.cpp`, `LuxEnemy_WaterLurker.cpp`, `LuxMapHelper.cpp`, `LuxPlayer.cpp`, `LuxPlayerHelpers.cpp`, `LuxProp.cpp`, `LuxProp_SwingDoor.cpp`, `LuxAvatarHandler.cpp`, `LuxPeerDrivenEntityHandler.cpp`, `LuxBase.cpp`; `HPL2/core/sources/physics/CharacterBody.cpp`, `HPL2/core/sources/scene/MeshEntity.cpp`, `HPL2/core/sources/resources/EntityLoader_Object.cpp`.
- Shipped entity data (installed game, not in this repository): `entities/enemy/servant_grunt/servant_grunt.ent`, `servant_brute/servant_brute.ent`, `waterlurker/waterlurker.ent`.
- `docs/research/physics-determinism.md`, `docs/research/avatars.md`, ADR 0003, ADR 0005.
