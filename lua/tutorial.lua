--[[ API:

void	Game.Log(msg)					Prints any message to Hexaverse log (client or server-private)
void	Game.DisplayMessage(string msg[,string imageName])		Prints a persistent message on screen. Once the user has pressed 'Got it' this stage will automatically advance to the next task

bool	Game.LowerResolutionPlanet		Set to true if the planet has reduced grid resolution (1/4 of cells, each cell twice as high)
float	Game.UnitScale					Set to a global unit scale corresponding to [Game.LowerResolutionPlanet]. This scale value is to be
										applied too units (hence the name). 1 if the planet is not of lowered resolution, 2 otherwise.


Table	Game.myUnits					Table of all player-owned units currently known to the client
Table	Game.selectedUnit				Table of the currently selected unit. May be nil. Contained by Game.myUnits, if owned by the client faction

Table	Game.myResearch					Table of all researchable topics, both completed, and open, as well as endless
Table	Game.myCurrentResearch			Currently active research topic. May be nil. Otherwise also contained by Game.myResearch
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

--test:



--tasks:
CreateTask(function() Game.DisplayMessage("Welcome to the Hexaverse tutorial.\nThe following tasks will introduce you to the general game mechanics.") end);

--buildings:
CreateTask(function() Game.DescribeTask("Please find and select your Generator.") end, 'onSelectionChanged', function() if Game.selectedUnit ~= nil and Game.selectedUnit.typeName == 'Generator' then TaskDone() end; end);
CreateTask(function() Game.DisplayMessage("Enabled generators supply active buildings with energy. This one will suffice for your Mine and one more consumer. After that you will need more.") end);

CreateTask(function() Game.DescribeTask("Please find and select your Mine.") end, 'onSelectionChanged', function() if Game.selectedUnit ~= nil and Game.selectedUnit.typeName == 'Mine' then TaskDone() end; end);
CreateTask(function() Game.DisplayMessage("Enabled mines collect natural resources from the ground underneath. The colorful stack atop indicates how much they mine per round. This one collects minerals and fuel. The resources never run out but they are also never enough.") end);

--build lab:
CreateTask(function() Game.DescribeTask("Please find and select your Terraformer.") end, 'onSelectionChanged', function() if Game.selectedUnit ~= nil and Game.selectedUnit.typeName == 'Hoverformer' then TaskDone() end; end);
CreateTask(function() Game.DisplayMessage("Terraformers are your primary worker force. While not all terraformers are equally manuverable and resilient, they can all shape the terrain and build new structures. Let's build something!","terraformer.png") end);
CreateTask(function() Game.DescribeTask("Please instruct your Terraformer to build a laboratory. Using the unit commands in the bottom screen section, select Build->Miscellaneous->Laboratory, and click anywhere on the terrain.") end, 
	'onSelectionChanged', 
	function() 
		if Game.selectedUnit ~= nil and Game.selectedUnit.typeName == 'Hoverformer' then 
			for i = 1, #Game.selectedUnit.tasks do
				local t = Game.selectedUnit.tasks[i];
				if t.name == 'construct Laboratory' then
					TaskDone();
					break;
				end;
			end;
		end; 
	end);

CreateTask(function() Game.DisplayMessage("The Terraformer will keep constructing the building each round until done, as long as enough resources are available. The bottom left resource table tells us that we have 12 Minerals more than we currently need. So let's press on.") end);

CreateTask(function() Game.DescribeTask("Please request new rounds, using the command in the bottom left section of the screen or by pressing the Return key, until the Laboratory is done."); end,
	'onRoundStart',
	function()
		for i = 1, #Game.myUnits do
			local u = Game.myUnits[i];
			if u.typeName == 'Laboratory' then
				TaskDone();
				break;
			end;
		end;
	end
);

CreateTask(function() Game.DisplayMessage("Laboratories allow two types of research: Unlocking and Improving. The more Laboratories you have enabled, the faster the research will progress."); end);
CreateTask(function() Game.DescribeTask("Task your laboratory to unlock the ReactorCore building type, by using the commands in the bottom section of the screen."); end,
	'onResearchChange',
	function()
		if Game.myCurrentResearch ~= nil and Game.myCurrentResearch.fullName == 'Unlock:ReactorCore' then
			TaskDone();
		end;
	end
);





CreateTask(function() Game.DescribeTask("Please find and select your Surveyor.") end, 'onSelectionChanged', function() if Game.selectedUnit ~= nil and Game.selectedUnit.typeName == 'Hoversurveyor' then TaskDone() end; end);

CreateTask(function() Game.DisplayMessage("Surveyors automatically identify new natural resources in their yellow scanning sphere. The dark discs on the ground indicate that it has so far found none beyond your starting resources. While Surveyors may be moved manually, it may be less time-consuming to automate them. Let's do that.","surveyor.png") end);
CreateTask(function() Game.DescribeTask("Please automate your Surveyor by selecting it and clicking on the Automate button at the bottom screen section.") end, 'onSelectionChanged', 
		function() 
			if Game.selectedUnit ~= nil and Game.selectedUnit.typeName == 'Hoversurveyor' and Game.selectedUnit.isAutomated then TaskDone() 
		end; 
	end);

CreateTask(function() Game.DisplayMessage("Notice how the surveyor has already chosen a route. This planned motion will be executed once the next round is processed.") end);




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

function OnRoundStart()
	CallEvent('onRoundStart');
end

function OnResearchChange()
	CallEvent('onResearchChange');
end

function OnMessageConfirm()
	NextTask();
end
