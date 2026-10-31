# Repository guide

## Layout

| Path | Contents |
|---|---|
| `client/` | Visual and console client. `clientMain.cpp` (main loop and start-up parameters), `components/` (network, animation, visual scene and HUD, audio, AI, input, configuration mode), `ExtRenderer/DirectX11.*`, `DX11/` (texture font), `shaders/` (`.hlsl` + `compiled/`), `objects/` (models: `.obj/.mtl`, `.cgs`, source `.blend/.max`), `resources/` (icons, textures, sounds, music, tutorial images), `Font/`, `game.ini`, `version.h`, `buildCounter.h`, runtime DLLs and installers |
| `server/` | `server.cpp` (main loop, CLI, start-up), `components/` (state: game, world, network, file IO; evolver; reflection; remote control; journal; AI process; UDP broadcast; Nexus threat; tests), `config/declaration.xml` (server variables), `config/tutorial.hsf` |
| `clientServerShared/` | Shared game model: `grid/`, `objects/` (incl. `structure/`, `research/`, `nexus/`), `lua/`, `protocol.*` + `protocol/`, `runtimeConfig.*`, `stringTable.h`, `logEvent.*`, `globalEvents.*`, `boot.*`, `typeDecl.h` |
| `clientConsoleShared/` | `serverProcess.*`: starting and supervising a local server process (used by the clients and ControlConsole) |
| `globalShared/` | `ports.h`, `startProcess.*` |
| `lua/` | `attributesAndResearch.lua`, `tutorial.lua`, `AI/`, `distributions/`, `util/`, `General Lua API Documentation.txt` |
| `predefinedPlanets/` | Planet templates (`*.ini`) |
| `ThirdParty/lua/` | Lua 5.3.3 |
| `ControlConsole/`, `ServerManager/`, `Bake/`, `BugReporter/`, `BuildCounter/`, `IncreaseVersion/`, `DocumentationEditor/`, `MetaServer.C#/` | Tools, see [tools.md](tools.md) |
| `documentation/` | In-game/website wiki (`documentation/`), images, PHP renderer |
| `ArtWork/` | Logos, Steam store art, screenshots (`*.psd` in Git LFS) |
| `issues/` | Notes on investigated bugs (`possibly-fixed/…`) |
| Root text files | `changelog.txt` (player changelog), `info.txt` (credits), `introduction.txt` / `introduction long.txt` (store texts), `concept.txt` (design notes for resource model 2.0, modding ideas, automation arena), `todo.txt`, `new  structures.txt`, `protocol documentation.txt`, `programmingNotes.md`, `tutorial maps.txt`, `*BakeRecipe.txt`, `repack.bat`, `documentation.odt` |

## Branches

Cleaned up on 2026-10-31. Four branches remain:

| Branch | Last change | Content |
|---|---|---|
| `master` | 2022-12-26 | The complete game. Every finished feature branch is merged: aerial units, T2 upgrades, plants, dynamic and advanced resources (plantation, composite materials), alliances, statistics, sensor rework, GUI refactoring, new Nexus presence, reworked Lua AI. |
| `master-resource_bridge` | 2022-12-26 | `master` + the Lua **modding** framework (scripted structure types, command callbacks, first example "Resource Relay"). Work in progress. |
| `master-styled_highres` | 2021-11-28 | `master` + styled dark rendering and super-large screenshot rendering. |
| `steam` | 2021-11-20 | `master` (June 2021) + the Steamworks integration and Steam upload artifacts. **Contains the Steamworks SDK. Do not publish.** |

Feature branches were kept small and adjusted so they merge into `master` without conflicts (commits like *"Reduced merge conflicts"*). Prototype branches (`main_prototype`, `main_steam_prototype`) used to collect several features at once and were recreated from time to time.

## Tags

### Releases

`v0.3.19` … `v0.9.12` (37 tags, including the odd `v.0.4.0`). Player-facing changes per version are in `changelog.txt`. `master` is at **v0.9.13** (unreleased).

### Archived experiments (`archive/*`)

Branches that were never merged into `master` but are worth keeping:

| Tag | From | Content |
|---|---|---|
| `archive/technite`, `archive/technite2` | 2015–2016 | Programmable "technite" cells living in the terrain, controlled by an external client through JSON messages. `technite2` is a rework with message relaying and selection |
| `archive/cft_pylon` | 2016–2017 | Wireless resource transfer through pylons. Discontinued as too complex |
| `archive/ft_orbital` | 2016 | Orbital stations, built on the pylon work |
| `archive/cft_new_artillery` | 2016–2017 | Artillery rebalance, layer-by-layer terrain digging |
| `archive/consensus`, `archive/ft_social` | 2015–2016 | Client side of the "consensus" meta-network (TLS certificates, AES, galaxy module) |
| `archive/master-better_gui` | 2018 | New GUI renderer (blur, edges, cut corners) |
| `archive/master-ft_nexus_tower` | 2018 | Nexus tower structure |
| `archive/master-gui_hex_background` | 2018 | Hexagonal GUI backgrounds |
| `archive/master-ft_server_network_0`, `_1` | 2016 | Grid server and peer-to-peer server networking |
| `archive/master-objectAnimationRenderer` | 2017 | Rendering object rotation frames |
| `archive/ft_progressive_lighting` | 2016 | Progressive grid light calculation |
| `archive/ft_music` | 2016 | First music titles |
| `archive/ft_radial_reshoriz_rendering` | 2016 | Compound visualization, resource sessions |
| `archive/main_prototype` | Feb 2018 | Old non-Steam prototype (`master` + aerial at the time) |

To pick one up again: `git switch -c my-branch archive/<name>`. Most of these are thousands of commits behind `master`, so expect to re-implement rather than merge.

## History notes

- **Nov 2021: LFS migration.** `git filter-repo` rewrote the history from about Feb 2017 onward to move `ArtWork/` into Git LFS (only `ArtWork/*.psd` is tracked by LFS today; see `.gitattributes`). Commit hashes from before the rewrite, which appear in old commit messages and notes, don't exist in the current history.
- Commit messages reference DeltaWorks commits (*"Adapt to changes in base library (<sha>)"*), which tells you which DeltaWorks version a Hexaverse commit builds against.
- The shared code folder was called `shared/` before it was renamed to `clientServerShared/`.

## Contributing workflow

- Branch from `master`. Name feature branches `master-<feature>` (the historical convention) or anything descriptive.
- Prefix commit messages with the area (`Objects:`, `GUI:`, `Server:`, `Client:`, `LuaAPI:`, `LuaAI:`, `Rendering:`, `Protocol:`, `IO:`, `General:` …).
- Add player-visible changes to the top section of `changelog.txt` (`+` new, `>` changed).
- Don't commit generated `.projector/` folders or local `.sln`/`.vcxproj` files.
