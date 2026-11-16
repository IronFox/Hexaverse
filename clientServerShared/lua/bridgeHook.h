#pragma once
#include <functional>
#include <string>

/*
In-memory route for Game.BridgeWrite / Game.BridgeRead (see luaGameAPI.cpp).
Set by a client that offers the bridge over another channel (the MCP server, client/components/ai/mcpServer.cpp).
When set, bridge files go through these functions; if HEXAVERSE_AI_BRIDGE names a folder as well, files are also
written there (as a record of the match), but reads come from the hook only.
*/
namespace BridgeHook
{
	extern std::function<bool(const std::string&name, const std::string&text)>	write;
	extern std::function<std::string(const std::string&name)>					read;
}
