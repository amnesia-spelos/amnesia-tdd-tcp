# Enemy hallucination Controller checks

Run the rebuilt game from an Amnesia installation with the current `mp-test-cs` Custom Story from `../amnesia-multiplayer/resources/custom_stories/mp-test-cs`, then run the Controller as described in the root README. Version 2 output appears as `uncategorised`.

The fixture is described in [amnesia-multiplayer#53](https://github.com/amnesia-spelos/amnesia-multiplayer/issues/53). Its Map Path is `custom_stories/mp-test-cs/maps/multiplayer-test.map`. The player starts near `(5, 0, 1.3)`. The Grunt is `grunt`, ID `139`, initially inactive at `(6.25, 0, 28)`, with a `HallucinationEndDist` of `3`. The steps make it a hallucination with `SetEnemyIsHallucination`. Reload the story between steps.

1. Alone on the map, a hallucination is vanilla and is not reported. Start the story, start a Controller Session, and type:

   ```text
   protocol 2 enemies
   exec:SetEnemyIsHallucination("grunt", true)
   exec:SetEntityActive("grunt", true)
   ```

   Let the Grunt come to you. When it gets within about 3 m, expect smoke, a dissolve, and the disappearance sound, once. No `EVENT enemydisappear` line appears.

2. The other player alone ends it, and the disappearance is reported. Reload the story, `/reconnect`, and type:

   ```text
   protocol 2 avatars enemies
   avatarcreate visitor
   avatarpose visitor 1000 0 6.25 0 26 0 0 0 0 custom_stories/mp-test-cs/maps/multiplayer-test.map
   exec:SetEnemyIsHallucination("grunt", true)
   exec:SetEntityActive("grunt", true)
   ```

   Stay at the start, about 25 m away. The Avatar stands 2 m from the Grunt and has no enemy sight. Expect the disappearance sound once, and exactly one:

   ```text
   EVENT enemydisappear 139 <timeMs> custom_stories/mp-test-cs/maps/multiplayer-test.map
   ```

   Walk to the Grunt's spot: it is gone.

3. A driven copy plays a delivered disappearance once and stays gone. Reload the story, `/reconnect`, walk forward until you can see down the corridor, and type:

   ```text
   protocol 2 enemies
   exec:SetEntityActive("grunt", true)
   enemydrive 139 custom_stories/mp-test-cs/maps/multiplayer-test.map
   enemypose 139 1000 0 6.25 0 20 0 - 0 0 custom_stories/mp-test-cs/maps/multiplayer-test.map
   ```

   The Grunt stands at `(6.25, 0, 20)`. Type:

   ```text
   enemydisappear 139 1500 custom_stories/mp-test-cs/maps/multiplayer-test.map
   ```

   Expect `RESPONSE enemydisappear ok 139`, and smoke, a dissolve, and the sound, once. Then type each line below. Expect `RESPONSE enemydisappear ok 139` for the first, `RESPONSE enemyrelease ok 139` and `RESPONSE enemydrive ok 139` for the release and drive, and nothing for the pose. After each line the Grunt stays gone, with no smoke and no sound:

   ```text
   enemydisappear 139 1500 custom_stories/mp-test-cs/maps/multiplayer-test.map
   enemypose 139 1400 0 6.25 0 20 0 - 0 0 custom_stories/mp-test-cs/maps/multiplayer-test.map
   enemyrelease 139 custom_stories/mp-test-cs/maps/multiplayer-test.map
   enemydrive 139 custom_stories/mp-test-cs/maps/multiplayer-test.map
   ```

4. A pose sampled after the disappearance brings the copy back. Continuing from step 3, type:

   ```text
   enemypose 139 2000 0 6.25 0 18 0 - 0 0 custom_stories/mp-test-cs/maps/multiplayer-test.map
   ```

   Expect the Grunt to reappear at `(6.25, 0, 18)`, with no smoke and no sound.

5. A disappearance delivered before the copy is driven still plays, and driving it afterwards does not bring it back. Reload the story, `/reconnect`, walk forward until you can see down the corridor, and type:

   ```text
   protocol 2 enemies
   exec:SetEntityActive("grunt", true)
   enemydisappear 139 1500 custom_stories/mp-test-cs/maps/multiplayer-test.map
   ```

   Expect `RESPONSE enemydisappear ok 139`, and smoke, a dissolve, and the sound, once. Then type:

   ```text
   enemydrive 139 custom_stories/mp-test-cs/maps/multiplayer-test.map
   enemypose 139 1400 0 6.25 0 20 0 - 0 0 custom_stories/mp-test-cs/maps/multiplayer-test.map
   ```

   Expect `RESPONSE enemydrive ok 139`, and the Grunt stays gone.
