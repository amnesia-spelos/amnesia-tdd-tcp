# Shared Death Controller checks

Run the rebuilt game from an Amnesia installation with the current `mp-test-cs` Custom Story from `../amnesia-multiplayer/resources/custom_stories/mp-test-cs`, then run the Controller as described in the root README. Version 2 output appears as `uncategorised`. Keep death hints on (the default), so the death screen stays up long enough to type a Command.

The fixture is described in [amnesia-multiplayer#53](https://github.com/amnesia-spelos/amnesia-multiplayer/issues/53#issuecomment-6231345835). Its Map Path is `custom_stories/mp-test-cs/maps/multiplayer-test.map`, written `<map>` below. The Grunt is `grunt`, ID `139`, initially inactive at `(6.25, 0, 28)`. The Map Script registers no checkpoint, so the map has only `_auto`. To test a death callback, the steps register `RoomLightColorChanged`, whose `(string, int)` signature a checkpoint callback uses: count `0` turns the room light red, `1` green, `2` blue, and other counts leave it as it is. The player start is `PlayerStartArea_1`. Reload the story and `/reconnect` before each step.

1. Checkpoint report and a vanilla death. Type:

   ```text
   protocol 2 enemies
   exec:SetPlayerHealth(0)
   ```

   Expect `RESPONSE protocol ok 2 enemies`, `EVENT checkpoint 0 0 _auto <map>`, `RESPONSE:exec:script executed`, and `EVENT playerdeath 1 0 0 _auto <map>`. No Avatar is on the map, so the death is vanilla: the death screen ends after the hint, you wake at the start, and `EVENT checkpoint 1 0 _auto <map>` follows.

2. Without a callback, Shared Enemies survive a death. Type:

   ```text
   protocol 2 avatars enemies
   avatarcreate visitor
   avatarpose visitor 1000 0 -2 0 20 0 0 0 1 <map>
   avatarenemysight visitor 1
   exec:ClearEnemyPatrolNodes("grunt")
   exec:SetEntityActive("grunt", true)
   ```

   The Grunt goes for the Avatar in the corridor and swings at it (`EVENT enemyattack 139 ...` lines). Type `exec:SetPlayerHealth(0)`. Expect `EVENT playerdeath 1 0 0 _auto <map>` and the game log line `Shared Death: no death callback, so Shared Enemies survive this death`. The death screen ends after the hint, with no extra wait. After you wake, the Grunt is still active where it was, still on the Avatar. Walk into its view: it turns on you, chases you, and can hit you. Repeat the death once more: the same holds. Without the `avatarpose` line, the same death deactivates the Grunt, as vanilla.

3. A callback death answered by an episode takes the agreed count. Type:

   ```text
   protocol 2 avatars enemies
   avatarcreate visitor
   avatarpose visitor 1000 0 -2 0 20 0 0 0 1 <map>
   exec:SetLightVisible("RoomLight", true)
   exec:CheckPoint("cp_test", "PlayerStartArea_1", "RoomLightColorChanged", "", "")
   exec:SetPlayerHealth(0)
   ```

   Expect `EVENT checkpoint 0 1 cp_test <map>`, then `EVENT playerdeath 1 0 1 cp_test <map>`. While the death screen is up, type:

   ```text
   shareddeath 1 1 2 cp_test <map>
   ```

   Expect `RESPONSE shareddeath ok`. You wake at the start, the room light turns blue (count `2`, not the local `0`), and `EVENT checkpoint 3 1 cp_test <map>` follows. Type the same `shareddeath` line again: expect `RESPONSE shareddeath duplicate` and nothing else.

4. An unanswered callback death waits, then keeps its own count. Continue from step 3 and type `exec:SetPlayerHealth(0)`. Expect `EVENT playerdeath 2 3 1 cp_test <map>`. Do not answer. The death screen stays up about 3 seconds longer than in step 2, then you wake. The game log shows `Shared Death: checkpoint 'cp_test' keeps its own count 3: no matching agreed count arrived`, and `EVENT checkpoint 4 1 cp_test <map>` follows. Now type:

   ```text
   shareddeath 2 2 0 cp_test <map>
   shareddeath 3 0 0 cp_test maps/other.map
   ```

   Expect `RESPONSE shareddeath stale` (that death already woke up) and `RESPONSE shareddeath wrong-map`. You do not die.

5. An episode kills a living player, and resets a driven copy with it. Type:

   ```text
   protocol 2 enemies
   exec:SetLightVisible("RoomLight", true)
   exec:CheckPoint("cp_test", "PlayerStartArea_1", "RoomLightColorChanged", "", "")
   exec:ClearEnemyPatrolNodes("grunt")
   exec:SetEntityActive("grunt", true)
   enemydrive 139 <map>
   enemypose 139 1000 0 -2 0 22 0 - 0 0 <map>
   ```

   The Grunt stands still at `(-2, 0, 22)`. Type:

   ```text
   shareddeath 1 0 1 cp_test <map>
   ```

   Expect `RESPONSE shareddeath ok`, and you die at once. No `EVENT playerdeath` line follows: this death is the episode's, not your own. You wake at the start with the room light green (count `1`), `EVENT checkpoint 2 1 cp_test <map>` follows, and the driven Grunt has gone. Type `enemyrelease 139 <map>`: expect `RESPONSE enemyrelease ok 139`, and the Grunt stays gone.

6. A player not in play is not killed. Start the story and type:

   ```text
   protocol 2 enemies
   exec:SetPlayerActive(false)
   shareddeath 1 0 0 _auto <map>
   exec:SetPlayerActive(true)
   ```

   Expect `RESPONSE shareddeath unavailable` and the log line `Game Interaction Protocol: cannot kill the player for a Shared Death: not in play`. You do not die. Return to the main menu and type `shareddeath 2 0 0 _auto <map>`: expect `RESPONSE shareddeath wrong-map`.

Automated tests cover the episode rules, the death-screen wait, the wire format, and Capability gating. The real-game checks above remain manual. Two games over LAN, a callback that changes maps, simultaneous deaths, unequal death-screen timing, and mismatched registrations need the Game Peer from amnesia-spelos/amnesia-multiplayer#75 and are verified there.
