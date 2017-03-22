--[[ API:

void	Game.Log(msg)					Prints any message to Hexaverse log (client or server-private)
void	Game.DisplayMessage(string msg[,string imageName])		Prints a persistent message on screen. Once the user has pressed 'Got it' this stage will automatically advance to the next task

bool	Game.LowerResolutionPlanet		Set to true if the planet has reduced grid resolution (1/4 of cells, each cell twice as high)
float	Game.UnitScale					Set to a global unit scale corresponding to [Game.LowerResolutionPlanet]. This scale value is to be
										applied too units (hence the name). 1 if the planet is not of lowered resolution, 2 otherwise.


Table	Game.myUnits					Table of all player-owned units currently known to the client
Table	Game.selectedUnit				Table of the currently selected unit. May be nil. Contained by Game.myUnits, if owned by the client faction

										
]]




currentTask = 0;	--none
tasks = {};


function CreateTask(begin, eventName, eventFunc)
	local t = {};
	t.begin = begin;
	if eventName ~= nil then
		t[eventName] = eventFunc;
	end;
	table.insert(tasks, t);
end


CreateTask(function() Game.DisplayMessage("Welcome to the Hexaverse tutorial.\nThe following tasks will introduce you to the general game mechanics.") end);

CreateTask(function() Game.DescribeTask("Please find and select your Generator.") end, 'onSelectionChanged', function() if Game.selectedUnit ~= nil and Game.selectedUnit.typeName == 'Generator' then TaskDone() end; end);
CreateTask(function() Game.DisplayMessage("Enabled generators supply active buildings with energy. This one will suffice for your Mine and one more consumer. After that you will need more.") end);

CreateTask(function() Game.DescribeTask("Please find and select your Mine.") end, 'onSelectionChanged', function() if Game.selectedUnit ~= nil and Game.selectedUnit.typeName == 'Mine' then TaskDone() end; end);
CreateTask(function() Game.DisplayMessage("Enabled mines collect natural resources from the ground underneath. This one collects minerals and fuel. The resources never run out but they are also never enough.") end);

--CreateTask(function() Game.DisplayMessage("Natural resources are the lifeblood of your faction. You have already found some. Now it is time to find more.","resources.png") end);
CreateTask(function() Game.DescribeTask("Please find and select your Surveyor.") end, 'onSelectionChanged', function() if Game.selectedUnit ~= nil and Game.selectedUnit.typeName == 'Hoversurveyor' then TaskDone() end; end);

CreateTask(function() Game.DisplayMessage("Surveyors identify natural resources you have not found so far. The dark discs on the ground indicate that it has so far found no other. While Surveyors may be moved manually, it may be less time-consuming to automate them.","surveyor.png") end);
CreateTask(function() Game.DescribeTask("Please automate your Surveyor by selecting it and clicking on the Automate button at the bottom screen section.") end, 'onSelectionChanged', 
	function() 
		if Game.selectedUnit ~= nil and Game.selectedUnit.typeName == 'Hoversurveyor' and Game.selectedUnit.isAutomated then TaskDone() 
	end; 
end);

CreateTask(function() Game.DisplayMessage("Notice how the surveyor has already chosen a route. This planned motion will be executed once the next round is processed.") end);

CreateTask(function() Game.DescribeTask("Please find and select your Terraformer.") end, 'onSelectionChanged', function() if Game.selectedUnit ~= nil and Game.selectedUnit.typeName == 'Hoverformer' then TaskDone() end; end);
CreateTask(function() Game.DisplayMessage("Terraformers are your primary worker force. While not all terraformers are equally manuverable and resilient, they can all shape the terrain and build new structures.","terraformer.png") end);



function Current()	--table
	if type(tasks[currentTask]) ~= nil then
		return tasks[currentTask];
	end
	return {};
end

function CallEvent(task--[[string]])
	local t = Current();
	if t ~= nil and type(t[task]) == 'function' then
		t[task]();
	end
end

function NextTask()
	currentTask = currentTask+1;
	local t = Current();
	if t ~= nil and type(t.begin)=='function' then
		t.begin();
	end
end


function TaskDone()
	Game.DescribeTask("Well done!");
	Game.CallDelayed(2,NextTask);
end

function OnBegin()
	Game.Log("Begin");
	
	NextTask();
end

function OnSelectionChanged()
	CallEvent('onSelectionChanged');
end

function OnMessageConfirm()
	NextTask();
end
