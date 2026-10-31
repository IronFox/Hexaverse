# Tools

Besides the client and the server, the repository contains several helper programs. The C++ ones are built with CMake or Projector (see [building.md](building.md)). The C# ones are plain Visual Studio solutions for .NET Framework 4.x.

| Tool | Language | Folder | Shipped to players | Purpose |
|---|---|---|:-:|---|
| ControlConsole | C++ | `ControlConsole/` | ✓ | Command-line remote control of local servers |
| Server Manager | C# (WinForms) | `ServerManager/` | ✓ | GUI to start, configure, monitor and stop local servers |
| BugReporter | C# (WinForms, .NET 10) | `BugReporter/` | ✗ | Optional crash reporter: collects logs and uploads them to your own server or saves them as a .zip. Not in the default packages |
| Bake | C# (console) | `Bake/` | – | Copies release files by recipe (packaging) |
| BuildCounter | C++ | `BuildCounter/` | – | Increments the client build number before each build |
| IncreaseVersion | C++ | `IncreaseVersion/` | – | Bumps the version number (hand-made `.vcxproj`) |
| DocumentationEditor | C# (WinForms) | `DocumentationEditor/` | – | Editor for the in-game wiki pages |
| Documentation renderer | PHP | `documentation/` | – (website) | Renders the wiki pages as HTML for hexaverse.net |
| MetaServer ("Consensus") | C# | `MetaServer.C#/` | – | Unfinished multi-planet meta-server, plus Probe and LogDisplay helpers |

## ControlConsole

`ControlConsole/main.cpp` uses `clientConsoleShared/` (to start and supervise a local server process) and `globalShared/` (ports). It connects to `localhost:<base+2>`. The server refuses remote connections on that port.

- Interactive shell over the server's configuration tree (`ls`, `cd`, `help`, `var=value`, tab completion).
- `-c "cmd1; cmd2"` for scripts. Output lines start with `=`, `+` or `-`.
- `-s` starts and supervises a server, with heartbeats in both directions. `-p` picks the base port.

The full command reference is in [running.md §6](running.md#6-control-console) and [`control console.txt`](game/config/control-console.md).

## Server Manager

`ServerManager/` (C#) listens for local servers (UDP 10245, heartbeats) and shows them in a table with live attributes. You can:

- start a new server (**Add Server…**) with the global configuration or from a save game,
- edit the global server configuration (double-click **Server Cfg**),
- connect a control console, save the game, open the log, or resume, stop or terminate a server,
- start the client (**Launch Client**).

Servers it starts are *dependent* and run without a console window. Only one Server Manager can run per machine. The `Protocol` sub-project (`ServerManager/Protocol/`, `Protocol.dll`) holds the message definitions it shares with the server. A prebuilt `ServerManager.exe` is committed.

## BugReporter

`BugReporter/` (C#, .NET 10) is an optional crash reporter. It gathers the relevant log files (`LogFile.cs`), shows them to the user, and either uploads them to the HTTPS address set in `BugReporter.json` or saves them as a `.zip`. No address is built in, and it isn't part of the default packages: the game starts it after a fatal error only if `BugReporter/BugReporter.exe` is installed. Configuration, the upload protocol and how to add it to a release are described in [bug-reporter.md](bug-reporter.md).

## Bake

`Bake/Program.cs` takes `Bake.exe <recipe> <oven-folder>`. It clears the output folder, then copies every file that matches the recipe's lines (paths or wildcards, relative to the repo root) and keeps the folder structure.

| Recipe | Produces |
|---|---|
| `mainBakeRecipe.txt` | The full game: client exes, shaders, fonts, objects, resources, server, ControlConsole, ServerManager, Lua, planets, license, info and changelog |
| `serverBakeRecipe.txt` | A dedicated-server package: server, console client (for AI), Lua, planets, declaration, tutorial save, ControlConsole, ServerManager |

`repack.bat` runs both recipes and zips the results into `game.7z` and `server.7z` with `7z.exe`.

## BuildCounter and IncreaseVersion

- `BuildCounter.exe <file>` increments the build number in a header. The `Increment Build Counter` project in `client/Client.solution` has no sources and only runs `..\BuildCounter\BuildCounter.exe buildCounter.h` as a pre-build command, so `client/buildCounter.h` goes up with every client build. A prebuilt `BuildCounter.exe` is committed.
- `IncreaseVersion/` is a small hand-made VS project for bumping `client/version.h`.

## DocumentationEditor and the wiki

The in-game and website documentation is a tree of DokuWiki-syntax `.txt` pages in `documentation/documentation/` (home, controls, terrain, server start, `concepts/`, `config/`, `units/`). Images are in `documentation/images/`. `documentation/wiki2md.py` converts the whole tree to Markdown in [`docs/game/`](game/README.md); run it from the repo root after editing the wiki.

- **`DocumentationEditor/`** (C#) browses that tree and edits pages: new document, new folder, delete, text editor.
- **Unit pages** combine a hand-written `<unit>.txt` with a `<unit>.prefix` that the visual client generates (`client/components/visual/autoDocumentation.cpp`: health, price, ranges …), so the stats always match the current attributes.
- **`documentation/html.php`** renders the pages for the website. It uses `documentation/renderer/` (WikiRenderer 3.1.8, LGPL-2.1, with its DokuWiki-to-XHTML rules) and `documentation/visual/` for the site layout.
- `documentation.odt` in the repo root is an older long-form document.

## MetaServer ("Consensus")

`MetaServer.C#/Consensus.sln` is an experiment from 2015–2016 for a persistent universe of many planets. It models solar systems, planets, factions and regions, and authenticates clients over TLS with certificates. `MetaServer.C#/Runtime/` holds sample config, world files and a committed `certificate.pfx`. It should not be used for anything real. It contains sub-projects for `Protocol`, `Serialization`, `Math3D`, `Logging`, `UnitTests`, a `Probe` test client and `LogDisplay` (`ServerLog.sln`) for viewing logs. The game doesn't use it. It's related to the archived `consensus` and `ft_social` branches (see [repository.md](repository.md)).
