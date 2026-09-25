# The game's interaction surface: grabbables and swing/spin entities

Findings for the interaction-ownership design (`amnesia-spelos/amnesia-multiplayer#26`, ticket #44). What the engine and game actually do when a player grabs, rotates, throws, swings, or spins something; what state is authoritative; what can be driven from outside; and what would diverge between two machines. Terms follow `CONTEXT.md`.

Paths are relative to `src/`. Line numbers are those at commit `ecb10df`.

## The shape of the whole surface

Every interaction goes through one funnel, and it is narrow:

- `iLuxPlayerState_DefaultBase::OnDoAction` (`amnesia/src/game/LuxPlayerState_DefaultBase.cpp:157-168`) is the only caller of `iLuxEntity::OnInteract` in the game. On the Interact press, if `CanInteractWithEntity()` (`:236-241`: entity in focus, within `GetMaxFocusDistance`, `CanInteract(body)` true, `GetInteractionDisabled()` false), it calls `mpEntityInFocus->OnInteract(mpBodyInFocus, mvFocusPos)`.
- Each prop's `OnInteract` stashes the focus body and point in three file-static variables and switches the player into an interaction state: `cLuxPlayerStateVars::SetupInteraction(apBody, avPos)` then `gpBase->mpPlayer->ChangeState(...)` (`LuxPlayerState.cpp:51-63`; `LuxPlayerState.h:42`). The variables are `mpInteractBody`, `mpInteractProp`, `mvInteractPos`, and the prop is derived from the body's `GetUserData()`.
- The state reads them in `SetupInteractVars` and marks the prop held: `mpCurrentProp->SetIsInteractedWith(true)` (`LuxPlayerState_Interact.cpp:90-97`), cleared in `ResetInteractVars` (`:101-104`). `mbIsInteractedWith` is the engine's own "someone is holding this" flag; several update paths key off it.
- The dispatch table, `iLuxProp::OnInteract` overrides:

  | Prop | Entered state | Source |
  |---|---|---|
  | `cLuxProp_Object` (`Grab`/`Push`/`Slide` subtypes) | `InteractGrab` / `InteractPush` / `InteractSlide` | `LuxProp_Object.cpp:329-365` |
  | `cLuxProp_SwingDoor` | `InteractSwingDoor` | `LuxProp_SwingDoor.cpp:156-199` |
  | `cLuxProp_Lever` | `InteractLever` | `LuxProp_Lever.cpp:164-192` |
  | `cLuxProp_Wheel` | `InteractWheel` | `LuxProp_Wheel.cpp:179-197` |
  | `cLuxProp_MultiSlider` | `InteractSlide` | `LuxProp_MultiSlider.cpp:136-163` |

**Pitfall:** the interaction state is a property of the *local player*, not of the prop. `cLuxPlayerStateVars` is a set of statics, and `cLuxPlayer` has exactly one state. There is no second "hand" and no per-Peer interaction state anywhere in the game. A remote Peer's grab cannot be expressed by entering a player state; it has to be expressed as forces, or as state writes, on the prop.

## Grabbables: pick up, rotate, throw

### Which class owns it, and what joint

`cLuxPlayerState_InteractGrab` (`amnesia/src/game/LuxPlayerState_InteractGrab.cpp`) owns the whole grab.

**There is no joint and no constraint.** The prop is never attached to anything. The grab is a pair of PID controllers driving `AddForce` and `AddTorque` on the focus body every logic tick, toward a goal transform computed from the camera. This matters more than any other fact in this document: a grab is a *continuous force stream*, not a link that a remote machine could recreate once.

`OnEnterState` (`:77-225`):

- `mpGrabData = mpCurrentProp->GetGrabData()` — the per-model tuning block `cLuxInteractData_Grab` (`LuxProp.h:36-52`), loaded from `.ent` variables (`LuxProp_Object.cpp:88-100`): `GrabThrowImpulse`, `GrabMassMul`, `GrabUseRotation`, depth min/max/increment, optional position and rotation offsets, force and torque multipliers.
- The held rotation `m_mtxBodyRotation` is seeded either from the model's authored offset, or as the body's current rotation expressed in the camera's frame (`:94-110`). `mvLocalBodyOffset` is the body's offset from the focus point, also in camera space.
- `mfDepth` is either the authored depth or the current camera-to-focus distance minus 0.08 (`:112-119`).
- `SaveBodyProperties` (`:608-627`) walks the body's joint graph recursively and records gravity, `CollideCharacter`, and mass for every body reachable through joints. Each such body's mass is multiplied by `mfGrabMassMul`; the focus body alone loses gravity and character collision; every body's linear and angular velocity is zeroed (`:132-146`).
- The player's character body mass rises by the *pre-multiplication* mass sum, and heavy props slow the player through `SetInteractionMoveSpeedMul` (`:148-164`).
- `mfMaxDistance` is fixed at entry from the current distance (`:166-169`); exceeding it drops the grab.

`OnLeaveState` (`:229-271`) restores gravity and mass per body, re-enables the player-overlap guard with `SetDisableCollisionUntilOutSidePlayer(true)`, and resets the player's mass and speed multiplier. Note the asymmetry: `CollideCharacter` is turned off on enter but **not restored on leave** — it is only reported back through `OnSaveBody` (`:516-527`) for the save file.

### How the prop is held

`PostUpdate` (`:280-380`), every logic tick:

1. Drop the grab if `Vector3Dist(camera, body) > mfMaxDistance` (`:292-297`).
2. Build `mtxGoal` from the camera's pitch/yaw/roll and position, `mfDepth`, `m_mtxBodyRotation`, and either the authored position offset or `mvLocalBodyOffset` (`:302-321`).
3. Position: `vForce = mForcePid.Output(vWantedPos - bodyPos, dt) * mfMassSum`, clamped to `GrabMaxForce`, then `AddForce(vForce * mfForceMul)` (`:325-333`). PID gains p=400, i=0, d=40 (`:53-55`).
4. Rotation, only if `mbUseRotation` (`:336-379`): a wanted angular velocity is built from the cross products and angles between the goal's up/right axes and the body's, scaled by 100, clamped to `GrabMaxAngularSpeed`; then `vTorque = mSpeedTorquePid.Output(wanted - actual, dt)` premultiplied by the body's inertia matrix, clamped to `GrabMaxTorque`, and applied as `AddTorque(vTorque * mfTorqueMul)`. PID gains p=40, i=0, d=0.4.

### How rotation while held is applied

Holding the Rotate action and moving the mouse **does not rotate the body directly** — it rotates the *goal*, and the torque PID chases it:

- `OnAddYaw` (`:461-471`): `m_mtxBodyRotation = MatrixRotateY(amount * -3.2) * m_mtxBodyRotation`.
- `OnAddPitch` (`:473-505`): the same with `MatrixRotateX`, after applying the invert-look preference.
- Both are gated on `pInput->IsTriggerd(eLuxAction_Rotate) && mpGrabData->mbUseRotation`, and both return `false` so the camera does not turn.
- `OnScroll` (`:452-457`) moves `mfDepth` by `mfGrabDepthInc`, clamped to the model's min/max.

The raw mouse delta is integrated, not rate-scaled: `cLuxInputHandler` calls `mpPlayer->AddYaw(-vFinalPos.x)` (`LuxInputHandler.cpp:1226-1227`) which dispatches to the active state's `OnAddYaw` (`LuxPlayer.cpp:674-690`).

### How a throw imparts velocity

`OnDoAction(eLuxPlayerAction_Attack, true)` (`:417-445`): zero the focus body's linear and angular velocity, then `AddImpulse(pCam->GetForward() * mpGrabData->mfGrabThrowImpulse)`, then leave the state. A single impulse along the camera forward, magnitude entirely from the model's `GrabThrowImpulse`.

A plain release, `OnDoAction(eLuxPlayerAction_Interact, false)` (`:388-413`), is not a no-op: every saved body has its linear speed clamped to `GrabMaxLeaveLinearSpeed` and its angular speed to `GrabMaxLeaveAngularSpeed` before the state changes. Whatever residual velocity the PID left is capped, then the prop is free.

### What state a remote machine needs, and at what rate

Two candidate cuts, and they are very different:

- **Relay the cause (the goal transform).** The goal is fully determined by: camera position and orientation, `mfDepth`, `m_mtxBodyRotation`, `mvLocalBodyOffset`, plus which body is held. Camera position/orientation and `mfDepth` change every tick; `m_mtxBodyRotation` changes only while the Rotate action is held; `mvLocalBodyOffset` is fixed for the grab. A remote machine could run the same PID against its own copy of the body. It would also have to reproduce the enter/leave side effects — gravity off, mass multiplied, character collision off, velocities zeroed — or its body will not behave like the holder's.
- **Relay the effect (the body transform).** Position and orientation of the held body, at the logic rate. Six to seven floats per tick per held prop, and it is exact by construction.

The grab loop is the 60 Hz logic tick (`cEngineInitVars`'s `mlUpdateRate` default is 60, `HPL2/core/include/engine/EngineInitVars.h:40`), so "at what rate does it change" is: **every tick, continuously, for the whole duration of the hold.** Discrete moments are only entry, throw, and release.

The grab's own save data is a good list of the per-grab constants (`LuxPlayerState_InteractGrab.h:52-60`): `m_mtxBodyRotation`, `mvLocalBodyOffset`, `mfDepth`, `mfMaxDistance`, plus the prop and body IDs from the base class (`LuxPlayerState_Interact.h:33-40`). A body is identified across machines by `iPhysicsBody::GetUniqueID()` within a prop found by `iLuxProp::GetID()` — `iLuxProp::GetBodyFromID` (`LuxProp.cpp:942-950`) is the existing lookup, and the save system already relies on both being stable (`LuxPlayerState_Interact.cpp:129-173`).

## Swing and spin entities

All four share `iLuxPlayerState_InteractRotateBase` except `MultiSlider`, which uses the slide state. The pattern is the same as the grab: **mouse motion sets a wanted angular speed, and a PID applies torque.** No joint is created by the interaction; the joint is authored in the `.ent` and already exists.

`iLuxPlayerState_InteractRotateBase` (`LuxPlayerState_InteractRotateBase.cpp`):

- `OnEnterState` (`:64-109`): takes `mpCurrentJoint = mpCurrentBody->GetJoint(0)`, turns the body's gravity off, and fixes `mfMaxDistance` from the feet-to-pivot distance.
- `Update` (`:206-275`): decays `mfRotSpeed` by `mfMoveSlowDownFactor`, zeroes it outright if the mouse did not move this tick (`:243-246`), adds `GetSpeedAdd(cam) * 3000 * mfMoveSpeedFactor * dt`, clamps to `mfMoveMaxSpeed`, then `vTorque = mRotatePid.Output(pinDir*mfRotSpeed - hingeVel, dt)` premultiplied by the inertia matrix and clamped to `MoveMaxForce`, applied with `AddTorque`.
- `OnLeaveState` (`:113-123`) restores gravity. Attack calls the subclass's `OnThrow()` and leaves (`:283-313`).

Subclass differences: the swing door's `GetSpeedAdd` projects the mouse push onto the hinge pin (`LuxPlayerState_InteractSwingDoor.cpp:66-81`) and its `OnThrow` adds `mfMoveThrowImpulse` along the joint-forward vector, sign-corrected by the camera (`:83-94`). The wheel's `GetSpeedAdd` (`LuxPlayerState_InteractWheel.cpp:111-190`) keeps a ten-entry history of mouse deltas and infers a circular gesture from tangent geometry; its `OnThrow` is empty (`:194-197`).

**Pitfall:** the wheel's `GetSpeedAdd` is *stateful across ticks* — `mlstMoveAdds` is a rolling window cleared only on entry (`:99-100`), and it returns 0 until it holds four samples (`:118`). Replaying a remote player's mouse deltas would have to replay them tick-for-tick, in order, into a matching window.

### `cLuxProp_SwingDoor`

**Authoritative state.** The joint angle itself is the continuous state: `mvJointData[i].mpHingeJoint->GetAngle()`, one entry per hinge, captured with its authored `mfMaxAngle` at `OnSetupAfterLoad` (`LuxProp_SwingDoor.cpp:203-231`). On top of it sit two booleans, `mbClosed` and `mbLocked`, and a derived tri-state.

- `GetDoorState()` (`:593-626`) derives -1 closed / 0 between / 1 open from the absolute hinge angle against 5° and 70 % of max, across all hinges. It is computed on demand, never stored.
- `SetClosed(bool, bool effects)` (`:552-591`) is the real state change: it clamps every hinge's max angle to 2° and turns sticky limits off (closed), or restores the authored max angle and sticky limits (open); then it nudges every body with `AddForce(1,1,1)` so the solver settles; then plays a sound.
- `SetLocked(bool, bool effects)` (`:628-643`) sets `mbLocked` and, when locking, calls `SetClosed(true, false)`.
- Save data (`LuxProp_SwingDoor.h:29-47`): `mbLocked`, `mbBroken`, `mbClosed`, `mbDisableBreakable`, `mbDisableAutoClose`, `mlCurrentMeshEntity`, damage-mesh entries. **The hinge angle is not in the save data** — the door's physical pose comes back through the joint's own engine-level serialization, not the prop's.

**Change events.** `OnInteract` itself opens an unlocked door (`SetClosed(false, true)`, `:188-191`) and clears `mbDisableAutoClose`. `UpdatePropSpecific` (`:349-366`) auto-closes a single-hinge door whenever the hinge is within 10° of zero *and nobody is holding it* (`IsInteractedWith()==false`). `OnConnectionStateChange` (`:674-679`) maps an incoming connection state to `SetLocked(state > 0, true)` — a door on the receiving end of a connection is **locked or unlocked, never opened**.

**Existing remote-settable setter.** Yes, three, all script-reachable: `SetSwingDoorLocked`, `SetSwingDoorClosed`, `SetSwingDoorDisableAutoClose` (`LuxScriptHandler.cpp:607-609`, implementations at `:2771-2812`), plus the read-backs `GetSwingDoorLocked` / `GetSwingDoorClosed` / `GetSwingDoorState` (`:614-616`).

**Pitfall:** `SetSwingDoorOpenAmount` is registered (`LuxScriptHandler.cpp:610`) but its body is commented out in this repository — `//pDoor->SetOpenAmount(afAmount); // TODO(amnesia-tdd-tcp): Reimplement` (`:2793-2802`). Calling it is a silent no-op. The machinery it would need still exists as the private `SetupDoorPhysics(float afOpenAmount)` (`:690-731`), which rotates each child body about its pivot by `mfMaxAngle * afOpenAmount` and is currently reached only from the loader (`:96`) and `OnResetProperties` (`:315`). **Placing a door at a given openness from outside is the one obviously missing setter in this whole surface**, and it is a short reimplementation rather than new design.

### `cLuxProp_Wheel`

**Authoritative state.** `mfAngle` — an *accumulated* angle, not the joint's. `UpdateAngle` (`LuxProp_Wheel.cpp:377-442`) reads `mpHingeJoint->GetAngle()`, takes the wrapped delta against `mfPrevJointAngle` with `GetAngleDistanceRad`, and adds it to `mfAngle`. This lets a valve turn past ±180° and track multiple revolutions. Alongside it: `mlCurrentState` (-1/0/1 against `mfMinLimit`/`mfMaxLimit` with their ranges), `mlStuckState`, `mbAutoMoving`/`mfAutoMoveGoal`, `mfLastToMax`/`mfLastToMin` for one-direction spin, and `mlSpinDir`. The full list is the save data (`LuxProp_Wheel.h:29-56`; serialization `LuxProp_Wheel.cpp:637-663`), and it is the most complete state description of any of the four.

**Change events.** `UpdateCheckLimit` (`:446-472`) calls `ChangeState(±1|0, true)` as `mfAngle` crosses the limit ranges, and latches `SetStuckState` when `mbMinLimitStuck`/`mbMaxLimitStuck` are set. `ChangeState` (`:584-618`) fires `ConnectionStateChange(±1)` (script callback plus entity connections) and `InteractConnectionLimit(mlCurrentState)`. Separately, `UpdateAngle` calls `InteractConnectionTurn(mfAngle, mfPrevAngle, mfMinLimit, mfMaxLimit)` **every tick** (`:441`) — that is the continuous valve→gate drive.

**Existing remote-settable setter.** Yes, and it is the best one on the surface: `SetAngle(float, bool abAutoMoveToAngle)` (`:327-358`), exposed as `SetWheelAngle(name, angle, autoMove)` (`LuxScriptHandler.cpp:618`, `:2888-2896`). With `abAutoMoveToAngle == false` it *snaps* the bookkeeping (`mfAngle = mfPrevAngle = clamped`) without touching physics. With `true` it sets `mbAutoMoving`/`mfAutoMoveGoal` and `UpdateAutoRotation` (`:476-522`) torques the body there over several ticks. Also `SetWheelStuckState` (`:619`) and `SetWheelInteractionDisablesStuck` (`:621`). There is **no** `GetWheelAngle` in the script API.

**Pitfall:** `SetAngle(x, false)` moves `mfAngle` but not the wheel body. The next `UpdateAngle` adds the real joint delta on top of the value you wrote, so the visual wheel and the logical angle stay desynchronized until something moves the body. The snap form is a bookkeeping correction; only the auto-move form actually turns the valve.

**Pitfall:** `SetStuckState` (`:277-323`) does not just set a flag — it rewrites the hinge's min and max angle to a ±1° window around the joint's *current* angle, and assigns `mfAngle` from the limit minus half the limit range. Replaying a stuck transition on a machine whose joint angle differs pins the wheel at a different physical angle.

### `cLuxProp_Lever`

**Authoritative state.** Unlike the wheel, the lever keeps no angle of its own — `UpdatePropSpecific` reads `mpHingeJoint->GetAngle()` fresh each tick and passes it down (`LuxProp_Lever.cpp:236-243`). The stored state is `mlCurrentState` (-1/0/1), `mlStuckState`, `mbAutoMoveToAngle`, `mlAutoMoveGoal`, `mfMiddleAngleAmount`, `mbInteractionDisablesStuck` — exactly the save data (`LuxProp_Lever.h:28-40`; `LuxProp_Lever.cpp:454-461`).

**Change events.** `UpdateCheckLimit` (`:340-366`) calls `ChangeState` on crossing the limit ranges; `ChangeState` (`:404-450`) fires `ConnectionStateChange(±1|0)` and `InteractConnectionLimit`. **The lever fires `ConnectionStateChange(0)` for the middle state; the wheel does not** (compare `LuxProp_Lever.cpp:428-432` with `LuxProp_Wheel.cpp:594-606`). `UpdateAutoMove` (`:369-402`) springs an unheld lever toward its goal angle.

**Existing remote-settable setter.** Partly. `SetLeverStuckState` (`LuxScriptHandler.cpp:620`) and `SetLeverInteractionDisablesStuck` (`:622`) exist, and `GetLeverState` (`:623`) reads back. **There is no setter for the lever's angle or for `mlCurrentState` directly.** `SetStuckState(±1, effects)` (`:263-295`) is the only way in from outside, and it works by clamping the hinge's min/max to the limit range — which does move the lever, as a side effect of the solver pushing it into the new window.

**Pitfall:** `UpdateAutoMove` calls `mpLeverBody->SetAngularVelocity(vWantedVel)` *and then* adds PID torque (`:394-401`), with a source comment admitting the author was unsure. A hard velocity write every tick makes the lever's motion much less a function of accumulated forces and much more a function of whether that tick ran at all.

### `cLuxProp_MultiSlider`

**Authoritative state.** A slider joint, not a hinge: `mpSliderJoint->GetDistance()`, read fresh each tick (`LuxProp_MultiSlider.cpp:205-212`). `OnSetupAfterLoad` (`:165-195`) divides the joint's authored min/max distance into `mlNumOfStates` evenly spaced positions in `mvStates`. Stored state is just `mlCurrentState` (an **index**, 0..n-1, or -1), `mlStuckState`, `mbInteractionDisablesStuck`, and `msChangeStateCallback` — the save data (`LuxProp_MultiSlider.h:28-38`; `:383-388`).

**Change events.** `UpdateCheckNewState` (`:300-318`) snaps `mlCurrentState` to whichever authored position is within `mfStickToStateMaxDist`. `ChangeState` (`:353-379`) plays a sound and runs `msChangeStateCallback(name, state)` as a script call. **`ChangeState` does not call `ConnectionStateChange` and does not call `InteractConnectionLimit`** — the MultiSlider is the one prop of the four that is outside the connection system entirely; its only outbound signal is its own script callback.

**Existing remote-settable setter.** Only `SetMultiSliderStuckState` (`LuxScriptHandler.cpp:625`, `:2958-2966`) and `SetMultiSliderCallback` (`:626`). **There is no setter for the slider position and none for `mlCurrentState`**, and no script getter either — `GetMultiSliderState()` exists in C++ (`LuxProp_MultiSlider.h:79`) but is not registered.

### Summary table

| | Continuous state | Discrete state | Change event | Remote setter today |
|---|---|---|---|---|
| SwingDoor | hinge angle (per hinge, engine) | `mbClosed`, `mbLocked`; `GetDoorState()` derived | none on angle; `SetClosed`/`SetLocked` sounds; `OnConnectionStateChange` → lock | `SetSwingDoorClosed`, `SetSwingDoorLocked`, `SetSwingDoorDisableAutoClose`. `SetSwingDoorOpenAmount` **stubbed out** |
| Wheel | `mfAngle` (accumulated, prop-owned) | `mlCurrentState`, `mlStuckState` | `ChangeState` → `ConnectionStateChange(±1)` + `InteractConnectionLimit`; `InteractConnectionTurn` every tick | `SetWheelAngle` (snap or auto-move), `SetWheelStuckState` |
| Lever | hinge angle (engine, not stored) | `mlCurrentState`, `mlStuckState` | `ChangeState` → `ConnectionStateChange(±1, **and 0**)` + `InteractConnectionLimit` | `SetLeverStuckState` only — no angle setter |
| MultiSlider | slider distance (engine, not stored) | `mlCurrentState` (index), `mlStuckState` | `ChangeState` → own script callback only; **no connection signal** | `SetMultiSliderStuckState` only — no position setter |

## The valve-controls-a-door case

**It is both, and the two halves behave differently. The answer to "does reproducing the cause reproduce the effect" is: yes, and that is exactly the double-apply hazard.**

There are two connection mechanisms, and a map can use either or both.

### Entity connections — script-registered, engine-driven, discrete

- Registered by the script function `ConnectEntities(name, mainEntity, connectEntity, invert, statesUsed, callback)` (`LuxScriptHandler.cpp:681`, `:3600-3607`), which calls `iLuxEntity::AddConnection` (`LuxEntity.cpp:182-190`).
- Fired by `iLuxEntity::ConnectionStateChange(int)` (`LuxEntity.cpp:304-342`): it runs the entity's own `ConnectionStateChangeCallback` script function, then for each connection filters by `GetStateUsed()`, inverts if asked, calls `pConn->GetEntity()->OnConnectionStateChange(this, state)` **in C++, immediately, inside the same tick**, and then optionally runs a per-connection script callback.
- The receiving prop decides what the state means. `cLuxProp_SwingDoor::OnConnectionStateChange` locks or unlocks (`LuxProp_SwingDoor.cpp:674-679`). `cLuxProp_Wheel`'s and `cLuxProp_Lever`'s jam themselves at the min limit (`LuxProp_Wheel.cpp:362-366`, `LuxProp_Lever.cpp:297-302`).

So: the *registration* is script-level; the *propagation* is engine-level C++ within the tick that crossed the limit.

### Interact connections — script-registered, engine-driven, continuous

- `InteractConnectPropWithMoveObject(name, propName, moveObjectName, interactOnly, invert, statesUsed)` (`LuxScriptHandler.cpp:680`, `:3581-3594`) and `InteractConnectPropWithRope(...)` (`:679`) build an `iLuxInteractConnection` and hang it on the prop (`iLuxProp::AddInteractConnection`, `LuxProp.cpp:888-892`).
- `iLuxProp::OnUpdate` (`LuxProp.cpp:380-405`) calls `Update` then `UpdateProp` on every interact connection **before** `UpdatePropSpecific`.
- Forward drive: `iLuxProp::InteractConnectionTurn` (`LuxProp.cpp:925-939`) normalizes the angle to `fT ∈ [0,1]` and calls `OnTurn(delta, fT)` on each connection; `cLuxInteractConnection_MoveObject::OnTurn` (`LuxInteractConnections.cpp:290-296`) calls `mpMoveObject->MoveToState(fT, 10, 5, 0.01, false)`, which slerps or lerps the gate between its closed and open transforms (`LuxProp_MoveObject.cpp:222-249`). `iLuxProp::InteractConnectionLimit` (`LuxProp.cpp:913-923`) is the discrete counterpart, calling `OnLimit(state)`.
- **Both are gated on the holder:** `if(pConnection->GetInteractionOnly() && mbIsInteractedWith==false) continue` (`LuxProp.cpp:930-933` and `:918-921`). With `abInteractOnly` true — the normal authoring — **a valve only drives its gate while a local player is holding the valve.**
- Reverse drive: `cLuxInteractConnection_MoveObject::UpdateProp` (`LuxInteractConnections.cpp:249-288`) reads `mpMoveObject->GetMoveState()` and, **only when the wheel is not held**, calls `pWheel->SetAngle(fAngle, true)` to spin the wheel to match the gate. The wheel-is-held branch is entirely commented out, with a `TODO: Can this be fixed? Right now it messes up stuff?` The rope variant does the same (`:87-114`).

### What this means for two machines

1. **Both machines already have the connection.** Connections are registered from the map's own script, which both machines ran when they loaded the map. Nothing about the wiring needs to be relayed.
2. **Reproducing the cause reproduces the effect for free — if `mbIsInteractedWith` is true on the remote machine.** Turn the remote wheel body and the remote gate follows, because `UpdateAngle` → `InteractConnectionTurn` → `MoveToState` runs locally. This is the good case, and it is why the interaction-ownership design should care about `SetIsInteractedWith` as much as about angles.
3. **It double-applies if you relay both.** Relay the wheel's angle *and* the gate's position and the receiving machine applies the gate twice: once from its own `InteractConnectionTurn`, once from your state write. `MoveToState` is a goal, not an increment, so the visible symptom is fighting and stutter rather than drift — but the gate's velocity will be wrong.
4. **It silently does nothing if you relay only the angle without the holder flag.** Write `mfAngle` on a machine where `mbIsInteractedWith` is false and `InteractConnectionTurn` skips every connection. The valve spins; the gate never moves. Worse, the *reverse* drive then engages and `UpdateProp` calls `SetAngle(fAngle, true)` to pull the wheel back toward where the un-moved gate says it should be — **the remote wheel will actively fight the relayed angle.**
5. **Entity connections fire from the crossing, not from the state.** `ChangeState` early-returns when the state is unchanged (`LuxProp_Wheel.cpp:586`), so a remote machine that reaches the same `mlCurrentState` by a different path fires the same single event. But a machine that is *set* directly to the end state without crossing the range never fires it at all. Locks and script callbacks hang off that edge.

**Pitfall:** the SwingDoor cannot be opened by a connection. If the acceptance walkthrough is "one spins a valve and the other sees the door it controls open", the door in that map is almost certainly a `MoveObject` gate driven by an interact connection, not a `SwingDoor` — `cLuxProp_SwingDoor::OnConnectionStateChange` only locks. Author the test room accordingly.

## What already exists to build on

**There is no prop-state entry point in the Game Interaction Protocol.** The adapter interface `iGameInteractionGameAdapter` (`GameInteractionGateway.h:243-267`) exposes exactly: map loaded, position, rotation, map file, `RunScript`, chat, custom stories, local Pose, and the four Avatar operations. Protocol Version 2's own keywords are `negotiate`, `avatarcreate`, `avatarremove`, `avatarcollision`, `avatarpose`, `localpose` (`GameInteractionProtocolVersion2.cpp:42-50`); everything else falls through to the legacy protocol (`:327`). No Command touches a prop.

**The one existing door is `exec:`.** `eGameInteractionCommand_ExecuteScript`, parsed from `exec:<script>` (`LegacyGameInteractionProtocol.cpp:211-212`), classified `StateChanging` so it needs the Authority Grant (`GameInteractionGateway.cpp:70-78`), and delivered to `RunScript`. Through it, a Peer can already call every registered script function — including `SetWheelAngle`, `SetWheelStuckState`, `SetLeverStuckState`, `SetMultiSliderStuckState`, `SetSwingDoorClosed`, `SetSwingDoorLocked`, `AddPropImpulse` (`LuxScriptHandler.cpp:666`, `:3470-3482`), `AddPropForce`, `SetEntityPosRot`, and `SetEntityInteractionDisabled`.

So the honest answer to "would each interaction need a new entry point" is: **no new entry point is strictly required for the discrete state of wheels, levers, sliders, and door lock/closed — `exec:` reaches all of it today.** What `exec:` cannot reach is (a) the held prop's continuous transform, (b) a door's openness, since `SetSwingDoorOpenAmount` is stubbed, (c) `SetIsInteractedWith`, which is not exposed at all, and (d) anything at 60 Hz, since every `exec:` is a string parsed and compiled by AngelScript per call.

**The exclusive-hold primitive already exists.** `SetEntityInteractionDisabled(name, disabled)` (`LuxScriptHandler.cpp:592`, backed by `iLuxEntity::SetInteractionDisabled`, `LuxEntity.h:165-166`) is checked in `CanInteractWithEntity` (`LuxPlayerState_DefaultBase.cpp:236-241`) and also greys the focus crosshair (`:183`). Denying one player a grab that another player holds needs no new engine surface — it needs a Command, or an `exec:`, that flips this flag. The "visible affordance for the player who lost the race" that #26 leaves open is partly answered: the crosshair already changes.

**Outbound observation is nearly absent.** `eGameInteractionEvent_ScriptCallObserved` is published from exactly one place — the collide-callback path in `cLuxCollideCallbackContainer` (`LuxTypes.cpp:182-184`). No prop state change, no `ConnectionStateChange`, no `ChangeState`, and no interaction entry or exit is reported to Peers. **Every outbound signal this feature needs is new.**

## What makes an interaction non-deterministic across two machines

Ordered roughly by how much damage each does.

**1. The interaction is a force stream, and the solver is not reproducible.** Physics is Newton (`HPL2/core/sources/impl/PhysicsWorldNewton.cpp`) at accuracy level Medium, `NewtonSetSolverModel(world, 1)` (`:176-191`), set for every loaded map along with a world size of ±300 and a max timestep of 1/60 (`HPL2/core/sources/resources/WorldLoaderHplMap.cpp:273-275`). An iterative constraint solver's output depends on contact ordering, warm-start caches, and body sleep state, none of which is part of any savable state. Two machines given identical forces will diverge. `SetNumberOfThreads` (`PhysicsWorldNewton.cpp:206-209`) is never called from the game, so at least the solver is single-threaded and the divergence is not *also* scheduler-dependent.

**2. The timestep is fixed but skippable.** `cLogicTimer` targets 60 Hz (`HPL2/core/sources/system/LogicTimer.cpp:91-95`; `EngineInitVars.h:40`), and `cPhysicsWorldNewton::Simulate` subdivides anything larger into 1/60 chunks (`:102-124`), so a slow frame does *not* produce a giant physics step. But `WantUpdate` refuses more than `mlMaxUpdates` catch-up ticks — `alUpdatesPerSec/10`, so 6 — and `EndUpdateLoop` then calls `Reset()`, throwing the backlog away (`LogicTimer.cpp:63-86`). **A machine that stalls simply runs fewer ticks than the other.** Everything downstream — PID integrals, `mfRotSpeed` decay, `mfAngle` accumulation — is per-tick, so the two machines apply different numbers of increments to the same nominal interval.

**3. PID controllers carry hidden per-tick history.** `cPidController::Output` (`HPL2/core/include/math/PidController.h:51-73`) keeps a ring of the last N errors *and their timesteps* — 20 for the grab's force and torque PIDs (`LuxPlayerState_InteractGrab.cpp:50-51`), 10 for the rotate base (`LuxPlayerState_InteractRotateBase.cpp:43`). The integral is the sum over the whole ring and the derivative divides by the current timestep. This state is not saved, not exposed, and is reset only on `Reset()`. Two machines whose tick counts differ by one produce different torque from identical inputs, and the difference persists for twenty ticks.

**4. Mouse input is integrated raw, and the wheel's gesture recognizer is stateful.** `AddYaw`/`AddPitch` deltas arrive once per rendered frame, not per logic tick (`LuxInputHandler.cpp:1226-1227`), and `cLuxPlayerState_InteractWheel::GetSpeedAdd` folds a ten-sample history into a tangent estimate (`:111-190`). Relaying inputs means relaying them at the sender's frame cadence.

**5. Velocity writes rather than forces.** `cLuxProp_Lever::UpdateAutoMove` writes `SetAngularVelocity` directly each tick (`LuxProp_Lever.cpp:394`); the grab zeroes both velocities on entry (`LuxPlayerState_InteractGrab.cpp:142-143`) and again before a throw (`:437-438`); release clamps both (`:399-405`). These are discontinuities, so a tick that lands on one side of them on one machine and the other side on the other produces a large, immediate divergence rather than a small one.

**6. Randomness: not a factor here.** The only `RandRectf` calls near props are a particle-system start-time jitter (`LuxProp.cpp:130`) and critter wandering (`LuxProp_CritterBase.cpp:403, 417`). Nothing in the grab or swing/spin path draws a random number.

**Conclusion for the seam.** Relaying *inputs* and re-simulating is not viable for the continuous part: points 1–3 guarantee the two simulations separate, and there is no mechanism to re-converge them. Relaying *resulting state* is viable, and the surface supports it unevenly — the wheel has a real angle setter, the lever and slider have none, and the door's openness setter is stubbed. The natural split is:

- **Continuous, relay state:** the held prop's body transform while grabbed; the swing door's hinge angle; the lever's hinge angle; the slider's distance; the wheel's `mfAngle`. Every one of these changes every tick while the interaction lasts.
- **Discrete, relay the event:** grab taken, grab released, thrown (with the impulse, so the receiving machine can let its own solver carry the flight), `mlCurrentState` / `mlStuckState` / `mbClosed` / `mbLocked` transitions.
- **Do not relay, let it happen locally:** anything an interact connection or an entity connection derives from the above — gates, ropes, locks, `ConnectionStateChange` script callbacks — provided `mbIsInteractedWith` is relayed so the `GetInteractionOnly()` gate opens on the receiving machine. Relaying the derived effect as well is the double-apply.
