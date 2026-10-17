# Enemy door break and enemy-triggered callback Controller checks

Run the rebuilt game from an Amnesia installation with the current `mp-test-cs` Custom Story from `../amnesia-multiplayer/resources/custom_stories/mp-test-cs`, then run the Controller as described in the root README. Version 2 output appears as `uncategorised`.

The fixture is described in [amnesia-multiplayer#53](https://github.com/amnesia-spelos/amnesia-multiplayer/issues/53#issuecomment-6231345835). Its Map Path is `custom_stories/mp-test-cs/maps/multiplayer-test.map`. The Grunt is `grunt`, ID `139`, initially inactive at `(6.25, 0, 28)`. `SpawnGruntArea` is ID `154`, the box around `(-2, 1.9, 18.9)` in the corridor. Reload the story between steps.

Steps 1–3 need nothing beyond the fixture. Steps 4–5 need a door, which the fixture does not have yet. See the setup before step 4.

1. A Grunt walking into an area runs the callback once, and shares it. Start the story, start a Controller Session, and type:

   ```text
   protocol 2 avatars sharedcallbacks enemies
   avatarcreate visitor
   avatarpose visitor 1000 0 5 0 2 0 0 0 0 custom_stories/mp-test-cs/maps/multiplayer-test.map
   exec:AddEntityCollideCallback("grunt", "SpawnGruntArea", "SpawnGrunt", false, 0)
   exec:SetEntityPosRot("grunt", -2, 0, 24, 0, 0, 0)
   exec:SetEntityActive("grunt", true)
   exec:ClearEnemyPatrolNodes("grunt")
   exec:AddEnemyPatrolNode("grunt", "PathNodeArea_13", 0, "")
   ```

   The awake Avatar puts another player on the map, so the Grunt is a Shared Enemy. Stay at the start. When the Grunt walks south through the area, expect exactly one each of:

   ```text
   SCRIPT_CALL:SpawnGrunt("grunt", "SpawnGruntArea", 1)
   EVENT sharedcallback entitycollide 139 154 1 custom_stories/mp-test-cs/maps/multiplayer-test.map
   ```

   and, when it leaves the area, the same two lines with `-1`.

2. Without the other player, nothing is shared. Reload the story, `/reconnect`, and repeat step 1 without the `avatarcreate` and `avatarpose` lines. Expect the `SCRIPT_CALL` lines and no `EVENT sharedcallback` line.

3. A driven copy never triggers the callback itself. The Command runs it once. Reload the story, `/reconnect`, and type:

   ```text
   protocol 2 sharedcallbacks enemies
   exec:AddEntityCollideCallback("grunt", "SpawnGruntArea", "SpawnGrunt", false, 0)
   exec:SetEntityActive("grunt", true)
   enemydrive 139 custom_stories/mp-test-cs/maps/multiplayer-test.map
   enemypose 139 1000 0 -2 0 24 0 - 0 0 custom_stories/mp-test-cs/maps/multiplayer-test.map
   enemypose 139 2000 0 -2 0 19 0 - 0 0 custom_stories/mp-test-cs/maps/multiplayer-test.map
   ```

   The Grunt now stands inside the area. Expect no `SCRIPT_CALL` and no `EVENT sharedcallback`. Then type:

   ```text
   sharedcallback entitycollide 139 154 1 custom_stories/mp-test-cs/maps/multiplayer-test.map
   sharedcallback entitycollide 139 154 1 custom_stories/mp-test-cs/maps/multiplayer-test.map
   enemyrelease 139 custom_stories/mp-test-cs/maps/multiplayer-test.map
   ```

   Expect `RESPONSE sharedcallback ok`, then `RESPONSE sharedcallback not-found` (the copy already counts as inside), and `RESPONSE enemyrelease ok 139` with no `SCRIPT_CALL`: releasing the Grunt while it stands inside does not run the entering callback again.

**Setup for steps 4–5.** Add a breakable swing door to `multiplayer-test.map` in the corridor the Grunt patrols, such as a door a Grunt breaks in the Main Story, named `test_door`. Look up its ID in the map file and use it as `<door>`. Run `exec:SetSwingDoorLocked("test_door", true, false)` so the Grunt has to break it.

4. A Grunt breaking the door reports it. Start the story and a Session with `protocol 2 enemies`. Stand behind the locked door and let the Grunt find you. It attacks the door until it breaks. Expect one `EVENT enemyattack 139 breakdoor ...` line per swing, then, as the door breaks:

   ```text
   EVENT enemydoorbreak <door> custom_stories/mp-test-cs/maps/multiplayer-test.map
   ```

   A door you broke yourself, for example with `exec:SetPropHealth("test_door", 0)`, sends no such line.

5. Breaking this game's own copy. Reload the story, `/reconnect`, and type:

   ```text
   protocol 2 interactions enemies
   ```

   Grab the door and hold it open, then type:

   ```text
   enemydoorbreak <door> custom_stories/mp-test-cs/maps/multiplayer-test.map
   enemydoorbreak <door> custom_stories/mp-test-cs/maps/multiplayer-test.map
   enemydoorbreak 139 custom_stories/mp-test-cs/maps/multiplayer-test.map
   enemydoorbreak <door> maps/other.map
   ```

   Expect `EVENT interactionstart <door> ...` when you grab it. After the first line, the door breaks in your hands into its debris, with sound and particles. Expect `EVENT interactionend <door> <body> destroyed ...`, `EVENT reportbroke <door> ...`, and `RESPONSE enemydoorbreak ok <door>`. Then `RESPONSE enemydoorbreak ok <door>` again with nothing changing, `RESPONSE enemydoorbreak wrong-type 139`, and `RESPONSE enemydoorbreak wrong-map <door>`. No `EVENT enemydoorbreak` is sent for these.

Automated tests cover the wire format, Capability gating, map checks, and which collides and patrol ends are shared. `autodisabled` and `ShowEnemyPlayerPosition` during a Shared Callback need Map Script callbacks the fixture does not have, so they are not covered here.
