---
status: accepted
---

# The game follows Peer-driven bodies and runs its own prop logic

To share interactions between two games, a Peer tells the game to make a map-placed entity a Peer-Driven Entity, then supplies the world transform and velocity of its bodies, sender-timestamped and interpolated on the same delay as Avatar Poses. The game sets those bodies from the stream each tick and mirrors whether the entity is being interacted with. It does not use per-type setters such as a wheel angle or door openness. A hinge's angle is computed from its bodies' matrices (`PhysicsJointHingeNewton.cpp:125-140`), so the receiving game's own wheel, lever, door, and slider logic observes the same motion. It crosses the same limits, drives its own interact connections, and runs its own `ConnectionStateChange` and map-script callbacks, once, against its own state. Relaying those derived effects as well would apply them twice. Relaying inputs instead would diverge, because Newton, the dropped-tick logic timer, and the PID controllers are not reproducible across machines (`docs/research/interaction-surface.md`, `docs/research/physics-determinism.md`).

The game holds no multiplayer policy. It reports the local player's interaction start and end, the bodies the player is interacting with or has just released or knocked, their contacts with other props, breaks, and when they come to rest. It drives Peer-Driven Entities when told to. It never decides between players.

## Consequences

- A Peer-Driven Entity belongs to its Session, like an Avatar (ADR 0002): the game stops driving it when the Session ends or the map changes, so no prop is left untouchable by a vanished Peer.
- The Peer-driven state is separate from `SetEntityInteractionDisabled`, which map scripts use for story gating. `CanInteractWithEntity` checks both.
- Driving an entity the local player is interacting with ends that interaction in the same update, which is how a lost contention reaches the player.
- A Peer-Driven Entity never breaks from its own contacts. Breaking is applied by Command with the breaking game's final transform and velocity, through the engine's own `Break()`. `DisableBreakable` is not usable for this because it still destroys the prop.
- Map scripts run state callbacks on every machine. That keeps per-player state consistent but lets scripts with randomness or timer phase diverge (`docs/research/script-state.md`).
- The Game Interaction Protocol gains an `interactions` Capability at Version 2. No Version 3 is needed.
