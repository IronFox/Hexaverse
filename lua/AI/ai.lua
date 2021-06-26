--[[ API:

Check General Lua API Documentation.txt for general purpose functions and tables

]]

--#require "vehicles.lua"
--#require "factories.lua"


function ExportState()
	return nil;
end;

function Init(state)
end;


function	Execute()
	Game.Log('Executing');
	
	ProcessFactories();
	ProcessVehicles();
end;

function OnMessage(sender--[[faction]], message--[[string]])
	if message == 'AI: Respond' then
		Game.SendMessage(sender,'Here');
	end;
end;
