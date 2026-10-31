<!-- Generated from documentation/documentation/config/server.txt (DokuWiki syntax). -->

# Server Configuration

The server can be configured in multiple ways.

1. Edit *[game folder](game-folder.md)\\server\\config\\server.ini*. This file must be configured before starting any local server. Once started, the file's contents have no impact on any running server instance. This file does not exist until the server (or other Hexaverse applications) have run for the first time.
1. Use the provided [control console](control-console.md) to manipulate a running server.
1. Start a server via the client [interactive visual configurator](interactive-configuration-mode.md).
1. Use the [server manager](server-manager.md).

## Planet Types

The server configuration loads planets as predefined templates. While their actual layout is mostly defined by fractal noise, the underlying rules are defined in separate configuration files located in `[install]\server\config\myPlanets\[planet name].ini` and
*[user](game-folder.md)\\server\\config\\myPlanets\\[planet name].ini*.
Configuration files located in the [user folder](game-folder.md) directory are prefixed with a * character.

New planet types can be created by writing new .ini files, copying and modifying existing files from the install directory, or interactively via the [interactive visual configurator](interactive-configuration-mode.md).
Once defined, use the server.ini configuration file, [control console](control-console.md), [interactive configuration mode](interactive-configuration-mode.md), or [server manager](server-manager.md) to load it by name.

The currently available planets may be listed using the [control console](control-console.md) command `/World/showPlanetTypes`.

## Research Script

[Laboratory research](../concepts/research.md) is controlled by a [Lua script](https://www.lua.org/) located in the installation folder `[install]\lua\attributesAndResearch.lua`.

However, if a Lua file exists in *[user](game-folder.md)\\server\\config\\lua\\attributesAndResearch.lua*, then this will override the factory default script.

The script is loaded at [server start](../server-start.md), but can be updated during runtime via the [control console](control-console.md) command `/IO/reloadResearchProgram`.
Clients will receive notifications of the generated attributes, topics, and states as needed.
The script is no longer transferred to the client.

The file defines which research is possible, and how this affects unit availability and attributes.
It also allows basic **modification of all unit attributes.**

---

[Game documentation index](../README.md)
