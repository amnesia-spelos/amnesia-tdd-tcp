---
status: accepted
---

# Negotiate Protocol Version 2 as a superset of the legacy protocol

Real-time multiplayer behavior (Avatars, the local Pose State Update) is only available to a Session that opens with a Protocol Version 2 negotiation selecting Capabilities, on the same socket, using space-separated fields with any map path last and fixed-precision "C"-locale numbers; Sessions that do not negotiate keep the legacy protocol byte-for-byte. The legacy colon grammar cannot carry map paths safely, lacks precision and timestamps, and demands a Response per Command; negotiation is also needed because an existing consumer treats any unknown-command warning as the answer to its pending Command.

## Considered Options

- A separate real-time channel (UDP or a second socket) was rejected: on loopback there is no loss to avoid, and it would still be serviced by the same 60 Hz update loop, so it removes no latency while adding lifecycle surface. Measured latency sources were Nagle, a one-tick deferred flush, and tick quantization, fixed directly instead.
- JSON lines were rejected: the game has no JSON parser and the fields are fixed.

## Consequences

The legacy protocol is intended to be retired. Streamnesia depends on it but pins an older build of this repository; when it upgrades, it moves to the negotiated protocol.
