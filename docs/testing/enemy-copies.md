# Enemy copy Controller checks

Run the rebuilt game from an Amnesia installation with the current `mp-test-cs` Custom Story from `../amnesia-multiplayer/resources/custom_stories/mp-test-cs`, then run the Controller as described in the root README. Version 2 output appears as `uncategorised`.

Use the existing encounter documented in [amnesia-multiplayer#53's entity table](https://github.com/amnesia-spelos/amnesia-multiplayer/issues/53#issuecomment-6231345835). Its Map Path is `custom_stories/mp-test-cs/maps/multiplayer-test.map`. The Grunt is named `grunt`, ID `139`, initially inactive at `(6.25, 0, 28)`. Walk through `SpawnGruntArea` at `(-2, 1.875, 18.875)` to activate it and start the patrol. The patrol queues `PathNodeArea_6`, `_13`, then `_2` five times; it is finite. Use the existing door `mansion_1` (136), closet `cabinet_nice_1` (137), and corridor lighting for encounter checks.

The base fixture contains one Grunt. Multiple-enemy enumeration, runtime spawning, and callback-counter checks require explicit additions to a disposable copy of the fixture; they are supplemental checks, not existing map features. Use animation names observed in `STATE enemypose` reports; the pose examples below use `Walk`.

1. Start a fresh Controller Session and type:

   ```text
   protocol 2
   enemyposes subscribe 20
   enemydrive 139 custom_stories/mp-test-cs/maps/multiplayer-test.map
   ```

   Expect `RESPONSE protocol ok 2`, then `RESPONSE enemyposes not-granted` and `RESPONSE enemydrive not-granted`. No `STATE enemypose` lines should arrive; the Grunt remains vanilla.

2. Type `/reconnect`, then:

   ```text
   protocol 2 enemies
   enemyposes subscribe 20
   ```

   Expect `RESPONSE protocol ok 2 enemies` and `RESPONSE enemyposes ok subscribe 20`. Before activation there should be no report for the Grunt. Enter `SpawnGruntArea`: lines beginning `STATE enemypose 139 ` should now arrive, ending with the fixture Map Path. Timestamps, positions, yaw, clip, and time vary with the game. The entity ID must match the #53 table.

   Type `exec:SetEntityActive("grunt", false)`: expect `RESPONSE:exec:script executed` and no further samples for ID 139. Type `exec:SetEntityActive("grunt", true)` to restore it; expect the same Response and resumed reports. Pause/unpause and return to the menu: reports must stop in the menu. Load the fixture again and enter `SpawnGruntArea` before continuing.

3. Stop reports to make responses easy to see and start a copy:

   ```text
   enemyposes unsubscribe
   enemydrive 139 custom_stories/mp-test-cs/maps/multiplayer-test.map
   enemypose 139 1000 0 -2 0 28.25 0 Walk 0.2 1 custom_stories/mp-test-cs/maps/multiplayer-test.map
   enemypose 139 1100 0 -2 0 27.25 90 Walk 0.4 1 custom_stories/mp-test-cs/maps/multiplayer-test.map
   ```

   Expect `RESPONSE enemyposes ok unsubscribe`, `RESPONSE enemydrive ok 139`, and no sample responses. The Grunt follows the samples and holds the final pose/clip time. For a smoothness check, pipe a recording of these `enemypose` lines at 20 Hz through the Controller (which forwards lines unchanged), with steadily increasing sender times, positions, yaw and clip time. Inspect smooth motion and clip playback, then stop the stream and inspect the hold. Two manually typed lines alone cannot establish network-rate smoothness.

4. Walk into attack range and approach the existing door while it is driven. The Grunt must neither chase nor damage the player or props. For the supplemental callback check, first register collide/LookAt callbacks that increment visible counters in a disposable copy of the Map Script (including a collide registration with the Grunt as the child); those counters must not change while it is driven. Then type:

   ```text
   enemyrelease 139 custom_stories/mp-test-cs/maps/multiplayer-test.map
   ```

   Expect `RESPONSE enemyrelease ok 139`; vanilla sensing and movement resume. Repeat step 3 and disconnect the Controller instead: simulation must likewise resume. Also stream an action clip that is not speed-dependent (for example `SwingLaunch`, if present in the model), then `Walk`, and release. Trigger that vanilla attack again: it must advance through its hit and completion events, rather than freeze. Repeat after saving while driven and loading that save. This checks restoration of every touched clip, including clips that were no longer current at release.

5. Drive again, then leave and re-enter the fixture or load a save on it. On the still-connected Session type:

   ```text
   enemypose 139 2000 0 -2 0 28.25 0 Walk 0.6 1 custom_stories/mp-test-cs/maps/multiplayer-test.map
   ```

   Expect `RESPONSE enemypose not-found 139`, and a vanilla enemy. A fresh `enemydrive` must be required. A checkpoint death without leaving the Map Visit should retain the driven copy. Save while driving and load it: no frozen animation or interrupted local attack should be resurrected.

Automated loopback tests cover wire parsing, Capability gating, and Session cleanup; model tests cover interpolation and delayed animation selection. The real-game checks above remain manual.
