--[[ API:

Check General Lua API Documentation.txt for general purpose functions and tables

]]



function Sqr(x)
	return x*x;
end;

function QuadraticDistance(u,v)
	return Sqr(u.x - v.x) + Sqr(u.y - v.y) + Sqr(u.z - v.z);
end;

--[[
TheatEstimation
{
	bool	beingSeen;
	int		damageThreat;
	float	unknownDistanceThreat;
}
]]

function ShouldBlockAtThreat(offensiveVehicle--[[bool]], estimation--[[ThreatEstimation]], reference--[[ThreatEstimation]])	-- bool
	if (not estimation.beingSeen and offensiveVehicle and estimation.unknownDistanceThreat < 0.2) then
		return false;
	end;
	if (estimation.damageThreat > reference.damageThreat) then
		return true;
	end;
	if (estimation.unknownDistanceThreat == reference.unknownDistanceThreat) then
		return false;
	end;

	return estimation.unknownDistanceThreat * 0.8 > reference.unknownDistanceThreat;
end;



function EstimateDeadThreatLevelFrom(u --[[Unit]], estimateAt --[[Loc]], estimation --[[ThreatEstimation]])
	local distance = QuadraticDistance(u.location.world,estimateAt.world);
	estimation.unknownDistanceThreat = estimation.unknownDistanceThreat + 1.0 / (1.0 + distance);
end;

function EstimateLivingThreatLevelFrom(u --[[Unit]], estimateAt --[[Loc]], canIgnoreThreat --[[bool]], estimation --[[ThreadEstimation]])
	if (canIgnoreThreat and u.ignoreThreatRounds and u.ignoreThreatRounds > 0) then
		return;
	end;
	local distance = QuadraticDistance(u.location.world,estimateAt.world);
	if (distance < Sqr(u.scanningRange)) then
		estimation.beingSeen = true;	--this is not necessarily true anymore, but a more extensive test will be added later
	end;

	if (u.turret and u.turret.canAttack and u.turret.IsInRange(estimateAt.world)) then
		estimation.damageThreat = estimation.damageThreat + 
			u.turret.damageLevel * u.turret.attacksPerRound;
	end;
end;

function EstimateThreatFromAll(estimateAt --[[Loc]], ignoreHostiles --[[bool]], canIgnoreThreat --[[bool]])
	local threat = {beingSeen = false, damageThreat = 0, unknownDistanceThreat = 0};
	if (not ignoreHostiles) then
		for _,u in pairs(Game.units) do
			EstimateLivingThreatLevelFrom(u,estimateAt,canIgnoreThreat,threat);
		end;
		for _,m in pairs(Game.myMarkers) do
			if m.type == 'HostileContact' then
				EstimateLivingThreatLevelFrom(m.unitReference,estimateAt,canIgnoreThreat,threat);
			end;
		end;
	end;
	for _,m in pairs(Game.myMarkers) do
		if m.type == 'EntityDeath' then
			EstimateDeadThreatLevelFrom(m.unitReference,estimateAt,threat);
		end;
	end;
	return threat;
end;

function BlockTest(overconfident,canIgnoreThreat,hunter,referenceThreat)
	return function(loc)
		if (Game.Grid.AffectedByEMP(loc)) then
			return true;
		end;
		local estimation = EstimateThreatFromAll(loc,overconfident,  canIgnoreThreat);
		return ShouldBlockAtThreat(hunter,estimation,referenceThreat);
	end;
end;

--[[
AttackTarget
{
	Loc 		target;
	Vector		explosionAt;
	float		priority;
	bool		midflightExplosion = false;

	bool		operator<(const TAttackTarget&other) const
	{
		return priority > other.priority;	//higher first
	}
};
]]

function ProcessAttackPriority(u--[[Unit]],explosionCenter--[[Vector]],maxExplosionRadius--[[float]])	--> float
	if (u.isOwnedByMe or QuadraticDistance(explosionCenter,u.location.world) > Sqr(maxExplosionRadius)) then
		return 0.0;
	end;
	local rs = u.scanningRange;
	if (u.turret and u.turret.canAttack) then
		rs = math.max(rs, u.turret.flatAttackRange + u.turret.explosionRadius);
	end;
	local health = u.health > 0 and u.health or u.maxHealth;
	return rs / health;
end;

function ProcessAttackTarget(source--[[Unit]], candidate--[[Unit]],  attackTarget--[[Table: AttackTarget[] ]])
	if (candidate.isOwnedByMe or candidate.isIndestructible) then
		return;
	end;
	-- float distance = Vec::quadraticDistance(source.system.w.xyz,candidate.system.w.xyz);
	-- const float range = source.GetAttackRange();
	local choice = source.turret.FindTarget(candidate.location);
	if (choice ~= nil) then
		local explosionRadius = source.turret.explosionRadius;

		local rs = source.turret.TraceTrajectory(choice);

		if (rs == nil) then
			Game.Log("Cannot trace good trajectory to destination. Invalid trajectory");
			return;
		end;
		
		if (rs.midFlightImpact) then
			if (rs.distanceToTarget > explosionRadius) then
				Game.Log("Cannot trace good trajectory to destination. Final distance is "..tostring(rs.distanceToTarget).."/"..tostring(explosionRadius));
				return;
			end;
		end;


		Game.Log("Trajectory checks out");
		local needAttacks = math.ceil(math.max(1,candidate.health)) / source.turret.damageLevel;
		if (rs.midFlightImpact) then
			needAttacks = needAttacks + 1;
		end;
		local priority = 0.0;
		for _,u in pairs(Game.units) do
			priority = priority + ProcessAttackPriority(u,rs.impactAt,explosionRadius);
		end;
		for _,m in pairs(Game.myMarkers) do
			if (m.type == 'EntityDeath') then
				priority = priority + ProcessAttackPriority(m.unitReference,rs.impactAt,explosionRadius);
			end;
		end;
		Game.Log("Base Priority considered "..tostring(priority));
		for i = 1,needAttacks do
			local t = {
				explosionAt = rs.impactAt,
				priority = priority / (1.0 + i*0.1),
				target = choice,
				midflightExplosion = rs.midFlightImpact
			};
			attackTarget[#attackTarget] = t;
		end;
	end;
end;

function EstimateInterest(turret, constructionCost)
	local rs = constructionCost;
	if (turret and turret.canAttack) then
		rs = rs / (1.0 + (turret.damageLevel * turret.attacksPerRound * turret.flatAttackRange * 0.1));
	end;
	return rs * 3.0;
end;


function ProcessVehicle(v)
	
	local t = v.turret;
	if t == nil then
		return;
	end;
	
	if (t.detectsResources or t.canConstruct) then
		Game.Log("Skipping unsupported vehicle type "..v.niceName);
		return;
	end;

	local referenceSpeed = v.movementSpeed*3;
	local minMotion = v.movementSpeed;
	local attacker = t.canAttack;
	local bestScore = 0.0;

	if (attacker and t.ammoDemandPerShot > t.ammoAvailability) then
		Game.Log("Out of ammo: destroying vehicle "..v.niceName);
		v.Destroy();
		return;
	end;
	
	Game.Log(v.name.." "..tostring(v.health));

	v.ClearTasks();


	local haveMarker = false;
	

	for _,m in pairs(Game.myMarkers) do
		if (m.type == "Visited") then
			local distance = Game.GetDistance(m.location,v.location);
			if (distance < 5.0) then
				m.Touch();
				haveMarker = true;
			end
		end;
	end;

	if (not haveMarker) then
		Game.CreateMarker(v,'Visited');
	end;


	if (attacker) then
		local explosionRadius = t.explosionRadius;
		local expR2 = explosionRadius * explosionRadius;
		local id = v.id;
		local attackTarget = {};

		for _,u in pairs(Game.units) do
			ProcessAttackTarget(v,u,attackTarget);
		end;
		
		for _,m in pairs(Game.myMarkers) do
			if m.type == 'HostileContact' then
				ProcessAttackTarget(v,m.unitReference,attackTarget);
			end;
		end;
		
		for i=1,#attackTarget do
			local a = attackTarget[i];
			if a.midflightExplosion then
				local ref = a.explosionAt;
				local conflict = false;
				for u in Game.units do
					if not conflict and u.isOwnedByMe then
						local distance = QuadraticDistance(u.location.world,ref);
						if (distance < expR2) then
							conflict = true;
							break;
						end;
					end;
				end;
				if conflict then
					table.remove(attackTarget, i);
					i = i-1;
				end;
			end;
		end;
		table.sort(attackTarget, function(a,b) return a.priority > b.priority end);
		while #attackTarget > t.attacksPerRound do
			attackTarget[#attackTarget] = nil;
		end;
		
		local targets = {};
		for _,t in pairs(attackTarget) do
			targets[#targets] = t.target;
		end;
		
		v.ScheduleAttacks(targets);

		if #attackTarget > 0 and v.motionIgnoresAttack then
			return;
		end;
	end;

	local proximityPresence = 0;
	local radarProximity = 0;
	for _,u in pairs(Game.units) do
		if u.isOwnedByMe and u.turret and QuadraticDistance(v.location.world,u.location.world) < 25.0 then
			if u.turret.canAttack then
				proximityPresence = proximityPresence+ 1;
			else
				radarProximity = radarProximity + 1;
			end;
		end;
	end;
	if not attacker and radarProximity > 1 and math.random() < 0.3 then
		--too many radars here
		Game.Log("");
		v.Destroy();
		return;
	end;

	local overconfident = proximityPresence > 5;
	
	
	

	local canIgnoreThreat = (attacker or math.random() < 0.5) and not t.dealsEMPDamage;
	
	local hunter = attacker and t.independentAttackAndMotion;
	local referenceThreat = EstimateThreatFromAll(v.location,true, canIgnoreThreat,overconfident);
	
	
	local reach = Game.Pathfinding.GetUnitReach(v,BlockTest(overconfident,canIgnoreThreat,hunter,referenceThreat)
		,referenceSpeed,true,false,false,false);
	
	if (#reach == 0) then
		--no chance to get out of danger, so try to reduce threat level
		referenceThreat = EstimateThreatFromAll(v.location,false, canIgnoreThreat,overconfident);
		local test = BlockTest(overconfident,canIgnoreThreat,hunter,referenceThreat);
		reach = Game.Pathfinding.GetUnitReach(v,test,referenceSpeed,true,false,false,false);
		if (#reach == 0) then
			reach = Game.Pathfinding.GetUnitReach(v,test,referenceSpeed,false,false,false,false);
		end;
	end;

	for _,n in pairs(reach) do
		local reference = n.location.world;
		local goodWeight = 0;
		local badWeight = 0;
		for _,e in pairs(Game.units) do
			if not e.isOwnedByMe then
				local distance = QuadraticDistance(e.location.world,reference);
				local fc = EstimateInterest(t,e.constructionCost);
				if (distance < expR2) then
					badWeight = badWeight + 20 / (1.0 + distance);
				else
					goodWeight = goodWeight + fc / (1.0 + distance);
				end;
			end;
		end;
		
		for _,m in pairs(Game.myMarkers) do
			if m.type == 'HostileContact' then
				local e = m.unitReference;
				local distance = QuadraticDistance(e.location.world,reference);
				local fc = EstimateInterest(t,e.constructionCost);
				if (distance < expR2) then
					badWeight = badWeight + 20 / (1.0 + distance);
				else
					goodWeight = goodWeight + fc / (1.0 + distance);
				end;
			end;
		end;
		
		for _,m in pairs(Game.myMarkers) do
			if m.type == 'Visited' then
				local distance = QuadraticDistance(m.location.world,reference);
				if (distance < 49.0 or (n.movementCostsFromRoot >= minMotion and distance < 16.0)) then
					badWeight = badWeight + 10.0 / (1.0 + (m.age));
				end;
			end;
		end;
		
		if (not haveMarker) then
			badWeight = badWeight + 1.0;
		end;
	
		local score = (1.0 + goodWeight) / (1.0+badWeight) / (1.0 + (math.abs(n.movementCostsFromRoot- minMotion * 3 / 2))*0.01);
		local extent = 1.0 + Sqr(Sqr(Sqr(math.random())));
		if (math.random() > 0.5) then
			extent = 1.0 / extent;
		end;
		score = score * extent;				
		
		if (score > bestScore) then
			best = n;
			bestScore = score;
		end;
	end;
	
	if (best ~= nil) then
		v.DirectTo(best);
	end;
end;


function ProcessIgnorance(e)
	if (e.isOwnedByMe) then
		return;
	end;
	if e.ignoreThreatRounds == nil then
		e.ignoreThreatRounds = 0;	--tables are maintained as long as relevant, so can write here, but should consider volatile. contact lost = field reset to nil
	else
		e.ignoreThreatRounds = e.ignoreThreatRounds-1;
		if (e.ignoreThreatRounds < 0) then
			e.ignoreThreatRounds = 0;
		end;
	end;
	local ref = e.location.world;
	if (e.canAttack and e.turret.damageLevel > 0) then
		local r = e.turret.flatAttackRange + e.turret.explosionRadius;
		local damagePotentialPerRound = 0.0;
		local participants = 0;
		for _,u in pairs(Game.myUnits) do
			if (u.isMobile and u.canAttack and not u.turret.dealsEMPDamage) then
				local d2 = QuadraticDistance(u.location.world,ref);
				if (d2 < Sqr(r*1.2)) then	--might be okay anyway
					local p = u.turret.damageLevel * u.turret.attacksPerRound;
					if (not u.turret.independentAttackAndMotion) then
						p = p / 2;
					end;
					damagePotentialPerRound = damagePotentialPerRound + p;
					participants = participants + 1;
				end;
			end;
		end;

		if (participants > 1) then
			local potentialLeft = damagePotentialPerRound * (participants -1) / (participants);
			local health = e.health;
			if (e.health < 0)	then --ghost
				health = e.maxHealth;
			end;
			if (potentialLeft > health * 2) then
				e.ignoreThreatRounds = 3;
			end;
		end;
	end;
end;


function ExportState()
	return nil;
end;

function ImportState(state)
end;


function	Execute()
	for _,m in pairs(Game.myMarkers) do
		if (m.type == 'HostileContact') then
			if (m.age >= 10 and m.unitReference and m.unitReference.isMobile) then	--after 10 rounds, assume vehicle has moved
				Game.Log("Deleting marker "..m.id.." since hostile vehicle markers are considered outdated after 10 rounds (age="..tostring(m.age)..")");
				m.Destroy();
			end;
		elseif (m.type == 'EntityDeath') then
			local estimation = EstimateThreatFromAll(m.location,false,false);
			if (estimation.damageThreat > 0) then	--knowing cause, dropping ghost
				Game.Log("Deleting marker "..m.id.." because cause of destruction is considered known");
				m.Destroy();
			end;
		end;
	end;
	for _,u in pairs(Game.units) do
		ProcessIgnorance(u);
	end;

	for _,u in pairs(Game.units) do
		if (u.isMobile) then
			ProcessVehicle(u);
		end;
	end;

	for _,m in pairs(Game.myMarkers) do
		if (m.type == 'Visited' and m.age > 100) then
			Game.Log("Deleting marker "..m.GetName()..". Reached end-of-life after 100 rounds");
			m.Destroy();
		end;
	end;
end;

