--[[ API:

Handle	Game.CreateResearch(Handle group, String name, String iconName, advance(ResearchState,int by), getProgress(ResearchState)->int, int maxProgress, Handle dependency)

	Creates a new research topic or topic group.
	Leave maxProgress at 0 to indicate that the research is a topic group

	@param group Topic group to insert this research into. Set nil to declare the research a root topic
	@param name String identifier of the research topic
	@param iconName Name of the component, prototype, or image to display. Leave empty to reuse the content of [name] instead
	@param advance Function/method that accepts an instance of ResearchState and an integer containing research points (>0), 
					to be assigned to this research topic. Leave empty if this is supposed to be a topic group
	@param getProgress Function/method that accepts an instance of ResearchState and returns the current progress of this research topic. 
					Leave empty if this is supposed to be a topic group
	@param maxProgress Maximum possible amount of research progress that can be invested into this topic. Use negative values to indicate
					an infinitely researchable topic. Leave at 0 if this is supposed to be a topic group
	@param dependency Research topic handle that this research depends on. Researching this topic will only be allowed once the specified
					dependency is completed
	@return Handle to the new topic group

	
void	Game.Log(msg)					Prints any message to Hexaverse log (client or server-private)

bool	Game.LowerResolutionPlanet		Set to true if the planet has reduced grid resolution (1/4 of cells, each cell twice as high)
float	Game.UnitScale					Set to a global unit scale corresponding to [Game.LowerResolutionPlanet]. This scale value is to be
										applied too units (hence the name). 1 if the planet is not of lowered resolution, 2 otherwise.


Hexaverse expects the following classes and methods to be defined:
	ResearchState - table, arbitrary content. Expected but not required to mimic a class
	ResearchState.New() - constructs a new instance (expected to be of its own type, but not required)
	ResearchState:Export() - expected to return the serializable content of the local research state.
							Expected but not required to operate on instances of type ResearchState. State is passed as first function
							parameter (e.g. [self] in a method).
							Result must not contain non-serializable content. Serializable content includes tables, ints, floats, and strings, 
							both as keys and values.
	ResearchState:Import(exported) - expected to integrate a previously exported, deserialized research state. Should only accept recognized data,
									and discard the rest. Data may come from older savegames containing outdated research formats.
									Expected but not required to operate on instances of type ResearchState. State is passed as first function
									parameter (e.g. [self] in a method).
]]




researchables = {}


ResearchEntry = {}
ResearchEntry.__index = ResearchEntry
function ResearchEntry.New(limit)
	local rs = {}
	setmetatable(rs,ResearchEntry)
	rs.limit = limit
	return rs
end
function ResearchEntry.NewBool(limit)
	return ResearchEntry.New(limit)
end
function ResearchEntry.NewFloat()
	return ResearchEntry.New(-1)
end
function ResearchEntry:Instantiate()
	local rs = ResearchEntry.New(self.limit)
	rs.progress = 0
	return rs
end


ResearchState = {}
ResearchState.__index = ResearchState
function ResearchState.New()
	local rs = {}
	setmetatable(rs,ResearchState)
	rs.research = {}
	for k,v in pairs(researchables) do
		rs.research[k] = v:Instantiate()
	end
	return rs
end

function ResearchState:GetState(key--[[string]])
	if self.research[key] == nil then
		Game.Log("Research state lacks key '"..tostring(key).."'. Instantiating new");
		self.research[key] = researchables[key]:Instantiate();
	end;
	return self.research[key];
end;

function ResearchState:Export()
	return self.research
end

function ResearchState:Import(exported)
	self.research = exported
end





UnlockProgress = {}
UnlockProgress.__index = UnlockProgress
function UnlockProgress.New()
	local rs = {}
	setmetatable(rs,UnlockProgress)
	rs.progress = 0;
	return rs
end


--[[ Helper functions ]]


function Round(value--[[float]], digits--[[optional int]])
	local factor = 10^(digits or 0);
	local result = math.floor(value * factor + 0.5) / factor;
	local floored = math.floor(value);
	if (floored==value) then
		return floored;
	end
	return result;
end

function DefaultHealthAtCost(cost--[[float]]) --float
	return cost /5
end

function ExplicitStructure(cost, health, scanningRange, isUnlocked)
	return
	{
		Cost = cost,
		Health = health,
		ScanningRange = scanningRange,
		IsUnlocked = isUnlocked
	}
end

function Structure(cost, scanningRange)
	return ExplicitStructure(cost, DefaultHealthAtCost(cost),scanningRange, true)
end

function DefaultStructure(cost)
	return Structure(cost,2)
end

function Get(attribute, state)
	if (attribute == nil) then
		return 0
	end
	t = type(attribute)
	if t == 'table' then
		return attribute.Get(state)
	end
	if t == 'function' then
		return attribute(state)
	end
	return attribute
end

function Describe(attribute, state)
	if (attribute == nil) then
		return '0'
	end
	t = type(attribute)
	if t == 'table' then
		return attribute.Describe(state)
	end
	if t == 'function' then
		return attribute(state)
	end
	return attribute
end


--[[ Turret configurations ]]
TurretConfig = 
{
	MissileLauncher = 
	{
		Cost = 40,
		Weight = 13,
		HealthMultiplier = 1.5,
		ScanningRange = 4,
		AttacksPerRound = 1,
		DamageLevel = 10,
		ExplosionRadius = 1,
		HorizontalAttackRange = 8,
		IsUnlocked = true,
	},
	Cannon = 
	{
		Cost = 15,
		Weight = 3,
		HealthMultiplier = 2,
		ScanningRange = 5,
		AttacksPerRound = 2,
		DamageLevel = 7,
		ExplosionRadius = 1,
		HorizontalAttackRange = 5,
		IsUnlocked = true,
	},
	Radar =
	{
		Cost = 10,
		Weight = 2,
		HealthMultiplier = 1,
		ScanningRange = 7,
		IsUnlocked = true,
	},
	Surveyor =
	{
		Cost = 10,
		Weight = 1,
		HealthMultiplier = 1,
		ScanningRange = 4,
		IsUnlocked = true,
	},
	Bomb =
	{
		Op_Nuclear = 50,
		Cost = 40,
		Weight = 5,
		HealthMultiplier = 1,
		ScanningRange = 1,
		ExplosionRadius = 4,
		DamageLevel = 10,
		HorizontalAttackRange = 0,
		IsUnlocked = true,
	},
	EMP =
	{
		Cost = 100,
		Weight = 5,
		HealthMultiplier = 1,
		ScanningRange = 3,
		AttacksPerRound = 1,
		DamageLevel = 5,
		ExplosionRadius = 3,
		HorizontalAttackRange = 15,
		AmmoConsumption = 50,
		IsUnlocked = true,
	},
	Artillery =
	{
		Cost = 25,
		Weight = 3,
		HealthMultiplier = 1.5,
		ScanningRange = 4,
		AttacksPerRound = 1,
		DamageLevel = 7,
		ExplosionRadius = 2,
		HorizontalAttackRange = 7,
		IsUnlocked = false,
	},
	Terraformer =
	{
		Cost = 40,
		Weight = 2,
		HealthMultiplier = 1,
		ScanningRange = 4,
		Op_Construct = 10,
		Op_TerrainRemove = 10,
		Op_TerrainAdd = 10,
		IsUnlocked = true,
	},
	Mechanic =
	{
		Cost = 30,
		Weight = 2,
		HealthMultiplier = 1,
		ScanningRange = 3,
		Op_Repair = 10,
		IsUnlocked = true,
	},
	
	--Planes:
	Raptor =
	{
		Cost = 80,
		Weight = 10,	--defines movement speed: 100/Weight
		HealthMultiplier = 5,	--directly equals health
		ScanningRange = 7,
		HorizontalAttackRange = 7,
		GroundScanningRange = 7,
		GroundScannerPenetration = 0,
		ExplosionRadius = 1,
		DamageLevel = 5,
		AttacksPerRound = 2,
		IsUnlocked = true,
	},
	CargoPlane = 
	{
		Cost = 40,
		Weight = 12,	--defines movement speed: 100/Weight
		HealthMultiplier = 15,	--directly equals health
		ScanningRange = 5,
		GroundScanningRange = 2,
		GroundScannerPenetration = 0,
		IsUnlocked = true,
	},
	SensorDrone = 
	{
		Cost = 10,
		Weight = 11,	--defines movement speed: 100/Weight
		HealthMultiplier = 4,	--directly equals health
		ScanningRange = 10,
		GroundScanningRange = 10,
		GroundScannerPenetration = 2,
		IsUnlocked = true,
	},}


VehicleBaseConfig = 
{
	Hoverbase = 
	{
		Cost = 30,
		MovementSpeed = 20,
		CarryingCapacity = 1,
		Health = 4,
		OperationSpeedMultiplier = 1,
		TurretRangeModifier = 0,
		IsUnlocked = true,
	},
	HeavyBase = 
	{
		Cost = 45,
		MovementSpeed = 15,
		CarryingCapacity = 10,
		Health = 12,
		OperationSpeedMultiplier = 2,
		TurretRangeModifier = 0,
		IsUnlocked = true,
	},
	CatamaranBase = 
	{
		Cost = 60,
		MovementSpeed = 8,
		CarryingCapacity = 15,
		Health = 12,
		OperationSpeedMultiplier = 2,
		TurretRangeModifier = 1,
		IsUnlocked = true,
	},
}


--[[
Declares an item as unlock-researchable.
Manipulates the IsUnlocked attribute of the specified item.
Automatically registers everything

@param researchGroup		Research-group handle (folder) that this new research should be filed under. May be nil, causing this to be filed as a root research topic
@param researchDependency	Research handle (actual research) that must be completed before this research may commence. Must be a non-improve research. May be nil
@param typeTable			Handle to the table that the specified type is a member in
@param typeName				Name (string) of the type the attribute belongs to
@param limit				Max research (int). Progress must reach this value for the item to become unlocked

@return 					Resulting research handle, as returned from Hexaverse
]]
function MakeUnlockResearchable(researchGroup, researchDependency, typeTable, typeName, limit)
	--entry = ResearchEntry.New(original)
	local key = 'unlock'..typeName
	researchables[key] = ResearchEntry.NewBool(limit)
	local typeHandle = typeTable[typeName]
	typeHandle.IsUnlocked =
	{
		Get = function(state)
			if state == nil then
				return false
			end
			return state.research[key].progress >= state.research[key].limit
		end,
		Describe = function(state)
			if state == nil then
				return false
			end
			return state.research[key].progress >= state.research[key].limit
		end
	}
	
	return Game.CreateResearch(researchGroup,typeName,nil,
		function(state,progress) state:GetState(key).progress = progress end,
		function(state) return state:GetState(key).progress; end, 
		researchables[key].limit, researchDependency
		);
	
end


--[[
Calculates the factor that is to be multiplied with the base attribute to get the final attribute

@param researchProgress		Actual progress (float) in the chosen research topic. Can be negative causing degeneration of the attribute towards zero
@param progressMultiplier	Positive speed modifier (float) that manipulates how quickly research progresses. 1 leaves research speed unmodified

@return 					Resulting attribute modifier
]]
function GetResearchImprovementFactor(researchProgress--[[float]],progressMultiplier--[[float]])  --[[float]]
	local totalProgress =  math.sqrt(math.abs(researchProgress)) * progressMultiplier;
	if (researchProgress < 0) then
		return 1 / (1 + totalProgress);
	end
	return (1 + totalProgress);
end


--[[
Declares a currently non-researchable attribute as improvement-researchable.
Works on all floating point attributes.
Improvement-research goes on indefinitely.
Automatically registers everything

@param researchGroup		Research-group handle (folder) that this new research should be filed under. May be nil, causing this to be filed as a root research topic
@param researchDependency	Research handle (actual research) that must be completed before this research may commence. Must be a non-improve research. May be nil
@param typeTable			Handle to the table that the specified type is a member in
@param typeName				Name (string) of the type the attribute belongs to
@param attributeName		Name (string) of the attribute to research
@param speed				Floating point speed modifier. The closer to zero, the slower. 
							Positive values affect the resulting factor linearly. 
							If 10 rounds are needed to reach 1.2 at factor 0.02, then another 10 rounds will produce 1.4.
							Negative values affect the resulting factor inversely.
							If 10 rounds are needed to reach 1/(1.2)=83% at factor -0.02, then another 10 rounds will produce 1/(1.4)=71%
							
@return 					Resulting research handle, as returned from Hexaverse
]]
function MakeImproveResearchable(researchGroup, researchDependency, typeTable, typeName, attributeName, speed)
	local key = 'improve'..typeName;
	local typeHandle = typeTable[typeName];
	--at this point we assume that typeHandle[attributeName] is still a simple variable. we can add a check here but then ResearchEntry.NewFloat would get really messy
	local original = typeHandle[attributeName];
	researchables[key] = ResearchEntry.NewFloat();
	typeHandle[attributeName] =
	{
		Get = function(state)
			if state == nil then
				return original;
			end
			local progress = state.research[key].progress;
			local factor = GetResearchImprovementFactor(progress,speed);
			local result = original * factor;
			return result;
		end,
		
		Describe = function(state)
			if state == nil then
				return original;
			end
			local progress = state.research[key].progress;
			if (progress == 0) then
				return original;
			end
			local factor = GetResearchImprovementFactor(progress,speed);
			return Round(original * factor,2)..' [='.. original..'*'..Round(factor,2)..']';
		end
	}
	
	return Game.CreateResearch(researchGroup,typeName,nil,
		function(state,progress) state.research[key].progress = progress end,
		function(state) return state.research[key].progress end, -1, researchDependency
		);
end



function CanAttack(turret--[[Table]])
	return Get(turret.AttacksPerRound,nil) > 0
end

function TowerScanningModifier(turret--[[Table]])
	if CanAttack(turret) then
		return -2.0	--penalty for offensive towers
	end
	return 2.0	--boost non-offensive towers
end
 

--[[
Constructs a tower structure configuration from a given turret entry.
Defines both Get and Describe functions
]]
function Tower(turret--[[Table]]) --Table
	rs =
	{
		Cost =
		{
			Get = 
				function(state) 
					return 40 + Get(turret.Cost,state)
				end,
			Describe = 
				function(state) 
					return '40+'.. Describe(turret.Cost,state)
				end,
		},
		Health = 
		{
			Get = 
				function(state) 
					return DefaultHealthAtCost(rs.Cost.Get(state)) * Get(turret.HealthMultiplier,state) 
				end,
			Describe = 
				function(state) 
					local multiplier = Describe(turret.HealthMultiplier,state);
					local health = DefaultHealthAtCost(rs.Cost.Get(state));
					if (multiplier == 1) then
						return Round(health);
					end
					return Round(health * Get(turret.HealthMultiplier,state),2)..' [='.. Round(health)..'*'.. multiplier..' (turret health multiplier)]';
				end,
		},
		ScanningRange =
		{
			Get = 
				function(state) 
					return Get(turret.ScanningRange,state) + TowerScanningModifier(turret)
				end,
			Describe = 
				function(state) 
					local modifier =  TowerScanningModifier(turret);
					local base = Describe(turret.ScanningRange,state);
					if (modifier == 0) then
						return base;
					end
					if (modifier > 0) then
						return Round(Get(turret.ScanningRange,state)+modifier,2)..' [='..  base ..'+'..Round(modifier,2)..' (tower modifier)]';
					end
					return Round(Get(turret.ScanningRange,state)+modifier,2)..' [='..  base ..'-'..Round(-modifier,2)..' (tower modifier)]';
				end,
		},
		IsUnlocked = 
			function(state) 
				return Get(turret.IsUnlocked,state) 
			end,
	}
	return rs;
end


StructureConfig =
{
	Mine = DefaultStructure(40),
	Generator = DefaultStructure(40),
	ReactorCore = DefaultStructure(120),
	VehicleStore = DefaultStructure(40),
	VehiclePlant = DefaultStructure(80),
	PlaneFactory = DefaultStructure(200),
	Laboratory = DefaultStructure(80),
	MunitionFactory = DefaultStructure(40),
	Connector = DefaultStructure(5),
	FuelContainer = DefaultStructure(10),
	MineralContainer = DefaultStructure(10),
	UraniumStorage = DefaultStructure(10),
	MunitionStorage = DefaultStructure(10),
	NexusPort = ExplicitStructure(0,1,2,false),
	MissileLauncherTower = Tower(TurretConfig.MissileLauncher),
	RadarTower = Tower(TurretConfig.Radar),
	CannonTower = Tower(TurretConfig.Cannon),
}




unlockGroup = Game.CreateResearch(nil,"Unlock","Research_Unlock");
improveGroup = Game.CreateResearch(nil,"Improve",'Increase');
--"Research_Improve");



artilleryUnlock = MakeUnlockResearchable(unlockGroup, nil, TurretConfig, 'Artillery',20);
empUnlock = MakeUnlockResearchable(unlockGroup, artilleryUnlock, TurretConfig, 'EMP',40);
bombUnlock = MakeUnlockResearchable(unlockGroup, artilleryUnlock, TurretConfig, 'Bomb',40);
launcherUnlock = MakeUnlockResearchable(unlockGroup, nil, TurretConfig, 'MissileLauncher',20);
--radarUnlock = MakeUnlockResearchable(unlockGroup, nil, TurretConfig, 'Radar',20);
heavyUnlock = MakeUnlockResearchable(unlockGroup, nil, VehicleBaseConfig, 'HeavyBase',20);
catUnlock = MakeUnlockResearchable(unlockGroup, nil, VehicleBaseConfig, 'CatamaranBase',20);
MakeUnlockResearchable(unlockGroup, nil, StructureConfig, 'ReactorCore',10);
planeFactoryUnlock = MakeUnlockResearchable(unlockGroup, nil, StructureConfig, 'PlaneFactory',20);
MakeUnlockResearchable(unlockGroup, planeFactoryUnlock, TurretConfig, 'CargoPlane',40);
MakeUnlockResearchable(unlockGroup, planeFactoryUnlock, TurretConfig, 'Raptor',60);

attackRangeGroup = Game.CreateResearch(improveGroup,'Attack Range','Research_AttackRange');
	MakeImproveResearchable(attackRangeGroup, artilleryUnlock, TurretConfig, 'Artillery', 'HorizontalAttackRange',0.005);
	MakeImproveResearchable(attackRangeGroup, launcherUnlock, TurretConfig, 'MissileLauncher', 'HorizontalAttackRange',0.005);
	MakeImproveResearchable(attackRangeGroup, nil, TurretConfig, 'Cannon', 'HorizontalAttackRange',0.005);

scanningRangeGroup = Game.CreateResearch(improveGroup,'Scanning Range','Research_ScanningRange');
	MakeImproveResearchable(scanningRangeGroup, radarUnlock, TurretConfig, 'Radar', 'ScanningRange',0.005);

healthGroup = Game.CreateResearch(improveGroup,'Health','Research_Health');
	MakeImproveResearchable(healthGroup, nil, VehicleBaseConfig, 'Hoverbase', 'Health',0.005);
	MakeImproveResearchable(healthGroup, heavyUnlock, VehicleBaseConfig, 'HeavyBase', 'Health',0.005);
	MakeImproveResearchable(healthGroup, catUnlock, VehicleBaseConfig, 'CatamaranBase', 'Health',0.005);


