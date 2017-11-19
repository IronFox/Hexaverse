--[[ API:

void	Game.Log(msg)					Prints any message to Hexaverse log (client or server-private)
void	Game.DisplayMessage(string msg[,string imageName])		Prints a persistent message on screen. Once the user has pressed 'Got it', OnMessageConfirm() is invoked

void	Game.CallDelayed(float seconds, function func)					Waits the specified number of seconds, then calls the given function without parameters
int		Game.FindHighestAvailableResourceLocation(string resourceName)	Searches among all known resource spots for the highest value where a mine could be constructed. Returns -1 if none are found
int		Game.GetResourceCoverage(string resourceName, int stackID)		Fetches the resource level at the specified location. Returns -1 if the specified location has not been surveyed yet
void	Game.FocusCameraAt(int stackID)									Smoothly focuses the camera on the specified stack index
int		Game.GetCompoundResourceChange(string resourceName)				Fetches the current balance of the specified resource type in the active player's compound. May be negative
int		Game.GetCompoundResourceStorage(string resourceName)			Fetches the current storage level of the specified resource type in the active player's compound. May be 0 but not negative
void	Game.SignalTaskStart(int)									Signals that a new task was started at the given index (>0)


bool	Game.LowerResolutionPlanet		Set to true if the planet has reduced grid resolution (1/4 of cells, each cell twice as high)
float	Game.UnitScale					Set to a global unit scale depending on [Game.LowerResolutionPlanet]. This scale value is to be
										applied to units (hence the name). 1 if the planet is not of lowered resolution, 2 otherwise.


Table	Game.myUnits					Table of all player-owned units currently known to the client
Table	Game.selectedUnit				Table of the currently selected unit. May be nil. Contained by Game.myUnits, if owned by the client faction

Table	Game.myResearch					Table of all researchable topics, both completed, and open, as well as endless
Research entry table:
{
	string	fullName;					Full path to the entry. Unique
	string	name;						Name of this entry. Not unique
	bool	isEndless;					True if the research can run indefinitely
	int		max;						Max research needed to complete research. -1 if endless
	int		progress;					Current research progress. >= 0
	bool	isCompleted;				True if research is completed. Always false if endless
	bool	isActive;					True if this topic is currently being researched. If true, implies isQueued
	bool	isQueued;					True if this topic is or will be researched
};


Table	Game.myCurrentResearch			Currently active research topic. May be nil. Otherwise also contained by Game.myResearch

Cell[]		Game.lowCells				Ordered table of all planar cells in the world, with:
Cell
{
	Vector	center;			Normalized center vector of the cell
	Cell[]	neighbors;		Ordered table of all planar neighbor cells
	int		index;			Linear index of this cell in Game.Cells in [1,#Game.lowCells]
};
Vector
{
	float	x,y,z;
};


]]




currentTask = 0;	--none
tasks = {};


function CreateTask(begin, checkFunc)
	local t = {};
	t.begin = begin;
	if checkFunc ~= nil then
		t.check = checkFunc;
	end;
	table.insert(tasks, t);
end

--test:





--tasks:
CreateTask(function() Game.DisplayMessage("Welcome to Hexaverse, the planetary strategy game.\n"..
		"Excavate caves, build stairs into orbit, or just expand around the globe.\n"..
		"Survive if you can.", 'game.png') end);
CreateTask(function() Game.DisplayMessage(
		"What you are seeing now is the planet you are trying to conquer.\n"..
		"The planetary display attempts to distinguish three primary areas:.\n"..
		"1: Undiscovered terrain.\n"..
		"2: Discovered and currently visible terrain.\n"..
		"3: Discovered but currently out of range.\n"..
		"Everything you are not currently observing (everything not in 2) may change without you knowing. Half the trick of winning Hexaverse is knowing what you don't know.\n"
		,'areas.png') end);


CreateTask(function() Game.DisplayMessage("Let's get you acquainted with the camera controls.") end);

CreateTask(function() Game.DescribeTask("Zoom the camera by using the mouse-wheel or the up/down arrow-keys."); end, function() if cameraZoomed then TaskDone() end; end);
CreateTask(function() Game.DescribeTask("Drag the camera by holding the right or middle mouse button and moving the mouse."); end, function() if cameraDragged then TaskDone() end; end);
CreateTask(function() Game.DescribeTask("Rotate the camera around your current focus by pressing the ASDW keys."); end, function() if cameraRotated then TaskDone() end; end);

CreateTask(function() Game.DisplayMessage("Now that you have some understanding of camera control, let's explore your units.") end);

--buildings:
CreateTask(function() Game.DescribeTask("Please find and select your Generator by left-clicking on it. The name of the building you currently selected is shown in the top-right section of the screen.") end, function() if Game.selectedUnit ~= nil and Game.selectedUnit.typeName == 'Generator' then TaskDone() end; end);
CreateTask(function() Game.DisplayMessage("Enabled generators supply active buildings with %Energy%energy while consuming %Fuel%fuel. This generator is enough to support your Mine and one more consumer. After that you will need more.","generator.jpg") end);

CreateTask(function() Game.DescribeTask("Please find and select your Mine.") end, function() if Game.selectedUnit ~= nil and Game.selectedUnit.typeName == 'Mine' then TaskDone() end; end);
CreateTask(function() Game.DisplayMessage("Enabled mines collect natural resources from the planet core underneath. The colorful stack atop indicates how much they mine per round. This one collects %Minerals%minerals and %Fuel%fuel. The resources never run out but they are also rarely enough.","mine.png") end);

--build lab:
CreateTask(function() Game.DescribeTask("Please find and select your Terraformer.") end, function() if Game.selectedUnit ~= nil and Game.selectedUnit.typeName == 'Hoverformer' then TaskDone() end; end);
CreateTask(function() Game.DisplayMessage("Terraformers are your primary working force. While not all terraformers are equally manuverable, resilient, or productive, they can all shape the terrain and build new structures.\n Now, let's build something!","terraformer.png") end);
CreateTask(function() Game.DescribeTask("Please instruct your Terraformer to build a laboratory. Using the unit commands in the bottom screen section, select Build->Miscellaneous->Laboratory, and click anywhere on the terrain.") end, 
	function() 
		local done = false;
		if Game.selectedUnit ~= nil and Game.selectedUnit.typeName == 'Hoverformer' then 
			for i = 1, #Game.selectedUnit.tasks do
				local t = Game.selectedUnit.tasks[i];
				if t.name == 'construct Laboratory' then
					TaskDone();
					done = true;
					break;
				end;
			end;
		end;
		--also done when it already exists
		if not done then
			for i = 1, #Game.myUnits do
				local u = Game.myUnits[i];
				if u.typeName == 'Laboratory' then
					TaskDone();
					break;
				end;
			end;
		end;
	end);

CreateTask(function() Game.DisplayMessage("The Terraformer will keep constructing the laboratory until done, as long as enough %Minerals%resources are available. The bottom left resource table tells us that we have sufficient %Minerals%minerals available: "..Game.GetCompoundResourceChange('Minerals') .." per round, "..Game.GetCompoundResourceStorage('Minerals').." in storage. So let's press on.") end);

CreateTask(function() Game.DescribeTask("Until the Laboratory is done please request new rounds by using the respective 'End Round' command in the bottom left section of the screen, or by pressing the Enter/Return key."); end,
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
CreateTask(function() Game.DescribeTask("Task your laboratory to unlock the ReactorCore building type by selecting it and using the commands in the bottom section of the screen."); end,
	function()
		local res = Game.myResearch['Unlock:ReactorCore'];
		--FindResearch('Unlock:ReactorCore');
		if res ~= nil and (res.isQueued or res.isCompleted) then
			TaskDone();
		else
			
		end;
	end
);

CreateTask(function() Game.DescribeTask("While we wait for the laboratory to finish, please find and select your Surveyor.") end, function() if Game.selectedUnit ~= nil and Game.selectedUnit.typeName == 'Hoversurveyor' then TaskDone() end; end);

CreateTask(function() Game.DisplayMessage("Surveyors automatically identify new natural resources in their yellow scanning sphere. The dark discs near your starting location indicate that there isn't anything there (except beneath your starting mine). Instruct your surveyor to move by right-clicking anywhere without discs.","surveyor.png") end);
CreateTask(function() Game.DescribeTask("Please move your surveyor until it found some more %Fuel%fuel (a green bar atop the terrain). Request new rounds to execute movement operations") end, 
		function() 
			local at = Game.FindHighestAvailableResourceLocation('Fuel');
			if (at ~= -1) then
				--Game.FocusCameraAt(at);
				TaskDone() 
			end;
		end
		);

CreateTask(function() Game.DisplayMessage("In some situations it may be smarter to survey the terrain manually, but for now let's instruct the unit to do so automatically.") end);
CreateTask(function() Game.DescribeTask("Please automate your Surveyor by selecting it and clicking on the Automate button at the bottom screen section.") end, 
		function() 
			if Game.selectedUnit ~= nil and Game.selectedUnit.typeName == 'Hoversurveyor' and Game.selectedUnit.isAutomated then 
				TaskDone() 
			end; 
		end
		);

CreateTask(function() Game.DescribeTask("Please construct a mine atop the newly found fuel resource using your terraformer. In the bottom screen section, select Build->Resources->Mine, and click on the terrain cell with the green bar. Request new rounds until done.") end,
	function()
		local count = 0;
		for i = 1, #Game.myUnits do
			local u = Game.myUnits[i];
			if u.typeName == 'Mine' and Game.GetResourceCoverage('Fuel', u.location.stackID) > 0 then
				count = count + 1;
			end;
		end;
		if (count >= 2) then
			TaskDone();
		end;
	end
);

CreateTask(function() Game.DisplayMessage("The list in the bottom left section of the screen shows your current resource output. With two mines and one laboratory we now require more %Energy%energy than a single generator can supply.") end);

CreateTask(function() Game.DescribeTask("Please construct a second generator using your terraformer. In the bottom screen section, select Build->Resources->Generator, and click anywhere on the terrain. Request new rounds until done.") end,
	function()
		local count = 0;
		for i = 1, #Game.myUnits do
			local u = Game.myUnits[i];
			if u.typeName == 'Generator' then
				count = count + 1;
			end;
		end;
		if (count >= 2) then
			TaskDone();
		end;
	end
);

CreateTask(function() Game.DisplayMessage("Both the new generator and mine are currently switched off, since they are not needed. Typically when requiring more %Energy%energy, available generators will be switched on automatically, which, in turn, may enable mines.") end);

CreateTask(
	function() 
		for i = 1, #Game.myUnits do
			local u = Game.myUnits[i];
			if (u.typeName == 'Laboratory') and not u.isEnabled then
				u.Enable(true);
			end;
		end;
		Game.DescribeTask("Please enable either your new mine or your new generator by selecting either, and pressing the Enable button in the bottom screen section.") 
	end,
	function()
		local enabled = 0;
		for i = 1, #Game.myUnits do
			local u = Game.myUnits[i];
			if (u.typeName == 'Generator' or u.typeName == 'Mine') and u.isEnabled then
				enabled = enabled + 1;
			end;
		end;
		if (enabled >= 4) then
			TaskDone();
		end;
	end
);


CreateTask(function() Game.DisplayMessage("Notice how enabling either the generator or mine also enabled the other because your existing base did not supply enough resources. The two buildings now supply each other. If either is destroyed, the other may be forced to switch off.") end);

CreateTask(function() Game.DescribeTask("Please construct a Vehicle Plant using your terraformer. In the bottom screen section, select Build->Factories->Vehicle Plant, and click anywhere on the terrain. Request new rounds until done.") end,
	function()
		for i = 1, #Game.myUnits do
			local u = Game.myUnits[i];
			if u.typeName == 'Vehicle Plant' then
				TaskDone();
				break;
			end;
		end;
	end
);

CreateTask(function() Game.DisplayMessage("Vehicle Plants can construct all sorts of vehicles. Let's start with another terraformer.") end);

CreateTask(function() Game.DescribeTask("Please task your Vehicle Plant to construct another hoverformer by selecting the plant and then choosing the Terraformer turret in the top row of its unit commands.") end,
	function()
		for i = 1, #Game.myUnits do
			local u = Game.myUnits[i];
			if u.typeName == 'Vehicle Plant' then
				local found = false;
				
				for j = 1, #u.productionQueue do
					if u.productionQueue[j] == "Hoverformer" then
						found = true;
						break;
					end;
				end;
				
				if (found) then
					TaskDone();
					break;
				end;
			end;
		end;
	end
);

CreateTask(function() Game.DisplayMessage("As long as it's enabled and has access to the required %Minerals%minerals, the vehicle plant will continue producing the items in its queue, left to right. You can abort any job by clicking on it in the list. To automate deployment you may also want to place a gathering point by selecting the vehicle plant in question and right-clicking at the desired destination.") end);

CreateTask(function() Game.DescribeTask("Please wait for your Reactor Core research to complete."); end,
	function()
		local res = Game.myResearch['Unlock:ReactorCore'];
		--FindResearch('Unlock:ReactorCore');
		if res ~= nil and (res.isCompleted) then
			TaskDone();
		else
			
		end;
	end
);


CreateTask(function() Game.DisplayMessage("Reactor Cores cause neighboring Generators to produce twice as much %Energy%energy each round. They are a very useful thing to have. The catch is that you need %Uranium%uranium.") end);

		

CreateTask(function() Game.DescribeTask("Please find some %Uranium%uranium, and construct a mine atop using your terraformer. In the bottom screen section, select Build->Resources->Mine, and click on the terrain cell with the yellow bar. Request new rounds until done.") end,
	function()
		local count = 0;
		for i = 1, #Game.myUnits do
			local u = Game.myUnits[i];
			if u.typeName == 'Mine' and Game.GetResourceCoverage('Uranium', u.location.stackID) > 0 then
				count = count + 1;
			end;
		end;
		if (count >= 1) then
			TaskDone();
		end;
	end
);




CreateTask(function() Game.DescribeTask("Now, please construct a Reactor Core next to one of your Generators. It must be on the same level") end,
	function()
		
		for i = 1, #Game.myUnits do
			local u = Game.myUnits[i];
			if u.typeName == 'Reactor Core' then
				Game.Log("Found reactor");
				local cell = Game.lowCells[u.location.stackID];
				local n = cell.neighbors;
				for j = 1, #n do
					Game.Log("Looking at "..n[j].index..", "..u.location.height);
					local neighbor = FindUnitIn(n[j].index,u.location.height);
					if (neighbor ~= nil and neighbor.typeName == 'Generator') then
						TaskDone();
						return;
					end;
				end;
			end;
		end;
	end
);



CreateTask(function() Game.DisplayMessage("Reactor Cores are a great way to boost the energy output of your compound. They do tend to concentrate your power production, however, which may pose a rewarding target for hostile forces.","Reactor Core.jpg") end);

CreateTask(function() Game.DisplayMessage("This concludes the interactive tutorial.\n"..
"With the basics covered, you will undoubtedly quickly master all other features as well.\n"..
"The best of luck for your future conquests!\n"..
"You may leave and close the server any time.") end);









--helper functions

function Current()	--table
	if type(tasks[currentTask]) ~= nil then
		return tasks[currentTask];
	end
	return {};
end

taskDone = false;
cameraDragged = false;
cameraRotated = false;
cameraZoomed = false;

function SetTask(task--[[int]])
	currentTask = task;
	local t = Current();
	if t ~= nil and type(t.begin)=='function' then
		t.begin();
	end
	taskDone = false;
	cameraRotated = false;
	cameraDragged = false;
	cameraZoomed = false;
	Game.SignalTaskStart(task);
end;

function NextTask()
	SetTask(currentTask+1);
end


function TaskDone()
	if taskDone then
		return;
	end;
	taskDone = true;
	Game.DescribeTask("Well done!");
	Game.CallDelayed(2,NextTask);
end



function FindUnitIn(stackID--[[int]], height--[[int]])
	for i = 1, #Game.myUnits do
		local u = Game.myUnits[i];
		if (u.location.stackID == stackID and u.location.height == height) then
			return u;
		end;
	end;
	return nil;
end;



--triggers called by game

function CheckTaskConditions()
	if (taskDone) then
		return;
	end;
	local t = Current();
	if t ~= nil and type(t.check) == 'function' then
		t.check();
	end
end


function OnBegin(atTask--[[int, optional]])
	if (atTask == nil) then
		atTask = 1;
	end;
	Game.Log("(Re)start at task #"..tostring(atTask));
	SetTask(atTask);
end


function OnMessageConfirm()
	NextTask();
end

function OnCameraDrag()
	cameraDragged = true;
	CheckTaskConditions();
end

function OnCameraRotate()
	cameraRotated = true;
	CheckTaskConditions();
end

function OnCameraZoom()
	cameraZoomed = true;
	CheckTaskConditions();
end
