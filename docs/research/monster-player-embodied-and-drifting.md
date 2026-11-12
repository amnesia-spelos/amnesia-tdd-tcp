# The Monster Player's own game: Embodied as a Grunt, and Drifting

Research for #124, part of the Monster Mode map (amnesia-spelos/amnesia-multiplayer#90). It covers what the Monster Player's own game needs in order to play as a Grunt (Embodied) and to fly without a body (Drifting). How the Session Host's game shows that Grunt is #125. Terms follow `GLOSSARY.md` here and amnesia-multiplayer's `CONTEXT.md` (Monster Player, Embodied, Drifting, Shared Enemy, Enemy Pose).

Paths are relative to `src/`. Line numbers are from commit `7f618df`. Grunt data is quoted from the installed game's `entities/enemy/servant_grunt/servant_grunt.ent` and player data from `config/game.cfg`. Neither file is in this repository.

## Summary

- **Embodied body: reshape the local player. Don't possess a real enemy.** Give the local `cLuxPlayer` a new player state and move state with the Grunt's capsule, speeds and restrictions. The camera, mouse look, input, map-change handling and Pose reporting then keep working. Possessing a runtime-spawned `iLuxEnemy` would give a ready-made Enemy Pose and attack state machine, but it means parking the player's own body, rerouting the camera, writing a new non-AI drive mode into `iLuxEnemy::OnUpdate`, and keeping a runtime enemy out of saves. See [Options for the body](#options-for-the-embodied-body).
- **A fly camera exists but is not Drifting yet.** It is the debug window's "Fly camera" (`cLuxPlayer::SetFreeCamActive`, `LuxPlayer.cpp:1093`). It only detaches the camera. The body stays in the world, still collides, and still sets off areas. The camera cannot move vertically, because the player camera is in walk move mode. Its speed is per tick, not per second. Its yaw goes through the pitch limiter. And a map change re-attaches the camera to the new body while the flag stays on. Drifting can be built on it, with those fixes and a body switch-off.
- **Turning the body off does not stop triggers.** `iCharacterBody::SetActive(false)` stops the body's update and its collision, so nothing hits it. But `CollidesWithPlayer` and the player's collide-callback check test shapes without looking at `IsActive`. Areas, collide callbacks, Flashback, insanity and slime areas need their own gate, and so do look-at callbacks.
- **Stamina is new.** The player's run has no stamina. The existing hook is the run-speed multiplier in `cLuxMoveState_Normal::UpdateSpeedMultipliers`.
- **Attack: emit what a Grunt emits; do no local damage.** The existing `enemyattack` carries entity, kind, box size and box offset (`LuxEnemy.cpp:3201-3213`). The receiving copy places the box from its own body (`GetDamageShapeMatrix`, `LuxEnemy.cpp:2219`). So the Monster Player's game only needs to report the Grunt's box from `servant_grunt.ent` at the swing's special-event time.
- **Map-placed enemies already don't collide with the Monster Player.** In a Joining Player's game every Shared Enemy is a driven copy with collision testing off, and inactive bodies are skipped by every collision query. Enemies that are not Shared Enemies still run vanilla AI there.

## Grunt numbers

From `servant_grunt.ent` (Biped, normal difficulty; hard mode multiplies walk by 1.1 and run by 1.2, `LuxEnemy.cpp:302-310`):

| Variable | Grunt | Player (`game.cfg`) |
|---|---|---|
| Body size (x y z) | `Body_Size` 1.2 1.85 1.2 | `Player_Body/Size` 0.8 1.8 0.8, crouch 0.8 1.2 0.8 |
| Mass | 50 | 70 |
| Max step | 0.5, climb speed 6.0 | 0.4, climb speed 5.0 |
| Push | `MaxPushMass` 40, `PushForce` 200 | 10, 55 |
| Walk forward / backward | 1.3 / 0.4 m/s (acc 2.0, deacc 18.0) | 2.5 / 2.0, sideways 2.25 |
| Run forward / backward | 5.1 / 2.0 m/s (acc 4.0, deacc 8.0) | ×1.7 → 4.25 / 3.4, sideways 3.8 |
| Sideways | none (enemies never strafe) | see above |
| Turning | `TurnSpeedMul` 6, `TurnMaxSpeed` 12 rad/s, slows by `TurnBreakMul` 2 per radian above `TurnMinBreakAngle` 10° | instantaneous mouse look, pitch ±70° (`LuxPlayer.cpp:82`) |
| Clip thresholds | stop→walk 0.15, walk→stop 0.1, walk→run 3.2, run→walk 2.8, `MoveSpeedAnimMul` 1.2 | — |
| Normal attack | `NormalAttackDistance` 1.9, `NormalDamageSize` 1 1.6 1.75, `NormalDamageOffset` 0 0 1.2, damage 30–60, strength 1 | — |
| Door attack | damage 26, strength 6, same box | — |

How the Grunt numbers are used in the code:

- Enemy speeds are loaded per pose and per walk/run (`LuxEnemy.cpp:218-226`) and copied into the active speed by `iLuxEnemy::SetMoveSpeed` (`LuxEnemy.cpp:2020`). They are applied forward-only to the body in `UpdateCharBody` (`LuxEnemy.cpp:1414-1421`).
- Turn speed is `min(TurnSpeedMul × |angle left|, TurnMaxSpeed)` in rad/s (`LuxEnemyMover.cpp:220-223`). While turning sharply, forward speed is multiplied by `1 − TurnBreakMul × angle` (`LuxEnemyMover.cpp:124-133`).
- 12 rad/s (about 690°/s) rarely binds a mouse. The part of the Grunt's turning that players would notice is the slowdown on sharp turns.

Animations (`servant_grunt.ent` `<Animations>`): `Idle`, `Walk` (Step events at 0.5 s and 1.5 s), `Run`, `SwingClaws01`/`SwingClaws02` (special event at 0.5 s, speed 1.1), `SwingLaunch` (0.45 s), `BreakDoor` (0.5 s), `Flinch`, `IdleExtra1–3`, `Notice1–2`. `SpecialEventTime` is in seconds of clip time: it is compared with `cAnimationState`'s time position (`HPL2/core/sources/scene/AnimationState.cpp:264`). There is no strafe or turn-in-place clip.

Head bones: `servant_grunt_A_CenterNeck`, `servant_grunt_A_CenterHead` (`servant_grunt.ent` `<Bones>`).

## Options for the Embodied body

### A. Reshape the local player (recommended)

The player body is created on each map from `mvBodySize` (`cLuxPlayer::CreateWorldEntities` → `CreateCharacterBody`, `LuxPlayer.cpp:521-526, 1450-1481`). Crouch is an extra capsule switched in with `SetActiveSize` (`LuxPlayer.cpp:1473-1474`, `LuxMoveState_Normal.cpp:187-192`). A Grunt capsule fits the same seam:

- **Size:** on each map entry, `AddExtraSize(1.2 1.85 1.2)` and `SetActiveSize(thatIndex)` (`HPL2/core/sources/physics/CharacterBody.cpp:497-541`).
  - `SetActiveSize` keeps the feet where they are.
  - Do not change `mvBodySize` itself. `cLuxAvatarHandler` sizes every Avatar from `GetBodySize()` (`LuxAvatarHandler.cpp:359, 589`), so the Session Host's Avatar would grow too.
- **Mass and push:** the player's push limits come from config (`LuxPlayer.cpp:1459-1464`). Set `MaxPushMass` and `CharacterMaxPushMass` to 0 while Embodied, or the Grunt shoves props in the Monster Player's game only.
  - Contacts by the local player are reported to Peers (`cLocalInteractionContact::mbByLocalPlayer`, `LocalInteractionReportModel.h:50-51`; props listen in `LuxProp.cpp:379`).
  - Those shoves could therefore become Peer-Driven Entity motion in the Session Host's game. That contradicts "Embodied: attack and break doors only".
- **Speeds:** a move state writes forward, backward and sideways caps into the body every update (`iLuxMoveState::Update`, `LuxMoveState.cpp:66-85`). A new `eLuxMoveState_Monster`, or a parameterised copy of `cLuxMoveState_Normal`, would set:
  - walk 1.3 forward and 0.4 backward;
  - run 5.1 and 2.0, with the Grunt's acc/deacc;
  - sideways at 0, or a small value (see [Risks](#risks-and-unknowns)).
- **Turning:** the body's yaw follows the camera's yaw (`cLuxPlayer::AddYaw`, `LuxPlayer.cpp:674-693`). To feel like a Grunt, the move state can apply the Grunt's turn slowdown (`LuxEnemyMover.cpp:124-133`) to forward speed, using yaw change per second. Capping the yaw rate itself is possible but feels bad with a mouse.
- **Camera height:** the body puts the camera at feet + the height of shape 0 + `CameraPosAdd` (`CharacterBody.cpp:2009-2016`; `Player_Body/CameraPosAdd` 0 −0.05 0). That is 1.75 m whatever capsule is active.
  - Pick a Grunt eye offset with a head-position add (`cLuxPlayer::SetHeadPosAdd`, `LuxPlayer.cpp:858`).
  - The head bone's bind height was not measured here. Read `GetBoneStateFromName("servant_grunt_A_CenterHead")` once in a prototype and pick a fixed offset from it.
  - Riding the animated head bone each frame would carry the run cycle's head motion into the view. The move state's own head bob (`Player_Movement_Normal/*BobMax`) is the gentler knob.
- **Pose out:** the Peer already polls the local player's Pose from the body (`LuxSocketServer.cpp:185-197`). An Enemy-Pose-shaped report (feet, yaw, clip, clip time, loop) needs a clip choice.
  - With no local Grunt mesh, the choice comes from speed. Use a small pure model in the style of `cAvatarClipModel` (`AvatarClipModel.h`) with the Grunt's thresholds from `cLuxEnemyMover::UpdateMoveAnimation` (`LuxEnemyMover.cpp:227-340`).
  - Alternatively, the Monster Player's game can keep a local Grunt mesh entity, built the way the Avatar module builds one (mesh and `<Animations>` only, no entity loader; `LuxAvatarHandler.cpp:340-357`). Its real clip times are then read like `iLuxEnemy::GetEnemyPose` (`LuxEnemy.cpp:3125-3141`). That mesh can also stay hidden, or be shown for its shadow.
- **Mesh and arms:** there is no local mesh by default, so there is nothing to hide.
  - Showing only arms needs an asset. `servant_grunt.msh` has two submeshes, body and hair (`servant_grunt.ent` `<Mesh>`), so the arms cannot be shown alone.
  - A claws hand object for `cLuxPlayerHands` (`LuxPlayer.cpp:160`, the lantern's `.ho` route) is the existing first-person seam.
- **Cost:** a new player state, a move state, a clip model and the trigger gate below. Everything else (camera, mouse look, map change, `PlaceAt`, teleport counter, Pose reporting) is reused.

### B. Drive a real Grunt entity

- **Spawning:** a Grunt can be created at runtime with `cLuxMap::CreateEntity` (`LuxMap.cpp:571-584`).
  - Its ID comes from `GetFreeEntityID`, so it will not match the ID of the Session Host's Grunt for the same Monster Player.
  - `AfterWorldLoad` is not run for it (`LuxMap.cpp:326-334`), so its pathfinder has no node container (`LuxEnemyPathfinder.cpp:68-80`). `MoveTo` tolerates that (`LuxEnemyPathfinder.cpp:104-107`).
- **Gains:** `GetEnemyPose` gives a ready-made Enemy Pose with real clip times. The Grunt's attack states (`LuxEnemy_Grunt.cpp:777-830`) already time `Attack()` to the special event.
- **Costs:**
  - **A new drive mode.** `iLuxEnemy::OnUpdate` runs AI or, when driven, `UpdateEnemyCopy` (`LuxEnemy.cpp:640`). Driving from input is a third mode. The mover only knows `MoveToPos` and `TurnToAngle`, and the body only moves forward.
  - **The player's own body.** It must be parked and switched off (the same work as Drifting). The camera must be moved from the player's body to the enemy's (`iCharacterBody::SetCamera`). `GetPlayerPose`, the sound listener, the 63 call sites that read the player camera, and the script functions that read the player position all keep pointing at the parked player.
  - **Local damage.** `Attack()` runs `ShapeDamage` on props and the player locally (`LuxEnemy.cpp:1993-2017`), which would break and push props in the Monster Player's game only.
  - **Saves.** The runtime enemy and its disabled or driven flag are saved (avatars.md, "Enemy possession").
  - **Sanity.** The player's sanity looks at the enemy every frame (`cLuxPlayerSanity::UpdateCheckEnemySeen`, `LuxPlayerHelpers.cpp:1340-1410`).
- **Verdict:** more code, and more coupling to the AI, for a Pose and an attack timer that A gets cheaply.

## What to turn off while Embodied

| Thing | Where it goes through | How to turn it off |
|---|---|---|
| Lantern | `cLuxPlayer::DoAction` asks the state (`LuxPlayer.cpp:736-745`); scripts reach `cLuxPlayerLantern::SetActive`, which checks `AllowLantern()` (`LuxPlayerHelpers.cpp:1772-1781`) | Monster state returns false from `OnDoAction(Lantern)` and from `AllowLantern()`. Turn the lantern off on entering the state. |
| Crouch, jump | `cLuxPlayer::Crouch`/`Jump` ask the state's `OnCrouch`/`OnJump` (`LuxPlayer.cpp:760-780`) | Return false. Also `SetCrouchDisabled`/`SetJumpDisabled` exist. |
| Lean | Input calls `cLuxPlayer::SetLean` straight into `cLuxPlayerLean` with no state check (`LuxInputHandler.cpp:1057-1063, 1143, 1218-1225`; `LuxPlayer.cpp:716-724`) | Needs a gate in `SetLean`/`AddLean` or in the input handler. |
| Interaction (Holds, pickups, doors, ladders, signs, examine) | Focus and interact live in `cLuxPlayerState_DefaultBase::OnDoAction` (`LuxPlayerState_DefaultBase.cpp:147`) | Make the Monster state derive from `iLuxPlayerState`, not `DefaultBase`, so no focus or interact runs. Its `OnDoAction(Attack)` starts a swing. |
| Inventory, journal, quest log, recent text | `UpdateGamePlayerInput` checks `AllowPlayerMenus()` (`LuxInputHandler.cpp:1002-1026`) | Return false. |
| Quick save/load | `UpdateGameInput` (`LuxInputHandler.cpp:938-948`) | Gate in Monster Mode; a Monster Player's save would capture a reshaped body. |
| Health damage | `cLuxPlayer::GiveDamage`/`SetHealth` (`LuxPlayer.cpp:587-606, 877-886`); copies' attacks land there through `ResolveEnemyAttack`, which tests only the local player (`LuxEnemy.cpp:3216-3239`); slime areas (`LuxArea_SlimeDamage.cpp:121-126`); falls; `AutoKillYPos` (`LuxPlayer.cpp:380-381`) | There is no god mode in the game. Add an invulnerability flag checked in `GiveDamage`/`SetHealth`. Skip `ResolveEnemyAttack` in the Monster Player's game if the hit sound and effect should not play either. |
| Sanity | Darkness drain (`cLuxPlayerInDarkness::Update`, `LuxPlayerHelpers.cpp:2892-2972`), seen enemies (`LuxPlayerHelpers.cpp:1340-1410`), `GiveSanityDamage`/`LowerSanity` (`LuxPlayer.cpp:610-640`), insanity collapse | Gate `LowerSanity`/`SetSanity` on the same flag. Hide the sanity and hurt HUD effects. |
| Map Script triggers | See [Triggers](#triggers-that-reach-map-scripts) | A single "this player triggers nothing" flag. |
| Prop pushing | Body push limits (`LuxPlayer.cpp:1459-1464`) | 0 while Embodied (see A). |

### Triggers that reach Map Scripts

None of these checks look at whether the player's body is active, so turning the body off does not stop them:

- **Player collide callbacks** (areas and entities registered with `AddEntityCollideCallback("Player", …)`).
  - `cLuxPlayer::Update` calls `CheckCollisionCallback("Player", map)` (`LuxPlayer.cpp:385`). That runs `CheckEntityCollision`, a bounding-volume and shape test against the player's current body (`LuxTypes.cpp:156-202, 271-300`; `LuxPlayer.h:179-180`).
  - The callback runs locally and is published as a `playercollide` Shared Callback (`LuxSharedCallbackHandler.cpp:346`).
  - Gate: skip that call while the player is the Monster Player.
- **Shape tests against the player body:** `iLuxEntity::CollidesWithPlayer` (`LuxEntity.cpp:395-416`) is used by Flashback areas (`LuxArea_Flashback.cpp:109-122`, a `flashback` Shared Callback), insanity areas (`LuxArea_Insanity.cpp:104`) and slime areas (`LuxArea_SlimeDamage.cpp:121`). Gate: return false.
- **Player look-at callbacks** (`SetEntityPlayerLookAtCallback`) test the player camera's frustum and centre line every 0.3 s (`LuxEntity.cpp:102, 220-280`). They are not Shared Callbacks, so they would run Map Script code in the Monster Player's game only. Gate them too.
- **Sticky areas** attach only bodies the player Holds (`LuxArea_Sticky.cpp:172`). No Holds means no sticky attach.
- **Liquid areas** skip inactive bodies (`LuxArea_Liquid.cpp:148`). An Embodied Grunt still floats and splashes, which is harmless.
- **Ladders, signs, examine areas, pickups** are reached only through interaction, which the Monster state removes.
- **Footsteps** broadcast an enemy sound message (`LuxMoveState_Normal.cpp:858-862`). In the Monster Player's game every Shared Enemy is a driven copy, and a copy ignores messages, so this is harmless there. The Monster state can play Grunt step sounds instead of player steps.

## Stamina

There is no stamina in the game. Running is a held flag (`cLuxPlayer::Run`, `LuxPlayer.cpp:751-757`; `cLuxMoveState_Normal::UpdateMovement`, `LuxMoveState_Normal.cpp:461-489`). It scales the caps by `1 + (RunMul − 1) × GetRunSpeedMul()` (`LuxMoveState_Normal.cpp:518-528`). `GetRunSpeedMul` is the product of the script and event run multipliers (`LuxMoveState_Normal.cpp:447-450`).

A Monster move state can own a small pure `StaminaModel`: drain while running and moving on the ground, refill after a delay once the player stops running. It feeds the run multiplier and drops to walk at empty. It should be its own multiplier rather than `SetPlayerRunSpeedMul`, which map scripts own. The HUD for it is an open map item. The nearest drawing seam is `cLuxPlayer::DrawHud` (`LuxPlayer.cpp:1145`).

## Drifting

### What exists

`cLuxPlayer::SetFreeCamActive` (`LuxPlayer.cpp:1093-1105`) is reachable only from the debug window's "Fly camera" checkbox (`LuxDebugHandler.cpp:1162-1174, 1578-1579`), which needs `LoadDebugMenu`. It detaches the camera from the body (`SetCamera(NULL)`). `Move`, `AddYaw` and `AddPitch` then move the camera instead of the body (`LuxPlayer.cpp:646-713`), and `UpdateCamera` skips FOV and roll effects (`LuxPlayer.cpp:1259`).

Gaps before it can be Drifting:

- **No vertical flight.** `cCamera::MoveForward` uses only yaw unless the camera's move mode is `Fly` (`HPL2/core/sources/scene/Camera.cpp:161-171, 559-570`). The player camera is created in `eCameraMoveMode_Walk` (`LuxPlayer.cpp:68`). Fix: switch it with `cCamera::SetMoveMode`, or add up/down on jump/crouch with `MoveUp`.
- **Speed is per call, not per second.** `Move` moves the camera `mfFreeCameraSpeed × afMul` each input update. The default is 0.1 (`LuxPlayer.cpp:71`). Fix: scale by the time step.
- **Yaw goes through the pitch limiter.** `AddYaw` in free cam calls `AddAndGetPitchAdd` (`LuxPlayer.cpp:676-680`), apparently a copy-paste slip. Fix: use the yaw variant.
- **Map change breaks it.** `CreateCharacterBody` re-attaches the camera (`LuxPlayer.cpp:1469`), but `mbFreeCameraActive` is cleared only in `Reset` (`LuxPlayer.cpp:336`). After a map load the body and the free cam would both move the camera. Fix: re-apply Drifting on map entry, or detach in `CreateCharacterBody` while Drifting.
- **The body stays in the world.** It keeps gravity and collision, and still trigger areas (see above). The Peer's `GetPlayerPose` still reports it (`LuxSocketServer.cpp:185-197`).

### Switching the body off and back

- **Off:** `iCharacterBody::SetActive(false)` (`CharacterBody.cpp:444-458`).
  - It deactivates the physics body and makes `iCharacterBody::Update` return at once (`CharacterBody.cpp:952`), so there is no gravity and no movement.
  - Every collision query skips inactive bodies: the shape query `PhysicsWorld.cpp:464`, other character bodies `CharacterBody.cpp:1948`, Newton's contact filter `PhysicsWorldNewton.cpp:472, 491`, and liquids `LuxArea_Liquid.cpp:148`.
  - So nothing collides with a Drifting player. Triggers still need the gate above.
- **Back on:** `SetActive(true)` re-activates the physics body only if collision testing is on (`CharacterBody.cpp:454-457`). No state is lost.
- **What else to do on the switch:**
  - Hide the HUD crosshair.
  - Stop the move state's head bob (`OnLeaveState` resets it, `LuxMoveState_Normal.cpp:229-233`).
  - Report the camera, not the body, if the Peer needs the Drifting position.
  - Keep `mbActive` on the player (`LuxPlayer.cpp:870`) so input still flows. `UpdateGamePlayerInput` returns early when the player is inactive (`LuxInputHandler.cpp:998`), so don't use `SetPlayerActive(false)`.

### Becoming Embodied where the camera is

- **The move:** `cLuxPlayer::PlaceAt(feet, yaw)` (`LuxPlayer.cpp:840-857`) is the existing move. It resets the state, sets feet and yaw, steps the body, and counts a teleport so Pose consumers snap instead of interpolating.
  - Feet = camera position − eye height.
  - Then re-attach the camera and re-activate the body.
- **Fit test:** `iCharacterBody::CheckCharacterFits(pos, true, gruntSizeIdx, &pushBack)` (`CharacterBody.cpp:1076-1098`). It tests a capsule at a position and returns the push-out vector.
- **Finding the nearest valid spot when the camera is inside geometry:**
  1. Cast a ray down from the camera to find a floor. Test the fit there, then retry once along the push-back vector.
  2. Otherwise search outward in rings for a fitting spot that has a floor under it and a free line from the camera (so the player never comes out on the other side of a wall).
  3. As a stronger fallback, use the map's AI nodes. `cWorld::CreateAINodeContainer("servant_grunt", "Default", GruntSize, …)` builds or loads the Grunt-sized node graph from the map's `PathNode`s (`HPL2/core/sources/scene/World.cpp:1101-1151`; the enemy does the same in `LuxEnemyPathfinder.cpp:68-73`). `cAINodeContainer::GetNodeIterator(pos, radius)` and `FreePath` find the nearest reachable node (`HPL2/core/include/ai/AINodeContainer.h`). Maps without `PathNode`s have no nodes, so keep the ray search as the floor.
  - Refuse rather than guess when nothing fits within a few metres; the Monster Player stays Drifting.
- **Session Host's sight:** "Not in the Session Host's sight" and the cooldown are checked by the Session Host's game (#125 / Peer). The Monster Player's game only proposes a spot.

## Attack

- **Input:** `eLuxAction_Attack` (right mouse) already reaches `cLuxPlayer::DoAction(eLuxPlayerAction_Attack)` through the state (`LuxInputHandler.cpp:1066-1067`).
- **On press:**
  - Start a swing: `SwingClaws01` or `SwingClaws02` standing, `SwingLaunch` while running, like the Grunt's short and long attacks (`LuxEnemy_Grunt.cpp:777-830`).
  - Put the clip in the reported Pose.
  - At the clip's special-event time (0.5 s of clip time at speed 1.1, so about 0.45 s; 0.45 s for the launch), publish the attack.
  - Block another swing until the clip ends.
  - Slow or stop movement during the swing. `SwingLaunch` lunges at 1.5× run speed (`LuxEnemy_Grunt.cpp:806-807`).
- **Wire:** `enemyattack` carries entity ID, kind (`normal` or `breakdoor`), box size, box offset and map (`iLuxEnemy::GetEnemyAttack`, `LuxEnemy.cpp:3201-3213`).
  - The receiving game finds the copy's attack shape by box size and places it from the copy's own body centre and yaw (`ResolveEnemyAttack`, `LuxEnemy.cpp:3216-3239`; `GetDamageShapeMatrix`, `LuxEnemy.cpp:2219-2232`).
  - The Monster Player's game therefore sends size 1 1.6 1.75 and offset 0 0 1.2 from the `.ent` (or from `LoadAttackSizeData`, `LuxEnemy.cpp:422-427`). The Session Host's Grunt decides the hit from where it shows the Grunt (ADR 0009's victim rule).
  - The entity ID has to name the Session Host's Grunt for this Monster Player. That ID is the Peer's or #125's business, because a runtime entity's ID differs per game.
- **No local damage.** Unlike `iLuxEnemy::Attack` (`LuxEnemy.cpp:1993-2017`), the Monster Player's game should not run `ShapeDamage`. The Session Host is only an Avatar there, and props hit locally would diverge.
  - Door breaks are an open map item. Either the Session Host's copy resolves a `breakdoor` attack against its own doors, or the Monster Player's game decides and sends the existing door-break event from the other direction.
  - Both need the door's ID, and the Monster Player's game cannot see a door the Session Host has already broken any differently than its own copy shows it.
- **Local feedback:** the swing sound `grunt/attack_claw` (an event in the clip), a short camera nudge, and the claws hand object if one is made.

## Map-placed enemies in the Monster Player's game

- **Shared Enemies:** in a Joining Player's game every Shared Enemy (Grunt, Brute, Kaernk; `iLuxEnemy::IsSharedEnemyType`, `LuxEnemy.cpp:3114-3117`) is a driven copy. `SetEnemyDriven(true)` turns off its gravity and collision testing (`LuxEnemy.cpp:3069-3070`). Collision testing off deactivates its physics body (`CharacterBody.cpp:471-485`), and inactive bodies are skipped by every query listed under Drifting.
  - So the copies already neither block nor push the Monster Player, Embodied or Drifting.
  - They also have no senses or AI (`LuxEnemy.cpp:640`), so they never react to the Monster Player.
  - Nothing new is needed beyond invulnerability: a copy's reported attack still tests the local player (`LuxEnemy.cpp:3233-3237`).
- **Other enemies:** enemy types that are not Shared Enemies (the ManPig, `LuxEnemy_ManPig.cpp`) run vanilla AI in each game and will hunt and collide with the Monster Player. Monster Mode should disable them in the Monster Player's game, or accept it for the stories that have them.
- **Session Host's side:** a Session Host's enemy seeing the Monster Player's Grunt (ADR 0009 targets Avatars too) is #125's question.

## Risks and unknowns

- **Map Scripts drive the local player in the Monster Player's game.** Each game runs its own map's `OnStart`/`OnEnter`, and Shared Callback replays run the Session Host's triggers there too. 66 script-handler sites touch `gpBase->mpPlayer` (`LuxScriptHandler.cpp`): `SetPlayerActive`, `TeleportPlayer`, `ChangePlayerStateToNormal` (which would knock the Monster state out), `StartPlayerLookAt`, `SetPlayerCrouching`, `AddPlayerBodyForce`, fades and more (`LuxScriptHandler.cpp:1361-1680`). `PlaceAt` itself forces `eLuxPlayerState_Normal` (`LuxPlayer.cpp:842`). Monster Mode needs a rule: either ignore these for the Monster Player, or re-assert the Monster state after them. This is the biggest unknown.
- **Strafing has no Grunt clip.** A sideways-moving Grunt will slide in the Session Host's view. Choose between no strafe, slow strafe playing `Walk`, or A/D turning the body.
- **Dark maps.** Without a lantern the Monster Player sees only the player's darkness ambient light (`Player_Darkness/AmbientLight*`). Grunts "see" 5 m in darkness (`DarknessSightRange`). A monster vision light may be needed. This is a design question.
- **Eye height and the head bone.** Not measured. Measure in the first prototype.
- **Whether the Monster Player's game should keep a hidden local Grunt mesh.** It gives exact clip times and a later arms or shadow view, at the cost of a second animated mesh. The speed-only clip model is cheaper but has to guess clip times for attacks.
- **Free-cam bugs.** The yaw limiter and the map-change re-attach are latent in the debug feature today. Fix them as part of Drifting, not separately.
- **Saving.** A save taken in Monster Mode would contain a reshaped player. Block saving for the Monster Player.
- **Hard mode** changes Grunt speeds (×1.1 walk, ×1.2 run) and damage (×2, `LuxEnemy.cpp:411-417`). Decide whether the Monster Player's Grunt follows the Session Host's difficulty.
