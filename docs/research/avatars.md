# Avatar research for later spikes

Findings from the Avatar spec (#28) that later spikes build on: head pitch, crouch, animations, sounds, and enemy possession. Each is out of scope for #28 and is recorded here so the next spike starts from the engine facts rather than rediscovering them. Terms follow `CONTEXT.md`, and the Avatar module's shape follows ADR 0002 and ADR 0003.

Paths are relative to `src/`. Line numbers are those at commit `99ea2af`.

## Where Avatars stand today

- `amnesia/src/game/LuxAvatarHandler.cpp` creates each Avatar from a bare `cMesh` (`CreateMesh` + `cWorld::CreateMeshEntity`) and a character body with gravity off, a mass of 1e6, and its own collide flag. Only the `.ent`'s `ModelData/Mesh` filename is read (`FindMeshFile`). The entity loader is never run, so the `.ent`'s bodies, joints, `Animations`, and prop variables are all ignored. This falls short of ADR 0002, which says an Avatar is built from its model's "mesh and animations only": animations are not loaded yet (see [Animations](#animations)).
- Every update the handler zeroes the body's force and velocity and calls `SetFeetPosition` and `SetYaw` with the interpolated Pose. The body carries the mesh through `SetEntity` and the offset from `AvatarMeshOffset.h`.
- `cLuxAvatarHandler::PoseAvatar` passes only time, teleport counter, feet position, yaw, raised lantern, and map to `cAvatarPoseSample` (`AvatarPoseModel.h`). **Pitch and crouch are parsed by the gateway (`cGameInteractionPose` in `GameInteractionGateway.h`) but dropped here.** A spike that renders either must first add them to `cAvatarPoseSample`/`cAvatarRenderedPose` and decide how to interpolate them. Pitch can be interpolated linearly. Crouch is a flag, so take it from the older sample, as `cAvatarPoseModel::Sample` already does for the lantern flag. Every Pose flag follows this rule: the older sample is the latest one at or before render time, so the flag switches at the sender's moment with the same render delay as the rest of the Pose, and it holds correctly through snaps and hold-last-Pose.
- While the rendered Pose has the lantern raised, the handler fades in a shadowless point light that copies the local lantern's gobo, colour, and radius (`cAvatarLanternModel` in `AvatarLanternModel.h` picks the brightness). It is placed at the standing camera height and turned by yaw only, so it ignores crouch and pitch.

## Head pitch via bones

### How pre/post transforms layer over animation

- Every bone of a skinned `cMeshEntity` is a `cBoneState`, which is a `cNode3D` (`HPL2/core/include/graphics/BoneState.h:29`). The entity creates one per skeleton bone, with the same index as the bone, and a name-to-index map (`HPL2/core/sources/scene/MeshEntity.cpp:159-169`).
- `cNode3D` has an optional **pre** and **post** transform (`HPL2/core/include/scene/Node3D.h:89-92`, applied in `HPL2/core/sources/scene/Node3D.cpp:355-372`):
  - pre: `local = local * pre`. This rotates in the bone's own frame, before its local transform.
  - post: `local = post * local`. This rotates in the parent bone's frame.
- `cMeshEntity::UpdateLogic` (`MeshEntity.cpp:363`) does the following for skinned meshes each frame:
  1. While an animation is active (or skeleton physics runs), it resets every active bone to its bind-pose local transform (`:426-448`).
  2. It applies every active animation track (`:484-526`).
  3. If an animation is active, it walks the bone tree with `UpdateNodeMatrixRec` (`:337`), which applies the animation, then the pre transform, then the post transform to each bone (`:344-345`).
  
  Pre/post transforms therefore stack on top of whatever the animation produced and are rebuilt from the bind pose every frame, so they do not accumulate.
- **Pitfall:** step 3 runs only when `bAnimationActive` (`:530-540`). **On a mesh with no active animation, pre/post transforms are never applied.** Head pitch therefore needs at least a looping idle animation playing (see [Animations](#animations)).
- **Pitfall:** the getters are wrong. `GetUsePreAnimTransform` returns the post flag and `GetPreAnimTransform` returns the post matrix (`Node3D.h:84-87`). Keep the pitch value on the Avatar and use only the setters.

### What `cLuxProp_NPC` does (the model to copy)

`amnesia/src/game/LuxProp_NPC.cpp` turns an NPC's head towards the player:

- At load (`:54-104`) it reads the `MoveHeadBones` and `MoveHeadBoneMuls` prop variables, which are parallel lists of bone names and weights. It looks each bone up with `cSkeleton::GetBoneIndexByName` (`:80`).
- For each bone it precomputes the rotation axis as the **model up axis expressed in the bone's bind-pose frame** (`cBone::GetWorldTransform` is in skeleton space): `MatrixInverse(bone->GetWorldTransform()) * (0,1,0)`, normalized (`:93-95`).
- Every update (`UpdateHeadMovement`, `:287`) it eases a single head angle towards its target. It then sets each bone's `SetUsePreTransform(true)` and `SetPreTransform(MatrixQuaternion(cQuaternion(angle * mul, axis)))` (`:330-338`).
- The weights spread one angle over several bones (for example neck 0.4 and head 0.6), so the turn looks natural.

For Avatar **pitch**, use the same recipe with a different axis. Use the model's **sideways (right) axis** in each bone's bind-pose frame instead of up, and use the Pose's camera pitch instead of the angle to the player. The Avatar's yaw rotates the whole mesh, so a bind-pose-space axis stays correct at any yaw. Clamp pitch to what the neck can plausibly show. The player's camera goes to roughly ±90°, and the head should not.

### Bone lookup by name

- `cMeshEntity::GetBoneStateFromName` / `GetBoneStateIndex` (`MeshEntity.cpp:864-897`) and `cSkeleton::GetBoneIndexByName` both return -1 (or NULL) for an unknown name. Resolve the bone indices once when the world objects are created, and skip bones that were not found. Do not look them up by string every frame.
- The NPC code checks `mlBoneIdx < -1`, which is always false, so a missing bone is never reported (`LuxProp_NPC.cpp:81`). Check `< 0`.

### What the Avatar model's armature must provide

- A skeleton in the mesh (`cMesh::GetSkeleton()` non-NULL), skinned to the whole visible mesh. Without a skeleton there are no bone states. The mesh is then attached rigidly (`MeshEntity.cpp:231-253`), and neither pitch nor animations are possible.
- Named neck and head bones. Pick fixed names (for example `neck` and `head`), or read them from the `.ent` the way the NPC reads `MoveHeadBones`, so other models can map their own bone names.
- A bind pose that faces the model's forward axis, which `AvatarMeshOffset.h` assumes.
- At least one idle animation, because of the pitfall above.
- No physics bodies in the `.ent` that the game would turn into a ragdoll. The Avatar module ignores them today, but keep the model mesh-only (#35) so it stays safe if something ever loads it through the entity loader.

## Crouch

The `avatarpose` line carries a crouch flag, which `CONTEXT.md` counts as part of the Pose. Rendering it has two independent parts.

### Capsule resizing on the Avatar's character body

- The player creates its crouch shape once with `iCharacterBody::AddExtraSize(Player_Body.CrouchSize)` (`amnesia/src/game/LuxPlayer.cpp:100-101, 1456`). It switches shapes with `SetActiveSize(1)` to crouch and `SetActiveSize(0)` to stand (`amnesia/src/game/LuxMoveState_Normal.cpp:356-410`).
- `AddExtraSize` (`HPL2/core/sources/physics/CharacterBody.cpp:495`) builds an extra cylinder body that uses the character's **current** collide flags. `SetCollideFlags` (`:736`) updates every shape. `SetActiveSize` (`:518`) keeps the feet where they are, activates the new shape only if the character is active and testing collision, and updates `GetSize()`.
- For an Avatar, call `AddExtraSize(gpBase->mpPlayer->GetBodyCrouchSize())` when the world objects are created, and switch sizes when the crouch flag changes. After a switch:
  - Re-set the mesh offset, since `GetAvatarMeshOffset` takes the body height.
  - Let `UpdateCollision` recompute the gap from the new `GetSize()`. It already reads the size every update.
- The player stands up only if `CheckCharacterFits` succeeds. An Avatar has no such check: it follows its Peer, and the player-overlap guard in `cAvatarCollisionModel` already covers a standing-up Avatar that suddenly overlaps the local player.

### Crouch visuals

There are three options, from cheapest to best:

1. **Only lower the mesh.** Keep the standing mesh and move it down by the height difference. This looks wrong (it sinks into the floor) and is only a stopgap.
2. **Crouch pose through bones.** Blend a post/pre transform on the spine and hip bones, as for head pitch. This needs an active animation and a lot of hand-tuning.
3. **Crouch animations.** Add `crouch_idle` and `crouch_walk` clips and pick them from the crouch flag plus speed (see below). This is what enemies do with their pose types (`eLuxEnemyPoseType`, used in `LuxEnemyMover.cpp:236`). **This is the recommended option.**

## Animations

### Loading animations for an Avatar

- The entity loader loads an `.ent`'s `<Animations>` into its mesh entity (`HPL2/core/sources/resources/EntityLoader_Object.cpp:435-470`). For each entry it creates the animation with `GetAnimationManager()->CreateAnimation(file)` (a bare filename is resolved next to the `.ent`) and adds it with `cMeshEntity::AddAnimation(anim, name, speed)`.
- The Avatar module bypasses the loader, so an Avatar has only the animations embedded in its mesh file (`MeshEntity.cpp:126-137`). Later spikes should read `ModelData/Animations` in `FindMeshFile`'s style and add the clips the same way. `AddAnimation` passes the animation manager, so the clips are released with the mesh entity.
- The Avatar's mesh entity is dynamic and active, so `cWorld::UpdateEntities` advances its animations with the world (`HPL2/core/sources/scene/World.cpp:426-437, 1353-1368`). No extra update call is needed.

### Driving walk/idle/crouch from interpolated speed

- Enemies choose clips in `cLuxEnemyMover::UpdateMoveAnimation` (`amnesia/src/game/LuxEnemyMover.cpp:227-340`). It is a small state machine (backward, stopped, walking, running) with hysteresis thresholds (`mfStoppedToWalkSpeed`, `mfWalkToRunSpeed`, `mfRunToWalkSpeed`, `mfWalkToStoppedSpeed`). It cross-fades with `iLuxEnemy::PlayAnim` (`LuxEnemy.cpp:887`, `FadeOut`/`FadeIn`, and syncing walk↔run time positions). It scales playback speed by `|speed| * mfMoveSpeedAnimMul` (`:327-339`).
- **Pitfall:** enemies measure speed with `iCharacterBody::GetVelocity` (`LuxEnemyMover.cpp:142-147`), which is `(mvPosition - mvLastPosition) / dt` (`CharacterBody.cpp:593`). `SetPosition`/`SetFeetPosition` sets `mvLastPosition` to the new position (`:602-606`), so **an Avatar's body always reports zero velocity**. Take speed from the Pose model instead: sample the rendered Pose at two nearby local times, or have `cAvatarPoseModel::Sample` also return horizontal speed from the two samples it interpolates between. This keeps the choice pure and testable (ADR 0003), and makes it zero while the buffer is dry and at a snap.
- Signed speed (forward or backward) is the dot product of the horizontal velocity with the yaw's forward vector. It picks a backward clip the way enemies do.
- Put the clip choice in a pure model beside `AvatarPoseModel` (speed and crouch in, clip name, fade, and playback speed out). Keep the engine calls (`GetAnimationStateFromName`, `SetActive`, `FadeIn`/`FadeOut`, `SetSpeed`) in `LuxAvatarHandler`.

### The ragdoll pitfall

At the end of loading, the entity loader plays animation 0 if the entity has any. Otherwise, if the mesh has a skeleton and the `.ent` has bodies, it calls `SetSkeletonPhysicsActive(true)` (`EntityLoader_Object.cpp:816-824`). **A skeletal `.ent` with bodies but no animations therefore turns into a ragdoll**, and its bones follow physics bodies (`MeshEntity.cpp:452-476`). This happened to the prop that the pre-Avatar prototype spawned through `exec:` once it gained an armature (see "Problem" in #28). The Avatar module avoids it because it never runs the loader and never creates the `.ent`'s bodies. Do not route Avatars through `CreateEntity`/the loader. If a future model needs skeleton colliders, add them without enabling skeleton physics.

## Sounds

### How enemies hear sounds

There are two paths, and both end in `cLuxMap::BroadcastEnemySoundMessage` (`amnesia/src/game/LuxMap.cpp:713`). It sends a sound message to every active enemy whose body is within the sound's max distance, and it skips sounds closer than 2 m.

1. **World sound entities, filtered by name.** `cMapHandlerSoundCallback` (`amnesia/src/game/LuxMapHandler.cpp:44-101`) is a global `cSoundEntity` callback. It loads `sounds/EnemySounds.dat` and broadcasts every started `cSoundEntity` whose sound-data name **starts with** one of the listed names (for example `player_step` and `impact_` in the shipped data, which is not in this repository). The callback sets both the min and the max distance to `GetMaxDistance()` (`:95-96`), so within range the enemy hears the full volume.
2. **Explicit broadcasts.** The player's own footsteps are played as GUI sounds (`step_<walk|run|sneak>_<material>` through `PlayGuiSoundData`), which are not sound entities. They reach enemies through a direct `BroadcastEnemySoundMessage` at the player's feet (`amnesia/src/game/LuxMoveState_Normal.cpp:808-862`).

### Implications for Avatar footsteps

- If an Avatar plays footsteps as world sound entities (3D, positioned at the Avatar) under names that match an `EnemySounds.dat` prefix, **local enemies will hear the Avatar and go to its position**. The same applies to explicit broadcasts. This is a design decision, not a detail. Either:
  - use sound-data names that match no prefix, so Avatars stay inaudible to enemies (consistent with "enemy perception ignores Avatars" in #28), or
  - broadcast deliberately, if a later spike wants each game's enemies to react to every Avatar.
- Enemy state is not synchronized between games, so audible Avatars make each game's enemies behave differently. Keep them inaudible by default.
- Avatar footsteps cannot be derived from the local player's step code (`FootSound`), which reads the local body's ground material and move state. They need the ground material under the Avatar (a downward ray), the move type from interpolated speed and the crouch flag (walk/run/sneak), and a step cadence driven by animation events or distance travelled.
- `cLuxEnemyMover::UpdateStepEffects` (`LuxEnemyMover.cpp:344`) shows how enemies time their step and water-splash effects, which is a useful reference.

## Enemy possession

A future mode where a Peer drives an existing enemy's character body while its AI is suspended. ADR 0002 keeps this possible by driving Avatars through an `iCharacterBody`.

### What `SetDisabled(true)` stops

- `iLuxEnemy::SetDisabled` (`amnesia/src/game/LuxEnemy.cpp:1139`) sets `mbDisabled` and stops the pathfinder. Scripts reach it through `SetEnemyDisabled` (`LuxScriptHandler.cpp:3055`).
- While disabled, `iLuxEnemy::OnUpdate` (`LuxEnemy.cpp:625-700`) returns immediately. This skips the pathfinder, the mover (turning, **move animation**, step effects), all senses, the state machine, enemy-specific updates, `UpdateCharBody` (speed limits), `UpdateAnimation` (special events and animation-over), `UpdateSoundState` (ambient growls), glow, regen, and hallucination.
- `iLuxEnemy::SendMessage` also returns while disabled (`:858`), so a disabled enemy ignores sounds and script messages.

### What keeps running

- The character body is updated by the physics world whatever the enemy's state (`HPL2/core/sources/physics/PhysicsWorld.cpp:89-96`). A disabled enemy therefore still falls under gravity and slides by its own velocity, and it keeps carrying its mesh (`LuxEnemy.cpp:203-206`, with smoothed Y).
- The mesh entity keeps playing whichever clip was active when the enemy was disabled, because the world advances animations independently.

### Feasibility and what would need re-running

Possession is feasible: drive the body exactly as `cLuxAvatarHandler::Update` drives an Avatar's body, with the same Pose model. The following would need attention:

- **Body:** each update, zero force and velocity and set the feet position and yaw from the Pose. Decide whether to turn gravity off, as for Avatars, or keep it and trust the Peer's feet position. The enemy's collide flags already exclude Avatar bodies (`LuxEnemy.cpp:187-188`). Its mass is the enemy's own, so the local player can push it unless it is raised as for Avatars. Its smoothed Y (`SetUseEntitySmoothYPos`) will lag behind snaps.
- **Animation:** the mover's move-animation state machine must be re-run with the interpolated speed, because body velocity is zero after `SetFeetPosition` (see [Animations](#animations)). The cleanest way is to factor the clip choice out of `cLuxEnemyMover::UpdateMoveAnimation` so that it takes a speed, or to call `PlayAnim` from the possession driver with the same thresholds. `UpdateAnimation` must also run if possession should play attack or special clips.
- **Sound state:** re-run `UpdateSoundState` if the possessed enemy should keep its ambient sounds. Step effects (`UpdateStepEffects`) likewise. Whether possessed-enemy sounds should alert *other* enemies is the same decision as for Avatar footsteps.
- **Saving:** `mbDisabled` is serialized (`LuxEnemy.cpp:2315`, `LuxSavedGameTypes.cpp:911`). A save taken during possession would restore a disabled enemy with no Peer driving it. The possession mode must re-enable the enemy on Session end, map leave, and before saving, or it must exclude possession from saves.
- **Scripts:** a map script may call `SetEnemyDisabled(name, false)` during possession and hand control back to the AI. The possession driver must detect this, or it must re-assert its state every update.
- **Damage and triggers:** a disabled enemy does not attack, because attacks come from the state machine. Any attack by a possessed enemy would need its own path.
