# Controller checks for Peer-Driven script changes

These checks need a running game; the model harnesses do not exercise Newton, visibility, or Map Script hooks. Run the rebuilt `artifacts/Release/Amnesia.exe` from a game installation, then start the [Controller](../../README.md#manual-testing-with-the-controller).

Use a disposable test map with a map-placed, single-body, grab-enabled breakable object named `TestProp` (entity ID 12), a script area named `TestArea` (entity ID 30) surrounding (0, 1, 0), and a target entity named `TestTarget` outside that area at (5, 1, 0). Leave the area large enough that the prop stays inside it after a physics tick. Add this function to the Map Script:

```cpp
void OnTestCollide(string &in parent, string &in child, int state)
{
    SetEntityActive("TestTarget", state < 0);
}
```

In the lines below, replace `<map>` with the path returned by `getmap`. Grab `TestProp` once after negotiating `interactions` and replace `<body>` with the body identifier in `EVENT interactionstart 12 <body> <map>`. Release it before starting. Each `exec:` line below answers `RESPONSE:exec:script executed`. Protocol Version 2 lines appear as `uncategorised` in the Controller.

## Collides and release inside the area

Type:

```text
protocol 2 interactions
getmap
exec:AddEntityCollideCallback("TestArea", "TestProp", "OnTestCollide", false, 0)
entitydrive 12 <map>
entitybodies 0 1 12 <body> 0 1 0 0 0 0 1 0 0 0 0 0 0 <map>
```

Expect `RESPONSE protocol ok 2 interactions`, `RESPONSE:getmap:<map>`, and `RESPONSE entitydrive ok 12`. A successful `entitybodies` has no Response. Wait at least one game tick. There must be no `SCRIPT_CALL:OnTestCollide("TestArea", "TestProp", 1)`.

Type `entityrelease 12 <map>`. Expect `RESPONSE entityrelease ok 12`, no `EVENT entityreleased`, and no entering `SCRIPT_CALL` while the prop remains inside. Move the prop out with `exec:SetEntityPos("TestProp", 5, 1, 0)`, wait a tick, then back with `exec:SetEntityPos("TestProp", 0, 1, 0)`. Expect the leaving and entering `SCRIPT_CALL:OnTestCollide("TestArea", "TestProp", -1)` and `SCRIPT_CALL:OnTestCollide("TestArea", "TestProp", 1)` respectively. This is the ordinary local/Holder path.

Repeat with the registration's sides reversed:

```text
exec:RemoveEntityCollideCallback("TestArea", "TestProp")
exec:AddEntityCollideCallback("TestProp", "TestArea", "OnTestCollide", false, 0)
entitydrive 12 <map>
```

Drive the prop outside and inside using the same `entitybodies` line with increasing timestamps and X values 5 and 0. Neither edge may emit a `SCRIPT_CALL`. Release inside; no entering callback may fire. This checks a Peer-Driven parent as well as a child.

## Silent motion still permits Shared Callback replay

Reload the fixture and reconnect the Controller, then type:

```text
protocol 2 interactions sharedcallbacks
exec:AddEntityCollideCallback("TestArea", "TestProp", "OnTestCollide", false, 0)
entitydrive 12 <map>
entitybodies 0 1 12 <body> 0 1 0 0 0 0 1 0 0 0 0 0 0 <map>
```

Expect `RESPONSE protocol ok 2 interactions sharedcallbacks` and `RESPONSE entitydrive ok 12`. Wait a tick; `TestTarget` remains visible, because sampled motion did not run the callback. Type `sharedcallback entitycollide 30 12 1 <map>`. Expect `RESPONSE sharedcallback ok` and `TestTarget` becomes hidden. There is no `SCRIPT_CALL` from a replay. Repeating that Command answers `RESPONSE sharedcallback not-found`.

Reload and repeat with the entering `sharedcallback` before the first `entitybodies` sample. The callback must still run once and hide `TestTarget`; arriving samples must not run it again. In both orders, `entityrelease 12 <map>` answers `RESPONSE entityrelease ok 12` without another entering callback.

## Health remains refused; deactivation ends following

Reload the fixture for each destructive check. Type:

```text
entitydrive 12 <map>
entityinteracting 12 1 <map>
exec:SetPropHealth("TestProp", 0)
exec:AddPropHealth("TestProp", -10000)
entityinteracting 12 0 <map>
exec:SetEntityActive("TestProp", false)
entityinteracting 12 1 <map>
entityrelease 12 <map>
```

Expect `RESPONSE entitydrive ok 12` and `RESPONSE entityinteracting ok 12` for the first two mark changes. The health calls must not break the prop or send `entityreleased`. Deactivation hides the prop and sends exactly `EVENT entityreleased 12 <map>` once. The final mark and release Commands answer `RESPONSE entityinteracting not-found 12` and `RESPONSE entityrelease not-found 12`. A later `entitybodies` for it answers `RESPONSE entitybodies not-found 12`; it must stay hidden. Repeating the deactivation sends no second Event.

## Movement and replacement persist

For each line below, reload the fixture, drive entity 12, and send a buffered body sample at (0, 1, 0), then execute the line:

```text
exec:SetEntityPos("TestProp", 5, 1, 0)
exec:SetEntityPosRot("TestProp", 5, 1, 0, 0, 90, 0)
exec:PlaceEntityAtEntity("TestProp", "TestTarget", "", true)
exec:ReplaceEntity("TestProp", "", "Replacement", "<prop-file>", false)
```

Replace `<prop-file>` with the original fixture object's entity file. Each script change sends one `EVENT entityreleased 12 <map>`. Movement stays at the new location across later ticks; old samples must not pull it back. Replacement removes the old prop and shows the replacement. In each case, `entityrelease 12 <map>` answers `RESPONSE entityrelease not-found 12` without another Event.

Finally reload, drive entity 12, then change maps or reconnect the Controller. Cleanup must not send `entityreleased`. The new Map Visit still starts with `EVENT mapentered <new-map>` when negotiated with `interactions`.

The amnesia-multiplayer Peer must handle `entityreleased` by ending the corresponding Hold; this repository does not implement that sibling change. Lever, wheel, and multi-slider state callbacks continue to run from driven motion as before.
