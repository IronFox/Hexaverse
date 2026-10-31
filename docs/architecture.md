# Architecture

Hexaverse is a client/server game. The **server is authoritative**: it owns the world, resolves every round and decides what each faction gets to see. **Clients** are thin in game-logic terms. They render, collect commands, and replay what the server sends. Client and server share one C++ game model (`clientServerShared/`), which is compiled into both.

```
                    ┌───────────────────────────── clientServerShared/ ─────────────────────────────┐
                    │ grid/      planet geometry, cell stacks, terrain content, resources coverage  │
                    │ objects/   entities, structures, vehicles, planes, components,               │
                    │            jobs, pathfinding, compounds, research, T2, Nexus estimation       │
                    │ lua/       Lua interface, Game API bindings, script cache                     │
                    │ protocol*  channel IDs, message structs, serialization                        │
                    │ runtimeConfig, stringTable, logEvent, globalEvents, boot                      │
                    └───────────────────────────────────────────────────────────────────────────────┘
                              ▲                                              ▲
        ┌─────────────────────┴─────────────┐                ┌───────────────┴──────────────────┐
        │ client/                           │                │ server/                          │
        │  clientMain.cpp   main loop       │   TCP          │  server.cpp      main loop,      │
        │  components/network               │◄──────────────►│                  CLI, phases     │
        │  components/animation/*           │                │  components/state/*  game, world,│
        │  components/visual/{scene,hud}    │                │                      network, IO │
        │  components/audio/*               │                │  components/evolver(s)  round    │
        │  components/ai/*  (Lua AI)        │                │                         simulation│
        │  ExtRenderer/DirectX11            │                │  reflection      config tree     │
        └───────────────────────────────────┘                │  remoteControl   control console │
              ▲ globalShared/ (ports, process start)         │  journalSaveGame, aiProcess,     │
              ▼ clientConsoleShared/ (local server process)  │  udpBroadcast, nexusThreat…      │
        ControlConsole/                                      └──────────────────────────────────┘
```

Everything is built on **DeltaWorks** (`DeltaWorksCore` for containers, math, strings, IO, TCP and the CLI parser, and `DeltaWorksEngine` for rendering, the GUI and the windowing behind the visual client). DeltaWorks headers are included as `<container/...>`, `<math/...>`, `<io/...>` and so on.

## 1. Programs

| Program | Entry point | Built from |
|---|---|---|
| Server | `server/server.cpp` `main()` | `server/`, `clientServerShared/`, `globalShared/`, DeltaWorksCore, lua |
| Visual Client | `client/clientMain.cpp` | `client/` (macros `CLIENT`, `HAS_WINDOW`), shared folders, DeltaWorksCore + Engine, lua, OpenAL, D3D11 |
| Console Client | `client/clientMain.cpp` | Same sources without `visual/`, `audio/` and `HAS_WINDOW`. Used as the headless AI player |
| ControlConsole | `ControlConsole/main.cpp` | `clientConsoleShared/`, `globalShared/`, DeltaWorksCore |

## 2. The round loop (server)

The server's main loop sits at the end of `server/server.cpp`:

1. **Every tick:** `Network::Tick()` (accept connections, dispatch messages), `RemoteControl::Resolve()` (control console commands) and vote timeouts.
2. **In `CommandPhase`:** the server waits until every living, non-Nexus faction with a connected player has requested a new round (`RequestRoundEnd`) and the Nexus AI agrees too, or until the round timer runs out, or until `/Game/endRound` is called.
3. **New round:** random seeds are refreshed, Nexus coverage is processed, a journal entry is started, and then
   `Game::BeginRoundProcessing()` → **evolver** (`server/components/evolver*`, `evolvers/core.cpp`) runs the `StructurePhase`, `AttackPhase` and `MovementPhase` simulation and collects the changes, explosions and terrain deltas,
   `Game::ProcessChanges()` → sends each faction the changes it is allowed to see.
4. **`AnimationPhase`:** the server waits for the computed animation length (at most ~10 s, capped and scaled), then calls `Game::FinishRoundProcessing()`, checks for a post-evolution winner, auto-saves, and returns to `CommandPhase`.

`Game::Network::ChangePhase()` sends `PhaseChange` to every client.

## 3. Networking

Ports come from `globalShared/ports.h`: base+0 info, base+1 session, base+2 localhost remote control, UDP 10246 discovery, UDP 10245 Server Manager. The transport is DeltaWorks' TCP `Channel`/`ObjectSender` system. Every message type has a fixed **channel ID**.

- **Info connection** (`Protocol::InfoChannel`): `ServerInfo` and `Ping`. Clients keep it open for every discovered server to fill the server list (`client/components/network.cpp`, `hud/guiHostList.cpp`).
- **Session connection** (`Protocol::ChannelID` in `clientServerShared/protocol.h`), roughly grouped:
  - session control: `Hello`, `ClientFaction`, `MyFaction`, `BeginGame`, `GameProgress`, `PhaseChange`, `RequestEverything` / `BeginEverything` / `EndEverything`, `RequestRoundEnd`, `RequestContinueRound`,
  - world: `ResourceTable`, `WorldDelta`, `Explosion`, `RestoreCoreLink`,
  - entities: `NewMobileEntity`, `MobileState`, `MobileEntityMotion`, `InstantiateStructure`, `UpdateStructure`, `DestroyEntity`, `RemoveEntity`, `EntityTurretRotation`, `PreciseAttackTrajectory` / `VagueAttackTrajectory`,
  - commands from the client: `EntityDirectCommand`, `DropTasks`, `SignalNewJob`, `SignalTerrainMod`, `ConstructionBegin` / `ConstructionAbort`, `EnableStructure`, `Automate`, `Sleep`, `TransferEntityOwnership`, `MobilizeStructure`, `SettleMobile`, `ChoiceIntersection` / `ChoiceResponse`,
  - social: markers, factions (`IdentifyFaction`, `KillFaction`, `ChangeMyName`, `FactionOnline`), alliances, votes, chat, statistics.
- The handshake and server discovery are described step by step in [`protocol documentation.txt`](../protocol%20documentation.txt).
- Message structs serialize through `clientServerShared/protocol/serialBaseTypes.h` and `multi_serializable.inc.h`. Save games use the same serialization.

**Visibility is enforced on the server** (details in [server-authority.md](server-authority.md)). `Game::Network::Describe(…)` builds per-faction views with an `Expose` level, so clients only get what their sensors (or their alliance's) cover. Terrain the client hasn't discovered is sent as `Clear`/`Unknown`, depending on `World/worldDiscovery`.

## 4. The world model (`clientServerShared/grid/`)

| File | Role |
|---|---|
| `base.*` | The spherical hex grid: cells, neighbors, corner vertices, cell IDs |
| `stack.*` | A **cell stack** is the vertical column of `CompactCell`s (content, structure counter, visibility) over a horizontal cell. `Content` enumerates the terrain materials |
| `planet.*` | Planet generation: noise sets, extended planet configuration (mountain layer, extended core, inner core, oceans) |
| `setup.*` | Grid setup and resolution (normal or `lowerResolutionPlanet`) |
| `resources.*` | Natural resource coverage per cell |
| `worldData.*` | The world state that holds it all together |

On the server, `server/components/state/worldState.*` owns the world and loads planet types from `../predefinedPlanets` and the user folder. The client renders it through `client/components/visual/scene/terrain*` (terrain compilers create the meshes) and `world.cpp`.

## 5. The object model (`clientServerShared/objects/`)

```
WorldEntity (entity.h)
 ├─ Structure (structure.h)
 │    ├─ OtherStructure<T>      passive structures, e.g. Connector
 │    └─ WorkingStructure → WorkingStructureT<T>
 │         Generator, ReactorCore, Laboratory, Mine, storages, VehiclePlant, VehicleStore,
 │         PlaneFactory, Plantation, Tower, NexusPort, SkyPort, Organic/Crystalline compound
 │         and CompositeMaterial factories …           (structure.h, objects/structure/*)
 └─ MobileEntity (mobileEntity.h, also a TurretBase)
      ├─ Vehicle (vehicle.h)    = vehicle base + turret component
      │    └─ Plane (subOrbital.h): Raptor, Bomber, CargoPlane (+Hangar), SensorDrone
      └─ Satellite (satellite.h): SkyHub, SkyGuardian, Ragnarok, StarPort  [compiled out]
```

`clientServerShared/config.h` switches whole subsystems off at compile time. In the current game `NO_SATELLITES` (planes replaced satellites), `NO_MUNITION` (weapons need no ammunition) and `NO_HIBERNATION` (a per-type unit limit replaces hibernation) are defined, and `ONE_COMPOUND_PER_FACTION` makes all of a faction's structures share one compound.

Supporting pieces:

| File | Role |
|---|---|
| `types.h`, `entityType.h`, `prototypes.*` | Type tree and global **prototype** instances. Every prototype is registered in `Objects::Prototypes::RegisterAll()` |
| `components.*`, `componentAttribute.*` | Turrets and vehicle bases, and their attributes (range, damage, weight, speed …). Attribute values come from the research script |
| `turretBase.*` | Shared turret logic for towers and vehicles |
| `jobs.*`, `entityCommand.*` | Construction and operation jobs, unit command queues |
| `pathfinding.*` | Reachability and route costs over the unit grid (terrain, liquids, forests) |
| `structureCompound.*` | Compounds: resource balancing between connected structures |
| `resource.*` | Resource vectors and the dynamic resource table |
| `faction.*` | Factions, alliances, AI script name and state |
| `research/*` | Research global state (from Lua), per-faction state, attribute details |
| `t2.*` | Tier-2 upgrades and XP |
| `nexus/*` | Nexus presence and threat estimation per pole (also exposed to Lua) |
| `hibernating.*` | Structures shutting down when uplink bandwidth runs short (compiled out by `NO_HIBERNATION`) |
| `packable.*` | Converting structures to and from mobile units (`MobilizeStructure` / `SettleMobile`) |
| `markers.*`, `statistics.*` | Map markers, per-faction statistics |
| `gridAndChoices.*`, `cursorMode.*`, `description.*` | Placement choices, client cursor modes, the text descriptions shown in the UI |

The same classes run on both sides. On the client they mirror the server state as it arrives, and the client also uses them to preview commands (reachability, ranges, costs).

## 6. Client structure (`client/components/`)

| Area | Contents |
|---|---|
| `network.*`, `objectNetworkHelper.*`, `objectStorage.*` | Server discovery, session, applying received state to local objects |
| `animation/*` | One class per animated event type (`newMobileEntity`, `mobileEntityMotion`, `trajectory`, `explosion`, `worldDelta`, `structureConstruction`, `resourceUpdate` …), all played during the animation phase |
| `visual/scene/*` | 3D scene: terrain, atmosphere, lighting/shadows, object rendering, icons, explosions, markers, progress bars |
| `visual/hud/*` | 2D GUI: menu, dialogs, state panel, resource gauges, faction table, host list, statistics, votes, tooltips, hex command grid, UI scaling |
| `visual/commandDefinitions.*` | All unit and structure commands (`Objects::Commands::DefineAll()`) |
| `visual/tutorial.*`, `visual/autoDocumentation.*` | Tutorial runner (Lua) and generated in-game unit documentation |
| `audio/*` | OpenAL audio: HUD sounds, object sounds, music, new-round sound |
| `ai/*` | Running Lua AI scripts in the client (`scriptedAI.*`) and the built-in factory/vehicle AI helpers |
| `configurationMode.*`, `startConfiguration.*` | Interactive server configuration mode, start-up parameters |
| `inputControl.*`, `cameraControl.*`, `gridControl.*` | Input and camera |
| `voting.*`, `timelapse.h`, `io.*`, `loading.*` | Votes, time-lapse capture, client IO, loading screen |

Rendering goes through `client/ExtRenderer/DirectX11.*` on top of the DeltaWorks engine. Shaders are in `client/shaders/*.hlsl`.

## 7. Lua integration

- `clientServerShared/lua/luaInterface.*` wraps the Lua 5.3 VM (`ThirdParty/lua`).
- `clientServerShared/lua/luaGameAPI.*` binds the `Game` table (units, factions, grid, AI estimates, markers, logging, randomness …).
- `clientServerShared/lua/scriptCache.*` caches scripts and sends them to clients on demand.
- Lua runs in four places: the **research script** (server), **resource distribution** scripts (server, planet setup), **AI scripts** (console or visual client, one per AI faction) and the **tutorial** (client). See [lua-scripting.md](lua-scripting.md).

## 8. Server configuration and the control console

`server/components/reflection.*` builds a virtual tree of folders, variables and commands on top of DeltaWorks' CLI interpreter. The variables are declared, with help texts and defaults, in `server/config/declaration.xml`, and some have change handlers (for example, assigning the Nexus AI script). The same tree feeds:

- `server.ini` loading and `--generate`,
- the localhost **remote control** service (`remoteControl.*`), which ControlConsole and ServerManager talk to,
- save-game variables (`Reflection::CheckAllSaveGameVariables()`).

## 9. Persistence

- `server/components/state/fileIO.*` reads and writes `.hsf` save games (world, entities, factions, alliances, research state, AI name and state, statistics), and handles auto-saves.
- `server/components/journalSaveGame.*` implements the **journal**. Every player action that reaches the server (entity commands, triggers, construction, faction creation and destruction …) is a `Journal::Round::*` entry with `Decode`/`Compile`/`Execute`. Each round records its entries plus an entity verification. The design goal, documented at the top of the header, is save games that can be rebuilt from an initial state plus the command log with repeatable random numbers, and eventually merged. Today the entries are mainly used to apply commands consistently and to verify state. Committing the journal to disk is commented out in the main loop.
- `clientServerShared/fileVersion.h` versions the formats. Loaders keep backward compatibility with older saves where they can.

## 10. Logging, events and tests

- `clientServerShared/logEvent.*` provides `LogEvent(EventClass/EventSignificance, …)`. `server/components/serverLogEvent.*` writes server log files.
- `clientServerShared/globalEvents.*` holds global event hooks (`frameBegin`, `serverConnectionEstablished` …).
- `server/components/tests.*` contains the self-tests run by `Server.exe --test` (serialization, terrain and others).

## 11. Optional and legacy subsystems

- `MetaServer.C#/` ("Consensus") is an unfinished C# meta-server for a persistent multi-planet universe (solar systems, factions across planets, TLS certificate authentication). The game does not use it.
- `steam-public` branch: the Steamworks integration (lobbies, auth tickets, upload artifacts), without the Steamworks SDK. Not part of `master`. See [repository.md](repository.md).
- `master-resource_bridge` branch: the Lua **modding** framework (scripted structure types, command callbacks). See [repository.md](repository.md).
