# Enemy copy Controller checks

Run the rebuilt game from an Amnesia installation, then run the Controller as described in the root README. Version 2 output appears as `uncategorised`. These checks require a disposable test map at `custom_stories/enemy-copy/maps/test.map`, with a flat floor at feet height zero and a map-placed Grunt named `grunt_1` with entity ID `7` near `(0, 0, 0)`. Add a second active Grunt with ID `8` to verify enumeration. Keep the player far enough from the test positions to see both. Register a Grunt callback and a collide callback (also try the Grunt as the child) that visibly increment a Map Script counter. Use the model's `Walk` clip. The numeric fixture below deliberately uses these exact IDs and path; adapt the fixture if using another map.

1. Start a fresh Controller Session and type:

   ```text
   protocol 2
   enemyposes subscribe 20
   enemydrive 7 custom_stories/enemy-copy/maps/test.map
   ```

   Expect `RESPONSE protocol ok 2`, then `RESPONSE enemyposes not-granted` and `RESPONSE enemydrive not-granted`. No `STATE enemypose` lines should arrive; the Grunt remains vanilla.

2. Type `/reconnect`, then:

   ```text
   protocol 2 enemies
   enemyposes subscribe 20
   ```

   Expect `RESPONSE protocol ok 2 enemies` and `RESPONSE enemyposes ok subscribe 20`, followed by lines beginning `STATE enemypose 7 ` and `STATE enemypose 8 `, ending with the fixture Map Path. The timestamps, positions, yaw, clip, and time vary with the game. Deactivate the second Grunt with `exec:SetEntityActive("grunt_2", false)` if named `grunt_2`: expect `RESPONSE:exec:script executed` and no further samples for ID 8. Spawn a runtime Grunt through the fixture Map Script and verify its assigned entity ID also appears in reports. Pause/unpause and return to the menu: reports must stop in the menu. Load the fixture again.

3. Stop reports to make responses easy to see and start a copy:

   ```text
   enemyposes unsubscribe
   enemydrive 7 custom_stories/enemy-copy/maps/test.map
   enemypose 7 1000 0 0 0 0 0 Walk 0.2 1 custom_stories/enemy-copy/maps/test.map
   enemypose 7 1100 0 1 0 0 90 Walk 0.4 1 custom_stories/enemy-copy/maps/test.map
   ```

   Expect `RESPONSE enemyposes ok unsubscribe`, `RESPONSE enemydrive ok 7`, and no sample responses. The Grunt follows the samples and holds the final pose/clip time. For a smoothness check, pipe a recording of these `enemypose` lines at 20 Hz through the Controller (which forwards lines unchanged), with steadily increasing sender times, positions, yaw and clip time. Inspect smooth motion and clip playback, then stop the stream and inspect the hold. Two manually typed lines alone cannot establish network-rate smoothness.

4. Walk into attack range and trigger the registered collide/LookAt callbacks while it is driven. The Grunt must neither chase nor damage the player or props, and callback counters must not change. Then type:

   ```text
   enemyrelease 7 custom_stories/enemy-copy/maps/test.map
   ```

   Expect `RESPONSE enemyrelease ok 7`; vanilla sensing and movement resume. Repeat step 3 and disconnect the Controller instead: simulation must likewise resume. Also stream an action clip that is not speed-dependent (for example `SwingLaunch`, if present in the model), then `Walk`, and release. Trigger that vanilla attack again: it must advance through its hit and completion events, rather than freeze. Repeat after saving while driven and loading that save. This checks restoration of every touched clip, including clips that were no longer current at release.

5. Drive again, then leave and re-enter the fixture or load a save on it. On the still-connected Session type:

   ```text
   enemypose 7 2000 0 2 0 0 0 Walk 0.6 1 custom_stories/enemy-copy/maps/test.map
   ```

   Expect `RESPONSE enemypose not-found 7`, and a vanilla enemy. A fresh `enemydrive` must be required. A checkpoint death without leaving the Map Visit should retain the driven copy. Save while driving and load it: no frozen animation or interrupted local attack should be resurrected.

Automated loopback tests cover wire parsing, Capability gating, and Session cleanup; model tests cover interpolation and delayed animation selection. The real-game checks above remain manual.
