# Contributing to Hexaverse

Hexaverse is released under the [MIT License](LICENSE). You may fork it, modify it, redistribute it and sell it, as long as you keep the copyright and license notice.

## The official Steam release

IronFox keeps Hexaverse's Steam app registration and publishes the official Steam build as a **free game**. That build comes from this repository:

```
master  ->  steam-public  ->  (private) steam  ->  Steam store
```

- `master` holds the game.
- `steam-public` is `master` plus the Steam integration, without the Steamworks SDK, which can't be redistributed.
- A private branch adds only the SDK and the store configuration, then builds and uploads.

So **every change that should reach the Steam release has to be merged here first, through a pull request.**

## Pull requests

- **Game changes** (gameplay, client, server, tools, docs): target `master`.
- **Steam integration changes** (`clientServerShared/network/steamInterface.*`, `net.*`, Steam lobby, auth or upload code): target `steam-public`. Changes that also apply without Steam belong on `master`.
- Build on Windows with Visual Studio, as described in the [README](README.md#building). CI builds `master` after a merge, but not pull requests, so build and test your change locally first.
- Never commit the Steamworks SDK, Steam DLLs, `steam_appid.txt`, credentials or app IDs. `.gitignore` covers the SDK paths.
- Keep pull requests focused, and describe what you changed and how you tested it.
