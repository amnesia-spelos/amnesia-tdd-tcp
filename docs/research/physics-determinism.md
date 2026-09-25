# Physics determinism across two machines, and knock-on props

Findings for the interaction-ownership design (`amnesia-spelos/amnesia-multiplayer#28`). The question: when a Holder's thrown prop knocks other loose props, can each machine simulate the knock-on itself (by seeding randomness, or by making Newton deterministic), or must the result be relayed? Paths are relative to `src/`. "Inference" marks reasoning not directly verified.

## Newton version and its determinism promise

- HPL2 links a prebuilt **Newton 2.00** (`HPL2/dependencies/include/Newton.h:13-14`; `HPL2/dependencies/lib/win32/newton.lib`).
- Newton 2.00 has `NewtonInvalidateCache` (`Newton.h:327`); HPL2 never calls it, nor `NewtonSetPlatformArchitecture`. `SetNumberOfThreads` exists (`HPL2/core/sources/impl/PhysicsWorldNewton.cpp:206-208`) but is never called.
- Newton's own documentation of `NewtonInvalidateCache` (3.14 source, <https://github.com/MADEAPPS/newton-dynamics/blob/master/newton-3.14/sdk/dgNewton/Newton.cpp#L239-L258>): call it "whenever you want to create a reproducible simulation … It does *not* suffice to merely reset the position and velocity of objects", because Newton exploits frame-to-frame coherence; calling it every update has "a heavy performance penalty". Reproducibility is promised only from an identical start state with caches reset.
- Newton's author, asked whether Newton is 100% deterministic for networked synchronization, answered "no" (<https://github.com/MADEAPPS/newton-dynamics/issues/84>).

## Randomness does not touch physics

- No `rand` or `cMath::Rand*` in `HPL2/core/sources/physics` or the Newton implementation files. **Seeding would not change any physics outcome.**
- `rand()` is never seeded (`cMath::Randomize`, `Math.cpp:86-96`, has no callers), so both processes start from the C default seed 1 — but the one global stream is shared by script `RandInt`/`RandFloat` (`amnesia/src/game/LuxScriptHandler.cpp:743-751`), module naming (`SqScript.cpp:59`), particles (`ParticleEmitter_UserData.cpp:661+`), animation start (`LuxProp.cpp:130`), player effects (`LuxPlayerHelpers.cpp`), critters and enemies. Draw counts differ between machines almost immediately. Inference: scripts could only agree through a separate script PRNG seeded per map, and even then only for calls made in the same order on both machines.

## Why lockstep or local re-simulation fails here

Even a deterministic solver needs bit-identical inputs every tick (<https://gafferongames.com/post/deterministic_lockstep/>, <https://gafferongames.com/post/floating_point_determinism/>). This game cannot supply them:

- An Avatar's character body is teleported every frame to an interpolated, delayed Pose (`amnesia/src/game/LuxAvatarHandler.cpp:109-119`); the local player's is simulated. Character bodies push props (`HPL2/core/sources/physics/CharacterBody.cpp:128-195`).
- A streamed body is set from interpolation on the receiver's own ticks.
- `cLogicTimer` drops catch-up ticks past six (`HPL2/core/sources/system/LogicTimer.cpp:63-95`).
- Update order follows a `std::set<iPhysicsBody*>` ordered by pointer (`HPL2/core/include/physics/PhysicsWorld.h:62,269`; iterated at `PhysicsWorld.cpp:106,158`).
- Runtime entities (break debris, script spawns) may be created in different orders.
- No `/fp:` or `/arch:` flags are set, so MSVC's default `/fp:precise` applies (inference from MSVC defaults); Newton is a fixed prebuilt binary.

**Lockstep is not feasible** without rewriting timing, character handling, and body ordering, and adding input delay.

## Authority propagation, the standard alternative

Glenn Fiedler, "Networking for Physics Programmers" (GDC 2010 slides, <https://www.gamedevs.org/uploads/networking-for-physics-programmers.pdf>; 2015 talk <https://www.gdcvault.com/play/1022195/>): authority means being the source of state for an object. A player has authority over what they control; players take **interaction authority** over objects they interact with "until it returns to rest"; ties on default authority go to the lowest player id; authority is taken locally without waiting, and a losing side's object pops to the correct state.

"Networked Physics in Virtual Reality" (<https://gafferongames.com/post/networked_physics_in_virtual_reality/>): authority propagates **recursively** to touched objects; **ownership** (holding) is stronger than authority and is never taken by propagation; each object carries authority and ownership sequence numbers, the authority sequence advancing when authority is taken and when the object comes to rest; the host arbitrates. Lockstep was ruled out because PhysX is not deterministic.

## Engine hooks for propagation

- Every material pair registers `ContactsProcessCallback` (`HPL2/core/sources/impl/PhysicsMaterialNewton.cpp:187-188`), which calls `OnCollide` on both bodies (`:399-400`), fanning out to each `iPhysicsBodyCallback::OnBodyCollide` (`HPL2/core/sources/physics/PhysicsBody.cpp:485-497`).
- It fires for every body pair with real contacts that passes `OnAABBOverlapCallback`'s filters (`PhysicsMaterialNewton.cpp:241-253`), including static mass-0 geometry — filter with `GetMass()==0`.
- Character bodies are mass-0 Newton bodies flagged `IsCharacter` (`CharacterBody.cpp:272-279`); their pushing of props goes through a shape-collision callback that adds forces (`:128-195`), not through `OnCollide`.
- Callbacks run inside `NewtonUpdate`, possibly several times per tick because of the 1/60 subdivision (inference): record contacts there, act on them after the step.
- A multiplayer body callback can sit beside `cLuxProp_Object_BodyCallback` (`amnesia/src/game/LuxProp_Object.cpp:201`, registered with `AddBodyCallback` at `:393`).
- **Rest:** `GetEnabled()` returns `NewtonBodyGetSleepState()==0` (`HPL2/core/sources/impl/PhysicsBodyNewton.cpp:339-342`). The auto-disable threshold setters are commented-out no-ops (`:357-391`) and nothing calls `SetAutoDisable`, so props sleep on Newton's default thresholds, which cannot be tuned without restoring those setters.

## Conclusion

Seeding changes nothing for physics, and deterministic local simulation is not achievable. Knock-on consistency comes from **propagating the Holder's authority to the props its streamed bodies touch**, streamed until they sleep, with divergence accepted past a streaming budget.
