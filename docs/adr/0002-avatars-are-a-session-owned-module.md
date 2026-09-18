---
status: accepted
---

# Avatars are a session-owned engine module, not map entities

An Avatar is owned by a global game module on behalf of its Session, and is built from a mesh (loaded from a Peer-named `.ent` model, mesh and animations only) plus a character body, rather than being spawned into the map as an `.ent` prop and moved by script. Map entities are serialized into saves and resurrected as frozen copies, block enemy line of sight, catch the player's focus ray, and become ragdolls when skeletal without animations; a character body instead gives player collision for free, is skipped by enemy sight and focus, can be teleported cheaply every frame, and is the same mechanism enemies use, which keeps a future mode where a Peer drives an existing enemy's body instead of its AI on the same seam.

## Consequences

The module, not the map, keeps Avatars alive: it destroys their world objects when a map is left or a save is loaded and recreates them in the next map. Scripts cannot see Avatars, and the `.ent` bodies and prop variables of an Avatar model are ignored.
