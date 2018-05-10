--[[ API:

void	Game.CallDelayed(float seconds, function func)					Waits the specified number of seconds, then calls the given function without parameters
void	Game.ClearAllDelayed()											Drops all non-executed delayed function calls
int		Game.FindHighestAvailableResourceLocation(string resourceName)	Searches among all known resource spots for the highest value where a mine could be constructed. Returns -1 if none are found
int		Game.GetResourceCoverage(string resourceName, int stackID)		Fetches the resource level at the specified location. Returns -1 if the specified location has not been surveyed yet, of the specified index is not in [1,#Game.lowCells]
void	Game.FocusCameraAt(int stackID)									Smoothly focuses the camera on the specified stack index
int		Game.GetCompoundResourceChange(string resourceName)				Fetches the current balance of the specified resource type in the active player's compound. May be negative
int		Game.GetCompoundResourceStorage(string resourceName)			Fetches the current storage level of the specified resource type in the active player's compound. May be 0 but not negative
void	Game.SignalTaskStart(int)									Signals that a new task was started at the given index (>0)


void	Game.Log(msg)					Prints any message to Hexaverse log (client or server-private)
void	Game.DisplayMessage(string msg[,string imageName])		Prints a persistent message on screen. Once the user has pressed 'Got it', OnMessageConfirm() is invoked

[client only]
	int		Game.GetResourceCoverage(string resourceName, Cell cell)		Fetches the resource level at the specified location. Returns -1 if the specified location has not been surveyed yet.
	int		Game.FindHighestAvailableResourceLocation(string resourceName)	Searches among all known resource spots for the highest value where a mine could be constructed. Returns -1 if none are found
	void	Game.FocusCameraAt(Cell cell)									Smoothly focuses the camera on the specified stack index
	int		Game.GetCompoundResourceChange(string resourceName)				Fetches the current balance of the specified resource type in the active player's compound. May be negative
	int		Game.GetCompoundResourceStorage(string resourceName)			Fetches the current storage level of the specified resource type in the active player's compound. May be 0 but not negative
float	Game.GetDistance(Loc a, Loc b)				Retrieves the geometric distance between the two specified locations



[client only]
	Unit[]	Game.myUnits				Table of all player-owned units currently known to the client
	Unit	Game.selectedUnit			Table of the currently selected unit. May be nil. Contained by Game.myUnits, if owned by the client faction

Unit[]		Game.units					Table of all (player-known on client) units
Faction[]	Game.factions				List of all (player-known on client) factions
Alliance[]	Game.alliances				List of all (player-known on client) alliances
	
	
Alliance
{
	string	id;
	Faction[]	members;
}
	
Faction
{
	string	name;
	string	id;
	Alliance	alliance;
	Unit[]	units;
}
	
Unit
{
	string	name;						Full entity name (for logging purposes). Includes type and unique ID
	string 	niceName;					Full entity name (as presented to the player). Includes type and nice representation of unique ID
	string	id;							Entity unique ID
	string	typeName;					String representation of the type. May be a compound type if isMovile is true
	Faction	owner;						Entity owner
	int		constructionCost;			Amount of resources necessary to build this type
	bool	isMobile;					True if this is a mobile entity
	bool	isStructure;				True if this is a building (isMobile and isStructure are mutually exclusive. one is always true)
	Loc		location;					Entity location. Only one location table instance is used per possible grid location. Therefor, they can be compared using == and ~=
	bool	isSleeping;					True if the local entity is currently sleeping
	int		health;						Current entity health. <= 0 for ghosts
	int		maxHealth;					Maximum possible health of the local entity. May change over time
	int		empDamageLevel;				Level of suffered EMP damage. 0 indicates no damage, higher levels prevent construction, reduce firing, scanning, and motion range
	bool	isWorkingStructure;			True if this entity is a structure, and actively performing actions (e.g. building, mining, etc)
	bool	isMobileEntityProducer;		True if this entity can produce mobile entities. Does not necessarily imply that this is a building
	func	Destroy();					Attempts to destroy the local entity. No effect if not owned by the local player, or outside command phase. Change will not affect local record until next invocation
	func	ScheduleAttacks(Loc[]);		Schedules one or more attacks
	func	DirectTo(Reachable);		Schedules a motion to the specified location along the path defined by the Reachable
	-- optional stuff:
	
	[Client only]
		bool	isOwnedByMe;			true if the local player owns this entity
		
	[isStructure]
		bool	isEnabled;				True if the local structure is active
		function Enable(bool);			Change enable state of the local structure. No effect if not owned by the local player, or outside command phase. Change will not affect isEnabled until next invocation
		
	[isMobileEntityProducer]
		Table	productionQueue;		String table of all production jobs currently enqueued by the local entity. Lists the respective entity type enqueued in order of production

	[has turret (tower, mobile)]
		Turret	turret;					Table containing turret information. May be set for structures or mobile entities
	
	[isMobile]
		bool	isAutomated;			True if the local entity has some behavior logic attached. Currently used by surveyor only
		Unit	storedIn;				Set if this unit is stored in some other unit. May be nil
		Task[]	tasks;					Task queue, in order of execution (first will be executed next, or is already being executed if taking more than one round)
		func	ClearTasks();			Flushes all tasks. No effect if not owned by the local player, or outside command phase. Change will not affect local record until next invocation
		int		movementSpeed;			Theoretical upper movement steps per round, including possible turret overweight and EMP damage. The effective speed depends on the encountered terrain
		bool	motionIgnoresAttack;	True if the local unit can move regardless of whether it has attacked during this round. False if attacks in some way do negatively affect subsequent motions
}

Loc
{
	int		height;						Layer difference from mean planetary height. Usually in the range [-16,15], though some units may be located above the regular grid
	Cell	cell;						Horizontal cell
	Vector	world;						World location
}


Turret
{
	string	typeName;					Turret type name
	int		weight;						Weight affecting mobile entity speed
	bool	canAttack;					True if offensive, false otherwise
	bool	detectsResources;			True if this turret can detect resources and look vertically into the ground
	[canAttack]
		string	trajectoryType;			String representation of the trajectory type. May be Artillery, Missile, VerticalBeam
		string	ammoType;				String representation of the used ammo. Currently supported: Fuel, Minerals, Energy, Uranium
		bool	independentAttackAndMotion;	Indicates that a vehicle using this turret can fire and move at 100% each in the same round, with neither action slowing the other
		int		ammoDemandPerShot;		Resource demand (ammoType) per shot
		int		ammoAvailability;		Resources available to the local entity of the demanded ammo type (may be shared)
		int		possibleAttacks;		Counts the number of attacks possible in the local unit's next attack phase
		float	attackActionLeft;		Percentage (0.0-1.0) of attack action left in the next attack phase
		float	attackInitiative;		Effective attack initiative applied during the next attack phase (if attacking). Automatically reduced by attackActionLeft. The higher, the better
		int		attacksPerRound;		Theoretical maximum number of attacks per round
		float	flatAttackRange;		Horizontal attack range. Increased by explosionRadius. Vertical range depends on trajectoryType
		float	explosionRadius;		Spherical explosion radius
		int		damageLevel;			Damage inflicted by the local turret
		bool	dealsEMPDamage;			True if the local turret deals EMP damage as opposed to permanent health damage
	bool	can[Operation];				True if the respective operation can be performed by this turret, where [Operation] may be: Construct, Nuclear (boom), Repair, TerrainAdd, TerrainRemove (e.g. canConstruct)
	int		speedOf[Operation];			Speed of the local turret at performing the specified operation, where [Operation] may be: Construct, Nuclear (boom), Repair, TerrainAdd, TerrainRemove (e.g. speedOfConstruct)
}

Task
{
	string	name;						Task name
	bool	isJob;						True if this is a construction job (e.g. by a terraformer), as opposed to motion or attack
	Loc[]	foci;						List of location tables, representing each one stage in the task (movement waypoints, construction destinations, and so forth, most tasks only have one focus point)
}

[client only]
	Research[]	Game.myResearch			Table of all researchable topics, both completed, and open, as well as endless
	Research	Game.myCurrentResearch	Currently active research topic. May be nil. Otherwise also contained by Game.myResearch
Research
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


bool		Game.Grid.lowerResolutionPlanet		Set to true if the planet has reduced grid resolution (1/4 of cells)

Loc			Game.Grid.ToLocation(Cell cell, int height)
int			Game.Grid.bottomHeight;		Minimum grid height
int			Game.Grid.topHeight;		Maximum grid height


Cell[]		Game.Grid.unitCells			Ordered table of all planar cells in the world, with:
Cell
{
	Vector	center;						Normalized center vector of the cell
	Cell[]	neighbors;					Ordered table of all planar neighbor cells
	int		index;						Linear index of this cell in Game.Cells in [1,#Game.lowCells]
};
Vector
{
	float	x,y,z;
};

[client only]
	Marker[]	Game.myMarkers			All client-created markers. Since this is a client-private feature, server does not provide this


	Marker
	{
		Loc		location;				World location
		string	type;					'Visited','EntityDeath','HostileContact'
		bool	confirmed;				True if this marker has been confirmed by the server, false if it is pending (or failed)
		int		age;					Number of rounds since this marker was created
		Unit	unitReference;			Referenced unit. May be nil
		func	Touch();				Resets marker age to 0. Instantly updates local record, even if server update should fail
	}

	Marker		Game.CreateMarker(Unit ref, string type)			Creates a new marker, and immediately adds it to markers, as though the operation had succeeded even if it fails

	
	
	@brief This function determines the full reach of the specified unit, excluding locations for which isBlocked returns true (if not nil)
	@param unit Unit to determine reach of
	@param isBlocked Optional function that excludes certain cells. Type: bool BlockTest(Loc): true if blocked. 
		If nil, all possible locations are considered. Otherwise invoked only once per candidate (result is cached)
	@param maxReach Maximum distance (in terms of mobile movementSpeed) to consider. Larger values increase time necessary to complete
	@param forceReversible If true only consider paths that the specified unit can return on (as opposed to e.g. falling safely from a cliff, but not being able to climb back up)
	@param allowAccessibleHangars If true also include possible destination hangars of the local unit, false to consider those areas blocked
	@param ignoreFriendlyVehiclesOnRoute If true, owned mobile units will be ignored, otherwise considered blocking
	@param cannotStandOnUnknown Set true to disallow relocation onto unknown terrain
	@return Array of reachable locations
	
	Reachable[]	Game.GetUnitReach(Unit unit,func isBlocked, int maxReach, bool forceReversible, bool allowAccessibleHangars, bool ignoreFriendlyVehiclesOnRoute, bool cannotStandOnUnknown);
	

	Reachable	
	{
		Loc		location;				Grid/world location
		int		movementCostsFromRoot;	Costs to reach this location from the current unit location
		Reachable	predecessor;		nil if this is root (the current unit location)
		bool	isHangarHere;			True if accessible hangars were included and there is a hangar here
	}
		
	



The following global functions must be defined, and are called by the game as required:
void	OnBegin(optional int atTask)		Called once when the tutorial is started or resumed
void	CheckTaskConditions()				Called when situations have changed such that completion of the current task should be reevaluated. Expect high, but not per-frame frequency
void	OnMessageConfirm()					Called when the user has confirmed the last issued message (via Game.DisplayMessage())
void	OnCameraDrag()						Called when the user has dragged the camera around the planet
void	OnCameraRotate()					Called when the user has rotated the camera around the pivot point
void	OnCameraZoom()						Called when the user has altered the camera zoom level

]]









currentTask = 0;	--none
tasks = {};


function CreateTask(begin, endConditionFunc)
	local t = {};
	t.begin = 
		function() 
			if (endConditionFunc == nil or not endConditionFunc()) then
				begin();
			else
				NextTask();
			end;
		end;
	if endConditionFunc ~= nil then
		t.check = function()
			if (endConditionFunc()) then
				TaskDone();
			end;
		end;
	end;
	table.insert(tasks, t);
end


function SurveyorIsAutomated() 
	for i = 1, #Game.myUnits do
		local u = Game.myUnits[i];
		if u.typeName == 'Hoversurveyor' and u.isAutomated then
			return true;
		end;
	end;
	return false;
end;
		

--test:





--tasks:
CreateTask(function() Game.DisplayMessage("Welcome to Hexaverse, the planetary strategy game.\n"..
		"Excavate caves, build stairs into orbit, or just expand around the globe.\n"..
		"Survive if you can.", 'game.png') end);
CreateTask(function() Game.DisplayMessage(
		"What you are seeing now is the planet you are trying to conquer.\n"..
		"The planetary display attempts to distinguish four primary areas:\n"..
		"1: Undiscovered terrain\n"..
		"2: Known terrain, but currently out of range\n"..
		"3: Discovered and currently visible terrain\n"..
		"4: EMP zones (currently inaccessible)\n"..
		"Everything you are not currently observing (everything not in 3) may change without you knowing. Half the trick of winning Hexaverse is knowing what you don't know.\n"
		,'areas.png') end);


CreateTask(function() Game.DisplayMessage("Let's get you acquainted with the camera controls.") end);

CreateTask(function() Game.DescribeTask("Zoom the camera by using the mouse-wheel or the up/down arrow-keys."); end, function() return cameraZoomed end);
CreateTask(function() Game.DescribeTask("Drag the camera by holding the right or middle mouse button and moving the mouse."); end, function() return cameraDragged end);
CreateTask(function() Game.DescribeTask("Rotate the camera around your current focus by pressing the ASDW keys."); end, function() return cameraRotated end);

CreateTask(function() Game.DisplayMessage("Now that you have some understanding of camera control, let's explore your units.") end);

--buildings:
CreateTask(function() Game.DescribeTask("Please find and select your Generator by left-clicking on it. The name of the building you currently selected is shown in the top-right section of the screen.") end, function() return Game.selectedUnit ~= nil and Game.selectedUnit.typeName == 'Generator'; end);
CreateTask(function() Game.DisplayMessage("Enabled generators supply active buildings with %Energy%energy while consuming %Fuel%fuel. This generator is enough to support your Mine and one more consumer. After that you will need more.","generator.jpg") end);

CreateTask(function() Game.DescribeTask("Please find and select your Mine.") end, function() return Game.selectedUnit ~= nil and Game.selectedUnit.typeName == 'Mine'; end);
CreateTask(function() Game.DisplayMessage("Enabled mines collect natural resources from the planet core underneath. The colorful stack atop indicates how much they mine per round. This one collects %Minerals%minerals and %Fuel%fuel. The resources never run out but they are also rarely enough.","mine.jpg") end);

--build lab:
CreateTask(function() Game.DescribeTask("Please find and select your Terraformer.") end, function() return Game.selectedUnit ~= nil and Game.selectedUnit.typeName == 'Hoverformer'; end);
CreateTask(function() Game.DisplayMessage("Terraformers are your primary working force. While not all terraformers are equally manuverable, resilient, or productive, they can all shape the terrain and build new structures.\n Now, let's build something!","terraformer.png") end);
CreateTask(function() Game.DescribeTask("Please instruct your Terraformer to build a laboratory. Using the unit commands in the bottom screen section, select Build->Miscellaneous->Laboratory, and click anywhere on the terrain.") end, 
	function() 
		if Game.selectedUnit ~= nil and Game.selectedUnit.typeName == 'Hoverformer' then 
			for i = 1, #Game.selectedUnit.tasks do
				local t = Game.selectedUnit.tasks[i];
				if t.name == 'construct Laboratory' then
					return true;
				end;
			end;
		end;
		--also done when it already exists
		for i = 1, #Game.myUnits do
			local u = Game.myUnits[i];
			if u.typeName == 'Laboratory' then
				return true;
			end;
		end;
		return false;
	end);

	
function HaveLab()
	for i = 1, #Game.myUnits do
		local u = Game.myUnits[i];
		if u.typeName == 'Laboratory' then
			return true;
		end;
	end;
	return false;
end

	
CreateTask(function() Game.DisplayMessage("The Terraformer will keep constructing the laboratory until done, as long as enough %Minerals%resources are available. The bottom left resource table tells us that we have sufficient %Minerals%minerals available: "..Game.GetCompoundResourceChange('Minerals') .." per round, "..Game.GetCompoundResourceStorage('Minerals').." in storage. So let's press on.") end, HaveLab);

CreateTask(function() Game.DescribeTask("Until the Laboratory is done, please request new rounds by using the 'End Round' command in the bottom left section of the screen, or by pressing the Enter/Return key."); end,
	HaveLab
);

CreateTask(function() Game.DisplayMessage("Laboratories allow two types of research: Unlocking and Improving. The more laboratories you have enabled, the faster the research will progress. It is also a good idea to build them next to each other, as they accelerate each other in close proximity.","laboratory.jpg"); end);
CreateTask(function() Game.DescribeTask("Task your laboratory to unlock the ReactorCore building type by selecting it and using the commands in the bottom section of the screen."); end,
	function()
		local res = Game.myResearch['Unlock:ReactorCore'];
		return res ~= nil and (res.isQueued or res.isCompleted);
	end
);

CreateTask(function() Game.DescribeTask("While we wait for the laboratory to finish, please find and select your Surveyor.") end, function() return Game.selectedUnit ~= nil and Game.selectedUnit.typeName == 'Hoversurveyor'; end);

CreateTask(function() Game.DisplayMessage("Surveyors automatically identify new natural resources in their yellow scanning sphere. The dark discs near your starting location indicate that there isn't anything there (except beneath your starting mine). Instruct your surveyor to move by right-clicking anywhere without discs.","surveyor.png") end);
CreateTask(function() Game.DescribeTask("Please move your surveyor until it found some more %Fuel%fuel (a green bar atop the terrain). Select the vehicle and right-click anywhere on the terrain to schedule the motion. Request new rounds to execute movement operations") end, 
		function() 
			local at = Game.FindHighestAvailableResourceLocation('Fuel');
			return (at ~= -1);
		end
		);


		
CreateTask(function() Game.DisplayMessage("In some situations it may be smarter to survey the terrain manually, but for now let's instruct the unit to do so automatically.") end, SurveyorIsAutomated);
CreateTask(function() Game.DescribeTask("Please automate your Surveyor by selecting it and clicking on the Automate button at the bottom screen section.") end, SurveyorIsAutomated);

CreateTask(function() Game.DescribeTask("Please construct a mine atop the newly found fuel resource using your terraformer. In the bottom screen section, select Build->Resources->Mine, and click on the terrain cell with the green bar. Request new rounds until done.") end,
	function()
		local count = 0;
		for i = 1, #Game.myUnits do
			local u = Game.myUnits[i];
			if u.typeName == 'Mine' and Game.GetResourceCoverage('Fuel', u.location.cell) > 0 then
				count = count + 1;
			end;
		end;
		return (count >= 2);
	end
);

CreateTask(function() Game.DisplayMessage("The list in the bottom left section of the screen shows your current resource output. With two mines and one laboratory we now require more %Energy%energy than a single generator can supply.") end,
	function()
		return CountType('Generator') ~= 1;
	end
);

CreateTask(function() Game.DescribeTask(
	"Please construct a second generator using your terraformer. In the bottom screen section, select Build->Resources->Generator, and click anywhere on the terrain.\n"
		.."Don't worry about proximity, resources are transferred across any distance without visible connection.\n"
		.."Request new rounds until done.") end,
	function()
		local count = 0;
		for i = 1, #Game.myUnits do
			local u = Game.myUnits[i];
			if u.typeName == 'Generator' then
				count = count + 1;
			end;
		end;
		return (count >= 2);
	end
);


function SecondGeneratorIsEnabled()
	local enabled = 0;
	for i = 1, #Game.myUnits do
		local u = Game.myUnits[i];
		if (u.typeName == 'Generator' or u.typeName == 'Mine') and u.isEnabled then
			enabled = enabled + 1;
		end;
	end;
	return (enabled >= 4);
end;


CreateTask(function() Game.DisplayMessage("Both the new generator and mine are currently switched off, since they are not needed. Typically when requiring more %Energy%energy, available generators will be switched on automatically, which, in turn, may enable mines.") end,
	SecondGeneratorIsEnabled
);

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
	SecondGeneratorIsEnabled
);


CreateTask(function() Game.DisplayMessage("Notice how enabling either the generator or mine also enabled the other because your existing base did not supply enough resources. The two buildings now supply each other. If either is destroyed, the other may be forced to switch off.") end);

CreateTask(function() Game.DescribeTask("Next, construct a Vehicle Plant using your terraformer. In the bottom screen section, select Build->Factories->Vehicle Plant, and click anywhere on the terrain. Request new rounds until done.") end,
	function()
		for i = 1, #Game.myUnits do
			local u = Game.myUnits[i];
			if u.typeName == 'Vehicle Plant' then
				return true;
			end;
		end;
		return false;
	end
);

CreateTask(function() Game.DisplayMessage("Vehicle Plants can construct all sorts of vehicles. Let's start with another terraformer.","vehiclePlant.jpg") end);

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
					return true;
				end;
			end;
		end;
		return false;
	end
);

CreateTask(function() Game.DisplayMessage("As long as it's enabled and has access to the required %Minerals%minerals, the vehicle plant will continue producing the items in its queue, left to right. You can abort any job by clicking on it in the list. To automate deployment you may also want to place a gathering point by selecting the vehicle plant in question and right-clicking at the desired destination.") end);



CreateTask(function() Game.DescribeTask("Please find some %Uranium%uranium, and construct a mine atop using your terraformer. In the bottom screen section, select Build->Resources->Mine, and click on the terrain cell with the yellow bar. Request new rounds until done.") end,
	function()
		local count = 0;
		for i = 1, #Game.myUnits do
			local u = Game.myUnits[i];
			if u.typeName == 'Mine' and Game.GetResourceCoverage('Uranium', u.location.cell) > 0 then
				count = count + 1;
			end;
		end;
		return count >= 1;
	end
);

CreateTask(function() Game.DescribeTask("Please wait for your Reactor Core research to complete."); end,
	function()
		local res = Game.myResearch['Unlock:ReactorCore'];
		--FindResearch('Unlock:ReactorCore');
		return res ~= nil and (res.isCompleted);
	end
);


--CreateTask(function() Game.DisplayMessage("Reactor Cores cause neighboring Generators to produce twice as much %Energy%energy each round. They are a very useful thing to have. The catch is that you need %Uranium%uranium.") end);

CreateTask(function() Game.DescribeTask("Now, please construct a Reactor Core next to one of your Generators. It must be on the same level") end,
	function()
		
		for i = 1, #Game.myUnits do
			local u = Game.myUnits[i];
			if u.typeName == 'Reactor Core' then
				Game.Log("Found reactor");
				local cell = u.location.cell;
				local n = cell.neighbors;
				for j = 1, #n do
					Game.Log("Looking at "..n[j].index..", "..u.location.height);
					local neighbor = FindUnitIn(n[j],u.location.height);
					if (neighbor ~= nil and neighbor.typeName == 'Generator') then
						return true;
					end;
				end;
			end;
		end;
		return false;
	end
);



CreateTask(function() Game.DisplayMessage("Reactor Cores are a great way to boost the %Energy%energy output of your compound. They require %Uranium%uranium, however, and do tend to concentrate your power production, however, which may pose a rewarding target for hostile forces.","Reactor Core.jpg") end);

CreateTask(function() Game.DisplayMessage("This concludes the interactive tutorial.\n"..
"With the basics covered, you will undoubtedly quickly master all other features as well.\n"..
"The best of luck for your future conquests!\n"..
"You may leave and close the server any time.") end);









--helper functions


function CountType(strTypeName)
	local cnt = 0;
	for i = 1, #Game.myUnits do
		local u = Game.myUnits[i];
		if u.typeName == strTypeName then
			cnt = cnt + 1;
		end;
	end;
	return cnt;
end


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
	Game.ClearAllDelayed();
	Game.CallDelayed(2,NextTask);
end



function FindUnitIn(cell--[[Cell]], height--[[int]])
	for i = 1, #Game.myUnits do
		local u = Game.myUnits[i];
		if (u.location.cell == cell and u.location.height == height) then
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
	end;
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
