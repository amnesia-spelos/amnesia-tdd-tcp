---
status: accepted
---

# Put external game interaction behind a deep TCP protocol gateway

External game interaction will pass through one deep TCP protocol gateway that owns protocol meaning, version compatibility, Session policy, and translation through a narrow game adapter. The current colon-delimited protocol will remain behind a legacy adapter, Winsock will remain private transport implementation until a second real adapter exists, and all game-state access will stay serialized on the game update thread; this concentrates growing multiplayer-oriented behavior without spreading wire or transport knowledge through the original game.

## Consequences

The gateway is the primary contract-test seam shared conceptually with Streamnesia. Network work must not block the game update thread, original game integration sites report typed Events rather than wire strings, and future multi-Peer authority, subscriptions, Snapshots, and structured messages extend the gateway rather than the transport or original game modules.
