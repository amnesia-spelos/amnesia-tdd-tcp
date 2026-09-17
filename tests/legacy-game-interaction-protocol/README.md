# Legacy Game Interaction Protocol contract

`contract.jsonl` is the language-neutral compatibility source of truth for what a legacy Peer observes. Each line is an independent JSON fixture so consumers can deserialize cases without depending on the C++ harness.

`request_wire` and `expected_wire` include the newline delimiters exchanged on the wire. Rotation inputs are radians because that is how the game adapter observes them; legacy Responses expose degrees. The `script_call_observation` case deliberately records `SCRIPT_CALL` as its own top-level category. It is not normalized into an Event because that is the current runtime behavior.

Chat Commands use the grammar `chat:<author>:<message>`. The first colon after `chat:` ends the Chat Author, so later colons belong to the message. Both fields must be valid UTF-8. After Unicode whitespace trimming, a Chat Author must contain 1–32 Unicode scalars and a message 1–256; control characters are rejected, and a Chat Author cannot contain a colon. Invalid input is rejected whole, without truncation or partial display.

Chat Responses are exactly `RESPONSE:chat:message displayed`, `RESPONSE:chat:invalid author`, `RESPONSE:chat:invalid message`, or `RESPONSE:chat:unavailable`. A local submission is serialized as `EVENT:CHAT:<author>:<message>`. This Event is best-effort: it is sent once to a currently connected Peer and is discarded when no Peer is present, with no later replay.

`getcustomstories` responds with `RESPONSE:getcustomstories:<id>|<name>` entries separated by tabs, or an empty payload when no valid Custom Story is installed. The identifier is the Custom Story Identifier (the installed folder name) and ends at the first `|`; the display name is UTF-8 with tabs, CR, and LF replaced by spaces.

`startcustomstory:<id>` responds with exactly `RESPONSE:startcustomstory:starting`, `RESPONSE:startcustomstory:not found`, `RESPONSE:startcustomstory:invalid`, or `RESPONSE:startcustomstory:not in main menu`. An accepted start is performed on a later game update, after its Response, so a second start while one is pending is `not in main menu`. Every fresh Custom Story start, whether from this Command or the menu's Start button, is serialized as `EVENT:CustomStoryStarted:<id>` before the start map's `EVENT:MapChanged`.

Run the game-side harness without starting the game:

```powershell
.\scripts\test-legacy-protocol.ps1
```
