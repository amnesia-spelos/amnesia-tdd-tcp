# Game Interaction Protocol Version 2 contract

Protocol Version 2 (ADR 0004) is a per-Session superset of the [legacy protocol](../legacy-game-interaction-protocol/README.md) on the same socket. A Session that never negotiates stays on the legacy protocol byte for byte. This document fixes the keyword set, field order, and Response shapes for every Protocol Version 2 message, including the messages whose behavior later tickets add.

`contract.jsonl` holds language-neutral fixtures, one JSON object per line:

- `session`: a fresh Session receives `request_wire` in one write. `expected_wire` is everything it answers after the greeting. The harness game has a map loaded and reports a fixed local Pose: time `123456`, teleport counter `3`, feet at `1.25 -2.5 3.75`, body yaw `90`, camera pitch `-45`, crouching, with the lantern raised, on map `custom_stories/My Story: Part 2/maps/cellar one.map`. It finds every Avatar model except `entities/missing.ent`. Its current map is `maps/main/level01.map`. It has no entity `404`, entity `405` is not holdable, and it can drive every other entity. Every body of an entity it drives exists except body `99`. Its report holds body `3` of entity `12` and body `0` of entity `-7` at game time `123456` on `maps/main/level01.map`. Its Map Script has a registration for every Shared Callback except those naming entity `404`.
- `local_pose`: how a local Pose is written as a `localpose` State Update.
- `avatar_pose`: which `avatarpose` lines are well formed, and the crouch flag, lantern flag, and map path read from them.
- `format_number`, `parse_number`: how numbers are written and which texts are accepted. The harness runs these under a comma-decimal system locale to prove locale independence.
- `fields`: how a line splits into fixed fields and an optional trailing map path.
- `avatar_identifier`: which Avatar Identifiers are valid.
- `entity_identifier`: which Entity and Body Identifiers are valid, and the value read from them.
- `entity_command`: which Peer-Driven Entity Command lines are well formed, and the Command written back canonically, which shows every field read in order.
- `reported_bodies`: how a report is written as a `reportedbodies` State Update.
- `interaction_event`: how each `interactions` Event, including `mapentered`, is written.
- `shared_callback_command`: which `sharedcallback` Command lines are well formed, and the Shared Callback written back canonically.
- `shared_callback_event`: how a `sharedcallback` Event is written.

Run the game-side harness without starting the game:

```powershell
.\scripts\test-protocol-version-2.ps1
```

## Negotiation

```text
protocol <version> [<capability>...]
```

- The only supported Protocol Version is `2`. The supported Capabilities are `avatars`, `localpose`, `interactions`, `sharedcallbacks`, `playerplace`, `enemies`, and `lean`.
- Success: `RESPONSE protocol ok 2 [<capability>...]`. The granted Capabilities are the requested ones that the game supports. Each is listed once, in the order `avatars`, `localpose`, `interactions`, `sharedcallbacks`, `playerplace`, `enemies`, `lean`. Unknown names are ignored, so a Session can request none and still negotiate.
- `RESPONSE protocol unsupported-version`: the version is a positive decimal integer other than `2`. Leading zeros are allowed, so `02` means `2`.
- `RESPONSE protocol invalid`: the version is missing, `0`, or not a decimal integer that fits in 32 bits, or the line has an empty field.
- `RESPONSE protocol already-negotiated`: the Session already negotiated.
- `RESPONSE protocol too-late`: the Session already sent some other Command. Events and the greeting do not count.
- A Session can negotiate only once, and only before its first other Command. After an `unsupported-version` or `invalid` outcome, the Session stays on the legacy protocol and can try again. For example, it can try `protocol 3` and then fall back to `protocol 2`.
- An older game answers `protocol ...` with `WARNING:Unknown command`. A Peer must not send any other Protocol Version 2 message unless negotiation succeeded.
- A game older than a Capability ignores its name like any unknown one, so the granted list tells a Peer whether the game supports it. A game without `interactions` still grants `avatars` and `localpose`.

A negotiated Session keeps every legacy Command, Response, and Event of the [legacy contract](../legacy-game-interaction-protocol/README.md) unchanged. Unknown Commands are still answered with `WARNING:Unknown command`.

## Line format

- Each line is terminated by LF. A CR before the LF is ignored.
- Fields are separated by exactly one space. A doubled or trailing space creates an empty field, which is invalid.
- If a message has a map or entity path, it is always the last field. The path extends to the end of the line, so it can contain spaces and `:`. The path must not be empty.
- The first field is the keyword. Keywords must match exactly: `avatarcreatex` is unknown.
- **Numbers** (`<x>`, `<y>`, `<z>`, `<yaw>`, `<pitch>`) are written with exactly 4 decimals and a `.` separator, whatever the system locale is. For example: `1.2500`, `-0.0001`, `0.0000`. They are rounded half away from zero, and a value that rounds to zero has no sign. Readers accept `-?digits(.digits)?` with at most 15 digits. Readers reject `,` separators, exponents, `+`, `.5`, `1.`, and `nan`. Angles are in degrees, and positions are in world units.
- **Map paths** (`<map>`) are the map file the game loaded, relative to the game's install folder and with `/` separators, for example `custom_stories/mystory/maps/map1.map`. They are the same on every installation, however a level door or script named the map. A map loaded from outside the install folder, or shipped only compressed, is named by its map folder and file name. Legacy `getmap` and `EVENT:MapChanged` carry the same value.
- **Time** (`<timeMs>`) is an unsigned decimal integer of milliseconds on the sender's monotonic game clock. It fits in 64 bits.
- **Teleport counter** (`<teleportCounter>`) is an unsigned decimal 32-bit integer. It changes whenever the sender's Pose is placed rather than moved.
- **Flags** (`<crouch>`, `<lantern>`, the collision toggle) are `0` or `1`.
- **Lean** (`<lean>`) is a number from `-1` (fully left) to `1` (fully right), written like any other number. A reader rejects a lean outside that range as malformed.
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
| `chatentry <authorColor> <messageColor> <Author>:<message>` | none | `ok`, `invalid`, `invalid-author`, `invalid-message`, `unavailable` |
| `avatarcreate <id> [<entityFile>]` | `avatars` | `ok <id>`, `exists <id>`, `limit <id>`, `model-not-found <id>`, `invalid` |
| `avatarremove <id>` | `avatars` | `ok <id>`, `not-found <id>`, `invalid` |
| `avatarcollision <id> <0\|1>` | `avatars` | `ok <id>`, `not-found <id>`, `invalid` |
| `avatarenemysight <id> <0\|1>` | `avatars` | `ok <id>`, `not-found <id>`, `invalid` |
| `avatarpose <id> <timeMs> <teleportCounter> <x> <y> <z> <yaw> <pitch> <crouch> <lantern> [<lean>] <map>` | `avatars` | none on success; `not-found <id>`, `invalid [<id>]` |
| `localpose subscribe <hz>` | `localpose` | `ok subscribe <hz>`, `invalid` |
| `localpose unsubscribe` | `localpose` | `ok unsubscribe`, `invalid` |
| `reportedbodies subscribe <hz>` | `interactions` | `ok subscribe <hz>`, `invalid` |
| `reportedbodies unsubscribe` | `interactions` | `ok unsubscribe`, `invalid` |
| `entitydrive <entityId> <map>` | `interactions` | `ok <entityId>`, `wrong-map <entityId>`, `not-found <entityId>`, `not-holdable <entityId>`, `invalid` |
| `entitybodies <timeMs> <count> [<entry>...] <map>` | `interactions` | none on success; `wrong-map`, `not-found <entityId>`, `invalid` |
| `entityinteracting <entityId> <0\|1> <map>` | `interactions` | `ok <entityId>`, `wrong-map <entityId>`, `not-found <entityId>`, `invalid` |
| `entitybreak <entityId> <state> <map>` | `interactions` | `ok <entityId>`, `wrong-map <entityId>`, `not-found <entityId>`, `invalid` |
| `entityrelease <entityId> <map>` | `interactions` | `ok <entityId>`, `wrong-map <entityId>`, `not-found <entityId>`, `invalid` |
| `sharedcallback <kind> <field>... <map>` | `sharedcallbacks` | `ok`, `not-found`, `wrong-map`, `invalid` |
| `playerplace <x> <y> <z> <yaw> <crouch> <map>` | `playerplace` | `ok`, `wrong-map`, `unavailable`, `invalid` |

- `chatentry`: see [Chat](#chat).
- `sharedcallback`: see [Shared Callbacks](#shared-callbacks).
- `playerplace`: see [Placing the local player](#placing-the-local-player).
- `avatarcreate`: `<entityFile>` is a path field and defaults to `entities/multiplayer/skeleton_spelos/TheSkeletonSpelos.ent`. The checks run in this order: `invalid` means the line is malformed or the Avatar Identifier is invalid, and the Response does not echo the identifier. `exists` means the Session already drives an Avatar with that identifier. `limit` means the Session already drives 16 Avatars. `model-not-found` means the game cannot find the model file or the mesh it names.
- `avatarremove`: `invalid` means the line is malformed or the Avatar Identifier is invalid, and the Response does not echo the identifier.
- `avatarcollision`: `1` turns on collision between the local player and the Avatar, and `0` turns it off. Collision is on for a new Avatar. The setting stays in place across map changes, save loads, and dormancy until the next `avatarcollision` for that Avatar. `invalid` means the line is malformed, the Avatar Identifier is invalid, or the flag is not `0` or `1`, and the Response does not echo the identifier.
- `avatarenemysight`: opts this Avatar into enemy perception (`1`) or out (`0`). Off for each newly created Avatar. The setting belongs to the Session's Avatar and survives map changes and dormancy; only awake Avatars on the current map can be perceived. Removing the Avatar or ending the Session removes the setting. Validation and Responses match `avatarcollision`. Enemies use the rendered Pose's crouch and lantern, map lighting, motion, FOV and line of sight. Each enemy keeps its current player while perceived, then selects the closest fully perceived player. Grunt/Brute sight takes three consecutive checks (close-range detection is immediate); Kaernk uses its immediate feet-height detection. Avatars remain silent and cannot be hit. `ShowEnemyPlayerPosition` and `AlertEnemyOfPlayerPresence` reveal the player who triggered the callback, which may be an Avatar (see [Enemy-triggered callbacks](#enemy-triggered-callbacks)). The simulating Peer should enable this only while sharing a map; a Peer driving enemy copies need not enable it.
- `avatarpose`: `<map>` is the map path of the sender's Pose, `<x> <y> <z>` is the feet position, and `<lantern>` is `1` while the sender's lantern is raised. A line without `<lantern>`, or with a flag other than `0` or `1`, is malformed. `<lean>` is present exactly when the Session was granted `lean` (see [Lean](#lean)). A successful `avatarpose` is not answered. `invalid` takes precedence over `not-found`. A failure is reported once per Avatar per failure streak, and the next success for that Avatar resets its streak. `invalid` echoes `<id>` only when that field is a valid Avatar Identifier. All lines without a valid identifier share one streak.
- `localpose subscribe`: `<hz>` is a decimal integer. The rate is clamped to 1–60, even when `<hz>` does not fit in 32 bits, and the Response reports the clamped rate. Subscribing again changes the rate and restarts the subscription, so the current Pose follows at once. `localpose unsubscribe` succeeds even if the Session is not subscribed. `invalid` means the line is malformed, including a negative or fractional `<hz>`. The subscription ends when the Session ends.

The `localpose` State Update is sent to a subscribed Session:

```text
STATE localpose <timeMs> <teleportCounter> <x> <y> <z> <yaw> <pitch> <crouch> <lantern> [<lean>] <map>
```

- It reports the local player's feet position, body yaw, camera pitch, crouch flag, and lantern flag in the current map, and in a Session granted `lean`, its lean. `<lantern>` is `1` exactly while the lantern is raised. It turns `0` as soon as the lantern is lowered, runs out of oil, or is disabled. `<timeMs>` is the game clock, which only moves forward while the game runs. `<teleportCounter>` changes whenever the player is placed rather than moved: `TeleportPlayer`, `SetPlayerPos`, `playerplace`, start position placement on map entry or at a checkpoint, and save load.
- The first State Update is the current Pose and follows the subscribing Response. After that, at most one State Update is sent per `1000 / <hz>` milliseconds of game time. The schedule holds that average rate even when game updates do not land on it exactly.
- State Updates are sent only while a map is loaded. None are sent in the main menu or while a map loads. While the game is paused or the player is in the inventory, journal, or another menu, a State Update is sent only when the Pose changed. Time does not count as a change.
- A newer Pose replaces any undelivered older one instead of queueing behind it. A Peer that reads slowly receives the newest Pose once it catches up, and is never disconnected because of State Updates.

## Lean

The `lean` Capability adds `<lean>` after `<lantern>` in both the `localpose` State Update and the `avatarpose` Command. A Session without it keeps both lines exactly as they were, and an Avatar it poses stands upright.

- `localpose` reports how far the local camera leans now, from `-1` (fully left) to `1` (fully right). The collision check cuts it short, so it is only as far as the player can actually lean. It fades back to `0` with the camera when the lean is released.
- `avatarpose` reads `<lean>` and tilts the Avatar's model at the hips and chest by up to 25 degrees either way. Its feet stay planted, and its body and lantern light stay upright. A line without `<lean>`, or with one outside `-1` to `1`, is malformed.
- A game older than `lean` ignores the name and does not grant it. That game would read `<lean>` as the start of the map path and keep the Avatar dormant, so a Peer that sends lean must refuse a game that did not grant it, at negotiation.

## Interactions

The `interactions` Capability shares what the local player's interactions do to map-placed entities, and lets a Peer drive such an entity in the local game (ADR 0005). The game holds no multiplayer policy. It reports what the local player does and drives what it is told to drive, and never decides between players.

A **holdable** entity is placed in its map file rather than created at runtime by a script or a break, and is an `Object` (Grab, Push, or Slide), `SwingDoor`, `Wheel`, `Lever`, or `MultiSlider`. Only holdable entities are reported or driven.

### The report

The game keeps a report of the holdable entities whose motion the local player decides:

- the entity the local player is interacting with, from its `interactionstart`;
- entities the player released, until they come to rest;
- free entities that a reported entity's moving body touched, or that the local player walked into, from their `reportcontact`, until they come to rest. An entity they touch in turn enters as well, so a toppled stack is reported. Static bodies, character bodies, Avatars, runtime-created entities, and Peer-Driven Entities never enter by contact, and a contact by anything else, such as an Avatar or an unreported prop, reports nothing.

An entity leaves the report when it settles (`reportsettled`), breaks (`reportbroke`), or is driven by a Peer, and every entity leaves it when its Map Visit ends. Settling and breaking send an Event for that entity; the end of a Map Visit is told by the next `mapentered`. Only an entity's moving bodies are reported; a static body, such as a door's frame, is not. The report holds at most 32 bodies. An entity whose bodies do not fit is not reported and stays under local physics: it gets no `reportcontact`, or its `interactionstart` is still sent but its bodies stay out of State Updates.

A Session granted `interactions` receives these Events:

```text
EVENT mapentered <map>
EVENT interactionstart <entityId> <bodyId> <map>
EVENT interactionend <entityId> <bodyId> <ending> <map>
EVENT reportcontact <entityId> <map>
EVENT reportsettled <entityId> <map>
EVENT reportbroke <entityId> <state> <map>
EVENT entityreleased <entityId> <map>
```

- `mapentered`: a Map Visit started on map `<map>`. A Map Visit starts when the game loads a map, starts a story (a Custom Story, the Main Story, or a restart), or loads a save, including a save of the map already loaded. A death or checkpoint reload, a map change to the map already loaded, and a return to the main menu do not start one, and nothing is sent for them. By the time `mapentered` is sent, the previous visit's report is empty and its Peer-Driven Entities are released. It precedes every other `interactions` Event and every `reportedbodies` State Update of the visit it starts, so a Peer can drop whatever it held for the previous visit on receiving it, even when `<map>` is unchanged.
- `interactionstart`: the local player started to grab, push, slide, swing, or spin body `<bodyId>` of a holdable entity. The entity enters the report.
- `interactionend`: that interaction ended. `<ending>` is `released` (the player let go, or a Peer drove the entity), `thrown`, `too-far` (the body moved out of reach and was dropped), or `destroyed` (the entity was destroyed or broke while held). After `released`, `thrown`, or `too-far` the entity stays in the report until it settles. After `destroyed` it leaves the report; an entity that broke while held is then told by `reportbroke`, and one destroyed without breaking, such as one a map script removed, gets no other Event.
- `reportcontact`: a free holdable entity entered the report by contact.
- `reportsettled`: a reported entity came to rest, because its bodies went to sleep or it reached the Settling cap of about 3 seconds, and left the report.
- `entityreleased`: a script deactivated, removed, or moved a Peer-Driven Entity, or a door that is one broke (see [Enemy door breaks](#enemy-door-breaks)), and the game stopped following it. Its interacting mark and buffered samples are cleared before this Event. Each end is reported once, even if the script changes the entity again. Release Commands, `entitybreak`, Session cleanup, and Map Visit cleanup do not send it. A Peer should end the corresponding Hold; that consumer change belongs to amnesia-multiplayer. Unlike `reportsettled`, this reports the end of following another player's entity, rather than the rest of a locally reported entity.
- `reportbroke`: a reported entity, or the entity the local player holds, broke and left the report. `<state>` is the final state of the body whose transform the entity follows: its break body, which is the body its entity file names as `BreakEntityAlignBody`, or else its first body. The debris, contained item, sound, and particles start from that state. An entity that breaks while the local player holds it, whether or not its bodies fit the report, is told by `interactionend ... destroyed` and then, in the same update and immediately after it, by `reportbroke`. A door that breaks is told the same way, with `<state>` the final state of its moving body. It stays in the map, broken, and its debris is local to each game.

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
- `entitydrive`: makes the entity a Peer-Driven Entity of this Session. `not-found` means the current map has no entity with that identifier. `not-holdable` means the entity is not holdable. Driving an entity this Session already drives succeeds and changes nothing. If the local player is interacting with the entity, that interaction ends first, in the same update, with `interactionend ... released`, and the entity leaves the report without another Event. The local player cannot interact with a Peer-Driven Entity. It never breaks from its own contacts, and its contacts never damage enemies. Map scripts cannot change its health: `SetPropHealth` and `AddPropHealth` change nothing while it is driven, so it never breaks from a script and breaks only by `entitybreak`, or, for a door, by an enemy (see [Enemy door breaks](#enemy-door-breaks)). This is not `DisableBreakable`, which still destroys a broken prop without debris. If a sticky area here holds one of its bodies, the body detaches first, as grabbing it detached it in the driving Peer's game: the area's `DetachFunction` runs in Shared Callback mode, if the area allows detaching.
- `entitybodies`: a batch of body samples at `<timeMs>` on the sender's clock, as in `avatarpose`. The game buffers them and plays them back with the same interpolation delay as Avatar Poses, setting each body's transform and velocities every tick. Consecutive samples of a body more than 3 m apart are snapped between rather than interpolated, a body holds its newest sample until a newer one arrives, and a sender clock more than 1 s behind the newest sample starts playback over, as it does for Avatar Poses. A body with no sample yet stays where it was, without gravity. A success is not answered. `not-found <entityId>` names the first entry whose entity this Session does not drive or that has no moving body with that identifier, and the other entries are still applied. A failure is reported once per failure streak, and the next success resets the streak. All `entitybodies` lines share one streak.
- `entityinteracting`: `1` marks the Peer-Driven Entity as being interacted with, and `0` clears the mark, so that door auto-close, lever and slider auto-move, wheel slow-down, and interaction-only connections behave as they do for the local player. Setting the mark starts an interaction as the local player's does: an unlocked door unlatches and stops auto-closing, a wheel's auto-move stops, and a lever, wheel, or slider that interaction unsticks comes unstuck. Repeating the current mark changes nothing. `not-found` means this Session does not drive that entity.
- `entitybreak`: snaps the break body of the Peer-Driven Entity to `<state>` and breaks it through the game's own break path, with its debris, connected props, contained item, sound, particles, and break callback. The debris starts from `<state>`, as it does on the game that sent `reportbroke`, even if the body moves before the entity is gone. Until then the entity stays out of the local player's reach and its contacts still neither break it nor damage enemies. Only an `Object` breaks this way; any other entity is released instead. The entity is then no longer driven. `not-found` means this Session does not drive that entity.
- `entityrelease`: ends driving and returns the entity to local physics from its current state, clearing its interacting mark. `not-found` means this Session does not drive that entity.
- A Peer-Driven Entity belongs to its Session, like an Avatar. The game releases every one when the Session ends and when its Map Visit ends.
- A sticky area neither attaches a body of a Peer-Driven Entity nor runs its `AttachFunction` for one. The Holder's game decides, and reports `stickyattach`.
- Local collide callbacks involving either a Peer-Driven parent or child do not run, including callbacks with `Player` as parent. The check still tracks whether the bodies collide, so releasing an entity while it stays inside an area does not fire an entering callback. The Holder's game still runs the callback. A `sharedcallback` Command can run the registration while either side is Peer-Driven. Lever, wheel, and multi-slider state callbacks continue to run from driven motion.
- `SetEntityActive(false)`, `ReplaceEntity`, `SetEntityPos`, `SetEntityPosRot`, and `PlaceEntityAtEntity` take effect on Peer-Driven Entities. They end following before changing the entity, restore local physics, and send `entityreleased`. Buffered samples cannot undo the change. `SetEntityActive(true)` does not end following. Other script effects, such as impulses and forces, keep their existing behavior.

See [the Controller checks for script changes and collides](../../docs/testing/peer-driven-script-changes.md) for the game adapter checks that require a running game.

## Shared Callbacks

The `sharedcallbacks` Capability shares the Map Script callbacks the local player triggers, so every other player's game can run the callback its own Map Script registered for the same entities (amnesia-multiplayer's ADR 0006). A Shared Callback names a registration by identifiers only: the Map Path, integer Entity Identifiers, a collide state, and an item name. It never carries a function name or script text, and the game never runs text it was sent.

```text
EVENT sharedcallback <kind> <field>... <map>
sharedcallback <kind> <field>... <map>
```

The Event and the Command carry the same fields, so a Peer can relay an Event's fields as a Command to another game. The kinds and their fields are:

| Kind | Fields | Callback |
| --- | --- | --- |
| `playercollide` | `<entityId> <state>` | A collide callback whose parent is `Player`, with `<entityId>` as its child. |
| `entitycollide` | `<parentId> <entityId> <state>` | A collide callback between two entities, either of which the local player Holds or is a Shared Enemy this game simulates (see [Enemy-triggered callbacks](#enemy-triggered-callbacks)). |
| `interact` | `<entityId>` | The entity's player interact callback (`SetEntityPlayerInteractCallback`), including the one a grab runs. |
| `useitem` | `<entityId> <item>` | The use-item callback (`AddUseItemCallback`) of item `<item>` on the entity. |
| `pickup` | `<entityId>` | The local player picked up the item `<entityId>`. Consumables (tinderboxes, oil, sanity potions, and laudanum) are never reported. |
| `flashback` | `<entityId>` | The local player set off the Flashback area `<entityId>` (`AreaType="Flashback"`), which started its flashback. The flashback file and callback are never sent. |
| `combine` | `<item> <otherItem>` | The local player combined the inventory items `<item>` and `<otherItem>`, and their combine callback (`AddCombineCallback`) is about to run. A combine of two consumables is never reported. The callback's name and function are never sent. |
| `stickyattach` | `<areaId> <entityId> <bodyId>` | The sticky area `<areaId>` (`AreaType="Sticky"`) attached body `<bodyId>` of the entity `<entityId>`, which the local player Holds, after its `AttachFunction` ran. The function and the area's sounds and particles are never sent. |
| `autodisabled` | `<entityId>` | The Shared Enemy `<entityId>`, which this game simulates, reached the end of its patrol unseen and disabled itself, and its `OnAutoDisabled` callback is about to run (see [Enemy-triggered callbacks](#enemy-triggered-callbacks)). |

- `<state>` is `1` when the bodies started colliding and `-1` when they stopped.
- `<bodyId>` is the body's identifier within its entity, as in `entitybodies`.
- `<item>` and `<otherItem>` are item names, each 1–64 printable ASCII characters (`!` through `~`), so neither has spaces.
- Every named entity is map-placed. A callback that names an entity created at runtime, or an item whose name falls outside these limits, is not reported.

### The report

A Session granted `sharedcallbacks` receives `EVENT sharedcallback` each time one of these callbacks runs because of the local player, or because of a Shared Enemy this game simulates, just before the callback runs. A local player Holds an entity while it is in the report of the `interactions` Capability; the other games follow such an entity rather than collide it. An entity collide where neither side is Held is not reported. A `stickyattach` is reported once the body has attached, after the `AttachFunction` ran, and only for an entity the local player Holds, held or Settling. A sticky area attaching any other entity, or attaching one because a script called `AttachPropToStickyArea` or `AttachBodyToStickyArea`, is not reported.

These are never reported: look-at, timer, prop animation, effect voice, checkpoint, lantern, and entity callback func (break, ignite) callbacks other than a shared pickup's `OnPickup`, the callback a flashback runs when it ends, connection-state, `ConnectEntities`, and multi-slider callbacks, and `OnStart`, `OnEnter`, `OnLeave`, and `OnUpdate`. A callback that runs because a `sharedcallback` Command ran it is never reported, and neither is any callback that runs nested inside it. A trigger the local player causes later is reported even if a Shared Callback set it up.

### Running a callback by its registration

`sharedcallback` runs the callback that this game's own Map Script registered for the identifiers, as the local trigger would. It is State-Changing, so it needs the Authority Grant.

- The checks run in this order: `invalid` (the line is malformed), `wrong-map` (the map is not the local current map, or no map is loaded), `not-found`.
- `ok` means the callback ran. `not-found` means this game's Map Script has no matching registration, so nothing ran: it was already removed, its child is inactive, the scripts drifted, or a collide registration's states filter excludes `<state>`. It is an outcome, not an error, and the game logs one line for it.
- The callback receives the arguments a local trigger would pass. One-shot registrations (`abDeleteOnCollide`, `abRemoveOnInteraction`, `abAutoDestroy`) are removed after it runs, so a second run answers `not-found`.
- An `entitycollide` run moves the registration's callback state to `<state>`. If the callback state is already there, because this game's own check or a replay handled it, the run answers `not-found` and runs nothing. For ordinary entities it also moves the sampled collision edge, so the local check does not fire it again when the bodies agree. While either side is Peer-Driven, sampled motion and callback state stay independent: silent motion cannot consume a replay, and a replay ahead of body samples cannot invent an entry after release. A `playercollide` run leaves the colliding state of the local player's body alone, because another player's body triggered it.
- While the callback runs, the game is in Shared Callback mode: scripts cannot take the local player's control or camera, so `SetPlayerActive`, `TeleportPlayer`, `StartPlayerLookAt`, `MovePlayerForward`, `MovePlayerHeadPos`, `SetPlayerMoveSpeedMul`, `SetPlayerRunSpeedMul`, `SetPlayerLookSpeedMul`, and `SetPlayerJumpForceMul` do nothing. Sanity, health, music, sounds, and screen effects run as usual. Timers and other follow-ups the callback starts run later as ordinary local callbacks.
- A `combine` run looks up this game's own combine callback for the two items, in either order. It runs only if the local inventory holds both items, and then runs exactly as combining them in the inventory would, so an `abAutoDestroy` callback is removed. The callback's own `RemoveItem` and `GiveItem` calls act on the local inventory. If the inventory lacks either item, the two items are the same, or no combine callback matches, the run answers `not-found` and nothing changes, so a combine never gives a player items they did not hold.
- A `stickyattach` run, in the receiving game, ends the entity's Peer-Driven state without `entityreleased`, runs the area's `AttachFunction` for the body, and attaches the body whatever `SetAllowStickyAreaAttachment` said, because the Holder's game already decided: the body snaps into place, loses its mass and gravity, and plays the area's attach sound and particles. If the area is gone, inactive, or not a sticky area, the entity is gone or inactive or has no such body, or the area already has a body attached, the run answers `not-found` and nothing runs.
- An `entitycollide` run may name a driven enemy copy as either side. The copy never triggers the callback itself.
- An `autodisabled` run disables this game's own copy of the enemy, as the end of its patrol would, and runs its entity callback func with `"OnAutoDisabled"`. A driven copy stays inactive when it is released. If the enemy is inactive, because it never activated here or was already disabled, or the entity is not an enemy, the run answers `not-found` and nothing runs.
- A `useitem` run does not need the item in the local inventory. Only the callback's own script, such as a `RemoveItem`, touches it.
- A `pickup` run picks the item up exactly as the local player would: the item leaves the world and enters the inventory, the pickup message and sound play, a note or diary opens and plays its voice, and the item's entity callback func runs with `"OnPickup"`. Opening the note or diary is part of the pickup and is not suppressed. If the item is no longer in the world, because this game picked it up first, a script removed or deactivated it, or it never existed, is not an item, or is a consumable, the run answers `not-found` and nothing runs. Two players picking up the same item at nearly the same time therefore run its callback once in each game.
- A `flashback` run sets off this game's own Flashback area exactly as the local player walking in would: the area deactivates and the full flashback plays, with its flash, voices, subtitles, sepia, and slowdown, and its callback runs when it ends as an ordinary local callback. The flashback's own effects on the player are not script control calls, so Shared Callback mode does not suppress them. A flashback that starts while another plays waits for it to end. If the area is inactive, because this game already had its flashback, or it never existed or is not a Flashback area, the run answers `not-found` and nothing starts. Walking into the area afterwards does nothing.

The legacy `SCRIPT_CALL:` Event and `exec:` Command are unchanged. A collide callback run by `sharedcallback` sends no `SCRIPT_CALL:`.

## Placing the local player

```text
playerplace <x> <y> <z> <yaw> <crouch> <map>
```

The `playerplace` Capability lets a Peer place the local player at the feet position, body yaw, and crouch of a Pose it knows, such as another player's latest `localpose`, so a player a locked door left behind can follow. A game without the Capability does not grant it, so the granted list tells a Peer whether it can place the player.

- `<x> <y> <z>` is the feet position, `<yaw>` the body yaw, and `<crouch>` is `1` to place the player crouching. `<map>` is the Map Path the Peer means. Each number's magnitude is at most `1000000000`, and the Map Path is at most 1024 bytes.
- The checks run in this order: `invalid` (the line is malformed), `wrong-map` (the map is not the local current map, or no map is loaded), `unavailable` (the local player is not in play: the game is paused or in a menu, the inventory, or the journal, a map is loading, a script made the player inactive with `SetPlayerActive(false)`, the player is dead, or a flashback plays). A refused Command changes nothing.
- `ok` means the player was placed as a script's `TeleportPlayer` places them. Holding a prop or another interaction ends, and the player leaves a ladder, is out of water, and is in normal movement. The feet move to the position, the body and camera turn to `<yaw>`, and the camera pitch resets to `0`. With `<crouch>` `1`, the player crouches before arriving, so a player placed in a crawlspace is not stuck in its ceiling. With `0`, the player stands up after arriving if they fit there, and otherwise stays crouched.
- The teleport counter changes, so the next `localpose` State Update tells every other game to snap the local player's Avatar rather than interpolate it.
- It is not a script call, so Shared Callback mode does not suppress it. It is State-Changing, so it needs the Authority Grant.

## Chat

```text
chatentry <authorColor> <messageColor> <Author>:<message>
```

- Displays a Chat Entry in the Chat Log of the current menu or game context. It needs no Capability. It replaces the legacy `chat:` Command, which every Session now answers with `WARNING:Unknown command` (ADR 0007).
- `<authorColor>` and `<messageColor>` are the Chat Colors of the Chat Author and the message. Each is exactly six hex digits `RRGGBB`, in either case and without alpha, or `-` for that part's default color. The Chat Log fade still sets their alpha.
- `<Author>:<message>` is the rest of the line. The first `:` ends the Chat Author, so later colons belong to the message. Both parts must be valid UTF-8. After Unicode whitespace trimming, a Chat Author must contain 1–32 Unicode scalars and a message 1–256. Control characters are rejected, and a Chat Author cannot contain `:`. Nothing is truncated or partly displayed.
- `invalid` means the line is malformed: a field is missing or empty, the rest of the line has no `:`, or a color is neither `-` nor `RRGGBB`, such as `ff40`, `#ff4040`, `ff4040aa`, or `zzzzzz`. A bad color rejects the whole Command; it never falls back to the default.
- `invalid-author` and `invalid-message` mean that part failed validation. The Chat Author is checked first.
- `unavailable` means the game cannot display chat right now.
- The local player's own Chat Entries always use the default colors, and `EVENT:CHAT:<Author>:<message>` carries no colors.

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

## Button and lamp state

The `sharedcallbacks` Capability also grants these Commands and Events:

```text
buttonstate <entityId> <state> <map>
lampstate <entityId> <state> <map>
RESPONSE buttonstate <outcome>
RESPONSE lampstate <outcome>
EVENT buttonstate <entityId> <state> <map>
EVENT lampstate <entityId> <state> <map>
```

`state` is `1` for switched on or lit, and `0` for switched off or unlit. Entity IDs are signed integers; the Map Path is the rest of the line. Outcomes are `ok`, `invalid`, `not-granted`, `wrong-map`, `not-found`, or `wrong-type`. Runtime-created entities are not shared. Sessions without `sharedcallbacks`, including legacy Sessions, receive neither Event.

Only the local player's button press or lamp lighting reports state, and only when it changes. Map Script setters and changes nested inside a Peer's Command do not report. Applying state uses the game's own setter with effects, so button connection callbacks and connected entities run as for a local press. An unchanged state does nothing. The whole application runs in Shared Callback mode: callbacks never report back and calls that take player control or camera are ignored. Applying lamp state never interacts with the lamp, runs its local `OnIgnite` callback, spends tinderboxes, or awards lighting sanity.

The multiplayer consumer must relay these Events and compose the corresponding Commands, including queuing them for another Map Path. That coordination belongs to `amnesia-multiplayer` (Shared Callbacks map #43).

### Manual verification with the Controller

Run `dotnet run --project ..\amnesia-csharp-controller\src\AmnesiaController` against a running game and type `protocol 2 sharedcallbacks`. Expect `RESPONSE protocol ok 2 sharedcallbacks`. Version 2 lines appear as `uncategorised`.

Use a map with a switchable button and an unlit candle, with a button connection callback that has a visible effect. The entity IDs and Map Path below are examples: use the exact fields from the Events emitted by your map.

1. Press the button. Expect exactly one `EVENT buttonstate 12 1 maps/test.map` for that button. In another game on the same map, type `buttonstate 12 1 maps/test.map`. Expect `RESPONSE buttonstate ok`, the button and its connected effect changing once, and no `EVENT buttonstate` or `EVENT sharedcallback` from that Command. Repeat the line: expect `RESPONSE buttonstate ok` with no callback or effect replay.
2. Note the tinderbox count and light the candle locally. Expect exactly one `EVENT lampstate 13 1 maps/test.map` and one tinderbox spent. In the other game, note its count and type `lampstate 13 1 maps/test.map`. Expect `RESPONSE lampstate ok`, lit candle effects, no inventory change, and no state or Shared Callback report. Repeat: expect `RESPONSE lampstate ok` with no replayed effects.
3. Type `lampstate 13 0 maps/test.map`. Expect `RESPONSE lampstate ok`, the candle going out with effects, no tinderbox change, and no state report.
4. Make the button's connection callback call `SetPlayerActive(false)` and `StartPlayerLookAt(...)`. Apply the button state by Command: expect the connection's ordinary effects once, with local control and camera retained. A local button press should still permit those script calls.
5. For a button named `test_button` and lamp named `test_candle`, type `exec:SetButtonSwitchedOn("test_button", false, true)` and `exec:SetLampLit("test_candle", false, true)`. Expect `RESPONSE:exec:script executed` for each line and no `EVENT buttonstate` or `EVENT lampstate` for script changes. Local interactions must still report after these changes.
6. Type `buttonstate 12 1 maps/other.map` on the test map. Expect `RESPONSE buttonstate wrong-map` and no effects.

7. Use a momentary button whose connection callback resets it to switched off. Press it locally: expect one `EVENT buttonstate 12 1 maps/test.map` even though it ends switched off. Type `buttonstate 12 1 maps/test.map` in the other game: expect `RESPONSE buttonstate ok`, the same connection effect and reset, and no report back.

## Enemy reporting and driven copies (`enemies`)

Negotiate `protocol 2 enemies` to grant this Capability. It is independent of `avatars`, `localpose`, and `interactions`; without it, the Commands below answer `not-granted` and no enemies are sampled. Legacy Sessions are unchanged.

```
enemyposes subscribe <hz>
enemyposes unsubscribe
STATE enemypose <entityId> <timeMs> <teleportCounter> <x> <y> <z> <yaw> <clip> <clipTime> <loop> <map>
enemydrive <entityId> <map>
enemypose <entityId> <timeMs> <teleportCounter> <x> <y> <z> <yaw> <clip> <clipTime> <loop> <map>
enemyrelease <entityId> <map>
```

Subscribe at the same rate as `localpose` (normally 20 Hz); rates clamp to 1�60 Hz. Subscribing again restarts sampling. Each active `Enemy_Grunt` entity (including models using that class) produces one line per interval, including driven copies. Runtime-created Grunts are reported too; other classes are excluded in this slice. Driving still requires a map-placed enemy. No reports are produced in the main menu or while loading. There is one replaceable pending line per enemy; a new report replaces the complete pending set, so inactive enemies cannot retain stale pending samples. Reports do not describe activation/death transitions.

Position is the character body's feet, and rotation is body yaw in degrees. `<clip>` is the case-sensitive mesh animation name, one non-space field of at most 128 characters; `-` means no clip. `<clipTime>` is seconds within the clip, from 0 to 1000000000; `<loop>` is `0` or `1`. Position and yaw magnitudes cannot exceed 1000000000. Maps are nonempty rest-of-line paths of at most 1024 bytes. The ordinary Version 2 numeric grammar applies. The current game report emits teleport counter zero; recipients still honor a changed counter supplied by a Peer, and jumps over three meters snap automatically.

Responses are `RESPONSE enemyposes ok subscribe <clampedHz>` or `RESPONSE enemyposes ok unsubscribe`. Malformed subscriptions answer `RESPONSE enemyposes invalid`.

Drive and release answer `RESPONSE <keyword> <outcome> <entityId>`. Outcomes are `ok`, `wrong-map`, `not-found`, or `wrong-type`; malformed lines answer `invalid` without an identifier. Driving again is idempotent. Only a map-placed Grunt can be driven; release and pose require an already driven enemy. An unknown animation is `invalid`. Successful `enemypose` is silent; only the first failure of a consecutive failure streak is reported. A successful sample or a new Map Visit ends that streak. Capability failures answer `not-granted` without an identifier.

Copies buffer sender-timestamped motion using the Avatar's 100 ms interpolation delay, shortest-path yaw, clock alignment, and discontinuity rules. Animation time interpolates within a monotonically advancing clip; clip changes, restarts, and wraps switch at their sample time. A stalled stream holds its last pose and animation. Local AI, senses, pathfinding, attacks, damage reactions, and enemy-triggered Map Script callbacks are suppressed, including collide callbacks in which the copy is either side. The game simulating the enemy shares them instead (see [Enemy-triggered callbacks](#enemy-triggered-callbacks)). Local ambient sound and darkness glow remain. A copy attacks only when an `enemyattack` Command delivers an attack.

Driving takes over an enemy in any state, including one active and mid-chase in this game: its current AI state ends as a state change would end it, undoing the chase's speed, field of view, terror, music, and ambient sounds, and the enemy may jump to the first reported pose. Release, Session end, and leaving the Map Visit discard buffered samples and hand the enemy back to its own AI from the newest driven pose, ahead of the delayed pose on show, restoring gravity, collision testing, and the prior active flag. A released enemy starts waiting where it was driven to, facing the driven yaw, and its senses then decide whether it patrols, searches, or chases the local player. A save does not store driven mode. A checkpoint death within the same Map Visit does not reset a driven copy, except a coordinated Shared Death, which deactivates it (see [Shared Deaths](#shared-deaths)). The Peer must drive it again after loading a save or entering another Map Visit. The multiplayer consumer must add negotiation, relay, and map/Session handoff in its own repository; this change does not enable enemy sharing there by itself.

See [the Controller checklist](../../docs/testing/enemy-copies.md) for game adapter verification.

### Enemy attacks

```
EVENT enemyattack <entityId> <attack> <sizeX> <sizeY> <sizeZ> <offsetX> <offsetY> <offsetZ> <map>
enemyattack <entityId> <attack> <sizeX> <sizeY> <sizeZ> <offsetX> <offsetY> <offsetZ> <map>
```

The game that simulates an enemy reports each Grunt attack at the clip's special-event time, the moment it tests for a hit, to a Session granted `enemies`. `<attack>` is `normal` or `breakdoor`, which selects the enemy's attack damage. The box is the one the attack tests: its size, then its center offset along the enemy body's right, up, and forward axes, in metres, using the ordinary Version 2 number grammar. The Event is reported whether or not the attack hits anything locally, and it never carries damage: each game rolls its own.

The `enemyattack` Command takes the same fields, so a Peer can deliver a reported attack to a driven copy unchanged apart from its prefix. The game tests the box from the copy's position and yaw as this game shows it, against the local player only, and applies a hit the vanilla way: damage within the attack's range, blood splash, red flash, head spin, knockback, and the enemy's hit sound. A miss does nothing. Attacks never hit an Avatar, in the simulating game or on a copy; the simulating game's own player is hit as before.

Responses are `RESPONSE enemyattack <outcome> <entityId>`, with outcome `ok` whether the attack hits or misses, `wrong-map`, `not-found` (including an enemy that is not driven), or `wrong-type`. A malformed line, an unknown attack, a size that is not positive, or a box size that none of the copy's own attacks uses answers `invalid` without an identifier. Without `enemies`, the Command answers `not-granted` and no attacks are reported.

### Enemy door breaks

```
EVENT enemydoorbreak <doorId> <map>
enemydoorbreak <doorId> <map>
```

The game that simulates an enemy reports, to a Session granted `enemies`, each map-placed door a Grunt's attack breaks. The door runs out of health in that game only, because a driven copy never attacks a door. The Event follows, in the same update, any Events that end a Hold on the door (below).

The `enemydoorbreak` Command breaks this game's own copy of the door through the game's own break path: the door's bodies disappear, and its broken entity, sound, particles, and `Break` callback appear and run here. The broken entity is created at runtime in each game, so it is never shared or Held. The break runs in Shared Callback mode, so its callback cannot take the local player's control or camera and is never reported back. It is State-Changing, so it needs the Authority Grant.

A break ends any Hold on the door, in every game, whichever game breaks it:

- If the local player is interacting with the door, the interaction ends with `interactionend ... destroyed`, then `reportbroke`. If the door is Settling, `reportbroke` alone is sent.
- If the door is a Peer-Driven Entity, the game stops following it and sends `entityreleased`. That includes a door the simulating game's Grunt breaks while the other player Holds it: an enemy's attack damages a Peer-Driven door as it damages any other door.

A Peer that relays `reportbroke` as an `entitybreak` adds nothing: in a game where the door already broke, the Session no longer drives it and the Command answers `not-found`. One that arrives before `enemydoorbreak` releases the door, as `entitybreak` releases any entity that is not an `Object`, and `enemydoorbreak` then breaks it.

Responses are `RESPONSE enemydoorbreak <outcome> <doorId>`, with outcome `ok`, also for a door already broken here, which stays broken, `wrong-map`, `not-found` (no map-placed entity with that identifier), or `wrong-type` (not a door, or a door that cannot break, such as one with `DisableBreakable`). A malformed line answers `invalid` without an identifier. Without `enemies`, the Command answers `not-granted` and no door breaks are reported.

### Enemy-triggered callbacks

A driven copy fires no Map Script callbacks, so the game that simulates a Shared Enemy shares the ones the enemy triggers, as Shared Callbacks (amnesia-multiplayer's ADR 0009). A Shared Enemy is a Grunt this game simulates, rather than drives, while a Session granted `enemies` lasts and the other player is on this map, shown by an awake Avatar. A Session granted `sharedcallbacks` then receives:

- `entitycollide`, for a collide callback either side of which is a Shared Enemy, such as an area the Grunt walks into. The other games run their own registration on their copy, which never triggers it itself, so the callback runs once in each game.
- `autodisabled`, when the Shared Enemy reaches the end of its patrol, unseen by the local player and every Avatar with enemy sight, and more than 10 m from the nearest of them, and disables itself. The other games disable their copy and run its `OnAutoDisabled` callback.

Alone on the map, or without such a Session, every enemy is vanilla and nothing is reported. Brutes and Kaernks are not Shared Enemies yet. A copy triggers nothing whenever it is driven, so a Peer must drive copies only while the simulating game has the other player's Avatar awake on the map; otherwise an enemy-triggered callback runs only in the simulating game, which does not report it. A Peer's Command run of any kind counts as another player's trigger for the two functions below, including the `Break` callback of an `enemydoorbreak`.

The other enemy behavior that reads the player:

- Activation range and out-of-range deactivation already measure the nearest player, the local one or an Avatar with enemy sight, and run no callback.
- `ShowEnemyPlayerPosition` and `AlertEnemyOfPlayerPresence` show the enemy the player who triggered the callback. A local trigger shows the local player, even to an enemy pursuing an Avatar. A callback run by a Peer's Command, as another player's Shared Callback, shows the nearest Avatar with enemy sight, or the local player when there is none. On a driven copy they do nothing; the game simulating the enemy runs them.
- Checkpoint callbacks that reactivate enemies run in each game in a Shared Death (see [Shared Deaths](#shared-deaths)).
- Pickup callbacks that start an encounter are shared as `pickup`.

See [the Controller checklist](../../docs/testing/enemy-doors-and-callbacks.md) for game adapter verification.

### Shared Deaths

```
EVENT checkpoint <count> <callback> <checkPoint> <map>
EVENT playerdeath <death> <count> <callback> <checkPoint> <map>
shareddeath <episode> <death> <count> <checkPoint> <map>
```

A Session granted `enemies` coordinates deaths (amnesia-multiplayer's ADR 0009). The `checkpoint` Event reports the current map's checkpoint registration: `<count>` is the count its callback receives on the next death, `<callback>` is `1` when a death runs a Map Script callback and `0` otherwise, and `<checkPoint>` is the checkpoint's name, or `-` when the name is not a single printable field of at most 64 characters. Every entered map has the automatic `_auto` checkpoint without a callback until its Map Script registers another, so only `<callback>` tells whether a death may change maps or puzzles. The Event is sent once a Session is granted `enemies`, whenever any field changes (a registration, a death's count increment, a map change), and again when a Map Visit starts. The callback's function name is never sent.

The `playerdeath` Event is sent when the local player starts dying on its own, numbered `<death>` from `1` within the Session, with the checkpoint as it stood. A death a `shareddeath` Command caused is not reported.

How a death treats enemies depends on whether the other player is on this map, shown by an awake Avatar:

- Alone on the map, or without an `enemies` Session, a death is vanilla.
- With the other player on the map and no callback, Shared Enemies (Grunts) survive the death: the checkpoint does not deactivate or reset them, and a Grunt does not leave range while the local player is dead. It still drops a dead local target as vanilla does, and keeps hunting a perceived Avatar. Other enemies, and the respawn itself, are vanilla.
- With the other player on the map and a callback, the death is coordinated: the death screen stays up to 3 more seconds until an episode answers the death, so its callback can receive the agreed count.

The `shareddeath` Command applies one episode of a Shared Death, numbered by the Session Host's Game Peer in the order it decided them. `<death>` is `0` to kill this game's player, or the number of this game's own reported death the episode answers, which never kills. `<count>` is the agreed pre-increment count, the Session Host's own. Every game in an episode follows its own checkpoint: it places its player, deactivates and resets the enemies it simulates and deactivates its driven copies so they stay inactive on release, then runs its own callback once. When `<checkPoint>` is this game's current checkpoint and it has a callback, the callback receives `<count>` and the stored count becomes `<count>` plus one, or one after a callback that registered a new checkpoint, as vanilla. Otherwise, or when no episode answers a death in time, the game keeps its own count and logs one line.

Responses are `RESPONSE shareddeath <outcome>`:

- `ok`: the episode killed the living player, or joined the death in progress.
- `duplicate`: this episode, or a newer one, was already applied. Nothing changes.
- `stale`: the episode answers a death that already reached its checkpoint or never happened, or a death already joined another episode. Nothing changes.
- `wrong-map`: `<map>` is not the current map, so a late episode never kills in a later Map Visit.
- `unavailable`: the player is not in play, such as while loading, in a flashback, or inactive, so it was not killed. The episode is spent.
- `invalid`: the line is malformed: an episode of `0`, a negative or non-integer number, a number above 2147483647, or a checkpoint field that is not a valid name.
- `not-granted`: the Session is not granted `enemies`.

A new Map Visit ends any death in progress and any pending kill; applied episodes stay applied for the Session. A Session that ends during a death takes it along: that death reaches its checkpoint as vanilla. Hard Mode, which loads the latest save after the checkpoint, and permanent death, which never reaches a checkpoint, are outside this guarantee: their deaths are reported, but the save load starts a new Map Visit and a permanent death has no checkpoint to coordinate.

See [the Controller checklist](../../docs/testing/shared-deaths.md) for game adapter verification.
