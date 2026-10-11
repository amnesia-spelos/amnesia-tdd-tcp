---
status: accepted
---

# Legacy Commands without a pinned consumer may change

ADR 0004 keeps the legacy protocol byte-for-byte for Sessions that do not negotiate a Protocol Version, because Streamnesia depends on it from a pinned older build. That reason covers only the Commands that build already had. A legacy-grammar Command added after it, such as `chat:`, has no pinned consumer: its only Peer is `amnesia-multiplayer`, which negotiates Protocol Version 2 and ships with a matching build of this repository. Such a Command may change incompatibly or move into Protocol Version 2, with `amnesia-multiplayer` migrating in step.

## Considered Options

- Keeping every legacy-grammar Command frozen was rejected. It protects no real consumer, and it would push new behavior such as Chat Colors into the colon grammar the protocol is moving away from.
- Extending `chat:` in place with color fields was rejected in favor of moving chat to Protocol Version 2. The migration costs `amnesia-multiplayer` the same either way, and only the move helps retire the legacy protocol.

## Consequences

- Before changing a legacy Command, check whether Streamnesia's pinned build already had it. If it did, the byte-for-byte rule still applies.
- Chat is the first legacy Command to move: the legacy `chat:` Command is replaced by the Protocol Version 2 `chatentry` Command, so a Session that does not negotiate can no longer display Chat Entries.
