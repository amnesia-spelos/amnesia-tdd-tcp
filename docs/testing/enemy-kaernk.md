# Kaernk Shared Enemy Controller checks

Run the rebuilt game from an Amnesia installation, then run the Controller as described in the root README. Version 2 output appears as `uncategorised`.

The fixture is the base game's Archives Cellar, Map Path `maps/main/ch01/07_archives_cellar.map`, written `<map>` below. It has water, meat, and Kaernks. `mp-test-cs` has no Kaernk. In the main menu, start a Controller Session and type, as its first lines:

```text
protocol 2 avatars enemies
startmainstory
```

`protocol` must be the first line of a Session; anything before it answers `RESPONSE protocol too-late`. Once you are in control in the first map, type:

```text
exec:ChangeMap("07_archives_cellar.map", "PlayerStartArea_x4", "", "")
```

You arrive on a ledge at `(143.5, -3, 18)`, about 1 m above the floor of the water in front of you. A Kaernk senses only players within 0.6 m of its own height, so it never senses you there. **Do not move** unless a step says so: the Map Script's areas start the map's own Kaernk encounters. Arriving runs `CollidePastFirstGate`, which registers checkpoint `check02` (with a callback), activates `waterlurker_2`, and deactivates the others:

| Name | ID | Position | On arrival |
| --- | --- | --- | --- |
| `waterlurker_2` | `5571` | `(150.5, -4, 6.26)` | active |
| `waterlurker_1` | `5490` | `(125.91, -4, 6.26)` | inactive |

Between steps, reconnect the Controller and repeat the setup, or carry on in the same Session where a step says so.

1. A Kaernk is reported at Pose rate. Type:

   ```text
   enemyposes subscribe 20
   ```

   Expect `RESPONSE enemyposes ok subscribe 20`, then `STATE enemypose 5571 ...` lines every 50 ms, with `-` for the clip because the Kaernk has no animations, ending with `<map>`. Type `enemyposes unsubscribe`.

2. On the Session Host, a Kaernk hunts an Avatar in the water, and its attacks are reported. Type:

   ```text
   avatarcreate visitor
   avatarpose visitor 1000 0 146 -4 6.26 0 0 0 0 <map>
   avatarenemysight visitor 1
   ```

   The Avatar stands in the water about 4.5 m from the Kaernk and 12 m in front of you. Within seconds the Kaernk splashes over to it and attacks it about every 2.5 s, with attack splashes. Each attack prints:

   ```text
   EVENT enemyattack 5571 normal 1.0000 1.0000 1.3000 0.0000 0.0000 1.0000 <map>
   ```

   The Avatar does not react, and you take no damage. Watching a hunting Kaernk drains sanity as vanilla; `exec:SetPlayerSanity(100)` restores it.

3. A death that preserves Shared Enemies leaves the Kaernk active. While the Kaernk attacks the Avatar, type:

   ```text
   exec:CheckPoint("nocb", "PlayerStartArea_x4", "", "", "")
   exec:SetPlayerHealth(0)
   ```

   Expect `EVENT checkpoint 0 0 nocb <map>`, then `EVENT playerdeath 1 0 0 nocb <map>`, and the game log line `Shared Death: no death callback, so Shared Enemies survive this death`. After you wake on the ledge, `EVENT enemyattack 5571 ...` lines continue: the Kaernk is still attacking the Avatar. Type `avatarremove visitor`.

4. A driven copy splashes as it moves. Type:

   ```text
   enemydrive 5571 <map>
   enemypose 5571 10000 0 146 -4 10.2 0 - 0 0 <map>
   ```

   Expect `RESPONSE enemydrive ok 5571`. Within a few seconds, paste this block in one go. A copy snaps without splashing across any jump over 3 m between poses, so the poses step 0.8 m:

   ```text
   enemypose 5571 11500 0 146 -4 11.0 0 - 0 0 <map>
   enemypose 5571 12000 0 146 -4 11.8 0 - 0 0 <map>
   enemypose 5571 12500 0 146 -4 12.6 0 - 0 0 <map>
   enemypose 5571 13000 0 146 -4 13.4 0 - 0 0 <map>
   enemypose 5571 14500 0 146 -4 13.4 0 - 0 0 <map>
   enemypose 5571 14700 0 146 -4 12.6 0 - 0 0 <map>
   enemypose 5571 14900 0 146 -4 11.8 0 - 0 0 <map>
   enemypose 5571 15100 0 146 -4 11.0 0 - 0 0 <map>
   enemypose 5571 15300 0 146 -4 10.2 0 - 0 0 <map>
   enemypose 5571 15500 0 146 -4 9.4 0 - 0 0 <map>
   enemypose 5571 15700 0 146 -4 8.6 0 - 0 0 <map>
   enemypose 5571 15900 0 146 -4 7.8 0 - 0 0 <map>
   enemypose 5571 16100 0 146 -4 7.0 0 - 0 0 <map>
   enemypose 5571 16300 0 146 -4 6.2 0 - 0 0 <map>
   enemypose 5571 16500 0 146 -4 5.4 0 - 0 0 <map>
   ```

   The copy walks 3.2 m toward you in 2 s, waits, then runs 8 m away in 2 s. Expect about two walk splashes with the medium water impact sound, then about four run splashes, closer together, with the high one. The farthest are faint. A copy standing still never splashes.

5. A driven copy shows reported bites and attacks. Carry on driving `5571` and type:

   ```text
   enemypose 5571 20000 1 146 -4 10.8 0 - 0 0 <map>
   enemyeat 5571 <map>
   enemyattack 5571 normal 1 1 1.3 0 0 1 <map>
   enemyattack 5571 breakdoor 1 1 1.3 0 0 1 <map>
   ```

   The copy snaps to 7 m in front of you. Each Command answers `ok 5571`. The bite shows an eat splash with the Kaernk's eating sound, the normal attack an attack splash with the high water impact sound, and the door-break attack nothing. Then type:

   ```text
   enemyrelease 5571 <map>
   enemyeat 5571 <map>
   ```

   Expect `RESPONSE enemyrelease ok 5571`, the game still running, and `RESPONSE enemyeat not-found 5571`: only a driven copy shows a reported bite. Releasing a Kaernk copy, including by closing the Controller, must never end the game with `FATAL ERROR: Tried calling undefined state` in the log.

6. A simulated Kaernk's bites are reported. With `5571` released, pick up a piece of meat and, staying on the ledge, throw it into the water in front of you. The Kaernk comes to it, splashing, and eats it. Each bite, about 1.2 s apart, prints:

   ```text
   EVENT enemyeat 5571 <map>
   ```

7. A coordinated death resets a driven copy. Type:

   ```text
   exec:CheckPoint("check02", "PlayerStartArea_x4", "CheckPoint02", "Hints", "DeathWaterLurker02")
   enemydrive 5490 <map>
   enemypose 5490 1000 0 125.91 -4 6.26 0 - 0 0 <map>
   shareddeath 1 0 0 check02 <map>
   ```

   The first line registers the map's own `check02` again, as the Map Script does, in case an earlier step replaced it. Expect `EVENT checkpoint 0 1 check02 <map>`, `RESPONSE enemydrive ok 5490`, and `RESPONSE shareddeath ok`, and you die at once. After you wake, `EVENT checkpoint 1 1 check02 <map>` follows. Type `enemyrelease 5490 <map>`, then `enemyposes subscribe 20`: expect `RESPONSE enemyrelease ok 5490`, and no `STATE enemypose 5490` lines, because the copy stays inactive.

A Kaernk can break doors: one stuck at a closed door while hunting attacks it with its `breakdoor` attack (`BreakDoorAttackDamage` `10 20`, strength `3`). That attack is reported as `EVENT enemyattack <id> breakdoor ...`, and a door it breaks as `EVENT enemydoorbreak`, the same as a Grunt's. The Archives Cellar has no door in the Kaernks' water, so [the door break checks](enemy-doors-and-callbacks.md) cover that path with the Grunt.

Automated tests cover the `enemyeat` wire format, Capability gating, and map checks, as well as the splash cadence a copy's movement implies. The real-game checks above, and the two-PC LAN check from the issue, remain manual. The Game Peer in `../amnesia-multiplayer` relays `enemyeat` only once it learns the Event (amnesia-spelos/amnesia-multiplayer#89); until then a Joining Player's copy shows walk, run, and attack splashes but no bites.
