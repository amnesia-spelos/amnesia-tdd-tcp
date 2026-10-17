---
status: accepted
---

# The game holds the death policy for Shared Enemies

Amends ADR 0005, whose game "holds no multiplayer policy" and "never decides between players". Shared Enemies already crossed that line: an enemy's senses choose between the local player and Avatars (#94), and a driven copy decides a reported attack against the local player (#95), both following amnesia-multiplayer's ADR 0009. For deaths (#96), the policy has to act inside the death sequence, and the only seams are game-side: the `PlayerDead` broadcast and the range check while the player is dead, the death screen's timing, and `LoadCheckPoint`. `LoadCheckPoint` deactivates and resets enemies, and then runs the callback with the count. So the game decides how a death treats enemies, and the Peer decides only what has to be agreed between games.

- **The game decides**, at each death, from what it can see: vanilla without a Session granted `enemies` or without an awake Avatar on the current map; Shared Enemies preserved when the current checkpoint has no callback; a coordinated death when it has one. Only Grunts are Shared Enemies. A coordinated death holds the death screen at most 3 seconds for its agreed count, then keeps its own count.
- **The Peer decides** each Shared Death episode: its order, which death it answers, and the checkpoint count every game passes to its own callback (the Session Host's). The game applies an episode once and never runs a callback it was sent.

## Considered Options

- **The Peer turns the policy on and off per map with a Command.** Rejected. It would add a Command and map-handoff state to the Game Peer, though the game already sees whether another player is on the map: an Avatar is awake there exactly then. ADR 0009's "only while both players are on the map" is the same condition.
- **The Peer drives the whole death**, telling the game when to reset which enemies and when to run the callback. Rejected. Reset ordering lives inside `LoadCheckPoint`, a few lines from the callback. Splitting it over the wire would add round trips inside one function call, and a Peer that disconnected mid-death could leave the game in a half-reset state.
- **No wait for the agreed count.** Rejected. A game that died first would run its callback before the episode arrived, and counts would drift under ordinary latency.

## Consequences

- A Peer that never sends `shareddeath` still gets a working game. Coordinated deaths wait 3 seconds longer and keep their own counts; preserving deaths need no Peer action at all.
- Changing which enemies are shared, or the wait, is a game change, not a protocol change.
- An Avatar left awake on the map after its player has gone would make the game treat deaths as shared. The Game Peer must remove an Avatar, or pose it on the player's new map, when that player leaves.
