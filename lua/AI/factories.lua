--#require "util.lua" 
--#require "resvec.lua"

function GroundTotal(threat)
	return threat.groundMobile + threat.groundStationary;
end;

function AirTotal(threat)
	return threat.airMobile + threat.airStationary;
end;

function GroundMobileTotal(support)
	return math.min(support.attackGroundMobile,support.sensorGroundMobile);
end;

function AirMobileTotal(support)
	return math.min(support.attackAirMobile,support.sensorAirMobile);
end;


State = {};

function State.New()
	return {enabled = 0,available = 0};
end;

function State.Make(_enabled, _available)
	return {enabled = _enabled, available = _available};
end;

function State.Add(v,w)
	v.enabled = v.enabled + w.enabled;
	v.available = v.available + w.available;
end;


function AutoDeploy(plant --[[Unit]])
	if Game.Grid.AffectedByEMP(plant.location) then
		return State.New();
	end;
	local loc = plant.GetIdealDeploymentOption();
	if loc ~= nil then
		plant.DeployProductionAt(loc);
		if #plant.productionQueue > 1 then
			return State.Make(1,1);
		else
			return State.Make(0,1);
		end;
	end;
	if plant.productionQueue ~= nil then
		return State.Make(1,1);
	end;
	return State.Make(0,1);
end;


ProductionTable = {};

function ProductionTable.New(poleIdx)
	local exist = {};
	local typeCounter = 0;
	for _,m in pairs(Game.AI.GetOwnMobiles(poleIdx)) do
		local cnt = exist[m.type];
		if not cnt then
			cnt = {count = 1};
			exist[m.type] = cnt;
			typeCounter = typeCounter + 1;
		else
			cnt.count = cnt.count + 1;
		end;
	end;

	return {
		pole = poleIdx,
		types = {},
		existing = exist,
		numExistingTypes = typeCounter
	};
end;

function ProductionTable.CountExisting(tab,unitType)
	local exist = tab.existing[unitType];
	if not existing then
		return 0;
	end;
	return existing.count;
end;

function ProductionTable.AddUnitType(tab,uType)
	Util.Push(tab.types,{unitType = uType, demand=0, offset = 0, existing = ProductionTable.CountExisting(tab,uType)});
end;


function ProductionTable.Add(tab, t)
	if t.typeClass == 'Turret' then
		ProductionTable.AddUnitType(tab,Game.Type.Vehicle(t,Game.Type.vehicleBases['Hoverbase']));
		if Game.AI.ShouldBuildHeavyVehicleBases(tab.pole) then
			ProductionTable.AddUnitType(tab,Game.Type.Vehicle(t,Game.Type.vehicleBases['Heavy Base']));
		end;
		return;
	end;
	if t.typeClass == 'Unit' then
		ProductionTable.AddUnitType(tab,t);
		return;
	end;
	error('Unexpected type passed to ProductionTable.Add(): '..t.typeClass);
end;

function ProductionTable.IsNotEmpty(tab)
	return #tab.types > 0;
end;

function ProductionTable.GetRandomChoice(tab)
	local sum = 0;
	for _,t in pairs(tab.types) do
		sum = sum + ProductionTable.CountExisting(tab,t.unitType);
	end;

	local average = 0;
	if tab.numExistingTypes > 0 then
		average = Util.RoundToInt(sum / tab.numExistingTypes);
	end;
	average = math.max(1,average);

	local totalDemand = 0;

	for _,c in pairs(tab.types) do
		c.demand = 1.0 / (1.0 + c.existing);
		c.offset = totalDemand;
		totalDemand = totalDemand  + c.demand;
	end;

	local rand = Game.Random() * totalDemand;

	for _,c in pairs(tab.types) do
		if (c.demand > 0 and rand >= c.offset and rand <= c.offset + c.demand) then
			return c.unitType;
		end;
	end;
	error("Should not be here");
end;

function EnableFactory(poleIndex--[[int]], port --[[Unit]], factories --[[ Unit[] ]], choice --[[UnitType]], enabled--[[unit->bool]]) --[[AiEst]]
	
	Game.Log('AI: Increasing production for ' .. port.name .. ', using type '..choice.name);

	for _,p in pairs(factories) do
	
		if not p.productionQueue and not enabled[p] then
		
			Game.Log('AI: Enqueueing vehicle production job in '..p.name..' due to high threat level...');
			p.BuildMobile(choice);
			enabled[p] = true;
			
			return Game.AI.Estimate(poleIndex, choice, p.location, Game.myFaction);
		end;
		
	end;
	
	Game.Log('AI: '..port.name .. ' no longer has idle plants capable of queueing jobs. Aborting');
	
	return nil;
end;



function ProcessFactories()

	Game.Log("AI: Processing factories");


	local vehiclePlantType = Game.Type.units['Vehicle Plant'];
	if not vehiclePlantType then
		Game.Log('AI: Vehicle Plant type not found');
		return;
	end;
	
	
	local vehiclePlantCostPerRound = Game.Type.units['Vehicle Plant'].balance.demand;
	local planeFactoryCostPerRound = Game.Type.units['Plane Factory'].balance.demand;

	local ConstructorThreshold =  3.5;

	local enabled = {};
	
	for i = 1,12 do
		local port = Game.AI.GetNexusPort(i);
		if port ~= nil and not Game.Grid.AffectedByEMP(port.location) and Game.AI.IsBase(i) then
			local threat = Game.AI.GetThreatLevel(i);
			local support = Game.AI.GetSupportLevel(i);
			local portProductionPerRound = port.balance.supply;

			Game.Log('AI: Threat level for ' .. port.name .. ' at pole '..i..' considered ' .. GroundTotal(threat)..'|'..AirTotal(threat)..
				' (support here: '..GroundMobileTotal(support)..'|'..AirMobileTotal(support)..', hostile constructor(s) at: '..threat.highestConstructorSignificance..'/'..threat.constructorSignificanceSum..')');

			local constructorViolation = (threat.highestConstructorSignificance > ConstructorThreshold);

			--let's take a look at producers...
			local groundState = State.New();
			local airState = State.New();


			for _,p in pairs(Game.AI.GetBaseVehiclePlants(i)) do
				State.Add(groundState,AutoDeploy(p));
			end;
			
			for _,p in pairs(Game.AI.GetBasePlaneFactories(i)) do
				State.Add(airState,AutoDeploy(p));
			end;

			if not Game.AI.ShouldBuildPlanes() then
				airState.available = 0;
				Game.Log('AI: Can\'t build planes yet');
			else
				groundState.available = math.min(groundState.available, RV.Div(RV.Sub(portProductionPerRound,planeFactoryCostPerRound),vehiclePlantCostPerRound));	--keep at least one plane factory free to enable
				local resourcesLeft = (RV.Sub(portProductionPerRound,RV.Mul(vehiclePlantCostPerRound,groundState.enabled)));
				local maxFit = RV.Div(resourcesLeft,planeFactoryCostPerRound);
				airState.available = math.min(airState.available,maxFit);
			end;

			
			if (RV.GreaterOrEqual(RV.Add(RV.Mul(vehiclePlantCostPerRound,groundState.enabled),RV.Mul(planeFactoryCostPerRound,airState.enabled)),portProductionPerRound)) then
				Game.Log('AI: Can\'t produce any more on this pole');
			else

				while (AirMobileTotal(support) < AirTotal(threat) or GroundMobileTotal(support) < GroundTotal(threat) or constructorViolation) do
				
					local airDemand = AirTotal(threat) - AirMobileTotal(support);
					local groundDemand = GroundTotal(threat) - GroundMobileTotal(support);
					local tab = ProductionTable.New(i);
					local loop = false;

					if (airDemand > groundDemand and not constructorViolation) then
					
						if (airState.enabled < airState.available and RV.LessOrEqual(RV.Add(RV.Mul(vehiclePlantCostPerRound,groundState.enabled),RV.Mul(planeFactoryCostPerRound,airState.enabled+1)),portProductionPerRound)) then
							ProductionTable.Add(tab,Game.Type.units['Raptor']);
							ProductionTable.Add(tab,Game.Type.units['Sensor Drone']);
						end;
						if (groundState.enabled < groundState.available and RV.LessOrEqual(RV.Add(RV.Mul(vehiclePlantCostPerRound,groundState.enabled+1),RV.Mul(planeFactoryCostPerRound,airState.enabled)),portProductionPerRound)) then
							ProductionTable.Add(tab,Game.Type.turrets['Anti Aircraft']);
						end;
						if (ProductionTable.IsNotEmpty(tab)) then
							local choice = ProductionTable.GetRandomChoice(tab);
							if (choice.isSuborbitalPlane) then
								support = support.Inc(EnableFactory(i,port,Game.AI.GetBasePlaneFactories(i),choice,enabled));
								airState.enabled = airState.enabled + 1;
							else
								support = support.Inc(EnableFactory(i,port,Game.AI.GetBaseVehiclePlants(i),choice,enabled));
								groundState.enabled = groundState.enabled + 1;
							end;
							loop = true;
						end;
					end;
					
					if not loop then
						constructorViolation = false;

						local includingPlaneFactoryTotalCosts = RV.Add(RV.Mul(vehiclePlantCostPerRound,groundState.enabled),RV.Mul(planeFactoryCostPerRound,airState.enabled+1));

						if (airState.enabled < airState.available and RV.LessOrEqual(includingPlaneFactoryTotalCosts,portProductionPerRound)) then
							ProductionTable.Add(tab,Game.Type.units['Bomber']);
							ProductionTable.Add(tab,Game.Type.units['Sensor Drone']);
						else
							Game.Log('AI: Insufficient resources/capacity to enable plane factories: '..airState.enabled..'/'..airState.available..', '.. RV.ToString(includingPlaneFactoryTotalCosts)..'/'..RV.ToString(portProductionPerRound));
						end;


						local includingGroundFactoryTotalCosts = RV.Add(RV.Mul(vehiclePlantCostPerRound,groundState.enabled+1),RV.Mul(planeFactoryCostPerRound,airState.enabled));

						if (groundState.enabled < groundState.available and RV.LessOrEqual(includingGroundFactoryTotalCosts,portProductionPerRound)) then
							ProductionTable.Add(tab,Game.Type.turrets['Cannon']);
							ProductionTable.Add(tab,Game.Type.turrets['Artillery']);
							ProductionTable.Add(tab,Game.Type.turrets['Radar']);

							--[[
							ProductionTable.Add(tab,Game.Type.turrets['Surveyor']);

							Leave this one off for now as it essentially adds a different strategy.
							First off, a resource scanner must be isolated from regular radar,
							else they blow each other up on close proximity, which is not prevented
							due to different turrets being used.
							Secondly, these follow a dramatically different behavior. they must
							seak out locations currently not observed by other scanners and stick
							to submerged hostile contacts.
							Finally, bombers must home into such locations and bomb submerged
							targets until hit.
							Work for later
							]]
							
						else
							Game.Log('AI: Insufficient resources/capacity to enable vehicle factories: '..groundState.enabled..'/'..groundState.available..', '..RV.ToString(includingGroundFactoryTotalCosts)..'/'..RV.ToString(portProductionPerRound));
						end;


						if (ProductionTable.IsNotEmpty(tab)) then
							local choice = ProductionTable.GetRandomChoice(tab);
							if (choice.isSuborbitalPlane) then
								support = support.Inc(EnableFactory(i,port,Game.AI.GetBasePlaneFactories(i),choice,enabled));
								airState.enabled = airState.enabled + 1;
							else
								support = support.Inc(EnableFactory(i,port,Game.AI.GetBaseVehiclePlants(i),choice,enabled));
								groundState.enabled = groundState.enabled + 1;
							end;
							loop = true;
						end;
						if not loop then
							Game.Log('AI: Unable to enqueue production. No capacity left');
							break;
						end;
					end;
				end;
			end;
		end;
		Game.Log('AI: Nothing left to do on pole '..i);

	end;


end;

