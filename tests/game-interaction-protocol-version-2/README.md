# Game Interaction Protocol Version 2 contract

Protocol Version 2 (ADR 0004) is a per-Session superset of the [legacy protocol](../legacy-game-interaction-protocol/README.md) on the same socket. A Session that never negotiates stays on the legacy protocol byte for byte. This document fixes the keyword set, field order, and Response shapes for every Protocol Version 2 message, including the messages whose behavior later tickets add.

`contract.jsonl` holds language-neutral fixtures, one JSON object per line:

- `session`: a fresh Session receives `request_wire` in one write. `expected_wire` is everything it answers after the greeting. The harness game has a map loaded and reports a fixed local Pose: time `123456`, teleport counter `3`, feet at `1.25 -2.5 3.75`, body yaw `90`, camera pitch `-45`, crouching, on map `custom_stories/My Story: Part 2/maps/cellar one.map`. It finds every Avatar model except `entities/missing.ent`.
- `local_pose`: how a local Pose is written as a `localpose` State Update.
- `format_number`, `parse_number`: how numbers are written and which texts are accepted. The harness runs these under a comma-decimal system locale to prove locale independence.
- `fields`: how a line splits into fixed fields and an optional trailing map path.
- `avatar_identifier`: which Avatar Identifiers are valid.

Run the game-side harness without starting the game:

```powershell
.\scripts\test-protocol-version-2.ps1
```

## Negotiation

```text
protocol <version> [<capability>...]
```

- The only supported Protocol Version is `2`. The supported Capabilities are `avatars` and `localpose`.
- Success: `RESPONSE protocol ok 2 [<capability>...]`. The granted Capabilities are the requested ones that the game supports. Each is listed once, in the order `avatars`, `localpose`. Unknown names are ignored, so a Session can request none and still negotiate.
- `RESPONSE protocol unsupported-version`: the version is a positive decimal integer other than `2`. Leading zeros are allowed, so `02` means `2`.
- `RESPONSE protocol invalid`: the version is missing, `0`, or not a decimal integer that fits in 32 bits, or the line has an empty field.
- `RESPONSE protocol already-negotiated`: the Session already negotiated.
- `RESPONSE protocol too-late`: the Session already sent some other Command. Events and the greeting do not count.
- A Session can negotiate only once, and only before its first other Command. After an `unsupported-version` or `invalid` outcome, the Session stays on the legacy protocol and can try again. For example, it can try `protocol 3` and then fall back to `protocol 2`.
- An older game answers `protocol ...` with `WARNING:Unknown command`. A Peer must not send any other Protocol Version 2 message unless negotiation succeeded.

A negotiated Session keeps every legacy Command, Response, and Event unchanged. Unknown Commands are still answered with `WARNING:Unknown command`.

## Line format

- Each line is terminated by LF. A CR before the LF is ignored.
- Fields are separated by exactly one space. A doubled or trailing space creates an empty field, which is invalid.
- If a message has a map or entity path, it is always the last field. The path extends to the end of the line, so it can contain spaces and `:`. The path must not be empty.
- The first field is the keyword. Keywords must match exactly: `avatarcreatex` is unknown.
- **Numbers** (`<x>`, `<y>`, `<z>`, `<yaw>`, `<pitch>`) are written with exactly 4 decimals and a `.` separator, whatever the system locale is. For example: `1.2500`, `-0.0001`, `0.0000`. They are rounded half away from zero, and a value that rounds to zero has no sign. Readers accept `-?digits(.digits)?` with at most 15 digits. Readers reject `,` separators, exponents, `+`, `.5`, `1.`, and `nan`. Angles are in degrees, and positions are in world units.
- **Time** (`<timeMs>`) is an unsigned decimal integer of milliseconds on the sender's monotonic game clock. It fits in 64 bits.
- **Teleport counter** (`<teleportCounter>`) is an unsigned decimal 32-bit integer. It changes whenever the sender's Pose is placed rather than moved.
- **Flags** (`<crouch>`, the collision toggle) are `0` or `1`.
- **Avatar Identifiers** (`<id>`) are 1–32 printable ASCII characters (`!` through `~`), and cannot contain `:`.

## Responses

Every Protocol Version 2 Response has this shape:

```text
RESPONSE <keyword> <outcome> [<field>...]
```

`<outcome>` is a single token: `ok` or one of the failure tokens listed below. Legacy Responses keep their `RESPONSE:<command>:<detail>` form, so a Peer can tell the two apart by the character after `RESPONSE`.

The gateway checks Capabilities before it parses a message. A Protocol Version 2 message whose Capability was not granted is answered with `RESPONSE <keyword> not-granted`, even if the message is malformed. In a Session that never negotiated, these keywords are unknown legacy Commands and get `WARNING:Unknown command`.

## Messages

| Command | Capability | Responses |
| --- | --- | --- |
| `avatarcreate <id> [<entityFile>]` | `avatars` | `ok <id>`, `exists <id>`, `limit <id>`, `model-not-found <id>`, `invalid` |
| `avatarremove <id>` | `avatars` | `ok <id>`, `not-found <id>`, `invalid` |
| `avatarcollision <id> <0\|1>` | `avatars` | `ok <id>`, `not-found <id>`, `invalid` |
| `avatarpose <id> <timeMs> <teleportCounter> <x> <y> <z> <yaw> <pitch> <crouch> <map>` | `avatars` | none on success; `not-found <id>`, `invalid [<id>]` |
| `localpose subscribe <hz>` | `localpose` | `ok subscribe <hz>`, `invalid` |
| `localpose unsubscribe` | `localpose` | `ok unsubscribe`, `invalid` |

- `avatarcreate`: `<entityFile>` is a path field and defaults to `entities/multiplayer/skeleton_spelos/TheSkeletonSpelos.ent`. The checks run in this order: `invalid` means the line is malformed or the Avatar Identifier is invalid, and the Response does not echo the identifier. `exists` means the Session already drives an Avatar with that identifier. `limit` means the Session already drives 16 Avatars. `model-not-found` means the game cannot find the model file or the mesh it names.
- `avatarremove`: `invalid` means the line is malformed or the Avatar Identifier is invalid, and the Response does not echo the identifier.
- `avatarcollision`: `1` turns on collision between the local player and the Avatar, and `0` turns it off. Collision is on for a new Avatar. The setting stays in place across map changes, save loads, and dormancy until the next `avatarcollision` for that Avatar. `invalid` means the line is malformed, the Avatar Identifier is invalid, or the flag is not `0` or `1`, and the Response does not echo the identifier.
- `avatarpose`: `<map>` is the map path of the sender's Pose, and `<x> <y> <z>` is the feet position. A successful `avatarpose` is not answered. `invalid` takes precedence over `not-found`. A failure is reported once per Avatar per failure streak, and the next success for that Avatar resets its streak. `invalid` echoes `<id>` only when that field is a valid Avatar Identifier. All lines without a valid identifier share one streak.
- `localpose subscribe`: `<hz>` is a decimal integer. The rate is clamped to 1–60, even when `<hz>` does not fit in 32 bits, and the Response reports the clamped rate. Subscribing again changes the rate and restarts the subscription, so the current Pose follows at once. `localpose unsubscribe` succeeds even if the Session is not subscribed. `invalid` means the line is malformed, including a negative or fractional `<hz>`. The subscription ends when the Session ends.

The `localpose` State Update is sent to a subscribed Session:

```text
STATE localpose <timeMs> <teleportCounter> <x> <y> <z> <yaw> <pitch> <crouch> <map>
```

- It reports the local player's feet position, body yaw, camera pitch, and crouch flag in the current map. `<timeMs>` is the game clock, which only moves forward while the game runs. `<teleportCounter>` changes whenever the player is placed rather than moved: `TeleportPlayer`, `SetPlayerPos`, start position placement on map entry or at a checkpoint, and save load.
- The first State Update is the current Pose and follows the subscribing Response. After that, at most one State Update is sent per `1000 / <hz>` milliseconds of game time. The schedule holds that average rate even when game updates do not land on it exactly.
- State Updates are sent only while a map is loaded. None are sent in the main menu or while a map loads. While the game is paused or the player is in the inventory, journal, or another menu, a State Update is sent only when the Pose changed. Time does not count as a change.
- A newer Pose replaces any undelivered older one instead of queueing behind it. A Peer that reads slowly receives the newest Pose once it catches up, and is never disconnected because of State Updates.

## Avatars

- An Avatar belongs to the Session that created it, not to a map. It is removed only by `avatarremove` or when the Session ends. Loading a save or changing maps keeps it.
- The game shows an Avatar as the mesh of its model, turned by body yaw only. The model's bodies and prop variables are ignored. Pitch and crouch are stored but not shown yet.
- An Avatar is dormant, which means invisible and not collidable, while its latest Pose names a map other than the local current map, or while no map is loaded. A new Avatar is dormant until its first Pose for the local map. A dormant Avatar wakes on the first Pose for the local map.
- The local player collides with an awake Avatar whose collision is on, so the player bumps into it rather than walking through it. Turning collision off leaves the Avatar visible and posed.
- An Avatar that overlaps the local player by more than 5 cm never traps the player or throws them out. This happens when it wakes, snaps, or walks onto the player, or when its collision is turned on around them. The Avatar becomes passable until it and the player are at least 5 cm apart, and then it collides again. A smaller overlap, such as the player pressing against the Avatar, keeps it solid. So a moving Avatar at most nudges a player standing in its way, and then passes through them.
- Enemies pass through Avatars, so an Avatar never blocks or traps an enemy.
- Avatars are not map entities. They are not saved, and scripts cannot reach them. Enemies do not perceive them and see through them, and the player's focus ray passes through them.

## Transport

- `TCP_NODELAY` is set on the Peer socket.
- Responses are flushed in the same game update that processed their Commands.
- An inbound line longer than 65,536 bytes, not counting its terminator, disconnects the Peer. The game logs `Peer exceeded the inbound line length limit`. Commands on earlier lines in the same read are processed, and their Responses are sent before the disconnect if the socket accepts them immediately.
