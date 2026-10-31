# Hexaverse

Hexaverse is a turn-based, multiplayer 4X strategy game played on **volumetric planets**. A hexagonal grid covers each planet and is stacked into 32 layers, so terrain can be dug out, built up, flooded or blasted away. All players act **at the same time** each round. They expand across the globe, mine resources that never run out but can be cut off, research upgrades, and fight with ground vehicles, planes, towers and satellites. The AI-controlled **Nexus** faction defends bases at up to 12 poles around the planet.

<p align="center"><img src="ArtWork/screenshot16.png" width="720" alt="Hexaverse screenshot"></p>

> [!IMPORTANT]
> **Hexaverse currently builds only on Windows, with the Visual Studio compiler (MSVC).** The code uses Direct3D 11, OpenAL, the Win32 API and MSVC extensions. You can build it with CMake or with Projector, but both need Windows and Visual Studio 2022 or newer. There is no Linux or macOS build. (The dedicated server has been run under Wine, but it is still built on Windows.)

## Purpose

IronFox developed Hexaverse between 2015 and 2022, and it was briefly available in Steam Early Access. This repository holds the complete game: the Windows client, the dedicated server, the Lua game scripts, the player documentation and the supporting tools. Development has stopped.

The last tagged release is `v0.9.12` (Feb 2021). `master` is at v0.9.13, which was never released.

## Composition

Hexaverse is a networked game. Even a single-player game runs a local server that the client connects to.

| Program | Language | Source | Role |
|---|---|---|---|
| **Visual Client** | C++ / Direct3D 11 | `client/` | The game itself: rendering, HUD, audio, input. Finds LAN servers and can start a local one. |
| **Console Client** | C++ | `client/` (without the visual and audio parts) | Headless client the server launches to run Lua AI players. |
| **Server** | C++ | `server/` | Dedicated server: simulates rounds, terrain and units, stores save games, hosts the Nexus. |
| **ControlConsole** | C++ | `ControlConsole/` | Command-line remote control for local servers. |
| ServerManager, Bake, BugReporter, DocumentationEditor | C# | own folders | GUI server manager, packaging tool, bug report uploader, wiki editor. |
| BuildCounter, IncreaseVersion | C++ | own folders | Build-number and version bump helpers. |
| MetaServer | C# | `MetaServer.C#/` | Experimental, unfinished meta-server for a multi-planet universe. |

The code and data are split into these folders:

```
client/               Visual and console client, shaders, models, textures, sounds, game.ini
server/               Dedicated server, server variable declarations, tutorial save
clientServerShared/   Game model shared by client and server: grid, objects, research, protocol, Lua API
clientConsoleShared/  Starting and supervising a local server process (clients and ControlConsole)
globalShared/         Port numbers and process start helpers used by every program
lua/                  Research tree and unit attributes, AI, tutorial and resource-distribution scripts
predefinedPlanets/    Planet type templates (.ini)
ThirdParty/lua/       Lua 5.3.3, built as a static library
documentation/        In-game player wiki (DokuWiki syntax), its images, the PHP renderer, wiki2md.py
docs/                 Markdown documentation (developer guides and the converted game wiki)
cmake/                CMake helper modules and the OpenAL import-library definition
ArtWork/              Logos, store art, screenshots (PSDs in Git LFS)
issues/               Notes on investigated bugs
```

More detail is in [docs/repository.md](docs/repository.md) and [docs/architecture.md](docs/architecture.md).

## Server authority and cheat resistance

The server is fully authoritative. Clients only send requests, such as "queue this task", and the server checks each one (right phase, own unit, legal operation) before it changes anything. Movement, combat, terraforming, production and research are all resolved on the server.

The server also keeps track, for every faction, of what that faction can see right now and what it has seen before: which terrain cells its sensors cover, the terrain as it last looked everywhere else, and when each foreign unit came into or went out of view. A client is only ever sent what its faction is allowed to know. Hostile units out of sensor range, terrain changed out of sight, enemy orders and production queues, and attackers you can't see (you only get a vague trajectory) never reach the client. A modified client therefore has nothing hidden to reveal.

How this works in the code, and which host settings deliberately share more, is described in [docs/server-authority.md](docs/server-authority.md).

## Dependencies

| Dependency | Needed for | Notes |
|---|---|---|
| Windows 10/11 x64 | everything | Direct3D 11, Win32 windowing and console APIs. |
| **Visual Studio** 2022 or 2026, with *Desktop development with C++* and a Windows 10/11 SDK | C++ programs | Projects compile with `/std:c++latest` and the static CRT (`/MT`). The Direct3D 11 headers and libraries come from the Windows SDK. |
| **CMake** 3.21 or newer, **or** [Projector](https://github.com/IronFox/Projector) 1.5 or newer | C++ programs | Two equivalent ways to build. CMake uses the `CMakeLists.txt` files; Projector generates Visual Studio solutions from the `.solution`/`.project` files. |
| [DeltaWorks](https://github.com/IronFox/DeltaWorks) | C++ programs | IronFox's base library (`DeltaWorksCore`, `DeltaWorksEngine`), linked statically. It brings zlib, libpng, libjpeg, bzip2 and OpenSimplex noise, from vcpkg or bundled copies. CMake downloads DeltaWorks automatically if there is no checkout next to Hexaverse. |
| [vcpkg](https://vcpkg.io) | C++ programs, optional | Supplies zlib, libpng, libjpeg-turbo and bzip2 (listed in `vcpkg.json`). Ships with Visual Studio. Without it, CMake uses DeltaWorks' bundled copies. |
| OpenAL 1.1 SDK (Creative Labs) | Visual Client, optional with CMake | Only its import library `OpenAL32.lib` is used; the headers are bundled with DeltaWorks. CMake generates the import library itself if the SDK isn't installed. Projector needs the SDK. The runtime DLL and installer are committed in `client/`. |
| Lua 5.3.3 | client and server | Bundled in `ThirdParty/lua/`, nothing to install. |
| .NET Framework 4.x developer packs | C# tools | Plain `.sln`/`.csproj` files. Prebuilt copies of most tools are committed. |
| Git LFS | cloning | `ArtWork/*.psd` is stored in LFS. |

Third-party code, assets and their licenses are listed in [docs/third-party.md](docs/third-party.md).

## Building

On Windows, with Visual Studio 2022 or newer installed. The full guide, with all options, configurations, packaging and troubleshooting, is in [docs/building.md](docs/building.md).

### With CMake

From a *Developer PowerShell for VS* (or with `VCPKG_ROOT` set):

```bat
git lfs install
git clone https://github.com/IronFox/Hexaverse.git
cd Hexaverse
cmake --preset vs-x64
cmake --build --preset vs-x64-release
```

CMake uses a DeltaWorks checkout in `../DeltaWorks` if there is one, or downloads the tested DeltaWorks commit. zlib, libpng, libjpeg and bzip2 come from vcpkg. Use the `vs-x64-bundled` preset to build without vcpkg. You can also open the folder directly in Visual Studio (*File → Open → Folder*), which reads the presets.

### With Projector

1. Clone [DeltaWorks](https://github.com/IronFox/DeltaWorks) and [Projector](https://github.com/IronFox/Projector), build Projector, and install the OpenAL 1.1 SDK.
2. Open `client/Client.solution` in Projector. The first time, it asks where `DeltaWorksCore.project`, `DeltaWorksEngine.project` and `lua.project` (`ThirdParty/lua/`) are.
3. **Generate**, **Open in Visual Studio**, and build `Visual Client` and `Console Client` in **Release | x64**.
4. Do the same for `server/Server.solution` and `ControlConsole/ControlConsole.solution`.

### Output

Both builds write the executables into the project folders, where the game expects them: `client/Visual Client.exe`, `client/Console Client.exe`, `server/Server.exe`, `ControlConsole/ControlConsole.exe` (Win32 builds are named `<Name> x86.exe`). Start `client/Visual Client.exe` and choose **Start local server…**, or start `server/Server.exe` first and join it from the server list. Running servers, ports and command lines are covered in [docs/running.md](docs/running.md).

## Branches

| Branch | Content |
|---|---|
| `master` | The complete game, with every finished feature merged (aerial units, T2 upgrades, plants and advanced resources, alliances, statistics, the reworked Lua AI …). This is the branch to build. |
| `master-resource_bridge` | `master` plus the work-in-progress Lua **modding** framework (scripted structure types, first example: a Resource Relay). |
| `master-styled_highres` | `master` plus styled dark rendering and very large screenshot rendering. |
| `steam` | `master` from June 2021 plus the Steamworks integration. It contains the **Steamworks SDK, which must not be published.** |

Releases are tagged `v0.3.19` to `v0.9.12`. Older experiments that were never merged (technite, pylons, orbital stations, a GUI renderer rework and more) are kept as `archive/*` tags. The full list, with dates and history notes, is in [docs/repository.md](docs/repository.md).

## Documentation

### Game documentation

The in-game player wiki, converted to Markdown, starts at **[docs/game/README.md](docs/game/README.md)**. It covers:

- [Controls](docs/game/controls.md), [terrain](docs/game/terrain.md) and [starting a server](docs/game/server-start.md)
- Game concepts, such as [rounds](docs/game/concepts/round.md), [resources](docs/game/concepts/resources.md), [compounds](docs/game/concepts/compound.md), [research](docs/game/concepts/research.md), [scanning range](docs/game/concepts/scanning-range.md) and [AI development](docs/game/concepts/ai-development.md)
- Configuration, such as [save games](docs/game/config/save-games.md)
- Every unit: structures, turrets, vehicle bases and planes ([unit list](docs/game/units/list.md))

The originals are the DokuWiki pages in `documentation/documentation/`, which the game also shows in-game. After editing them, run `python3 documentation/wiki2md.py` from the repository root to regenerate `docs/game/`.

### Developer documentation

| Document | Contents |
|---|---|
| [docs/building.md](docs/building.md) | Toolchain, dependencies, CMake and Projector builds, build outputs, packaging |
| [docs/running.md](docs/running.md) | Servers and clients, ports, command lines, game folder, save games, control console |
| [docs/gameplay.md](docs/gameplay.md) | Rules overview for programmers: phases, terrain, resources, units, sensors, Nexus |
| [docs/architecture.md](docs/architecture.md) | How client, server and shared code fit together: networking, world, object model, persistence |
| [docs/server-authority.md](docs/server-authority.md) | Why the server is authoritative and how it hides what a faction can't see |
| [docs/lua-scripting.md](docs/lua-scripting.md) | Research script, AI scripts, resource distributions, tutorial, Lua API |
| [docs/extending.md](docs/extending.md) | Adding unit and structure types, commands, protocol channels, server variables |
| [docs/tools.md](docs/tools.md) | Server Manager, Control Console, Bake, BugReporter, DocumentationEditor, BuildCounter, MetaServer |
| [docs/repository.md](docs/repository.md) | Repository layout, branches, tags, LFS and history notes |
| [docs/third-party.md](docs/third-party.md) | Bundled third-party code and assets, with their licenses |

The full Lua API reference is [`lua/General Lua API Documentation.txt`](lua/General%20Lua%20API%20Documentation.txt).

## License

No license has been chosen yet. Until a `LICENSE` file is added, all rights are reserved by the author. Third-party components keep their own licenses (see [docs/third-party.md](docs/third-party.md)).
