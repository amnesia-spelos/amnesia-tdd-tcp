# Game Interaction Protocol Version 2 contract

Protocol Version 2 (ADR 0004) is a per-Session superset of the [legacy protocol](../legacy-game-interaction-protocol/README.md) on the same socket. A Session that never negotiates stays on the legacy protocol byte for byte. This document fixes the keyword set, field order, and Response shapes for every Protocol Version 2 message, including the messages whose behavior later tickets add.

`contract.jsonl` holds language-neutral fixtures, one JSON object per line:

- `session`: a fresh Session receives `request_wire` in one write. `expected_wire` is everything it answers after the greeting. The harness game has a map loaded and reports a fixed local Pose: time `123456`, teleport counter `3`, feet at `1.25 -2.5 3.75`, body yaw `90`, camera pitch `-45`, crouching, with the lantern raised, on map `custom_stories/My Story: Part 2/maps/cellar one.map`. It finds every Avatar model except `entities/missing.ent`. Its current map is `maps/main/level01.map`. It has no entity `404`, entity `405` is not holdable, and it can drive every other entity. Every body of an entity it drives exists except body `99`. Its report holds body `3` of entity `12` and body `0` of entity `-7` at game time `123456` on `maps/main/level01.map`.
- `local_pose`: how a local Pose is written as a `localpose` State Update.
- `avatar_pose`: which `avatarpose` lines are well formed, and the crouch flag, lantern flag, and map path read from them.
- `format_number`, `parse_number`: how numbers are written and which texts are accepted. The harness runs these under a comma-decimal system locale to prove locale independence.
- `fields`: how a line splits into fixed fields and an optional trailing map path.
- `avatar_identifier`: which Avatar Identifiers are valid.
- `entity_identifier`: which Entity and Body Identifiers are valid, and the value read from them.
- `entity_command`: which Peer-Driven Entity Command lines are well formed, and the Command written back canonically, which shows every field read in order.
- `reported_bodies`: how a report is written as a `reportedbodies` State Update.
- `interaction_event`: how each `interactions` Event is written.

Run the game-side harness without starting the game:

```powershell
.\scripts\test-protocol-version-2.ps1
```

## Negotiation

```text
protocol <version> [<capability>...]
```

- The only supported Protocol Version is `2`. The supported Capabilities are `avatars`, `localpose`, and `interactions`.
- Success: `RESPONSE protocol ok 2 [<capability>...]`. The granted Capabilities are the requested ones that the game supports. Each is listed once, in the order `avatars`, `localpose`, `interactions`. Unknown names are ignored, so a Session can request none and still negotiate.
- `RESPONSE protocol unsupported-version`: the version is a positive decimal integer other than `2`. Leading zeros are allowed, so `02` means `2`.
- `RESPONSE protocol invalid`: the version is missing, `0`, or not a decimal integer that fits in 32 bits, or the line has an empty field.
- `RESPONSE protocol already-negotiated`: the Session already negotiated.
- `RESPONSE protocol too-late`: the Session already sent some other Command. Events and the greeting do not count.
- A Session can negotiate only once, and only before its first other Command. After an `unsupported-version` or `invalid` outcome, the Session stays on the legacy protocol and can try again. For example, it can try `protocol 3` and then fall back to `protocol 2`.
- An older game answers `protocol ...` with `WARNING:Unknown command`. A Peer must not send any other Protocol Version 2 message unless negotiation succeeded.
- A game older than a Capability ignores its name like any unknown one, so the granted list tells a Peer whether the game supports it. A game without `interactions` still grants `avatars` and `localpose`.

A negotiated Session keeps every legacy Command, Response, and Event unchanged. Unknown Commands are still answered with `WARNING:Unknown command`.

## Line format

- Each line is terminated by LF. A CR before the LF is ignored.
- Fields are separated by exactly one space. A doubled or trailing space creates an empty field, which is invalid.
- If a message has a map or entity path, it is always the last field. The path extends to the end of the line, so it can contain spaces and `:`. The path must not be empty.
- The first field is the keyword. Keywords must match exactly: `avatarcreatex` is unknown.
- **Numbers** (`<x>`, `<y>`, `<z>`, `<yaw>`, `<pitch>`) are written with exactly 4 decimals and a `.` separator, whatever the system locale is. For example: `1.2500`, `-0.0001`, `0.0000`. They are rounded half away from zero, and a value that rounds to zero has no sign. Readers accept `-?digits(.digits)?` with at most 15 digits. Readers reject `,` separators, exponents, `+`, `.5`, `1.`, and `nan`. Angles are in degrees, and positions are in world units.
- **Time** (`<timeMs>`) is an unsigned decimal integer of milliseconds on the sender's monotonic game clock. It fits in 64 bits.
- **Teleport counter** (`<teleportCounter>`) is an unsigned decimal 32-bit integer. It changes whenever the sender's Pose is placed rather than moved.
- **Flags** (`<crouch>`, `<lantern>`, the collision toggle) are `0` or `1`.
- **Avatar Identifiers** (`<id>`) are 1–32 printable ASCII characters (`!` through `~`), and cannot contain `:`.
- **Entity Identifiers** (`<entityId>`) name a map-placed entity by the integer `ID` it has in its map file. **Body Identifiers** (`<bodyId>`) name one of its bodies by the integer `ID` the body has in its entity file. Entities and bodies are never named by name. Both are signed decimal 32-bit integers, `-?digits` from `-2147483648` to `2147483647`. Readers accept leading zeros and reject `+`. Writers write no leading zeros and write zero without a sign.
- **Body states** (`<state>`) are 13 numbers: `<x> <y> <z> <qx> <qy> <qz> <qw> <vx> <vy> <vz> <wx> <wy> <wz>`. `<x> <y> <z>` is the translation of the body's world matrix. `<qx> <qy> <qz> <qw>` is its world rotation as a unit quaternion. `<vx> <vy> <vz>` is its linear velocity in world units per second, and `<wx> <wy> <wz>` is its angular velocity in degrees per second, both along the world axes. Every number's magnitude is at most `1000000000`, and the quaternion's length is within `0.01` of 1. A reader rejects any other state as malformed. A writer never writes one: it writes a number beyond the bound as the bound with its sign, a non-finite number as `0.0000`, and the quaternion normalized, or as `0.0000 0.0000 0.0000 1.0000` when it has a non-finite component or no length.
- **Body entries** (`<entry>`) are `<entityId> <bodyId> <state>`, 15 fields. A list of entries follows `<count>`, the number of entries, a decimal integer from `0` to `32`. A line whose count is above 32, or that ends before its counted entries and map, is malformed. Everything after the counted entries is the map path, so a Peer that sends more entries than it counts names a map that is not current.

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
| `avatarpose <id> <timeMs> <teleportCounter> <x> <y> <z> <yaw> <pitch> <crouch> <lantern> <map>` | `avatars` | none on success; `not-found <id>`, `invalid [<id>]` |
| `localpose subscribe <hz>` | `localpose` | `ok subscribe <hz>`, `invalid` |
| `localpose unsubscribe` | `localpose` | `ok unsubscribe`, `invalid` |
| `reportedbodies subscribe <hz>` | `interactions` | `ok subscribe <hz>`, `invalid` |
| `reportedbodies unsubscribe` | `interactions` | `ok unsubscribe`, `invalid` |
| `entitydrive <entityId> <map>` | `interactions` | `ok <entityId>`, `wrong-map <entityId>`, `not-found <entityId>`, `not-holdable <entityId>`, `invalid` |
| `entitybodies <timeMs> <count> [<entry>...] <map>` | `interactions` | none on success; `wrong-map`, `not-found <entityId>`, `invalid` |
| `entityinteracting <entityId> <0\|1> <map>` | `interactions` | `ok <entityId>`, `wrong-map <entityId>`, `not-found <entityId>`, `invalid` |
| `entitybreak <entityId> <state> <map>` | `interactions` | `ok <entityId>`, `wrong-map <entityId>`, `not-found <entityId>`, `invalid` |
| `entityrelease <entityId> <map>` | `interactions` | `ok <entityId>`, `wrong-map <entityId>`, `not-found <entityId>`, `invalid` |

- `avatarcreate`: `<entityFile>` is a path field and defaults to `entities/multiplayer/skeleton_spelos/TheSkeletonSpelos.ent`. The checks run in this order: `invalid` means the line is malformed or the Avatar Identifier is invalid, and the Response does not echo the identifier. `exists` means the Session already drives an Avatar with that identifier. `limit` means the Session already drives 16 Avatars. `model-not-found` means the game cannot find the model file or the mesh it names.
- `avatarremove`: `invalid` means the line is malformed or the Avatar Identifier is invalid, and the Response does not echo the identifier.
- `avatarcollision`: `1` turns on collision between the local player and the Avatar, and `0` turns it off. Collision is on for a new Avatar. The setting stays in place across map changes, save loads, and dormancy until the next `avatarcollision` for that Avatar. `invalid` means the line is malformed, the Avatar Identifier is invalid, or the flag is not `0` or `1`, and the Response does not echo the identifier.
- `avatarpose`: `<map>` is the map path of the sender's Pose, `<x> <y> <z>` is the feet position, and `<lantern>` is `1` while the sender's lantern is raised. A line without `<lantern>`, or with a flag other than `0` or `1`, is malformed. A successful `avatarpose` is not answered. `invalid` takes precedence over `not-found`. A failure is reported once per Avatar per failure streak, and the next success for that Avatar resets its streak. `invalid` echoes `<id>` only when that field is a valid Avatar Identifier. All lines without a valid identifier share one streak.
- `localpose subscribe`: `<hz>` is a decimal integer. The rate is clamped to 1–60, even when `<hz>` does not fit in 32 bits, and the Response reports the clamped rate. Subscribing again changes the rate and restarts the subscription, so the current Pose follows at once. `localpose unsubscribe` succeeds even if the Session is not subscribed. `invalid` means the line is malformed, including a negative or fractional `<hz>`. The subscription ends when the Session ends.

The `localpose` State Update is sent to a subscribed Session:

```text
STATE localpose <timeMs> <teleportCounter> <x> <y> <z> <yaw> <pitch> <crouch> <lantern> <map>
```

- It reports the local player's feet position, body yaw, camera pitch, crouch flag, and lantern flag in the current map. `<lantern>` is `1` exactly while the lantern is raised. It turns `0` as soon as the lantern is lowered, runs out of oil, or is disabled. `<timeMs>` is the game clock, which only moves forward while the game runs. `<teleportCounter>` changes whenever the player is placed rather than moved: `TeleportPlayer`, `SetPlayerPos`, start position placement on map entry or at a checkpoint, and save load.
- The first State Update is the current Pose and follows the subscribing Response. After that, at most one State Update is sent per `1000 / <hz>` milliseconds of game time. The schedule holds that average rate even when game updates do not land on it exactly.
- State Updates are sent only while a map is loaded. None are sent in the main menu or while a map loads. While the game is paused or the player is in the inventory, journal, or another menu, a State Update is sent only when the Pose changed. Time does not count as a change.
- A newer Pose replaces any undelivered older one instead of queueing behind it. A Peer that reads slowly receives the newest Pose once it catches up, and is never disconnected because of State Updates.

## Interactions

The `interactions` Capability shares what the local player's interactions do to map-placed entities, and lets a Peer drive such an entity in the local game (ADR 0005). The game holds no multiplayer policy. It reports what the local player does and drives what it is told to drive, and never decides between players.

A **holdable** entity is placed in its map file rather than created at runtime by a script or a break, and is an `Object` (Grab, Push, or Slide), `SwingDoor`, `Wheel`, `Lever`, or `MultiSlider`. Only holdable entities are reported or driven.

### The report

The game keeps a report of the holdable entities whose motion the local player decides:

- the entity the local player is interacting with, from its `interactionstart`;
- entities the player released, until they come to rest;
- free entities that a reported body or the local player's body touched, from their `reportcontact`. Static bodies, character bodies, Avatars, runtime-created entities, and Peer-Driven Entities never enter by contact.

An entity leaves the report when it settles (`reportsettled`), breaks (`reportbroke`), or is driven by a Peer, and every entity leaves it when the map changes or a save loads. Only settling and breaking send an Event. The report holds at most 32 bodies. An entity whose bodies do not fit is not reported and stays under local physics: it gets no `reportcontact`, or its `interactionstart` is still sent but its bodies stay out of State Updates.

A Session granted `interactions` receives these Events:

```text
EVENT interactionstart <entityId> <bodyId> <map>
EVENT interactionend <entityId> <bodyId> <ending> <map>
EVENT reportcontact <entityId> <map>
EVENT reportsettled <entityId> <map>
EVENT reportbroke <entityId> <state> <map>
```

- `interactionstart`: the local player started to grab, push, slide, swing, or spin body `<bodyId>` of a holdable entity. The entity enters the report.
- `interactionend`: that interaction ended. `<ending>` is `released` (the player let go, or a Peer drove the entity), `thrown`, `too-far` (the body moved out of reach and was dropped), or `destroyed` (the entity was destroyed or broke while held). After `released`, `thrown`, or `too-far` the entity stays in the report until it settles. After `destroyed` it leaves the report without another Event.
- `reportcontact`: a free holdable entity entered the report by contact.
- `reportsettled`: a reported entity came to rest, because its bodies went to sleep or it reached the Settling cap of about 3 seconds, and left the report.
- `reportbroke`: a reported entity broke and left the report. `<state>` is the final state of the body whose transform the entity follows.

A legacy Event keeps its `EVENT:<name>:<data>` form, so a Peer can tell the two apart by the character after `EVENT`. A Session without `interactions` receives none of these Events.

The `reportedbodies` State Update is sent to a subscribed Session:

```text
STATE reportedbodies <timeMs> <count> <entry>... <map>
```

- It carries every reported body, at `<timeMs>` on the same game clock as `localpose`, and the current map. `<count>` is 1–32.
- `reportedbodies subscribe <hz>` and `reportedbodies unsubscribe` behave like their `localpose` forms. The rate is clamped to 1–60 and reported, subscribing again restarts the subscription, and the subscription ends with the Session. The schedule, the rules while no map is loaded or play is suspended, and the replacement of an undelivered older State Update by a newer one are those of `localpose`.
- A State Update is sent only while the report holds a body. The first one follows the subscribing Response if the report holds a body then. No State Update says that the report emptied; the `reportsettled` and `reportbroke` Events do.

### Peer-Driven Entities

A Peer-Driven Entity is a map-placed entity whose motion a Peer supplies in place of local physics. Every `entity...` Command is State-Changing, so it needs the Authority Grant. The gateway serves one Peer at a time, and that Peer holds the grant, as it does for `exec:` and `avatarcreate`.

- Every `entity...` Command ends with the map path the Peer means. If that is not the local current map, or no map is loaded, the Command changes nothing and is answered `wrong-map`. So a late Command never reaches an entity with the same identifier in another map.
- The checks run in this order: `invalid`, `wrong-map`, `not-found`, `not-holdable`. `invalid` means the line is malformed, and the Response does not echo the identifier.
- `entitydrive`: makes the entity a Peer-Driven Entity of this Session. `not-found` means the current map has no entity with that identifier. `not-holdable` means the entity is not holdable. Driving an entity this Session already drives succeeds and changes nothing. If the local player is interacting with the entity, that interaction ends first, in the same update, with `interactionend ... released`, and the entity leaves the report without another Event. The local player cannot interact with a Peer-Driven Entity, and it never breaks from its own contacts.
- `entitybodies`: a batch of body samples at `<timeMs>` on the sender's clock, as in `avatarpose`. The game buffers them and plays them back with the same interpolation delay as Avatar Poses, setting each body's transform and velocities every tick. A success is not answered. `not-found <entityId>` names the first entry whose entity this Session does not drive or that has no body with that identifier, and the other entries are still applied. A failure is reported once per failure streak, and the next success resets the streak. All `entitybodies` lines share one streak.
- `entityinteracting`: `1` marks the Peer-Driven Entity as being interacted with, and `0` clears the mark, so that door auto-close, lever auto-move, and interaction-only connections behave as they do for the local player. `not-found` means this Session does not drive that entity.
- `entitybreak`: snaps the Peer-Driven Entity to `<state>` and breaks it through the game's own break path, with its debris, connected props, contained item, sound, particles, and break callback. The entity is then no longer driven. `not-found` means this Session does not drive that entity.
- `entityrelease`: ends driving and returns the entity to local physics from its current state. `not-found` means this Session does not drive that entity.
- A Peer-Driven Entity belongs to its Session, like an Avatar. The game releases every one when the Session ends, when the map changes, and when a save loads.

## Avatars

- An Avatar belongs to the Session that created it, not to a map. It is removed only by `avatarremove` or when the Session ends. Loading a save or changing maps keeps it.
- The game shows an Avatar as the mesh of its model, turned by body yaw. The model's bodies and prop variables are ignored. Camera pitch turns the neck and head bones. The Avatar plays `idle`, `walk`, `crouch_idle`, or `crouch_walk`, chosen from its rendered horizontal speed and crouch flag and cross-faded on a change; a walking clip's playback speed scales with that speed. Its character body switches to a smaller crouched size on a crouch change, and its mesh offset and collision follow the active size.
- While its Pose has the lantern raised, an awake Avatar gives off a light like the local lantern: the same gobo, colour, and radius, without shadows. It sits at standing eye height on the Avatar's facing side, or at the crouched eye height while the Avatar crouches, and ignores pitch. It fades in over about 1 s when the lantern is raised and out over about 0.5 s when it is lowered. It does not dim with oil. A Dormant Avatar gives off no light. The Peer's flag decides the light even where the local map disables the local lantern.
- An Avatar is dormant, which means invisible and not collidable, while its latest Pose names a map other than the local current map, or while no map is loaded. A new Avatar is dormant until its first Pose for the local map. A dormant Avatar wakes on the first Pose for the local map.
- The local player collides with an awake Avatar whose collision is on, so the player bumps into it rather than walking through it. Turning collision off leaves the Avatar visible and posed.
- An Avatar that overlaps the local player by more than 5 cm never traps the player or throws them out. This happens when it wakes, snaps, or walks onto the player, or when its collision is turned on around them. The Avatar becomes passable until it and the player are at least 5 cm apart, and then it collides again. A smaller overlap, such as the player pressing against the Avatar, keeps it solid. So a moving Avatar at most nudges a player standing in its way, and then passes through them.
- Enemies pass through Avatars, so an Avatar never blocks or traps an enemy.
- Avatars are not map entities. They are not saved, and scripts cannot reach them. Enemies do not perceive them and see through them, and the player's focus ray passes through them.

## Transport

- `TCP_NODELAY` is set on the Peer socket.
- Responses are flushed in the same game update that processed their Commands.
- An inbound line longer than 65,536 bytes, not counting its terminator, disconnects the Peer. The game logs `Peer exceeded the inbound line length limit`. Commands on earlier lines in the same read are processed, and their Responses are sent before the disconnect if the socket accepts them immediately.
