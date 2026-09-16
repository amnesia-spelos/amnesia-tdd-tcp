# Legacy Game Interaction Protocol contract

`contract.jsonl` is the language-neutral compatibility source of truth for what a legacy Peer observes. Each line is an independent JSON fixture so consumers can deserialize cases without depending on the C++ harness.

`request_wire` and `expected_wire` include the newline delimiters exchanged on the wire. Rotation inputs are radians because that is how the game adapter observes them; legacy Responses expose degrees. The `script_call_observation` case deliberately records `SCRIPT_CALL` as its own top-level category. It is not normalized into an Event because that is the current runtime behavior.

Chat Commands use the grammar `chat:<author>:<message>`. The first colon after `chat:` ends the Chat Author, so later colons belong to the message. Both fields must be valid UTF-8. After Unicode whitespace trimming, a Chat Author must contain 1–32 Unicode scalars and a message 1–256; control characters are rejected, and a Chat Author cannot contain a colon. Invalid input is rejected whole, without truncation or partial display.

Chat Responses are exactly `RESPONSE:chat:message displayed`, `RESPONSE:chat:invalid author`, `RESPONSE:chat:invalid message`, or `RESPONSE:chat:unavailable`. A local submission is serialized as `EVENT:CHAT:<author>:<message>`. This Event is best-effort: it is sent once to a currently connected Peer and is discarded when no Peer is present, with no later replay.

Run the game-side harness without starting the game:

```powershell
.\scripts\test-legacy-protocol.ps1
```
