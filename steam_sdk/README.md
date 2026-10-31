# Steamworks SDK (not included)

The Steam build links against the Steamworks SDK, which Valve does not allow
to be redistributed. It was removed from this branch, so the Steam build does
not compile out of the box.

To build it, download the Steamworks SDK from
https://partner.steamgames.com/downloads/list (requires a Steamworks account)
and copy it here:

| From the SDK | To |
|---|---|
| `public/steam/*` | `steam_sdk/steam/` |
| `redistributable_bin/steam_api.lib`, `public/steam/lib/win32/sdkencryptedappticket.lib` | `steam_sdk/lib/win32/` |
| `redistributable_bin/win64/steam_api64.lib`, `public/steam/lib/win64/sdkencryptedappticket64.lib` | `steam_sdk/lib/win64/` |
| `steam_api.dll`, `steam_api64.dll`, `sdkencryptedappticket.dll`, `sdkencryptedappticket64.dll` | `client/` and `server/` |
| `tools/ContentBuilder/` | `steam_sdk/ContentBuilder/` (only needed for `repackAndUpload.bat`) |

The original code was written against a mid-2017 SDK (`SteamUser019`,
roughly SDK 1.41). Newer SDKs rename some interfaces and libraries
(for example `steam_api64.lib` instead of `steam_api.lib` in `lib/win64`),
so the project files and `steamInterface.cpp` may need small adjustments.

The ContentBuilder scripts for app 468350 (client) and 815050 (dedicated
server) were also removed with the SDK folder.
