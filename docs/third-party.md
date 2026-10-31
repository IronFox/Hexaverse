# Third-party components

This page lists everything in the repository, or required to build it, that wasn't written for Hexaverse. The player-facing credits are in [`info.txt`](../info.txt), which ships with the game.

## Bundled source code

| Component | Version | Location | License | Notes |
|---|---|---|---|---|
| Lua | 5.3.3 (May 2016) | `ThirdParty/lua/` | MIT | Built as a static library (`lua.project`, without `lua.c`). Lua 5.3.6 is the last 5.3 release, so an update is worth considering. The license text is in `doc/readme.html`. |
| WikiRenderer | 3.1.8 | `documentation/renderer/` | **LGPL-2.1** (`LICENCE`) | PHP library used by the website's documentation renderer. Not part of the game binaries. |

## Libraries pulled in through DeltaWorks

Hexaverse links DeltaWorks statically. DeltaWorks bundles or fetches (through vcpkg) zlib, libpng, libjpeg(-turbo), bzip2 and open-simplex-noise. Their licenses and versions are tracked in the DeltaWorks repository. `info.txt` credits **libbzip2** and **libpng**.

## External SDKs and runtime files (not open source)

| Component | Location | Notes |
|---|---|---|
| OpenAL 1.1 SDK | installed separately | Needed to build the Visual Client |
| `OpenAL32.dll`, `oalinst.exe` | `client/` | OpenAL redistributable and installer, committed for convenience |
| `D3DCompiler_43.dll`, `D3DCompiler_47.dll`, `dxwebsetup.exe` | `client/` | Microsoft DirectX redistributables, committed for convenience |
| DirectX SDK (June 2010) | installed separately, optional | Legacy headers |
| Steamworks SDK | **only on the `steam` branch** (`steam_sdk/`) | Its license doesn't allow public redistribution. It isn't in `master`, any `archive/*` tag or any version tag. Keep the `steam`, `main_steam_prototype` and `steam-ft_html_gui` branches out of the public repository. |

Before publishing, decide whether the Microsoft and OpenAL redistributables should stay in the repo. Their redistribution terms allow shipping them with an application, but not every open-source host or package welcomes binaries.

## Assets

| Asset | Source | License |
|---|---|---|
| `newRound.wav` | "begin.wav" by tim.kahn (freesound 85032) | CC BY 3.0 |
| `construct.wav` | cut from "road_construction.wav" by aesqe (freesound 138214) | CC BY 3.0 |
| `catbase.wav` | cut from "boat speeding, water.wav" by jazbavac (freesound 198519) | CC0 1.0 |
| `beam.wav` | mix of "Laser 1 rev down.ogg" by jobro (freesound 245153) | CC0 1.0 |
| `buttonDown.wav` | "Click Tick.wav" by malle99 (freesound 384187) | CC0 1.0 |
| Console Client icon | "TERMINAL ICON", GitHub Octicons | MIT |

The CC BY 3.0 sounds need attribution in any redistribution. `info.txt` provides it. The other sounds (explosions, units, turret rotation, music `Hexaverse Title M4.mp3`), the 3D models (`client/objects/*.obj/.blend/.max`), textures, icons, the font and `ArtWork/` aren't listed as third-party. `sources.txt` in `client/resources/audio/` and `objectSounds/` only says "declare in info.txt". Before release, confirm they're original work and put them under an explicit asset license.

## Online services referenced in code

| Where | Endpoint | Note |
|---|---|---|
| `BugReporter/MainForm.cs` | `https://www.hexaverse.net/bugReport/store.php` and `privacyNotice.php` | Original bug-report upload |
| `info.txt`, docs | `https://www.hexaverse.net` | Original website |

## Files to review before a public release

- `MetaServer.C#/Runtime/certificate.pfx`: a committed certificate (possibly with a private key) for the experimental meta-server.
- Committed binaries: `Bake/Bake.exe`, `ServerManager/ServerManager.exe`, `ServerManager/Protocol.dll`, `BugReporter/BugReporter.exe`, `BuildCounter/BuildCounter.exe`, and the client DLLs and installers above.
- No top-level `LICENSE` exists yet.
