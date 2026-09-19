# External Game Interaction

This context covers communication through which external applications observe or control Amnesia: The Dark Descent.

## Language

**Game Interaction Protocol**:
The versioned language exchanged between the game and independently developed external applications.
_Avoid_: TCP protocol, socket protocol

**Protocol Version**:
A compatible edition of the Game Interaction Protocol selected for a Session.
_Avoid_: Schema version, client version

**Capability**:
An optional area of Game Interaction Protocol behavior explicitly selected for a Session.
_Avoid_: Feature flag, extension

**Privileged Capability**:
A Capability that permits unusually broad or sensitive changes to game state and requires explicit authorization.
_Avoid_: Admin mode, trusted command

**Peer**:
An external application instance participating in the Game Interaction Protocol.
_Avoid_: Client, connection

**Authoritative Peer**:
The Peer permitted to issue Commands that change game state. At most one Peer is authoritative at a time.
_Avoid_: Controller, host

**Observing Peer**:
A Peer permitted to inspect game state and receive Events but not change game state.
_Avoid_: Spectator, read-only client

**Session**:
A Peer’s continuous participation in the Game Interaction Protocol from successful negotiation until disconnection.
_Avoid_: Connection

**Authority Grant**:
The explicit permission that makes one Peer the Authoritative Peer for its Session.
_Avoid_: Ownership, lock

**Command**:
A request from a Peer to inspect or change game state.
_Avoid_: Instruction, request

**Request Identifier**:
A value chosen by a Peer that uniquely identifies a Command within its Session.
_Avoid_: Command tag, correlation ID

**Response**:
The game’s outcome for a Command.
_Avoid_: Reply, result

**Command Outcome**:
The stable success or failure classification carried by a Response, optionally accompanied by human-readable detail.
_Avoid_: Error message, status text

**Event**:
A game occurrence reported to Peers independently of a Command.
_Avoid_: Notification, message

**Event Subscription**:
A Session’s negotiated selection of Event categories it will receive.
_Avoid_: Filter, listener

**Snapshot**:
A consistent representation of the game state exposed to a Peer when it joins an active game.
_Avoid_: Initial state, full Event history

**State Update**:
A replaceable report of currently exposed game state, for which a newer value supersedes an undelivered older value.
_Avoid_: Event, update event

**Custom Story**:
A player-installed story with its own maps and start position, started from the beginning rather than resumed from a save.
_Avoid_: Mod, campaign

**Main Story**:
The game's own story, started from the beginning from its configured start map rather than resumed from a save.
_Avoid_: Main game, base game, campaign, new game

**Custom Story Identifier**:
The installed folder name that uniquely identifies a Custom Story, as opposed to its possibly duplicated display name.
_Avoid_: Story name, story title

**Chat Author**:
Display text attributing a Chat Entry, without asserting an authenticated player identity.
_Avoid_: Username, user identity

**Chat Entry**:
An ephemeral Chat Author and message pair shown in the game's chat log.
_Avoid_: Stored message, chat history

**Chat Log**:
The bounded, transient view of recent Chat Entries for the current menu or game context.
_Avoid_: Chat history, message archive

**Chat Composer**:
The temporary input mode in which the local player writes a Chat Entry while ordinary game controls are suppressed without pausing the game.
_Avoid_: Chat box, console

**Avatar**:
An in-world stand-in whose Pose is driven by a Peer rather than by local input or AI. An Avatar belongs to its Session rather than to a map, and a Session may drive several Avatars.
_Avoid_: Remote player, ghost, puppet, other player

**Avatar Identifier**:
A Peer-chosen value that uniquely identifies an Avatar within a Session.
_Avoid_: Player name, entity name

**Pose**:
The position, facing, crouch, and raised lantern of the local player or an Avatar at a moment in time.
_Avoid_: Transform, position update

**Dormant Avatar**:
An Avatar whose latest Pose belongs to a different map than the local one, and which is therefore neither visible nor collidable until a Pose for the local map arrives.
_Avoid_: Hidden avatar, inactive avatar
