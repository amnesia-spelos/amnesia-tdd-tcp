# Script and map state: what two machines share, and what could be synced

Findings for the interaction-ownership design (`amnesia-spelos/amnesia-multiplayer#28`). The question was whether prop state changes that run each machine's own map script, once per machine, are acceptable, and whether map/script state could be synchronized instead. Paths are relative to `src/`. No `.hps` or map files are in this repository, so how often shipped maps use state-change callbacks for story progression was not measured.

## How AngelScript is hosted per map

- One AngelScript engine per process (`HPL2/core/sources/impl/LowLevelSystemSDL.cpp:405`); each script is a `cSqScript` (`:449-451`).
- `cLuxMap::LoadFromFile` loads and compiles `<map>.hps` (or `.chps`) through `cScriptManager::CreateScript` (`amnesia/src/game/LuxMap.cpp:152-190`; `HPL2/core/sources/resources/ScriptManager.cpp:59-89`), a reference-counted resource.
- Each script gets its own module, named `"Module_"+RandRectl(0,1000000)+"_"+handle` (`SqScript.cpp:59,151-159`) — a draw from the global `rand()`.
- Every callback is a string built in C++ and executed with `ExecuteString(engine, "Func(args)", module)` (`SqScript.cpp:195-199`).
- `cLuxMap::RunScript` returns early unless the map is current (`LuxMap.cpp:404-410`). `OnStart` runs on first entry only, `OnEnter` every entry (`:347-360`).
- The module lives as long as the `cLuxMap` (`LuxMap.cpp:116`; `SqScript.cpp:66`). A map change destroys the map and later reloads it from file (`LuxMapHandler.cpp:639-688`).
- **Global variables declared in the `.hps` are invisible to C++.** They are plain module globals; nothing enumerates or serializes them (no `GetGlobalVarCount` / `GetAddressOfGlobalVar` use). They reset to their initializers when the map reloads. Inference: loading a save into the same map does not rebuild the module (`LuxSaveHandler.cpp:436-452`), so the globals keep stale values.

## Variables and save state

- `cLuxScriptVar { msName, msVal }` — every script variable is a string — in a `std::map<tString, cLuxScriptVar>` (`LuxTypes.h:813-824`).
- Local variables: `cLuxMap::m_mapVars` (`LuxMap.h:238`), created on first access by `GetVar` (`LuxMap.cpp:1205-1213`; `LuxScriptHandler.cpp:810-825`). Global variables: `cLuxGlobalDataHandler::m_mapVars` (`LuxGlobalDataHandler.h:64`; `LuxScriptHandler.cpp:921-985`). Inference: typically tens of entries, well under 10 KB.
- A full save, `cLuxSaveGame_SaveData` (`LuxSavedGameTypes.h:460-470`, built at `LuxSaveHandler.cpp:334-356`), holds inventory, journal, player, music, effects, global data, hints, insanity, load screen, and `cLuxSavedMap` (`LuxSavedMap.h:35-89`; `FromMap` at `LuxSavedMap.cpp:58-219`) — map settings, checkpoint, sky/fog, lights, looping sounds, particles, ropes, a full `iLuxEntity_SaveData` per saved entity, local variables, timers, and use-item callbacks.
- Maps already left are kept as the smaller `cLuxSavedGameMap` in `cLuxSavedGameMapCollection` (`LuxSavedGame.h:93-150`).
- Everything serializes through `cSerializeClass` into one TinyXML tree, optionally compressed (`HPL2/core/sources/system/SerializeClass.cpp:206-222`). Inference, not measured: hundreds of KB of uncompressed XML for a map with a few hundred entities.

## Timers and callbacks

- Timers: `cLuxMap::mlstTimers`, a list of `cLuxEventTimer { name, function, mfCount }` (`LuxTypes.h:727-736`), counted down by the map step in `UpdateTimers` (`LuxMap.cpp:1338-1360`), saved (`LuxSavedMap.cpp:171-179`).
- Collide callbacks: per entity, `iLuxCollideCallbackContainer::mlstCollideCallbacks` (`LuxTypes.h:650-686`). Callbacks on `"Player"` go to `gpBase->mpPlayer` (`LuxScriptHandler.cpp:3410-3412`) and are checked only against the local player (`LuxPlayer.cpp:385`). Saved with entity and player save data (`LuxEntity.cpp:471,505-511`; `LuxSavedGameTypes.cpp:1185-1191`).
- Use-item callbacks: `cLuxMap::mlstUseItemCallbacks` (`LuxMap.h:253`), saved.
- Connections and their callback names are serialized per entity (`LuxEntity.cpp:416-471`).

## Quests, journal, inventory

Game-wide, not per map: quests in `cLuxJournal::mvQuestNotes` (`LuxJournal.h:280`; `AddQuestNote` at `LuxJournal.cpp:752-769`), items in `cLuxInventory::mvItems` (`LuxInventory.h:419`). `AddQuest` also triggers a hint, a message, and a progress-log entry (`LuxScriptHandler.cpp:1749-1763`). Only quest counts and completion amounts are per map.

## Where two machines' scripts diverge

- **`rand()` is never seeded.** `RandInt`/`RandFloat` call it directly (`LuxScriptHandler.cpp:743-751`; `Math.cpp:42-54`) and `cMath::Randomize` has no call sites. Both processes start from the CRT default seed, but module naming, particles, AI, and more also draw from `rand()`, so the sequences split quickly.
- **Timers** run on each machine's own step, phased from when `OnStart` ran locally.
- **`"Player"` collide callbacks** fire only for the local player; an Avatar never triggers them (Avatars are not map entities, ADR 0002).
- **Physics-dependent outcomes** — loose props, enemies, entity-to-entity collide callbacks — may fire on both machines at different frames, or on only one.
- **Runtime entity IDs.** `GetFreeEntityID` returns the lowest unused ID (`LuxMap.cpp:1279-1294`), so runtime-created entities (`CreateEntityAtArea`, break debris) get matching IDs only while both machines' entity sets match exactly. Map-placed entities take their ID from the `.map` file (`HPL2/core/sources/resources/WorldLoaderHplMap.cpp:1625`) and always match.

## Applying state to a running map

- **There is no live-apply path.** `cLuxSavedMap::ToMap` rebuilds entities from save data (`LuxSavedMap.cpp:326-345`) and requires `DestroyAllEntities` first, or a load without dynamic entities (`LuxSaveHandler.cpp:423-452`). Loading a save into the same map is a stop-the-world reset: destroy all entities, recreate from save data, reset every handler (`:380-396`). The debug handler's quick reload does save-then-reload (`LuxDebugHandler.cpp:1329-1348`).
- **Easy to patch live:** variables (string maps), timers (small structs), use-item callbacks.
- **Hard to patch live:** entity state — `LoadFromSaveData` / `SetupSaveData` assume a freshly created entity (`LuxEntity.cpp:517-556`) — and AngelScript globals, which C++ cannot reach at all.
- Practical sync points (inference): map entry, through the load-game path with one side authoritative; or replicating discrete variable writes as events. A full mid-map resync means a load-screen-style rebuild.

## What prop state changes run

- `iLuxEntity::ConnectionStateChange` (`LuxEntity.cpp:304-342`) runs the entity's `ConnectionStateChangeCallback(name, state)`, then `OnConnectionStateChange` on each connected child — a lever's sets its stuck state and can call `ChangeState` again, so connections chain (`LuxProp_Lever.cpp:297-301`) — then each connection's own callback.
- Lever and wheel `ChangeState` return early on an unchanged state, then fire `ConnectionStateChange` and a limit sound (`LuxProp_Lever.cpp:404-440`; `LuxProp_Wheel.cpp:584-610`). They are triggered from `UpdateCheckLimit` reading the hinge angle, which is computed from body matrices (`HPL2/core/sources/impl/PhysicsJointHingeNewton.cpp:125-140`) — so a machine whose body follows a streamed transform crosses the same limits.
- MultiSlider runs its own `msChangeStateCallback` (`LuxProp_MultiSlider.cpp:369-371`); Button calls `ConnectionStateChange` (`LuxProp_Button.cpp:151`); a broken prop runs the `"Break"` callback (`LuxProp_Object.cpp:651`).
- This fork already publishes `eGameInteractionEvent_ScriptCallObserved` for each collide callback it runs (`LuxTypes.cpp:182`).

## Conclusion for the seam

When a streamed prop crosses a limit on both machines, **each machine runs its own copy of the map script once, against its own state**. That is not a double application: a local variable is set once per world, a quest completes once per journal, sanity is boosted once per player. What can go wrong is divergence — randomness, timer phase, scripts assuming a single player — not doubling.

Map-state synchronization is not worth doing for interaction sync. Variables are cheap to replicate, but AngelScript globals are unreachable and entity state has no live-apply path; the only credible full sync point is map entry through the load-game path. That belongs to a future effort on story authority, and this document is its starting point.
