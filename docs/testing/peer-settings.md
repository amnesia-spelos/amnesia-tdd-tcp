# Peer Settings checks

Run the rebuilt game and connect a Peer that sends each line below as typed. `<TAB>` below is a tab character, because the `settingannounce` fields after the key are separated by tabs. The interactive Controller drops a typed Tab, so send these lines from a Peer that can send tabs, such as a short Python script on a socket to `127.0.0.1:5150`. Open the options from the main menu with **Options**, then pick the **TDD TCP** tab.

Before you start, close the game and remove any `PeerSettings` element from `main_settings.cfg` in `Documents/Amnesia/Main`, so no `checks.` value is stored yet.

1. With no Peer connected, the TDD TCP tab shows the **Connection** group (Host and Port) and, below it, the **Peer Settings** group with the dim line "No connected Peer has announced settings." Neither group overlaps the tip line or the OK and Cancel buttons. Close the options with Cancel.

2. Announce two settings. Send:

   ```text
   protocol 2 settings
   settingannounce checks.name Name<TAB>Player<TAB>The name other players see.
   settingannounce checks.color Color<TAB>FFFFFF
   ```

   Expect `RESPONSE protocol ok 2 settings`, `RESPONSE settingannounce ok checks.name Player`, and `RESPONSE settingannounce ok checks.color FFFFFF`. Open the options and the TDD TCP tab: the empty line is gone, and two rows show, in this order, `Name` with `Player` and `Color` with `FFFFFF`. Hover over the Name row: the tip line reads "The name other players see." Hover over the Color row: the tip line is empty.

3. Live updates. Keep the options open. In the Name box, type `Daniel` in place of `Player`, and leave the cursor there. Send:

   ```text
   settingannounce checks.name Player name<TAB>Somebody<TAB>Shown above your Avatar.
   settingannounce checks.extra Extra<TAB>x
   ```

   Expect `RESPONSE settingannounce ok checks.name Somebody` and `RESPONSE settingannounce ok checks.extra x`. In the open window, the first row's label becomes `Player name` while its box still reads `Daniel`, and its tip becomes "Shown above your Avatar." A third row `Extra` with `x` appears.

4. Cancel sends nothing. Press Cancel. The Peer receives nothing. Open the options again: the first row reads `Somebody` (the edit is gone). Press Cancel again.

5. OK sends `settingchanged`. Open the options, change Color to `FF0000`, and press OK. Expect exactly one line, `EVENT settingchanged checks.color FF0000`. Send `settingvalue checks.color`: expect `RESPONSE settingvalue ok checks.color FF0000`. Open the options again: Color reads `FF0000`. Press OK without editing: nothing is sent.

6. Ending the Session removes its rows. Open the options on the TDD TCP tab and type `Bob` into the first row without pressing OK. Disconnect the Peer. The three rows disappear and the empty line comes back. Press OK: nothing is stored for `checks.name` (step 7 shows it).

7. The stored value survives a restart; the untouched one follows a new default. Close the game, start it again, connect a Peer, and send:

   ```text
   protocol 2 settings
   settingannounce checks.name Name<TAB>NewName
   settingannounce checks.color Color<TAB>00FF00
   ```

   Expect `RESPONSE settingannounce ok checks.name NewName` (never changed, so it follows the new default) and `RESPONSE settingannounce ok checks.color FF0000` (the value changed in step 5 wins over the new default). The options rows show `NewName` and `FF0000`.

8. Text box limits. In a row, press Tab: no tab is typed, and the text stays as it was. Paste text that holds a tab or a line break: those characters are dropped. Type more than 256 characters: the box stops at 256. A paste that would take the box over 256 characters is not pasted at all.

9. Scrolling. Connect a fresh Peer, send `protocol 2 settings`, then announce 16 settings, `settingannounce checks.s1 Setting 1<TAB>1` up to `settingannounce checks.s16 Setting 16<TAB>16`. Every Response is `ok`. In the TDD TCP tab, the Peer Settings group shows a vertical scroll bar, and scrolling reaches Setting 16 without the rows crossing the group's edge.

10. The pause menu. Start a new game or load a save, press Escape, open **Options** and the TDD TCP tab, and repeat steps 3, 5, and 6 there. They behave the same.
