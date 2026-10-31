# Technite branch: status and how to run it

This branch (`technite`) is a frozen snapshot of Hexaverse **0.3.28 from June 2016**.
It adds technites: terrain cells that belong to a faction and that you program
from an external logic client. For the game-side rules, see
[`An introduction into Technites.txt`](An%20introduction%20into%20Technites.txt).

This file explains what you need to build and run the branch, and whether the
branch can be brought up to date.

> **Untested.** Everything below comes from reading the sources, not from a
> real build. The build is Windows-only and could not be run here. If you build it,
> please correct this file with what you find.

---

## 1. Can it be updated? (short answer: not by merging)

| | |
|---|---|
| Common ancestor with `master` | `ecd33898` (2016-06-05) |
| Commits on `master` since then | about 4,170 |
| What `technite` changes since then | 68 files, about 9,100 lines added and 310 removed |
| Trial merge into current `master` | **41 files conflict**, including 14 modify/delete conflicts |

Since 2016, `master` has deleted or moved most of the files the technite
work depends on: `shared/` was split into `clientServerShared/`,
`globalShared/` and `clientConsoleShared/`, and `shared/protocol.*`,
`shared/sphereGrid.*`, `shared/runtimeConfig.h`, `shared/objects/faction.*`,
`shared/objects/visualSelection.*`, `client/components/ui.*` and others are
gone or moved. `master` also builds with CMake and vcpkg, against today's
DeltaWorks layout.

**Recommendation:** keep this branch as a runnable historical snapshot (see section 2).
If technites should come back in current Hexaverse, re-implement them on `master`
and use this branch as the reference. These parts carry over almost unchanged:

- The **logic-client protocol** (`client/TechniteLogic/techniteProtocol.h` plus
  `client/components/techniteLogicLink.*`). It has no Hexaverse game state
  baked in and matches the external C# client exactly (see section 3).
- The **technite rules** in `shared/technite.*` and
  `server/components/evolvers/techniteEvolver.*`.
- Rendering in `client/shaders/gridTechnites.hlsl` and
  `gridTechniteShadow.hlsl`.

`technite2` is a separate, older rework from March and April 2016 (protocol "Aquinas v2.0").
It is **not** compatible with this branch or with the logic client listed below.

---

## 2. Building the 2016 snapshot

### 2.1 What you need

| Dependency | Version | Notes |
|---|---|---|
| Visual Studio with the **v140** toolset (VS 2015) | VS 2015, or newer VS with the *MSVC v140 – VS 2015 C++ build tools* component | `server/server.vcxproj` pins `PlatformToolset` v140. Newer toolsets will probably need source fixes. |
| **DeltaWorks** at a 2016 commit | `IronFox/DeltaWorks` **`3bfc206`** (2016-06-05). `8538152` (2016-06-19) should also work | **Current DeltaWorks will not work**: its headers moved (for example `general/monitoredProcess.h` is now `Core/io/monitoredProcess.h`). Every `#include <...>` on this branch resolves against `3bfc206`. |
| **Microsoft DirectX SDK (June 2010)** | June 2010 | Needed for `d3dx11.h`, `d3dx10.h` and `DxErr.h`, which the Windows SDK no longer ships. Found through the registry key `...\Microsoft DirectX SDK (June 2010)\InstallPath`. |
| **OpenAL 1.1 SDK** (Creative Labs) | 1.1 | Found through the registry key `...\Creative Labs\OpenAL 1.1 Software Development Kit\3.05` (or `3.03`). The runtime `client/OpenAL32.dll` is checked in. |
| **Projector** (`IronFox/Projector`) | any | Generates the client `.sln`/`.vcxproj` from `client/Client.solution` and `*.project`. Only the server ships a ready `.vcxproj`. |

### 2.2 Folder layout

Back then, the projects expected DeltaWorks to sit next to the projects folder:

```
<root>/
  include/               <- DeltaWorks checked out at 3bfc206
    DeltaWorks.project
    mvc12_project/DeltaWorks/DeltaWorks.vcxproj   (only if you use server.vcxproj as is)
  projects/
    Hexaverse/           <- this branch
```

`server/server.vcxproj` references
`..\..\..\include\mvc12_project\DeltaWorks\DeltaWorks.vcxproj`, and it has no
explicit include directories. It relies on `<root>/include` being on the
VC++ include path, which used to be a global setting. The simplest fix is to
skip `server.vcxproj` and generate everything with Projector:

1. `git clone https://github.com/IronFox/DeltaWorks include && git -C include checkout 3bfc206`
2. In Projector, register `include/DeltaWorks.project` under the name **DeltaWorks**,
   so that `<referenceProject name="DeltaWorks"/>` resolves.
3. Open `client/Client.solution` (Visual Client, Console Client,
   Increment Build Counter) and `server/Server.solution` in Projector, choose
   the v140 toolset, and generate.
4. Build the server and the Visual Client (x64 or Win32).
   The pre-build step runs `shared/BuildCounter.exe`, which is checked in.

---

## 3. The technite logic client

The game does **not** contain the technite AI. The game client acts as a
*logic server*, and you write the logic in a separate program that connects to it.

- **Repository:** <https://github.com/SystemSoftware/TechniteLogic>, branch
  **`master`**. It is C# for .NET Framework 4.5 and builds `Client.exe` together with
  `Protocol.dll`, `Logging.dll` and `Math3D.dll`.
- **Compatibility checked:** the channel IDs in `TechniteLogic/Interface.cs` are
  identical to `client/TechniteLogic/techniteProtocol.h`, and both send the
  handshake `"Aquinas v1.7." + ChannelID.Count`. The game client rejects any
  other version string. TechniteLogic's `technite2` branch sends v2.0 and does **not** work.
- **License:** BSD 3-Clause, © 2015 System Software Group, University of Trier.
  You may redistribute it alongside this MIT code if you keep their notice. It is
  not part of this repository.
- `IronFox/TechniteSimulation` is **not** this client. It is an unrelated 2016
  WinForms experiment about consistency between neighbouring sectors.

The logic you write goes in `TechniteLogic.Logic.ProcessTechnites()` (`Logic.cs`).

---

## 4. Running it

1. Start the **server** from `server/`. It reads `server/config/declaration.xml`,
   where `supportedFactions` defaults to `Spectator, Regular, Technite, Hybrid`.
   The technite options are `limitTechniteLife` and `extraTechniteRounds`.
2. Configure the **game client** in `client/game.ini`, section `[technites]`:
   - `fixedServerPort = true` and `fixedServerPortNumber = 30000` (the defaults).
   - For automatic start, build TechniteLogic, copy `Client.exe` and its three DLLs
     into `client/TechniteLogic/`, and set `autoStartClient = true`. The game
     then runs `./TechniteLogic/Client <port>`.
   - Otherwise, leave `autoStartClient = false` and start the logic client yourself
     with the port as its first argument, for example by setting `30000` as the
     command argument in the TechniteLogic Debug settings in Visual Studio.
3. Start the **Visual Client**, connect to the server, and create a **Technite** or
   **Hybrid** faction. Until a logic client connects, the game shows
   *"Technite logic link active, no logic clients connected"*.
4. You can recompile and restart the logic client while the game is running.
