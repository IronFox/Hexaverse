<!-- Generated from documentation/documentation/server start.txt (DokuWiki syntax). -->

# Server Start

Hexaverse requires servers to operate.
While certain modes may appear as though no server was involved, as soon as units move, a server is always running in the background.

These servers are generally referred to as *dedicated servers*, even if they are running under the supervision of a client.

There are several ways to start a server:

1. Directly run the shipped server executable in *`[install dir]\server\server[ x86].exe`*. Execute with a `--help` runtime parameter to review other options (should be executed from a command line interpretor, e.g. cmd.exe).
1. Use the [server manager](config/server-manager.md).
1. Start a [control console](config/control-console.md) with the `--server` runtime parameter.
1. Use the 'Start local server...' on the server selection screen of a running visual client. The button provides three options:
   1. Start using the current global [server configuration](config/server.md).
   1. [Visually configure](config/interactive-configuration-mode.md) the server.
   1. Load a previously created [save game](config/save-games.md).

**Note that servers started via visual client will not survive closing the client**.

Upon start up, it will load the current [server configuration](config/server.md) and initialize accordingly.

## UDP Broadcasts

Servers advertise their presence via UDP broadcasts which are received by any client running on any LAN shared by the server host (the server broadcasts on all found network devices).
Clients attempt to listen for these broadcasts, and display found servers on the server selection screen.
Only one client can listen per system.
If multiple clients are used concurrently on the same system, then server addresses must be added manually.

## Save Games

The server manages all [save games](config/save-games.md).
If you are not hosting the server, then you cannot create or load save games.

## Client Authentication

Upon faction creation, clients receive a 256 bit access token that identifies their faction.
Clients maintain these in a persistent [world ID]->[faction key] map, stored in *[game folder](config/game-folder.md)/client/worldAccess.m32*.
During faction login, this access token is hashed together with a 256 bit, connection-unique salt, and matched by the server.
Access tokens are perserved in [save games](config/save-games.md).

## Logs

Regardless of how a server is started, it always creates a log file in *[game folder](config/game-folder.md)\\server\\logs\\*.
In case of crashes, check these logs.
The file used by a specific server can be queried using the `/IO/logFile` [control console](config/control-console.md) variable, or directly opened by the [server manager](config/server-manager.md).

## NAT (Network Address Translation, i.e. Routers)

The server requires two TCP ports to operate which must be accessible to anyone attempting to connect via TCP.
By default these are `10247` and `10248`.
If you are located behind a NAT router, and you wish others to connect to you via TCP, then these ports must be forwarded to your physical server.

## Runtime Parameters

For the more proficient user, a number of runtime parameters allow customization of specific server/control console settings.
Execute the respective program with the *`--help`* for a list of all switches.

If you wish to run multiple servers on the same system, then an adjustment of base ports is required.
The current game base port is 10247. Starting from there, two more TCP ports are used (effectively: `10247, 10248, and 10249`).
The third port is used for localhost-only control console communication and need not be NATed, but must be accounted for.
Consecutive servers should thus allocate base ports (via the `-p [base port]` parameter) in steps of 3.

External clients trying to connect must specify non-default base ports as additional address parameter (`yourhost:baseport`), e.g. `localhost:20000`, if 20000 is your non-standard server base port.
LAN computers will automatically receive these ports via the UDP discovery function.

If the server was started first, then use the `-p [base port]` runtime parameter of a [control console](config/control-console.md) to connect to these servers.
Otherwise, using this option in combination with `-s` will start the server on that port and establish a connection to it.
E.g. *[ControlConsole.exe](config/control-console.md) -p 20000 -s*

---

[Game documentation index](README.md)
