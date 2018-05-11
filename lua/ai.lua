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
	if (not estimation.beingSeen and offensiveVehicle and unknownDistanceThreat < 0.2f) then
		return false;
	end;
	if (estimation.damageThreat > reference.damageThreat) then
		return true;
	end;
	if (estimation.unknownDistanceThreat == reference.unknownDistanceThreat) then
		return false;
	end;

	return estimation.unknownDistanceThreat * 0.8f > reference.unknownDistanceThreat;
end;



function EstimateDeadThreatLevelFrom(u --[[Unit]], estimateAt --[[Loc]], estimation --[[ThreatEstimation]])
	local distance = QuadraticDistance(u.location.world,estimateAt.world);
	estimation.unknownDistanceThreat = estimation.unknownDistanceThreat + 1.f / (1.f + distance);
end;

function EstimateLivingThreatLevelFrom(u --[[Unit]], estimateAt --[[Loc]], canIgnoreThreat --[[bool]], estimation --[[ThreadEstimation]])
	if (canIgnoreThreat and u.ignoreThreatRounds > 0) then
		return;
	end;
	local distance = QuadraticDistance(u.location.world,estimateAt.world);
	if (distance < Sqr(u.scanningRange)) then
		estimation.beingSeen = true;	--this is not necessarily true anymore, but a more extensive test will be added later
	end;

	if (u.turret and u.turret.CanHit(estimateAt.world)) then
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
		if my.type == 'EntityDeath' then
			EstimateDeadThreatLevelFrom(m.unitReference,estimateAt,threat);
		end;
	end;
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





function ProcessVehicle(v)
	
	local t = v.turret;
	if t == nil then
		return;
	end;
	
	if (t.detectsResources or t->can["Construct"]) then
		Game.Log("Skipping unsupported vehicle type "..v.niceName);
		return;
	end;

	local referenceSpeed = v.movementSpeed*3;
	local minMotion = v.movementSpeed;
	local attacker = t.canAttack;
	local bestScore = 0.f;

	if (attacker and t.ammoDemandPerShot > t.ammoAvailability) then
		Game.Log("Out of ammo: destroying vehicle "..v.niceName);
		v.Destroy();
		return;
	end;

	v.ClearTasks();


	local haveMarker = false;
	

	for _,m in pairs(Game.myMarkers) do
		if (m.type == "Visited") then
			local distance = Game.GetDistance(m.location,v.location);
			if (distance < 5.f) then
				m.Touch();
				haveMarker = true;
			end
		end;
	end;

	if (!haveMarker) then
		Game.CreateMarker(v,'Visited');
	end;

	local explosionRadius = t.explosionRadius;
	local expR2 = explosionRadius * explosionRadius;

	if (attacker) then
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
		if u.isOwnedByMe and QuadraticDistance(v.location.world,u.location.world) < 25.f then
			if u.turret.damageLevel > 0 then
				proximityPresence = proximityPresence+ 1;
			else
				radarProximity = radarProximity + 1;
			break;
		end;
	end;
	if not attacker and radarProximity > 1 and math.random() < 0.3f then
		--too many radars here
		v.Destroy();
		return;
	end;

	local overconfident = proximityPresence > 5;
	
	
	

	local canIgnoreThreat = (attacker or math.random() < 0.5f) and not t.dealsEMPDamage;
	
	local hunter = attacker and t.independentAttackAndMotion;
	local referenceThreat = EstimateThreatFromAll(v.location,true, canIgnoreThreat,overconfident);
	
	
	local reach = Game.GetUnitReach(v,BlockTest(overconfident,canIgnoreThreat,hunter,referenceThreat)
		,referenceSpeed,true,false,false,false);
	
	if (#reach == 0) then
		--no chance to get out of danger, so try to reduce threat level
		referenceThreat = EstimateThreatFromAll(v.location,false, canIgnoreThreat,overconfident);
		local test = BlockTest(overconfident,canIgnoreThreat,hunter,referenceThreat);
		reach = Game.GetUnitReach(v,test,referenceSpeed,true,false,false,false);
		if (#reach == 0) then
			reach = Game.GetUnitReach(v,test,referenceSpeed,false,false,false,false);
		end;
	end;

	for _,n in reach do
		local reference = n.location.world;
		local goodWeight = 0;
		local badWeight = 0;
		for _,e in Game.units do
			if not e.isOwnedByMe then
				local distance = QuadraticDistance(e.location.world,reference);
				local fc = EstimateInterest(t,e.location.height,e.constructionCost,distance);
				if (distance < expR2)
					badWeight = badWeight + 20 / (1.f + distance);
				else
					goodWeight = goodWeight + fc / (1.f + distance);
			end;
		end;
		
		for _,m in pairs(Game.myMarkers) do
			if m.type == 'HostileContact' then
				local distance = QuadraticDistance(e.location.world,reference);
				local fc = EstimateInterest(t,e.location.height,e.constructionCost,distance);
				if (distance < expR2)
					badWeight = badWeight + 20 / (1.f + distance);
				else
					goodWeight = goodWeight + fc / (1.f + distance);
			end;
		end;
		
		for _,m in pairs(Game.myMarkers) do
			if m.type == 'Visited' then
				local distance = QuadraticDistance(m.location.world,reference);
				if (distance < 49.f or (n.movementCostsFromRoot >= minMotion and distance < 16.f))
					badWeight = badWeight + 10.f / (1.f + (m.age));
				end;
			end;
		end;
		
		if (not haveMarker) then
			badWeight = badWeight + 1.f;
		end;
	
		local score = (1.f + goodWeight) / (1.f+badWeight) / (1.f + (abs(n.movementCostsFromRoot- minMotion * 3 / 2))*0.01f);
		local extent = 1.f + Sqr(Sqr(Sqr(math.random())));
		if (math.random() > 0.5) then
			extent = 1.f / extent;
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
{
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
		local damagePotentialPerRound = 0.f;
		local participants = 0;
		for _,u in pairs(Game.myUnits) do
			if (u.isMobile and u.canAttack and not u.turret.dealsEMPDamage) then
				local d2 = QuadraticDistance(u.location.world,ref);
				if (d2 < Sqr(r*1.2f)) then	--might be okay anyway
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



function	ExecuteAI()
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

	for (_,m in pairs(Game.myMarkers)) do
		if (m.type == 'Visited' and m.age > 100) then
			Game.Log("Deleting marker "..m.GetName()..". Reached end-of-life after 100 rounds");
			m.Destroy();
		end;
	end;
end;

