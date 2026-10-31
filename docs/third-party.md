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
| OpenAL 1.1 SDK | installed separately | Supplies `OpenAL32.lib` for the Visual Client. Required by the Projector build; the CMake build can generate the import library from `cmake/OpenAL32.def` instead |
| `OpenAL32.dll`, `oalinst.exe` | `client/` | OpenAL redistributable and installer, committed for convenience |
| `D3DCompiler_43.dll`, `D3DCompiler_47.dll`, `dxwebsetup.exe` | `client/` | Microsoft DirectX redistributables, committed for convenience |
| Steamworks SDK | **only on the `steam` branch** (`steam_sdk/`) | Its license doesn't allow public redistribution. It isn't in `master`, any `archive/*` tag or any version tag. The `steam` branch is therefore kept private. |

The Microsoft and OpenAL redistributables are committed so the game runs straight from a build. Their redistribution terms allow shipping them with an application.

## Assets

| Asset | Source | License |
|---|---|---|
| `newRound.wav` | "begin.wav" by tim.kahn (freesound 85032) | CC BY 3.0 |
| `construct.wav` | cut from "road_construction.wav" by aesqe (freesound 138214) | CC BY 3.0 |
| `catbase.wav` | cut from "boat speeding, water.wav" by jazbavac (freesound 198519) | CC0 1.0 |
| `beam.wav` | mix of "Laser 1 rev down.ogg" by jobro (freesound 245153) | CC0 1.0 |
| `buttonDown.wav` | "Click Tick.wav" by malle99 (freesound 384187) | CC0 1.0 |
| Console Client icon | "TERMINAL ICON", GitHub Octicons | MIT |

The CC BY 3.0 sounds need attribution in any redistribution. `info.txt` provides it. The other sounds (explosions, units, turret rotation, music `Hexaverse Title M4.mp3`), the 3D models (`client/objects/*.obj/.blend/.max`), textures, icons, the font and `ArtWork/` aren't listed as third-party. `sources.txt` in `client/resources/audio/` and `objectSounds/` only says "declare in info.txt". They are original work and fall under the repository's [MIT License](../LICENSE).

## Online services referenced in code

| Where | Endpoint | Note |
|---|---|---|
| `BugReporter/MainForm.cs` | `https://www.hexaverse.net/bugReport/store.php` and `privacyNotice.php` | Original bug-report upload |
| `info.txt`, docs | `https://www.hexaverse.net` | Original website |

## Other committed files

- Prebuilt tools, so the game can be packaged without building them: `Bake/Bake.exe`, `ServerManager/ServerManager.exe`, `ServerManager/Protocol.dll`, `BugReporter/BugReporter.exe`, `BuildCounter/BuildCounter.exe`.
- `MetaServer.C#/Runtime/certificate.pfx`: a test certificate for the experimental meta-server. Don't use it for anything real.

## License

Everything in this repository that isn't listed above as third-party is covered by the [MIT License](../LICENSE).
