#pragma once
#include <image/image.h>

/*
MCP interface for playing: a Model Context Protocol server inside the client.

Off unless the client is started with mcpPort=<n> (an AI player client launched by the server may also be given the
port through the environment variable HEXAVERSE_AI_MCP_PORT, since the server builds its command line).
Listens on 127.0.0.1 only, serves one connection at a time, MCP Streamable HTTP (JSON-RPC 2.0 over POST /mcp,
answered as application/json).

Tools: get_help, get_state, wait_for_turn, orders, end_turn (the bridge of lua/AI/bridge.lua, in memory instead of
files: see clientServerShared/lua/bridgeHook.h), screenshot and look_at (Visual Client only).
Orders run inside the AI script's Lua sandbox exactly like an orders file; the server gives no file or process access.
*/
namespace Mcp
{
	/** Starts listening on 127.0.0.1:port and installs the bridge hook. Call once, after logging has begun */
	void	Start(unsigned port);
	bool	IsOn();

	/** Visual Client: declares that frames can be captured (otherwise screenshot answers that there is no window) */
	void	SetHasWindow();
	/** Visual Client, main thread, once per frame: applies pending camera requests (look_at) */
	void	MainThreadFrame();
	/** Visual Client, render thread: true if a screenshot has been asked for and not yet delivered */
	bool	ScreenshotWanted();
	/** Visual Client, render thread: hands over the captured frame */
	void	DeliverScreenshot(const DeltaWorks::Image&frame);
}
