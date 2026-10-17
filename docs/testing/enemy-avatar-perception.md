# Enemy perception of Avatars: Controller checks

Build the game with `scripts/build-windows.ps1 -Rebuild` and run it from an Amnesia installation containing the current `mp-test-cs` Custom Story from `../amnesia-multiplayer/resources/custom_stories/mp-test-cs`. Run the Controller as described in the root README. Protocol Version 2 output appears as `uncategorised`.

The existing encounter is documented in [amnesia-multiplayer#53](https://github.com/amnesia-spelos/amnesia-multiplayer/issues/53). Its Map Path is `custom_stories/mp-test-cs/maps/multiplayer-test.map`. `grunt` (139) starts inactive at `(6.25, 0, 28)`. Entering `SpawnGruntArea` at `(-2, 1.875, 18.875)` activates it and queues `PathNodeArea_6` (5 s), `_13` (10 s), `_2` (5 s), repeated five times. The patrol is finite. `mansion_1` (136) is the breakable door at `(-2, 0, 14)`; `cabinet_nice_1` (137) is the closet at `(3.25, 0, 12)`. The point lights are `PointLight_2` (156) at `(-2, 2.5, 28.25)` and `PointLight_3` (157) at `(-0.5, 2.5, 9.75)`; `BoxLight_3` (155) supplies faint ambient light.

Use a disposable run of the fixture. The setup below clears its patrol and controls the existing lights to make the comparisons repeatable. Reload the story between independent comparisons; do not let the Grunt reach the test Avatar before comparing dark and lantern conditions. Keep the local lantern lowered, stand still, and avoid making sounds. Pose successes have no Response; wait at least one second after a Pose for interpolation and three sight checks. A changed Pose may briefly count as motion; wait for it to settle before judging darkness.

1. Establish the opt-in boundary in a fresh Controller Session:

   ```text
   protocol 2
   avatarenemysight visitor 1
   ```

   Expect `RESPONSE protocol ok 2`, then `RESPONSE avatarenemysight not-granted`. Type `/reconnect`, then:

   ```text
   protocol 2 avatars
   avatarcreate visitor
   avatarpose visitor 1000 0 -2 0 15 0 0 0 0 custom_stories/mp-test-cs/maps/multiplayer-test.map
   exec:ClearEnemyPatrolNodes("grunt")
   exec:SetEntityPosRot("grunt", -2, 0, 22, 0, 0, 0)
   exec:SetPlayerPos(-2, 0, 30)
   exec:SetLightVisible("PointLight_2", false)
   exec:SetLightVisible("PointLight_3", false)
   exec:SetEntityActive("grunt", true)
   ```

   Expect `RESPONSE protocol ok 2 avatars`, `RESPONSE avatarcreate ok visitor`, no Pose Response, and `RESPONSE:exec:SUCCESS` for every `exec:` line. The local player is behind the Grunt, outside its initial FOV; the Avatar is seven metres in front. Without opt-in the Avatar must not attract the Grunt, even if its lantern is raised in a later Pose.

2. Enable perception while the Avatar is still in darkness:

   ```text
   avatarenemysight visitor 1
   ```

   Expect `RESPONSE avatarenemysight ok visitor`. Once motion settles, the stationary unlit Avatar at seven metres is outside the Grunt's five metre darkness range. The Grunt must not acquire it. Then type:

   ```text
   avatarpose visitor 2000 0 -2 0 15 0 0 0 1 custom_stories/mp-test-cs/maps/multiplayer-test.map
   ```

   Expect no Response. The Grunt must acquire the Avatar and progress through Alert to Hunt toward it, even though the local player remains behind it. Attacks may swing and miss. Repeat from a fresh fixture run with the lantern down and `exec:SetLightVisible("PointLight_3", true)` instead: expect `RESPONSE:exec:SUCCESS` and acquisition in the existing point light. If necessary move the Avatar nearer that light, while keeping it farther than five metres from the Grunt. Repeat with crouch `1`, observing the reduced unlit acquisition range. Compare against a local player at the same feet position and stance in a separate run.

3. Test sticky selection in a fresh run of step 1, with opt-in and the Avatar lantern raised. Once the Grunt is chasing the Avatar, make the local player visible and closer, while keeping the Avatar visible:

   ```text
   exec:SetPlayerPos(-1, 0, 20)
   ```

   Expect `RESPONSE:exec:SUCCESS`. The Grunt must continue toward the Avatar. Then move the Avatar to a different map:

   ```text
   avatarpose visitor 3000 1 -2 0 15 0 0 0 1 maps/not-the-current-map.map
   ```

   Expect no Response. The Avatar becomes dormant; after the local player is perceived, the Grunt must switch to the local player. Reverse the order in another run: first establish a local-player chase, then introduce a closer visible Avatar. It must retain the local player until that player is hidden behind the existing closet or a wall, then acquire the visible Avatar.

4. Test target loss and search with a fresh Avatar chase. Before the Grunt closes within five metres, lower the Avatar lantern in a stationary Pose while both point lights remain off:

   ```text
   avatarpose visitor 4000 0 -2 0 15 0 0 0 0 custom_stories/mp-test-cs/maps/multiplayer-test.map
   ```

   Expect no Response. After the Pose settles, the enemy must lose sight and enter its normal delayed search around the Avatar, rather than the local player. Repeat by moving the Avatar behind the existing door/closet to check occlusion. Removing it must also lose the target:

   ```text
   avatarremove visitor
   ```

   Expect `RESPONSE avatarremove ok visitor`. Recreating `visitor` must start with enemy perception off. Repeat with `avatarenemysight visitor 0` (expect `RESPONSE avatarenemysight ok visitor`) and with `/reconnect`: both remove the opted-in candidate. Leave and reload the map while the Session remains connected: the setting persists, but the Avatar is eligible only while awake on that map.

The base fixture contains one Grunt. Brute, Kaernk and multiple-enemy help checks require explicitly adding those enemies to a disposable fixture. The Brute shares the Grunt AI with different range values. For Kaernk compare local and Avatar feet heights against the water surface: detection is height-based, immediate and independent of light. For help, have one Grunt chase an Avatar within another Grunt's activation radius; the second should investigate/chase that Avatar's location, while a visible current target remains sticky. Local death must not interrupt an existing Avatar chase before checkpoint loading. Checkpoint resets and damage to Avatars belong to later tickets.

Automated tests cover the gateway, acquisition counts, sticky choice, darkness/crouch/lantern range rules and Session cleanup. Real light sampling, ray casts, state timing, navigation and map transitions require these game checks. They have not been executed as part of implementation.

The consumer in `../amnesia-multiplayer` currently creates Avatars without this opt-in. Its Shared Enemy coordinator must send `avatarenemysight <id> 1` on the simulating game while players share a map and `0` when they stop sharing it; that coordinated work belongs in that repository. Existing consumers remain vanilla until they enable it.
