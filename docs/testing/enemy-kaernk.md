# Kaernk Shared Enemy Controller checks

Run the rebuilt game from an Amnesia installation, then run the Controller as described in the root README. Version 2 output appears as `uncategorised`.

The fixture is the base game's Archives Cellar, `maps/main/ch01/07_archives_cellar.map`, which has water, meat, and four Kaernks, all initially inactive. `mp-test-cs` has no Kaernk. To get there, type `startmainstory` in the main menu and, once you are in control, type:

```text
exec:ChangeMap("07_archives_cellar.map", "PlayerStartArea_1", "", "")
```

Copy the Map Path from the first `STATE enemypose` or `EVENT checkpoint` line; it is written `<map>` below. Each step uses one Kaernk:

| Name | ID | Position |
| --- | --- | --- |
| `waterlurker_1` | `5490` | `(125.91, -4, 6.26)` |
| `waterlurker_3` | `5745` | `(137, -4, 4.25)` |

`PlayerStartArea_1`, at `(107.75, -4, -52.25)`, is about 60 m from both Kaernks, out of their 20 m activation range. A Kaernk senses anyone in the water at its own height, however far away, once someone is in range. So stay at the start unless a step says to move. Load the map again with the `ChangeMap` line, and `/reconnect`, between steps.

1. A Kaernk is reported at Pose rate. Type:

   ```text
   protocol 2 enemies
   enemyposes subscribe 20
   exec:SetEntityActive("waterlurker_3", true)
   ```

   Expect `RESPONSE protocol ok 2 enemies` and `RESPONSE enemyposes ok subscribe 20`. Then `STATE enemypose 5745 ...` lines arrive, with `-` for the clip because the Kaernk has no animations, ending with `<map>`. Type `enemyposes unsubscribe` to stop them.

2. On the Session Host, a Kaernk hunts an Avatar in the water, and its attacks are reported. Type:

   ```text
   protocol 2 avatars enemies
   avatarcreate visitor
   avatarpose visitor 1000 0 125.91 -4 6.26 0 0 0 0 <map>
   avatarenemysight visitor 1
   exec:SetEntityActive("waterlurker_3", true)
   ```

   The Avatar stands in the water about 11 m from the Kaernk. Expect the Kaernk's hunt sounds, then running splashes heading for the Avatar, then attack splashes. Each attack prints one line:

   ```text
   EVENT enemyattack 5745 normal <sizeX> <sizeY> <sizeZ> <offsetX> <offsetY> <offsetZ> <map>
   ```

   The Avatar does not react, and you take no damage. Copy the six box numbers for step 4.

3. A driven copy splashes as it moves. Type:

   ```text
   protocol 2 enemies
   exec:SetEntityActive("waterlurker_1", true)
   enemydrive 5490 <map>
   ```

   Expect `RESPONSE enemydrive ok 5490`. Walk to the edge of the water near `(125.91, -4, 6.26)`, without stepping in. Paste both lines at once:

   ```text
   enemypose 5490 1000 0 125.91 -4 6.26 0 - 0 0 <map>
   enemypose 5490 3000 0 125.91 -4 9.46 0 - 0 0 <map>
   ```

   The copy walks 3.2 m in 2 s. Expect about two walk splashes with the medium water impact sound, then nothing. Paste:

   ```text
   enemypose 5490 4000 0 125.91 -4 9.46 0 - 0 0 <map>
   enemypose 5490 6000 0 125.91 -4 1.46 0 - 0 0 <map>
   ```

   The copy runs 8 m in 2 s. Expect about four run splashes, closer together, with the high water impact sound. A copy standing still never splashes, and stepping into the water does not make it hunt you.

4. A driven copy shows reported bites and attacks. Keep driving `5490` from step 3, stand at the water's edge in front of the copy, and type, putting your box numbers in place of `<box>`:

   ```text
   enemyeat 5490 <map>
   enemyattack 5490 normal <box> <map>
   enemyattack 5490 breakdoor <box> <map>
   ```

   Expect `RESPONSE enemyeat ok 5490` with an eat splash and the Kaernk's eating sound. The first `enemyattack` gets `RESPONSE enemyattack ok 5490` with an attack splash, and a hit if you are within reach. The `breakdoor` attack gets `RESPONSE enemyattack ok 5490` with no splash. Type `enemyrelease 5490 <map>`, then `enemyeat 5490 <map>`: expect `RESPONSE enemyrelease ok 5490`, then `RESPONSE enemyeat not-found 5490`, because only a driven copy shows a reported bite.

5. A simulated Kaernk's bites are reported. Type:

   ```text
   protocol 2 enemies
   exec:SetEntityActive("waterlurker_3", true)
   ```

   Pick up a piece of meat and, staying out of the water, throw it into the water near `(137, -4, 4.25)`. The Kaernk walks to it, splashing, and eats it. Each bite prints:

   ```text
   EVENT enemyeat 5745 <map>
   ```

6. A death that preserves Shared Enemies leaves the Kaernk active. Repeat step 2's setup and let the Kaernk start attacking the Avatar. Type `exec:SetPlayerHealth(0)`. Expect `EVENT playerdeath 1 0 0 _auto <map>` and the game log line `Shared Death: no death callback, so Shared Enemies survive this death`. After you wake, the Kaernk is still splashing at the Avatar. Without the `avatarpose` line, the same death deactivates the Kaernk, as vanilla.

7. A coordinated death resets a driven copy. Type:

   ```text
   protocol 2 enemies
   exec:CheckPoint("check01", "PlayerStartArea_1", "CheckPoint01", "Hints", "DeathWaterLurker01")
   exec:SetEntityActive("waterlurker_1", true)
   enemydrive 5490 <map>
   enemypose 5490 1000 0 125.91 -4 6.26 0 - 0 0 <map>
   shareddeath 1 0 1 check01 <map>
   ```

   Expect `RESPONSE shareddeath ok`, and you die at once. After you wake, `EVENT checkpoint 2 1 check01 <map>` follows. Type `enemyrelease 5490 <map>`: expect `RESPONSE enemyrelease ok 5490`, and the Kaernk stays inactive (no splashes, no `STATE enemypose 5490` lines on `enemyposes subscribe 20`).

A Kaernk can break doors: one stuck at a closed door while hunting attacks it with its `breakdoor` attack (`BreakDoorAttackDamage` `10 20`, strength `3`). That attack is reported as `EVENT enemyattack <id> breakdoor ...`, and a door it breaks as `EVENT enemydoorbreak`, the same as a Grunt's; the Archives Cellar has no door in the Kaernks' water, so [the door break checks](enemy-doors-and-callbacks.md) cover that path with the Grunt.

Automated tests cover the `enemyeat` wire format, Capability gating, and map checks, as well as the splash cadence a copy's movement implies. The real-game checks above, and the two-PC LAN check from the issue, remain manual. The Game Peer in `../amnesia-multiplayer` relays `enemyeat` only once it learns the Event; until then a Joining Player's copy shows walk, run, and attack splashes but no bites.
