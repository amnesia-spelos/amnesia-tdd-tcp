# Legacy Game Interaction Protocol contract

`contract.jsonl` is the language-neutral compatibility source of truth for what a legacy Peer observes. Each line is an independent JSON fixture so consumers can deserialize cases without depending on the C++ harness.

`request_wire` and `expected_wire` include the newline delimiters exchanged on the wire. Rotation inputs are radians because that is how the game adapter observes them; legacy Responses expose degrees. The `script_call_observation` case deliberately records `SCRIPT_CALL` as its own top-level category. It is not normalized into an Event because that is the current runtime behavior.

Run the game-side harness without starting the game:

```powershell
.\scripts\test-legacy-protocol.ps1
```
