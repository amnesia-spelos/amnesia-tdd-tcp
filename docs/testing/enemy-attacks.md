# Enemy attack Controller checks

Run the rebuilt game from an Amnesia installation with the current `mp-test-cs` Custom Story from `../amnesia-multiplayer/resources/custom_stories/mp-test-cs`, then run the Controller as described in the root README. Version 2 output appears as `uncategorised`.

The fixture is described in [amnesia-multiplayer#53](https://github.com/amnesia-spelos/amnesia-multiplayer/issues/53#issuecomment-6231345835). Its Map Path is `custom_stories/mp-test-cs/maps/multiplayer-test.map`. The Grunt is `grunt`, ID `139`, initially inactive at `(6.25, 0, 28)`; entering `SpawnGruntArea` activates it and starts a finite patrol. Reload the story between steps.

1. Learn the reported box. Start the story, start a Controller Session, and type:

   ```text
   protocol 2 enemies
   ```

   Expect `RESPONSE protocol ok 2 enemies`. Enter `SpawnGruntArea` and let the Grunt find and hit you. Each swing prints one line at the moment of the hit test, whether it hits or not:

   ```text
   EVENT enemyattack 139 normal <sizeX> <sizeY> <sizeZ> <offsetX> <offsetY> <offsetZ> custom_stories/mp-test-cs/maps/multiplayer-test.map
   ```

   A swing that lands hits you the vanilla way. Copy the six box numbers for the next step.

2. Resolve an attack on a driven copy. Reload the story, `/reconnect`, and type, putting your box numbers in place of `<box>`:

   ```text
   protocol 2 enemies
   exec:SetEntityActive("grunt", true)
   exec:ClearEnemyPatrolNodes("grunt")
   enemydrive 139 custom_stories/mp-test-cs/maps/multiplayer-test.map
   enemypose 139 1000 0 -2 0 22 0 - 0 0 custom_stories/mp-test-cs/maps/multiplayer-test.map
   ```

   Expect `RESPONSE protocol ok 2 enemies`, `RESPONSE:exec:script executed` twice, `RESPONSE enemydrive ok 139`, and no Response to the Pose. The Grunt stands still at `(-2, 0, 22)`. Walk up and stand right in front of its face, then type:

   ```text
   enemyattack 139 normal <box> custom_stories/mp-test-cs/maps/multiplayer-test.map
   ```

   Expect `RESPONSE enemyattack ok 139`, and a vanilla hit: health lost, blood splash, red flash, head spin, a push away from the Grunt, and the hit sound. Repeat it a few times: the damage varies.

3. Miss. Walk behind the Grunt, or three metres away, and type the same `enemyattack` line. Expect `RESPONSE enemyattack ok 139` and nothing else: no damage, no effects.

4. Bad attacks. Type:

   ```text
   enemyattack 139 normal 9 9 9 0 0 1 custom_stories/mp-test-cs/maps/multiplayer-test.map
   enemyattack 139 normal <box> maps/other.map
   enemyrelease 139 custom_stories/mp-test-cs/maps/multiplayer-test.map
   enemyattack 139 normal <box> custom_stories/mp-test-cs/maps/multiplayer-test.map
   ```

   Expect `RESPONSE enemyattack invalid`, `RESPONSE enemyattack wrong-map 139`, `RESPONSE enemyrelease ok 139`, then `RESPONSE enemyattack not-found 139` (a vanilla Grunt is not driven). None of them hurt you.

5. An attack on an Avatar does nothing. Reload the story, `/reconnect`, and type:

   ```text
   protocol 2 avatars enemies
   avatarcreate visitor
   avatarpose visitor 1000 0 -2 0 20 0 0 0 1 custom_stories/mp-test-cs/maps/multiplayer-test.map
   avatarenemysight visitor 1
   exec:SetPlayerPos(3.25, 0, 9.75)
   exec:ClearEnemyPatrolNodes("grunt")
   exec:SetEntityPosRot("grunt", -2, 0, 24, 0, 0, 0)
   exec:SetEntityActive("grunt", true)
   ```

   The Avatar stands in the corridor with its lantern raised, and you stand away by the cabinets. The Grunt goes for the Avatar and swings at it: each swing prints an `EVENT enemyattack 139 ...` line, but the Avatar does not move, and you take no damage and see no effects.

Automated loopback tests cover the wire format, Capability gating, and map checks. The real-game checks above remain manual.
