---
status: accepted
---

# The game interpolates Avatar Poses from sender-timestamped samples

Peers send Avatar Poses at network rate stamped with the sending game's clock, and the receiving game buffers them and renders each Avatar a fixed delay in the past on its own update thread, instead of a Peer issuing a position change every frame. Smoothness then depends neither on loopback socket timing nor on the external application's scheduling, the protocol talks in Poses rather than frame-by-frame moves, and relay jitter is not baked into motion as it would be with arrival-time stamping.

## Consequences

Poses carry a timestamp, the sender's map, and a teleport counter so the game can snap rather than glide across discontinuities; when samples stop arriving the Avatar holds its last Pose rather than extrapolating.
