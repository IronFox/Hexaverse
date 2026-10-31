# Hexaverse

Hexaverse is a turn-based, multiplayer 4X strategy game played on **volumetric planets**. Each planet is covered by a hexagonal grid that is stacked into layers, so terrain can be dug out, built up, flooded or blasted away. Players act **simultaneously** each round. They expand across the globe, mine infinite but blockable resources, research upgrades and fight with ground vehicles, planes, towers and satellites. The AI-controlled **Nexus** faction defends bases at the planet's poles.

The game was developed by IronFox between 2015 and 2022 and was briefly available in Steam Early Access. This repository holds the complete game: the Windows client, the dedicated server, the Lua scripts, and the supporting tools.

> **Status:** No longer under active development, and being prepared for open-source release. The last tagged release is `v0.9.12` (Feb 2021). `master` is at v0.9.13 (unreleased), followed by the LFS migration and library updates.

<p align="center"><img src="ArtWork/screenshot16.png" width="720" alt="Hexaverse screenshot"></p>

## Highlights

- **Volumetric hex planets.** Hexagonal cells cover the whole sphere and are stacked vertically. Ten predefined planet types are included (lush, desert, ocean, lava, moon, hollow core, floating islands and more), and you can define your own in `.ini` files.
- **Simultaneous turns.** All players give orders in the command phase. The server then resolves structures, attacks and movement, and every client animates the result.
- **Terraforming.** Build walls, bridges and tunnels, or bury your base under a mountain.
- **Sensors and fog of war.** You only know what your radars can see. EMP weapons blind and slow enemies.
- **Research.** Defined entirely in Lua (`lua/attributesAndResearch.lua`), which can change every unit attribute.
- **Scriptable AI.** Nexus behavior can be replaced with your own Lua AI scripts.
- **Alliances, voting, statistics, T2 unit upgrades, planes and plantations**, all merged into `master`.
- **LAN discovery and dedicated servers**, plus a GUI server manager and a scriptable control console.

## Quick start

Hexaverse is **Windows-only** (Direct3D 11, OpenAL, Win32). The C++ projects are written as Projector `.solution`/`.project` files, and Projector generates the Visual Studio solutions from them.

1. Clone [DeltaWorks](https://github.com/IronFox/DeltaWorks) (the base library) and [Projector](https://github.com/IronFox/Projector), and build Projector.
2. Clone this repo with Git LFS enabled (`git lfs install` first), so `ArtWork/*.psd` is fetched properly.
3. Install the OpenAL 1.1 SDK. The DirectX SDK (June 2010) is optional.
4. In Projector, generate `client/Client.solution`, `server/Server.solution` and `ControlConsole/ControlConsole.solution`, then build them in Visual Studio (Release, x64). Release builds write their executables straight into the project folders.
5. Start `client/Visual Client.exe` and use **Start local server…**, or run `server/Server.exe` yourself first.

The full guide is in [docs/building.md](docs/building.md). How to run games, servers and tools is in [docs/running.md](docs/running.md).

## Documentation

| Document | Contents |
|---|---|
| [docs/building.md](docs/building.md) | Toolchain, dependencies, Projector, build outputs, packaging |
| [docs/running.md](docs/running.md) | Starting servers and clients, ports, command lines, config folder, save games, control console |
| [docs/gameplay.md](docs/gameplay.md) | Game concepts: rounds and phases, terrain, resources, compounds, sensors, combat, research, Nexus |
| [docs/architecture.md](docs/architecture.md) | How client, server and shared code fit together, plus the networking, world, object model and persistence |
| [docs/lua-scripting.md](docs/lua-scripting.md) | Research script, AI scripts, resource distributions, tutorial, Lua API reference |
| [docs/extending.md](docs/extending.md) | Adding unit and structure types, commands, protocol channels, server variables |
| [docs/tools.md](docs/tools.md) | Server Manager, Control Console, Bake, BugReporter, DocumentationEditor, BuildCounter, MetaServer |
| [docs/repository.md](docs/repository.md) | Repository layout, branches, tags, LFS and history notes |
| [docs/third-party.md](docs/third-party.md) | Bundled third-party code and assets, with their licenses |

The in-game player wiki lives in [`documentation/documentation/`](documentation/documentation/home.txt). It's written in DokuWiki syntax and covers controls, every unit, and server configuration in detail. The Lua API reference is [`lua/General Lua API Documentation.txt`](lua/General%20Lua%20API%20Documentation.txt).

## Repository layout

```
client/               Visual and console client (C++ / Direct3D 11), shaders, resources, game.ini
server/               Dedicated server (C++), server config declaration, tutorial save
clientServerShared/   Game model shared by client and server: grid, objects, research, protocol, Lua API
clientConsoleShared/  Code shared by the clients and the control console (local server process control)
globalShared/         Port numbers and process start helpers used by every program
lua/                  Research, tutorial, AI and resource-distribution scripts, plus the Lua API documentation
predefinedPlanets/    Planet type definitions (.ini)
ThirdParty/lua/       Lua 5.3.3 (static library)
ControlConsole/       Command-line control console for local servers (C++)
ServerManager/        GUI manager for local servers (C#)
Bake/                 Packaging tool driven by *BakeRecipe.txt files (C#)
BugReporter/          Crash/bug report uploader (C#)
DocumentationEditor/  Editor for the in-game wiki (C#)
BuildCounter/, IncreaseVersion/   Build-number and version bump helpers
MetaServer.C#/        Experimental "consensus" meta-server for a multi-planet universe (C#, unfinished)
documentation/        In-game/website wiki pages and the PHP renderer
ArtWork/              Logos, store art, screenshots (PSDs via Git LFS)
issues/               Notes on investigated bugs
```

More detail is in [docs/repository.md](docs/repository.md).

## Branches

`master` is the complete game. A few feature branches stay open (`master-resource_bridge` for Lua modding, `master-styled_highres` for styled screenshot rendering), and `steam` holds the Steamworks integration, which can't be published with the open-source release. Old experiments are kept as `archive/*` tags. See [docs/repository.md](docs/repository.md).

## License

No license has been chosen yet. Until a `LICENSE` file is added, all rights are reserved by the author. Third-party components keep their own licenses (see [docs/third-party.md](docs/third-party.md)).
