# Building Hexaverse

Hexaverse is a Windows-only C++ code base, plus a few C# tools. The C++ programs aren't described by `.vcxproj` files in the repository. Instead they use small XML **`.solution` / `.project`** files that [Projector](https://github.com/IronFox/Projector) turns into Visual Studio solutions. The game depends on IronFox's base library [DeltaWorks](https://github.com/IronFox/DeltaWorks).

## 1. Prerequisites

| Requirement | Used by | Notes |
|---|---|---|
| Windows 10/11 x64 | everything | The client uses Direct3D 11 and Win32 windowing; the server uses Win32 console APIs. |
| Visual Studio 2019, 2022 or 2026 with the **Desktop development with C++** workload | C++ projects | All projects compile with `/std:c++latest`. Projector detects the installed toolsets (v142/v143/v145). |
| Windows 10/11 SDK | C++ projects | Projector picks the newest installed one. |
| [Projector](https://github.com/IronFox/Projector) | C++ projects | .NET 10 WinForms app. Build it with `dotnet build Projector.sln -c Release`. |
| [DeltaWorks](https://github.com/IronFox/DeltaWorks) | all C++ projects | Provides `DeltaWorksCore` and `DeltaWorksEngine`. Clone it anywhere. |
| **OpenAL 1.1 SDK** (Creative Labs) | Visual Client | Found through the registry key `HKLM\SOFTWARE\Wow6432Node\Creative Labs\OpenAL 1.1 Software Development Kit\3.05` (or `3.03`) `InstallDir`. |
| Microsoft DirectX SDK (June 2010) | Visual Client (optional) | Marked `warn_if_not_found="false"`. The Windows SDK supplies `d3d11.lib` and `d3dcompiler.lib`, so the old SDK is only needed for legacy headers. |
| .NET Framework 4.0–4.6.1 developer packs | C# tools | ServerManager, Bake, BugReporter, DocumentationEditor, MetaServer. These are plain `.sln`/`.csproj` files that open directly in Visual Studio. |
| Git LFS | repository | `ArtWork/*.psd` is stored in LFS. Run `git lfs install` before cloning. |

## 2. Getting the sources

```bat
git lfs install
git clone https://github.com/IronFox/DeltaWorks.git
git clone https://github.com/IronFox/Projector.git
git clone https://github.com/IronFox/Hexaverse.git
```

The folder layout is up to you. Projector asks where each referenced project lives the first time (see below).

## 3. How the C++ projects are organized

| Solution file | Projects | Output (Release, x64) |
|---|---|---|
| `client/Client.solution` | `Visual Client` (Windows app), `Console Client` (console app), `Increment Build Counter` | `client/Visual Client.exe`, `client/Console Client.exe` |
| `server/Server.solution` | `Server` (console app) | `server/Server.exe` |
| `ControlConsole/ControlConsole.solution` | `ControlConsole` | `ControlConsole/ControlConsole.exe` |
| `BuildCounter/BuildCounter.solution` | `BuildCounter` | `BuildCounter/BuildCounter.exe` |
| `ThirdParty/lua/lua.project` | `lua` (static library, Lua 5.3.3 without `lua.c`) | – |

Each `.project` pulls in shared code from the sibling folders through `<source path="…" include="true">`:

| Shared folder | Visual Client | Console Client | Server | ControlConsole |
|---|:-:|:-:|:-:|:-:|
| `clientServerShared/` | ✓ | ✓ | ✓ | |
| `clientConsoleShared/` | ✓ | ✓ | | ✓ |
| `globalShared/` | ✓ | ✓ | ✓ | ✓ |

The **Console Client** is built from the same `client/` sources, but excludes `visual/`, `audio/` and the consensus link, and drops the `HAS_WINDOW` macro. The server launches it as the headless process that runs Lua AI scripts.

Projects reference each other by name (`<referenceProject name="DeltaWorksCore" />`). Projector resolves a name in this order:

1. an explicit `path` attribute,
2. a `<name>.project` file next to the `.solution`,
3. the per-user registry `Documents\projector\pathRegistry.txt`,
4. otherwise a file dialog, whose answer is saved into the registry.

So the first time you open `client/Client.solution`, Projector asks for `DeltaWorksCore.project` (`DeltaWorks/Core/`), `DeltaWorksEngine.project` (`DeltaWorks/Engine/`) and `lua.project` (`Hexaverse/ThirdParty/lua/`). After that they're remembered.

The `.project`/`.solution` format is documented in the Projector repository.

## 4. Build steps

1. **BuildCounter.** `Increment Build Counter` runs `..\BuildCounter\BuildCounter.exe buildCounter.h` as a pre-build step. A prebuilt `BuildCounter.exe` is committed, so this normally just works. If you change BuildCounter, or the exe is missing, build `BuildCounter/BuildCounter.solution` (Release) first.
2. Open `client/Client.solution` in Projector, choose a toolset, **Generate**, then **Open in Visual Studio** (or use Projector's *Build Solutions* dialog for a parallel bulk build).
3. Build `Visual Client` and `Console Client`. Use **Release** or **OptimizedDebug** for playable speed; plain Debug is slow.
4. Repeat for `server/Server.solution` and `ControlConsole/ControlConsole.solution`.
5. Optional: build the C# tools by opening their `.sln` files directly (`ServerManager/ServerManager.sln`, `BugReporter/BugReporter.sln`, `Bake/Bake.sln`, `DocumentationEditor/DocumentationEditor.sln`). Prebuilt copies of `ServerManager.exe`, `Bake.exe` and `BugReporter.exe` are committed next to their sources.

### Configurations and output names

Projector generates three configurations for `Win32` and `x64`:

| Configuration | Macro | Notes |
|---|---|---|
| `Debug` | `_DEBUG` | No optimization. In Debug, the server starts the **Visual** client with a window as its AI process, so you can watch it. |
| `OptimizedDebug` | `_OPTIMIZED_DEBUG` | Optimized, with symbols. |
| `Release` | – | Writes the executable directly into the `.project` folder (`client/`, `server/` …). |

Release names are `<Project>.exe` on x64 and `<Project> x86.exe` on Win32, so packaging recipes use wildcards like `server/Server*.exe`. All projects link the static CRT (`/MT`), so no VC++ redistributable is needed.

Generated files land in `.projector/` folders next to the projects and solutions. They contain absolute paths, so don't commit them.

### Shaders and binary data

HLSL sources are in `client/shaders/*.hlsl`. The compiled shader blobs the client loads at runtime are committed in `client/shaders/compiled/`, and the compiled geometries in `client/objects/*.cgs`. Projector does **not** compile `.hlsl` files.

The runtime DLLs and installers the client needs (`OpenAL32.dll`, `D3DCompiler_43.dll`, `D3DCompiler_47.dll`, `oalinst.exe`, `dxwebsetup.exe`) are committed in `client/`.

## 5. Running from the build tree

The programs find their data through paths relative to their **working directory**: for example `../lua/`, `../predefinedPlanets/`, and `..\client\` when the server starts an AI client. Always start them with their own project folder as the working directory. Release builds put the executables there anyway, and Projector sets the debugger working directory to the project folder. See [running.md](running.md).

## 6. Packaging a release

The `Bake` tool copies files that match a recipe into an output folder:

```bat
repack.bat
```

1. `Bake\Bake.exe mainBakeRecipe baked` uses `mainBakeRecipe.txt` and collects the game: client executables, shaders, fonts, resources, server, ControlConsole, ServerManager, Lua scripts, planets and so on.
2. The result is zipped into `game.7z` with `7z.exe`, which must be on `PATH`.
3. The same is repeated with `serverBakeRecipe.txt` to produce `server.7z`, a dedicated-server-only package.

Recipe syntax: one path or wildcard pattern per line, relative to the repository root. The folder structure is kept in the output.

## 7. Version numbers

- `client/buildCounter.h` is bumped automatically on every client build by `Increment Build Counter`/`BuildCounter.exe`.
- `client/version.h` holds the game version. `IncreaseVersion/` is a small hand-made VS project for bumping it.
- `changelog.txt` is the player-facing changelog. The newest section is v0.9.13.

## 8. Troubleshooting

| Symptom | Fix |
|---|---|
| Projector keeps asking for `DeltaWorksCore.project` | Point it at `DeltaWorks/Core/DeltaWorksCore.project`. To change a wrong answer, edit `Documents\projector\pathRegistry.txt`. |
| `Increment Build Counter` fails with a missing exe | Restore `BuildCounter/BuildCounter.exe`, or build `BuildCounter` (Release) first. |
| OpenAL headers not found | Install the OpenAL 1.1 SDK, or check the registry key above. |
| Client starts but shows a black screen or crashes when loading shaders | Start it from `client/` as the working directory, and make sure `shaders/compiled/` exists. |
| The server can't find planets or Lua scripts | Start it from `server/` as the working directory. |
| Compile errors in DeltaWorks headers | DeltaWorks has moved on since Hexaverse's last change. Check out the DeltaWorks commit named in the newest *"Adapt to changes in base library (…)"* commit message. For `master` that is `dc4e4d0` (`git -C DeltaWorks checkout dc4e4d0`). |
