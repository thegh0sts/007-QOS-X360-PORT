# Multiplayer (System Link)

007: Quantum of Solace's multiplayer runs in this port as **System Link**: games between PCs on the same network, as
between Xbox 360s on a LAN. Xbox LIVE in the multiplayer menu doesn't work yet.

> Multiplayer has been tested with two copies of the game on one PC. Games between two PCs should work the same way,
> but haven't been tested yet: if you try it, please report how it went.

## 1. Set up your profile

In the launcher, open the **Multiplayer** tab:

- **Gamertag**: your name in games (letters, digits and spaces, up to 15 characters). **Every player in a game needs
  a different gamertag.**
- **XUID**: your profile's ID, made once for each install. You don't need to change it; the game uses it to tell
  players apart.

## 2. Start multiplayer

Click **Play**, then choose **MULTIPLAYER** on the game's main menu. Multiplayer starts as a separate program (with
the same display, graphics and control settings as single player). Leaving multiplayer from its main menu takes you
back to single player.

**Windows firewall:** the first time, Windows may ask whether to let the game use the network. Allow it on **private
networks**, or other players won't find your games. If you clicked Cancel, allow `qosmp.exe` in *Windows Security →
Firewall & network protection → Allow an app through firewall*.

## 3. Host or join a game

Every player chooses **SYSTEM LINK** in the multiplayer menu.

- **Host:** choose **Create Game**, pick the map and game mode, wait for the others to join, then **Start Match**.
- **Others:** choose **Join Game**. The host's game appears in the list after a few seconds; select it to join.

All players' PCs must be on the same network (the same router, or the same virtual network: see below).

## Playing over the internet

System Link finds games with LAN broadcasts, so players in different places need to be on one virtual network:

- **ZeroTier** or similar tools that create a virtual LAN and pass broadcasts: install it on every PC, join the same
  network, then host and join as above.
- **Tailscale** and other VPNs that drop broadcasts: games won't show up in the list on their own. Put the host's
  address (its VPN IP, or several separated by commas) in the launcher's Multiplayer tab under **Host addresses**
  (it passes `--netplay_peers` to the game).
  Only players joining need it.
- **Port forwarding** (no VPN): the game uses UDP ports **11000 and 11001** for the first copy on a PC (each further
  copy on the same PC adds 16: 11016 and 11017, ...). Forwarding alone still doesn't make the game appear in the list
  for players outside the LAN, so a virtual LAN is the easier way for now.

## Downloadable content (map pack)

The **Camille Map Pack** adds four maps: Canals, Baggage, Cistern and Dam.

1. Copy the map pack's file from an Xbox 360 drive. It's a single file with a long hex name, usually in
   `Content\0000000000000000\415607FF\00000002\`. You can keep that folder structure, or put the file in any folder.
2. In the launcher's **Multiplayer** tab, under **Downloadable content**, click **Choose folder...** and pick the
   folder with the file (or a folder above it). The launcher lists the packs it finds, e.g. *Found: Camille Map Pack*.
3. Start multiplayer. The first start after adding the pack installs it (a few seconds longer). The maps then appear
   in the map list with their names and pictures.

**Every player in a game needs the map pack** to play its maps.

## Troubleshooting

- **No games in the Join Game list:** check that the host has created the game and is waiting, that both PCs are on
  the same network (or virtual network), and that Windows allowed the game through the firewall on both PCs.
- **Can't join, or players drop out:** make sure every player has a different gamertag.
- **A DLC map isn't listed:** check the launcher shows *Found: ...* under Downloadable content, then restart
  multiplayer. The pack must be this game's (title ID 415607FF); other games' packs are skipped.
- **The game shows an error and restarts:** that's the game's own error handling, as on the console. If it keeps
  happening, use **Save crash report** in the launcher's Game files tab and report it with the logs (**Open logs**).
