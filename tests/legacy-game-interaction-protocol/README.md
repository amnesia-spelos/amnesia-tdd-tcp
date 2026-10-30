# Legacy Game Interaction Protocol contract

`contract.jsonl` is the language-neutral compatibility source of truth for what a legacy Peer observes. Each line is an independent JSON fixture so consumers can deserialize cases without depending on the C++ harness.

A Session that negotiates [Protocol Version 2](../game-interaction-protocol-version-2/README.md) with `protocol 2 ...` keeps every behavior described here. A Session that never negotiates observes exactly this contract, with two exceptions for every Session. First, a line that begins with the `protocol` field is answered as a negotiation. Second, an inbound line longer than 65,536 bytes, not counting its terminator, disconnects the Peer.

`request_wire` and `expected_wire` include the newline delimiters exchanged on the wire. Rotation inputs are radians because that is how the game adapter observes them; legacy Responses expose degrees. The `script_call_observation` case deliberately records `SCRIPT_CALL` as its own top-level category. It is not normalized into an Event because that is the current runtime behavior.

There is no legacy chat Command. The `chat:<author>:<message>` Command was replaced by the Protocol Version 2 `chatentry` Command (ADR 0007), so `chat:` lines get `WARNING:Unknown command` in every Session, and a Session that does not negotiate cannot display Chat Entries. A local submission is still serialized as `EVENT:CHAT:<author>:<message>`. This Event is best-effort: it is sent once to a currently connected Peer and is discarded when no Peer is present, with no later replay.

`getcustomstories` responds with `RESPONSE:getcustomstories:<id>|<name>` entries separated by tabs, or an empty payload when no valid Custom Story is installed. The identifier is the Custom Story Identifier (the installed folder name) and ends at the first `|`; the display name is UTF-8 with tabs, CR, and LF replaced by spaces.

`startcustomstory:<id>` responds with exactly `RESPONSE:startcustomstory:starting`, `RESPONSE:startcustomstory:not found`, `RESPONSE:startcustomstory:invalid`, or `RESPONSE:startcustomstory:not in main menu`. An accepted start is performed on a later game update, after its Response, so a second start while one is pending is `not in main menu`. Every fresh Custom Story start, whether from this Command or the menu's Start button, is serialized as `EVENT:CustomStoryStarted:<id>` before the start map's `EVENT:MapChanged`. Other Events wait for the next game update, but this Event is written as soon as it is reported, before the start map begins loading, so the Peer can start loading alongside the game. Lines queued before it are written first, in order. The game never blocks on delivery, so whatever the Peer's socket does not accept, or what lies beyond the 64 KiB written per attempt, waits for the next update.

Run the game-side harness without starting the game:

```powershell
.\scripts\test-legacy-protocol.ps1
```
