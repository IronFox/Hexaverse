# Hexaverse documentation

| Start here if you want to… | Read |
|---|---|
| get the game building | [building.md](building.md) |
| play locally, host a server, or configure one | [running.md](running.md) |
| understand the game rules | [gameplay.md](gameplay.md) |
| find your way around the code | [architecture.md](architecture.md), then [repository.md](repository.md) |
| understand why clients can't cheat | [server-authority.md](server-authority.md) |
| write research, AI, map or tutorial scripts | [lua-scripting.md](lua-scripting.md) |
| add units, structures, commands or settings | [extending.md](extending.md) |
| use or change the helper tools | [tools.md](tools.md) |
| build, configure or ship the optional bug reporter | [bug-reporter.md](bug-reporter.md) |
| check licenses and bundled components | [third-party.md](third-party.md) |
| see which files were created with AI assistance | [ai-contribution.md](ai-contribution.md) |

Other sources in the repository:

- **Game documentation:** [game/README.md](game/README.md). The in-game player wiki converted to Markdown: controls, every unit, concepts, configuration. The DokuWiki originals stay in `documentation/documentation/` (shown in-game and on the original website). Regenerate the Markdown with `python3 documentation/wiki2md.py` from the repo root.
- **Lua API reference:** [`../lua/General Lua API Documentation.txt`](../lua/General%20Lua%20API%20Documentation.txt).
- **Server variables:** [`../server/config/declaration.xml`](../server/config/declaration.xml). Every configurable variable, with help text.
- **Protocol walkthrough:** [`../protocol documentation.txt`](../protocol%20documentation.txt). Server discovery and connection handshake.
- **Design notes:** [`../concept.txt`](../concept.txt), [`../todo.txt`](../todo.txt), [`../changelog.txt`](../changelog.txt).
