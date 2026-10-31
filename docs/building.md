# Building Hexaverse

Hexaverse is a C++ code base, plus a few C# tools. **It currently builds only on Windows, with the Visual Studio compiler (MSVC)**: it uses Direct3D 11, the Win32 API and MSVC extensions. There is no Linux or macOS build.

There are two equivalent ways to build the C++ programs:

- **CMake** (`CMakeLists.txt` in the repository root and in each program folder). Works from the command line, from Visual Studio's *Open Folder*, and in CI.
- **[Projector](https://github.com/IronFox/Projector)**, which turns the small XML **`.solution` / `.project`** files into Visual Studio solutions. This is how Hexaverse was originally built.

Both compile the same files with the same settings, and both put the executables in the same places. Both depend on IronFox's base library [DeltaWorks](https://github.com/IronFox/DeltaWorks).

## 1. Prerequisites

| Requirement | Used by | Notes |
|---|---|---|
| Windows 10/11 x64 | everything | The client uses Direct3D 11 and Win32 windowing; the server uses Win32 console APIs. |
| Visual Studio 2022 or 2026 with the **Desktop development with C++** workload | C++ projects | DeltaWorks needs VS 2022 or newer. All projects compile with `/std:c++latest`. |
| Windows 10/11 SDK | C++ projects | Supplies `d3d11.lib`, `d3dcompiler.lib` and the Direct3D headers. The old DirectX SDK (June 2010) isn't needed: the code no longer uses D3DX or DxErr. |
| CMake 3.21 or newer | CMake build | Visual Studio installs one ("C++ CMake tools for Windows"). |
| vcpkg | CMake build, optional | Supplies zlib, libpng, libjpeg-turbo and bzip2 (`vcpkg.json`). Ships with Visual Studio ("vcpkg package manager"). Without it, CMake uses the copies bundled in DeltaWorks. |
| [Projector](https://github.com/IronFox/Projector) 1.5 or newer | Projector build | .NET 10 WinForms app. Build it with `dotnet build Projector.sln -c Release`. |
| [DeltaWorks](https://github.com/IronFox/DeltaWorks) | all C++ projects | Provides `DeltaWorksCore` and `DeltaWorksEngine`. CMake downloads it if needed. |
| OpenAL 1.1 SDK (Creative Labs) | Visual Client: required for Projector, optional for CMake | Only its import library `OpenAL32.lib` is needed; the OpenAL headers are bundled in DeltaWorks (`Engine/al`). Projector finds it through the registry key `HKLM\SOFTWARE\Wow6432Node\Creative Labs\OpenAL 1.1 Software Development Kit\3.05` (or `3.03`) `InstallDir`. |
| .NET Framework 4.0–4.6.1 developer packs | C# tools | ServerManager, Bake, DocumentationEditor, MetaServer. These are plain `.sln`/`.csproj` files that open directly in Visual Studio. |
| .NET 10 SDK | Bug reporter, optional | Only for the optional bug reporter, see [bug-reporter.md](bug-reporter.md). |
| Git LFS | repository | `ArtWork/*.psd` is stored in LFS. Run `git lfs install` before cloning. |

## 2. Building with CMake

### Quick start

From a *Developer PowerShell for VS 2022/2026* (which sets `VCPKG_ROOT`), or with `VCPKG_ROOT` pointing at a vcpkg checkout:

```bat
git lfs install
git clone https://github.com/IronFox/Hexaverse.git
cd Hexaverse
cmake --preset vs-x64
cmake --build --preset vs-x64-release
```

The first configure installs the vcpkg packages, which takes a few minutes. In Visual Studio you can instead use *File → Open → Folder* on the repository; it reads `CMakePresets.json`.

### Presets

| Configure preset | Platform | Third-party libraries |
|---|---|---|
| `vs-x64` | x64 | vcpkg |
| `vs-win32` | Win32 | vcpkg |
| `vs-x64-bundled` | x64 | DeltaWorks' bundled copies, no vcpkg needed |
| `vs-win32-bundled` | Win32 | DeltaWorks' bundled copies |
| `ninja-msvc` | x64, Ninja Multi-Config | vcpkg (run from a developer prompt) |

The Visual Studio presets use the newest installed Visual Studio. Build presets: `vs-x64-debug`, `vs-x64-optimizeddebug`, `vs-x64-release`, `vs-win32-debug`, `vs-win32-release`, `vs-x64-bundled-release`, `vs-win32-bundled-release`, `ninja-msvc-release`.

Without presets:

```bat
cmake -S . -B build -A x64 -DDELTAWORKS_THIRD_PARTY=BUNDLED
cmake --build build --config Release --parallel
```

### Where DeltaWorks comes from

1. `HEXAVERSE_DELTAWORKS_DIR`, if set (a DeltaWorks checkout with a `CMakeLists.txt`),
2. otherwise `../DeltaWorks`, if that folder has a `CMakeLists.txt`,
3. otherwise CMake downloads DeltaWorks at the commit in `HEXAVERSE_DELTAWORKS_GIT_TAG` (FetchContent).

DeltaWorks is built as part of the Hexaverse build (`DeltaWorks::Core`, `DeltaWorks::Engine`). Its own options apply, for example `DELTAWORKS_THIRD_PARTY` (`PACKAGES`, `BUNDLED` or `AUTO`); see DeltaWorks' `BUILDING.md`.

### Targets

| CMake target | Mirrors | Output (Release, x64) |
|---|---|---|
| `VisualClient` | `client/Visual Client.project` | `client/Visual Client.exe` |
| `ConsoleClient` | `client/Console Client.project` | `client/Console Client.exe` |
| `IncrementBuildCounter` | `client/Increment Build Counter.project` | bumps `client/buildCounter.h` |
| `Server` | `server/Server.project` | `server/Server.exe` |
| `ControlConsole` | `ControlConsole/ControlConsole.project` | `ControlConsole/ControlConsole.exe` |
| `BuildCounter` | `BuildCounter/BuildCounter.project` | `BuildCounter/BuildCounter.exe` |
| `lua` | `ThirdParty/lua/lua.project` | static library |

### Options

| Option | Default | Meaning |
|---|---|---|
| `HEXAVERSE_DELTAWORKS_DIR` | empty | DeltaWorks checkout to use (see above) |
| `HEXAVERSE_DELTAWORKS_GIT_TAG` | tested commit | DeltaWorks commit to download |
| `HEXAVERSE_OPENAL` | `AUTO` | Where `OpenAL32.lib` comes from. `SDK`: the OpenAL 1.1 SDK (registry, `OPENAL_SDK_DIR` or `OPENALDIR`). `GENERATED`: an import library made from `cmake/OpenAL32.def`, the export list of the OpenAL 1.1 router DLL committed in `client/`. `AUTO`: the SDK if installed, otherwise generated. |
| `HEXAVERSE_BUILD_CLIENT`, `HEXAVERSE_BUILD_SERVER`, `HEXAVERSE_BUILD_CONTROLCONSOLE` | `ON` | Which programs to build |
| `HEXAVERSE_INCREMENT_BUILD_COUNTER` | `ON` | Bump `client/buildCounter.h` before each client build, like Projector does. Turn it off to keep the working tree clean. |
| `HEXAVERSE_OUTPUT_TO_PROJECT_FOLDERS` | `ON` | Put executables where Projector puts them (see below). With `OFF` they stay in the build folder, but then the programs can't find each other. |
| `HEXAVERSE_CXX_STANDARD` | `23` | C++ standard for Hexaverse and DeltaWorks (MSVC: `/std:c++latest`) |

The CMake lists mirror the `<source>`/`<exclude>` rules of the `.project` files. When you change one, change the other.

## 3. Building with Projector

### Getting the sources

```bat
git lfs install
git clone https://github.com/IronFox/DeltaWorks.git
git clone https://github.com/IronFox/Projector.git
git clone https://github.com/IronFox/Hexaverse.git
```

The folder layout is up to you. Projector asks where each referenced project lives the first time (see below).

### How the projects are organized

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

So the first time you open `client/Client.solution`, Projector asks for `DeltaWorksCore.project` (`DeltaWorks/Core/`), `DeltaWorksEngine.project` (`DeltaWorks/Engine/`) and `lua.project` (`Hexaverse/ThirdParty/lua/`). After that they're remembered. `DeltaWorksCore.project` declares `<vcpkg />`, so the generated projects install zlib, libpng, libjpeg and bzip2 through vcpkg.

The `.project`/`.solution` format is documented in the Projector repository.

### Build steps

1. **BuildCounter.** `Increment Build Counter` runs `..\BuildCounter\BuildCounter.exe buildCounter.h` as a pre-build step. A prebuilt `BuildCounter.exe` is committed, so this normally just works. If you change BuildCounter, or the exe is missing, build `BuildCounter/BuildCounter.solution` (Release) first.
2. Open `client/Client.solution` in Projector, choose a toolset, **Generate**, then **Open in Visual Studio** (or use Projector's *Build Solutions* dialog for a parallel bulk build).
3. Build `Visual Client` and `Console Client`. Use **Release** or **OptimizedDebug** for playable speed; plain Debug is slow.
4. Repeat for `server/Server.solution` and `ControlConsole/ControlConsole.solution`.

Generated files land in `.projector/` folders next to the projects and solutions. They contain absolute paths, so don't commit them.

## 4. Configurations and output locations

Both builds have three configurations, for `Win32` and `x64` (CMake also keeps its usual `RelWithDebInfo` and `MinSizeRel`):

| Configuration | Macro | Executable location | Notes |
|---|---|---|---|
| `Debug` | `_DEBUG` | `<project folder>/.projector/x64/Debug/` (Win32: `.projector/Debug/`) | No optimization. In Debug, the server starts the **Visual** client with a window as its AI process, so you can watch it. |
| `OptimizedDebug` | `_OPTIMIZED_DEBUG` | `<project folder>/.projector/x64/OptimizedDebug/` | Optimized, with symbols. |
| `Release` | – | the project folder itself (`client/`, `server/` …) | |

These locations matter: the programs find each other through them (`globalShared/startProcess.cpp`). Release names are `<Project>.exe` on x64 and `<Project> x86.exe` on Win32, so packaging recipes use wildcards like `server/Server*.exe`. All projects link the static CRT (`/MT`), so no VC++ redistributable is needed.

## 5. C# tools

Build the C# tools by opening their `.sln` files directly (`ServerManager/ServerManager.sln`, `BugReporter/BugReporter.sln`, `Bake/Bake.sln`, `DocumentationEditor/DocumentationEditor.sln`). Prebuilt copies of `ServerManager.exe` and `Bake.exe` are committed next to their sources. They aren't part of the CMake build. The optional bug reporter is an SDK-style .NET 10 project (`dotnet build BugReporter\BugReporter.csproj`); it isn't packaged by default, see [bug-reporter.md](bug-reporter.md).

### Shaders and binary data

HLSL sources are in `client/shaders/*.hlsl`. The compiled shader blobs the client loads at runtime are committed in `client/shaders/compiled/`, and the compiled geometries in `client/objects/*.cgs`. Neither build compiles `.hlsl` files.

The runtime DLLs and installers the client needs (`OpenAL32.dll`, `D3DCompiler_43.dll`, `D3DCompiler_47.dll`, `oalinst.exe`, `dxwebsetup.exe`) are committed in `client/`.

## 6. Running from the build tree

The programs find their data through paths relative to their **working directory**: for example `../lua/`, `../predefinedPlanets/`, and `..\client\` when the server starts an AI client. Always start them with their own project folder as the working directory. Release builds put the executables there anyway, and both Projector and CMake set the Visual Studio debugger working directory to the project folder. See [running.md](running.md).

## 7. Packaging a release

The `Bake` tool copies files that match a recipe into an output folder:

```bat
repack.bat
```

1. `Bake\Bake.exe mainBakeRecipe baked` uses `mainBakeRecipe.txt` and collects the game: client executables, shaders, fonts, resources, server, ControlConsole, ServerManager, Lua scripts, planets and so on.
2. The result is zipped into `game.7z` with `7z.exe`, which must be on `PATH`.
3. The same is repeated with `serverBakeRecipe.txt` to produce `server.7z`, a dedicated-server-only package.

Recipe syntax: one path or wildcard pattern per line, relative to the repository root. The folder structure is kept in the output. `source/*.* -> target/*.*` copies matching files into a different folder (used to add the [bug reporter](bug-reporter.md#adding-it-to-a-release)).

### GitHub releases

`.github/workflows/release.yml` does the same on GitHub Actions and publishes the result as a GitHub release. It builds Release x64 with CMake, runs Bake with both recipes, zips the output as `Hexaverse-<tag>-win64.zip` and `Hexaverse-Server-<tag>-win64.zip`, and creates the release with the newest `changelog.txt` section as its notes. It starts when a `v*` tag is pushed (`git tag v0.9.14 && git push origin v0.9.14`), or from the Actions tab (*Release → Run workflow*), which creates the tag on the chosen commit.

### CI

`.github/workflows/cmake.yml` compiles every pull request and every push to `master` on Windows: x64 Release with vcpkg, x64 Debug and Win32 Release with the bundled libraries. Both workflows check out DeltaWorks with `actions/checkout`; if DeltaWorks is private, add a `DELTAWORKS_TOKEN` secret with read access to it.

## 8. Version numbers

- `client/buildCounter.h` is bumped automatically on every client build by `Increment Build Counter`/`BuildCounter.exe` (CMake: the `IncrementBuildCounter` target, unless `HEXAVERSE_INCREMENT_BUILD_COUNTER` is off).
- `client/version.h` holds the game version. `IncreaseVersion/` is a small hand-made VS project for bumping it.
- `changelog.txt` is the player-facing changelog. The newest section is v0.9.13.

## 9. Troubleshooting

| Symptom | Fix |
|---|---|
| Projector keeps asking for `DeltaWorksCore.project` | Point it at `DeltaWorks/Core/DeltaWorksCore.project`. To change a wrong answer, edit `Documents\projector\pathRegistry.txt`. |
| `Increment Build Counter` fails with a missing exe | Restore `BuildCounter/BuildCounter.exe`, or build `BuildCounter` (Release) first. |
| `OpenAL32.lib` not found (Projector) | Install the OpenAL 1.1 SDK, or check the registry key above. With CMake, leave `HEXAVERSE_OPENAL` at `AUTO`. |
| CMake: `HEXAVERSE_DELTAWORKS_DIR … has no CMakeLists.txt` | Your DeltaWorks checkout predates its CMake support. Pull it, or clear the variable so CMake downloads DeltaWorks. |
| CMake: vcpkg packages not found | Run from a VS developer prompt, set `VCPKG_ROOT`, or use a `*-bundled` preset. |
| Client starts but shows a black screen or crashes when loading shaders | Start it from `client/` as the working directory, and make sure `shaders/compiled/` exists. |
| The server can't find planets or Lua scripts | Start it from `server/` as the working directory. |
| Compile errors in DeltaWorks headers | Use the DeltaWorks commit CMake downloads by default (`HEXAVERSE_DELTAWORKS_GIT_TAG` in `CMakeLists.txt`), which is the one Hexaverse is tested against. |
