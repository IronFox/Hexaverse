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
function ResearchEntry.New(limit,original)
	local rs = {}
	setmetatable(rs,ResearchEntry)
	rs.limit = limit
	rs.original = original
	return rs
end
function ResearchEntry.NewBool(limit)
	return ResearchEntry.New(limit,false)
end
function ResearchEntry.NewFloat(original)
	return ResearchEntry.New(-1,original)
end
function ResearchEntry:Instantiate()
	local rs = ResearchEntry.New(self.limit,self.original)
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
function DefaultHealthAtCost(cost)
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
		DamageLevel = 3,
		ExplosionRadius = 1,
		HorizontalAttackRange = 5,
		IsUnlocked = true,
	},
	Radar =
	{
		Cost = 10,
		Weight = 2,
		HealthMultiplier = 1,
		ScanningRange = 12,
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
		Cost = 40,
		Weight = 5,
		HealthMultiplier = 1,
		ScanningRange = 1,
		ExplosionRadius = 7,
		DamageLevel = 50,
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
		HorizontalAttackRange = 17,
		AmmoConsumption = 50,
		IsUnlocked = false,
	},
	Artillery =
	{
		Cost = 25,
		Weight = 3,
		HealthMultiplier = 1.5,
		ScanningRange = 4,
		AttacksPerRound = 1,
		DamageLevel = 7,
		ExplosionRadius = 3.5,
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
}


VehicleBaseConfig = 
{
	Hoverbase = 
	{
		Cost = 40,
		MovementSpeed = 20,
		CarryingCapacity = 1,
		Health = 4,
		OperationSpeedMultiplier = 1,
		TurretRangeModifier = 0,
		IsUnlocked = true,
	},
	HeavyBase = 
	{
		Cost = 80,
		MovementSpeed = 15,
		CarryingCapacity = 10,
		Health = 12,
		OperationSpeedMultiplier = 2,
		TurretRangeModifier = 0,
		IsUnlocked = true,
	},
	CatamaranBase = 
	{
		Cost = 100,
		MovementSpeed = 8,
		CarryingCapacity = 15,
		Health = 12,
		OperationSpeedMultiplier = 2,
		TurretRangeModifier = 1,
		IsUnlocked = true,
	},
}



function MakeUnlockResearchable(researchGroup, typeTable, typeName, limit)
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
		end
	}
	
	return Game.CreateResearch(researchGroup,typeName,nil,
		function(state,progress) state.research[key].progress = state.research[key].progress + progress end,
		function(state) return state.research[key].progress end, researchables[key].limit
		);
	
end


function MakeImproveResearchable(researchGroup, researchDependency, typeTable, typeName, attributeName)
	local key = 'improve'..typeName
	local typeHandle = typeTable[typeName]
	--at this point we assume that typeHandle[attributeName] is still a simple variable. we can add a check here but then ResearchEntry.NewFloat would get really messy
	local original = typeHandle[attributeName]
	researchables[key] = ResearchEntry.NewFloat(original)
	typeHandle[attributeName] =
	{
		Get = function(state)
			if state == nil then
				return original
			end
			local progress = state.research[key].progress
			return original * (2 ^ (0.1 * progress) )
		end,
		
		Describe = function(state)
			if state == nil then
				return original
			end
			local progress = state.research[key].progress
			return original..'*'..(2 ^ (0.1 * progress) )
		end
	}
	
	return Game.CreateResearch(researchGroup,typeName,nil,
		function(state,progress) state.research[key].progress = state.research[key].progress + progress end,
		function(state) return state.research[key].progress end, -1, researchDependency
		);
	
end



function CanAttack(turret)
	return Get(turret.AttacksPerRound,nil) > 0
end

function TowerScanningModifier(turret)
	if CanAttack(turret) then
		return -2.0	--penalty for attaching towers
	end
	return 2.0	--boost non-attacking towers
end
 


function Tower(turret)
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
					return DefaultHealthAtCost(rs.Cost.Get(state))..'*'.. Describe(turret.HealthMultiplier,state) 
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
					return Describe(turret.ScanningRange,state) ..'+'.. TowerScanningModifier(turret) 
				end,
		},
		IsUnlocked = 
			function(state) 
				return Get(turret.IsUnlocked,state) 
			end,
	}
	return rs
end


StructureConfig =
{
	Mine = DefaultStructure(40),
	Generator = DefaultStructure(40),
	ReactorCore = DefaultStructure(120),
	VehicleStore = DefaultStructure(40),
	VehiclePlant = DefaultStructure(80),
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
improveGroup = Game.CreateResearch(nil,"Improve","Research_Improve");
attackRangeGroup = Game.CreateResearch(improveGroup,'Attack Range','Research_AttackRange');



artilleryUnlock = MakeUnlockResearchable(unlockGroup, TurretConfig, 'Artillery',20)
MakeUnlockResearchable(unlockGroup, StructureConfig, 'ReactorCore',20)

MakeImproveResearchable(attackRangeGroup, artilleryUnlock, TurretConfig, 'Artillery', 'HorizontalAttackRange')


