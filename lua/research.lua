ResearchState = {}
ResearchState.__index = ResearchState
function ResearchState.New()
	local rs = {}
	setmetatable(rs,ResearchState)
	return rs
end

function ResearchState:SelectUnlock(tech)
	print "hello"
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
	if type(attribute) == 'function' then
		return attribute(state)
	end
	return attribute
end


--[[ Turret configurations ]]
TurretConfig = 
{
	Launcher = 
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
}



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
	return
	{
		Cost = function(state) return 40 + Get(turret.Cost,state) end,
		Health = function(state) return DefaultHealthAtCost(Cost.Get(state)) * Get(turret.HealthMultiplier,state) end,
		ScanningRange = function(state) return Get(turret.ScanningRange,state) + TowerScanningModifier(turret) end,
		IsUnlocked = function(state) return Get(turret.IsUnlocked,state) end,
	}
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
	MissileLauncherTower = Tower(TurretConfig.Launcher),
	RadarTower = Tower(TurretConfig.Radar),
	CannonTower = Tower(TurretConfig.Cannon),

}


unlockGroup = API.CreateResearch(nil,"Unlock");

artillery = UnlockProgress.New();

API.CreateResearch(unlockGroup,"Artillery","Artillery",function(state) return state:SelectUnlock("Artillery") end);






--[[
Account = {}
Account.__index = Account

function Account.create(balance)
   local acnt = {}             -- our new object
   setmetatable(acnt,Account)  -- make Account handle lookup
   acnt.balance = balance      -- initialize our object
   return acnt
end

function Account:withdraw(amount)
   self.balance = self.balance - amount
end

-- create and use an Account
acc = Account.create(1000)
acc:withdraw(100)
]]--