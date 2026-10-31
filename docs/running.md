# Running Hexaverse

This page covers playing a local game, hosting dedicated servers, and the runtime files and command lines of every program. The player-oriented originals of most of this live in the in-game wiki under the [game documentation](game/README.md) (configuration pages) and [`server start.txt`](game/server-start.md).

## 1. The processes involved

```
          ┌──────────────────┐   TCP base+0 (info), base+1 (session)   ┌──────────────┐
          │  Visual Client   │◄────────────────────────────────────────►│    Server    │
          └──────────────────┘                                          │ (Server.exe) │
          ┌──────────────────┐   same protocol, started by the server   │              │
          │  Console Client  │◄────────────────────────────────────────►│              │
          │  (Lua AI player) │                                          │              │
          └──────────────────┘                                          │              │
          ┌──────────────────┐   TCP base+2, localhost only             │              │
          │  ControlConsole  │◄────────────────────────────────────────►│              │
          └──────────────────┘                                          │              │
          ┌──────────────────┐   UDP 10245 / heartbeats                 │              │
          │  ServerManager   │◄────────────────────────────────────────►│              │
          └──────────────────┘                                          └──────────────┘
                     LAN discovery: the server broadcasts on UDP 10246
```

There is always a server, even in a single-player-looking game. A client that "starts a local server" just launches `Server.exe` in the background and connects to it.

## 2. Playing a local game

1. Start `client/Visual Client.exe`, with `client/` as the working directory.
2. On the server selection screen, click **Start local server…** and choose one of:
   - **current configuration**: uses `server.ini` from the game folder (see §5),
   - **visual configuration**: the [interactive configuration mode](game/config/interactive-configuration-mode.md), where you design the planet and rules in the client and then start the server,
   - **load save game**: resumes a `.hsf` file.
3. The new server appears in the list within a few seconds. Click it to connect, then create your faction.
4. Each round, issue orders, then press **End round** (or Return). The round continues once every connected player has ended it, or when the round timer runs out.

A server started by the client closes with the client. To keep a game running, start a dedicated server instead.

The tutorial is started from the main menu. Under the hood it's a server started with `--tutorial` on a port at or above 50000, which loads `server/config/tutorial.hsf`, plus the script `lua/tutorial.lua`.

## 3. Dedicated servers

You can start a server in any of these ways:

- run `server/Server.exe` from a console (working directory `server/`),
- use the **Server Manager** (`ServerManager/ServerManager.exe`) and its **Add Server…** button,
- run `ControlConsole.exe --server`, which starts a server and supervises it,
- use the client's **Start local server…** button (see above).

### Server command line

```
Server.exe [options]
  --load/-l <file>          Load a save game (.hsf)
  --loadAuto/-a <0-9>       Load an auto-save; 0 = most recent
  --offline/-o              Start offline
  --heartbeat/-b            Quit unless a localhost client sends a heartbeat every 3 s
  --tutorial/-t             Start in tutorial mode
  --port/-p <1025-65533>    Base port (default 10247); three consecutive ports are used
  --noRemote/-r             Disable the remote control (ControlConsole) service
  --rv4                     Run the remote control service over IPv4 instead of IPv6
  --test                    Run self-tests, then quit
  --config/-c <path>        Load this configuration file instead of the user's server.ini
  --eraseCfg                Delete the file given with -c once it has been loaded
  --generate/-g <path>      Write a configuration file to <path>, then quit
  --version/-v              Print version, then quit
  --help/-h                 Print this list
```

The help text comes from `ParameterConfig::PrintHelp()` in `server/server.cpp`.

### Ports

Defined in `globalShared/ports.h`:

| Port | Protocol | Purpose | Needs forwarding? |
|---|---|---|---|
| base + 0 (default **10247**) | TCP | Info channel: server info, pings | yes |
| base + 0 | UDP | Heartbeats | – |
| base + 1 (**10248**) | TCP | Game session | yes |
| base + 2 (**10249**) | TCP | Remote control. The server rejects connections that don't come from localhost | no |
| **10246** | UDP | LAN discovery broadcasts (fixed) | – |
| **10245** | UDP | Server Manager unicast (fixed) | – |
| ≥ **50000** | TCP | Base ports for tutorial servers | – |

To run several servers on one machine, space their base ports 3 apart (`-p 20000`, `-p 20003` …). Clients outside the LAN connect with `host:baseport`. LAN clients learn the port from the broadcast. Only **one** client per machine can listen for broadcasts, so a second client on the same machine has to add servers by hand.

### Authentication

When a faction is created, the server hands the client a 256-bit access token. The client stores it in `[game folder]\client\worldAccess.m32`, mapped by world ID. On login, the token is hashed with a per-connection salt and checked by the server. Tokens are stored in save games, so you can rejoin after a reload. A server can also require a password.

### Logs

Every server writes a log file to `[game folder]\server\logs\`. Look there after a crash. The control console variable `/IO/logFile` tells you which file a running server uses, and the Server Manager can open it directly.

## 4. Client command line

The visual and console clients take `key=value` pairs (values may be quoted):

| Parameter | Meaning |
|---|---|
| `serverURL=host[:baseport]` | Connect to this server on start-up |
| `password=…` | Server password |
| `profile=name` | Use a separate local profile (lower-cased, prefix `name_`), e.g. to run two clients side by side |
| `udpListen=true\|false` | Whether to listen for LAN discovery broadcasts |
| `aiAccessToken=<hex>` | Run as the AI client for the faction that owns this token. The client ends rounds automatically and quits if it can't connect within 15 s |
| `--aiDebug` / `--debugAI` | Debug builds only: act as AI client against `localhost` |

When a faction has an AI script, the server starts the AI client itself (`server/components/aiProcess.cpp`). It runs `..\client\Console Client.exe` (in Debug builds the windowed Visual Client) with `aiAccessToken=… password=… udpListen=false serverURL=localhost:<port>`.

## 5. The game folder

Every program stores its runtime data under **`Documents\My Games\Hexaverse\`** (`clientServerShared/runtimeConfig.cpp`):

| Sub-folder | Owner | Contents |
|---|---|---|
| `client\` | Visual Client | Client settings, `worldAccess.m32` (faction tokens), screenshots |
| `aiClient\` | Console Client in AI mode | AI client settings |
| `server\` | Server | `config\server.ini`, `config\lua\` (research script override), `logs\`, `saves\` |
| `shared\` | all | `config\myPlanets\` (your planet types), `AIScripts\` (Lua AI scripts), `config\lua\` |

The folders are created on first run. Delete a sub-folder to reset it to defaults.

### Client settings: `client/game.ini`

`client/game.ini` ships with commented defaults in two sections:

- `[runtime]`: FPS cap, vsync, FSAA samples, `alwaysDay`, resource display style, sound switches and volume, `shadowQuality` (`Filtered6x6`, `Filtered3x3`, `Unfiltered`, `Off`), command queueing behavior and more.
- `[timelapse]`: settings for recording time-lapse frames.

Every key has a comment in the file explaining it.

### Server settings

The server configuration is a tree of **folders and variables**, declared and documented in [`server/config/declaration.xml`](../server/config/declaration.xml). The top-level folders are `Game`, `World`, `Planet`, `Mountains`, `ExtendedCore`, `Liquid`, `Trees`, `Crystals`, `Plants`, `Rules`, `Nexus`, `Factions`, `Structures`, `MobileTurrets`, `UnitLimits`, `Server`, `Config` and `IO`. You can change the configuration in several ways:

1. Edit `[game folder]\server\config\server.ini` before the server starts. It's created on first run, or with `Server.exe -g <path>`.
2. Change variables on a running server with the **ControlConsole** (§6).
3. Use the client's interactive configuration mode.
4. Double-click **Server Cfg** in the **Server Manager**.

### Planet types

Planet templates are `.ini` files in `predefinedPlanets/`: Desert, FloatingIslands, HollowCore, IceDesert, IvoryPillars, Lava, LushGreen, Moon, Ocean, plus `Template.ini`. Your own planets live in `[game folder]\shared\config\myPlanets\`, and the game lists them with a leading `*`. Each file sets the noise layers (mountain layer, extended core, inner core), oceans and vegetation, and the fractal noise fills in the actual layout. `/World/showPlanetTypes` in the control console lists every known type.

## 6. Control console

`ControlConsole/ControlConsole.exe` connects to a server on the same machine (default base port) and gives you a shell-like interface over the server's configuration tree:

```
ls                 list current virtual directory
cd Rules/Nexus     change directory (absolute paths start with /)
help <x>           show help for a command or variable
<var>              print a variable
<var>=<value>      set a variable (may trigger actions)
exit
```

Tab completes names. The working directory is shared by every connected console, so scripts should use absolute paths.

| Option | Effect |
|---|---|
| `-p/--port <base>` | Connect to a server on a non-default base port |
| `-s/--server` | Start a server and supervise it (heartbeats in both directions) |
| `-c/--command "<cmd>[;<cmd>…]"` | Run commands non-interactively and exit. Output lines start with `=` (directory change), `+` (result) or `-` (error) |
| anything else | Treated like `-c` |

Useful commands:

| Command | Purpose |
|---|---|
| `/Game/endRound` | Force the current command phase to end |
| `/Game/Factions/names`, `/Game/Factions/find <name>` | List or search factions |
| `/IO/autoSaveNow`, `/IO/saveTo <file>` | Save now. Paths are relative to `[game folder]\server\saves` |
| `/IO/loadFrom <file>`, `/IO/loadAutoSave [index] [worldID]` | Load a save. This disconnects every client |
| `/IO/reloadResearchProgram` | Reload the Lua research script |
| `/IO/logFile` | Show the current log file |
| `/World/showPlanetTypes` | List planet types |
| `Rules/Nexus/aiScript=<name>` | Give the Nexus faction a Lua AI from `shared\AIScripts\<name>.lua` |

## 7. Save games

- Format: binary `.hsf` (Hexaverse Save File). It stores the full current state, but no history.
- Auto-saves happen every 10 rounds (configurable) and when the last player disconnects. The last 10 are kept per world in `[game folder]\server\saves\auto\<world ID>\snapshot0.hsf` (newest) … `snapshot9.hsf`.
- To load one, use `Server.exe -l <file>`, `-a <index>`, the control console, or the Server Manager.
- Save games include faction access tokens, the AI script name and its exported state, alliances and statistics.

## 8. Server Manager

`ServerManager/ServerManager.exe` (C#, needs the [.NET 10 Desktop Runtime](https://dotnet.microsoft.com/download/dotnet/10.0)) lists every server running locally (it listens on UDP 10245). It can start new ones, edit the global server configuration, attach a control console, save, open logs, and resume, stop or terminate servers. Only one instance can run at a time. Servers it starts are "dependent" and don't open a console window. Details are in [`server manager.txt`](game/config/server-manager.md).
