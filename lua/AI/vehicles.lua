#include "util.lua"
#include "queue.lua"



--[[
Context
{
	bool	selfIsOffensiveVehicle;
	bool	selfCanIgnoreThreat;
	bool	selfIsOverconfident;
	bool	selfIsScanner;
	Unit	self;
}
]]

--[[
ThreatEstimation
{
	bool	beingSeen;
	int		damageThreat;
	float	unknownDistanceThreat;
}
]]

--[[
Table-Type: Loc (Location)
{
	int		height;						-- Layer difference from mean planetary height. Usually in the range [-16,15], though some units may be located above the regular grid
	Cell	cell;						-- Horizontal cell
	Vector	world;						-- World location
	bool	isSuborbital;				-- True if this is an airborne location
}
]]


--[[
Table-Type: AAProtected
{
	Unit	target;
	AA[]	effectiveProtectors;		-- AntiAirs that can fire
	AA[]	theoreticalProtectors;		-- AntiAirs that cannot fire due to being in motion (or other reasons)
}

]]

--[[
Table-Type: AA
{
	Unit	unit;
	Loc		suborbitalCenter;
	float	effectiveProtectionRange,
			effectiveProtectionRangeSqr;
	bool	canAttack;
	AAProtected[]	protectedUnits;
}

]]

Context = {};

function Context.New(unit, canIgnoreThreat --[[bool]], overConfident --[[bool]])
	local rs = {};
	rs.selfIsOffensiveVehicle = unit.turret and unit.turret.type.canAttack and unit.turret.type.independentAttackAndMotion;
	rs.selfCanIgnoreThreat = canIgnoreThreat;
	rs.selfIsOverconfident = overConfident;
	rs.selfIsScanner = (unit.turret and unit.turret.type.name == 'Radar') or unit.type.name == 'Sensor Drone';
	rs.self = unit;
	return rs;
end;


maxHostileBomberRange = 1.0;

AA = {};

function AA.New(unit)
	local rs = {};
	rs.unit = unit;
	rs.suborbitalCenter = Util.ToSuborbit(unit.location);
	rs.effectiveProtectionRange = unit.turret.flatAttackRange - maxHostileBomberRange;
	rs.effectiveProtectionRangeSqr = Sqr(rs.effectiveProtectionRange);
	rs.canAttack = unit.turret.possibleAttacks > 0;
	rs.protectedUnits = {};
	return rs;
end;


AAProtected = {};
function AAProtected.New(unit)
	local rs = {};
	rs.target = unit;
	rs.effectiveProtectors = {};
	rs.theoreticalProtectors = {};
	return rs;
end;


aaProtected = {};	--AAProtection[]
aaUnprotected = {};	--Unit[]
aaUnits = {};	--ID -> AA[]
ofInterestToAttacker = {};	--ID -> true
destHistory = {};
threatCache = {};	--Loc -> Estimation

Work = {
	attackers = {},
	scanners = {},
	aa = {},
	processed = {},
	pending = Queue.New()
};

function Work.Clear()
	Work.attackers = {};
	Work.scanners = {};
	Work.aa = {};
	Work.processed = {};
	Work.pending = Work.New();
end;

function Work.AddMobile(unit)
	if unit.turret.type.trajectoryType == 'AAMissile' then
		Util.Push(Work.aa,unit);
	elseif unit.turret.type.canAttack then
		Util.Push(Work.attackers,unit);
	elseif (unit.turret.type.name == 'Radar'
		or unit.turret.type.name == 'Sensor Drone'
		or unit.turret.type.name == 'Surveyor')
		Util.Push(Work.scanners,unit);
	else
		Game.Log("Skipping "+unit.name);
	end;
end;



function ShouldBlockAtThreat(ctx--[[Context]], estimation--[[ThreatEstimation]], reference--[[ThreatEstimation]], ignoreUnknownDistanceThreat--[[bool]])	-- bool
	if (not estimation.beingSeen and (ctx.selfIsOffensiveVehicle or ctx.selfIsScanner) and estimation.unknownDistanceThreat < 0.2) then
		return false;
	end;
	if (estimation.damageThreat > reference.damageThreat) then
		return true;
	end;
	if (ignoreUnknownDistanceThreat) then
		return false;
	end;

	return estimation.unknownDistanceThreat * 0.8 > reference.unknownDistanceThreat;
end;



function EstimateDeadThreatLevelFrom(ctx --[[Context]], u --[[Unit]], estimateAt --[[Loc]], estimation --[[ThreatEstimation]])
	if (u.location.isSuborbital ~= ctx.self.location.isSuborbital)
		return;
	local distance = QuadraticDistance(u.location.world,estimateAt.world);
	estimation.unknownDistanceThreat = estimation.unknownDistanceThreat + 1.0 / (1.0 + distance);
end;

function EstimateLivingThreatLevelFrom(ctx --[[Context]], u --[[Unit]], estimateAt --[[Loc]], estimation --[[ThreadEstimation]])
	if (ctx.selfCanIgnoreThreat and u.ignoreThreatRounds and u.ignoreThreatRounds > 0) then
		return;
	end;
	if (u.isOwnedByMe) then
		return;
	end;
	if (Util.CheckVisibility(u, estimateAt).isVisible) then
		estimation.beingSeen = true;
	end;
	
	if (u.turret and u.turret.type.canAttack and u.turret.IsInRange(estimateAt.world)) then
		estimation.damageThreat = estimation.damageThreat + 
			u.turret.damageLevel * u.turret.attacksPerRound;
	end;
end;

function EstimateThreatFromAll(ctx --[[Context]], estimateAt --[[Loc]], friendlyDeathsOnly --[[bool]])
	local estimation = threatCache[estimateAt];
	if estimation then
		return estimation;
	end;
	
	estimation = {beingSeen = false, damageThreat = 0, unknownDistanceThreat = 0};
	if (not friendlyDeathsOnly) then
		Util.ForeachHostileIncludingGhosts(function(u)
			EstimateLivingThreatLevelFrom(ctx,u,estimateAt,estimation);
		end);
	end;
	for _,m in pairs(Game.myMarkers) do
		if m.type == 'EntityDeath' then
			EstimateDeadThreatLevelFrom(ctx,m.unitReference,estimateAt,estimation);
		end;
	end;
	threatCache[estimateAt] = estimation;
	return estimation;
end;

function BlockTest(ctx --[[Context]], referenceThreat --[[ThreatEstimation]])
	return function(loc)
		if (Game.Grid.AffectedByEMP(loc)) then
			return true;
		end;
		if (ctx.self.type.isSuborbitalPlane and loc ~= ctx.self.location and Game.AI.IsPlaneFactoryHere(loc.cell)) then
			return true;
		end;
		local estimation = EstimateThreatFromAll(ctx, loc, false);
		return ShouldBlockAtThreat(ctx, estimation,referenceThreat,false);
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
	if (u.turret and u.turret.type.canAttack) then
		rs = math.max(rs, u.turret.flatAttackRange + u.turret.explosionRadius);
	end;
	local health = u.health > 0 and u.health or u.maxHealth;
	return rs / health;
end;

function ProcessAttackTarget(source--[[Unit]], candidate--[[Unit]],  attackTarget--[[Table: AttackTarget[] ]]
		, attacksAirborne--[[bool]], attacksGroundTargets--[[bool]])
	--Game.Log("Processing attack target "..candidate.niceName);
	if (candidate.isOwnedByMe or candidate.isIndestructible) then
		return;
	end;
	if (candidate.location.isSuborbital and not attacksAirborne) then
		return;
	end;
	if (not candidate.location.isSuborbital and not attacksGroundTargets) then
		return;
	end;
	-- float distance = Vec::quadraticDistance(source.system.w.xyz,candidate.system.w.xyz);
	-- const float range = source.GetAttackRange();
	local choice = source.turret.SearchAttackTarget(candidate.location);
	if (choice ~= nil) then
		local explosionRadius = source.turret.explosionRadius;

		local rs = source.turret.TraceTrajectory(choice);

		if (rs == nil) then
			Game.Log("Cannot trace good trajectory to ".. candidate.niceName ..". Invalid trajectory");
			return;
		end;
		
		if (rs.midFlightImpact) then
			if (rs.distanceToTarget > explosionRadius) then
				Game.Log("Cannot trace good trajectory to destination. Final distance is "..tostring(rs.distanceToTarget).."/"..tostring(explosionRadius));
				return;
			end;
		end;


		--Game.Log("Trajectory checks out");
		local needAttacks = math.ceil(math.max(1,candidate.health) / source.turret.damageLevel);
		if (rs.midFlightImpact) then
			needAttacks = needAttacks + 1;
		end;
		local priority = 0.0;
		
		Util.ForeachHostileIncludingGhosts(function(u)
			priority = priority + ProcessAttackPriority(u,rs.impactAt,explosionRadius);
		end);
		
		Game.Log("Base Priority for ".. candidate.niceName .." considered "..tostring(priority)..". Will need "..tostring(needAttacks).." attack(s)");
		for i = 1,needAttacks do
			local t = {
				explosionAt = rs.impactAt,
				priority = priority / (1.0 + i*0.1),
				target = choice,
				midflightExplosion = rs.midFlightImpact
			};
			attackTarget[#attackTarget + 1] = t;
		end;

	end;
end;


function QuadraticInterestDistance(selfLoc --[[loc]], other --[[unit]], interestedInAirborne --[[bool]], interestedInGround --[[bool]])
	if (selfLoc.isSuborbital) then
		if (other.location.isSuborbital) then
			return QuadraticDistance(selfLoc.world,other.location.world);
		end;
		return QuadraticDistance(selfLoc.world, Util.ToSuborbit(other.location).world);
	else
		if (other.location.isSuborbital) then
			return QuadraticDistance(Util.ToSuborbit(selfLoc).world,other.location.world);
		end;
		return QuadraticDistance(selfLoc.world,other.location.world);
	end;
end;



function GetSameTurretBadness(myLoc, myUnit, theirUnit)
	local isAA = theirUnit.turretType and theirUnit.turretType.trajectoryType == 'AAMissile';
	if (isAA) then
		local distance = QuadraticDistance(Util.ToSuborbit(myLoc).world,Util.ToSuborbit(theirUnit.location).world);
		local minDist = Sqr(math.max(myUnit.suborbitalScanningRange,theirUnit.suborbitalScanningRange));
		if (distance > minDist)
			return 0.0;
		return 30.0 / (1.0 + distance * 0.2);
	end;
	
	local distance = QuadraticDistance(myLoc.world,theirUnit.location.world);
	local minDist = Sqr(math.max(myUnit.scanningRange,theirUnit.scanningRange));
	if (distance > minDist)
		return 0.0;
	return 30.0 / (1.0 + distance * 0.2);
end;	
	
	

function EstimateInterest(turret, constructionCost)
	local rs = constructionCost;
	if (turret and turret.type.canAttack) then
		rs = rs / (1.0 + (turret.damageLevel * turret.attacksPerRound * turret.flatAttackRange * 0.1));
	end;
	return rs * 3.0;
end;

function IsRadar(u)
	return u.turret and u.turret.type.name == 'Radar';
end;

function ProcessMobile(v)
	
	local t = v.turret;
	if t == nil then
		return;
	end;
	
	if (t.type.canConstruct) then
		Game.Log("Skipping unsupported vehicle type "..v.niceName);
		return;
	end;

	local referenceSpeed = v.movementSpeed*3;
	local minMotion = v.movementSpeed;

	local bestScore = 0.0;
	local currentScore = 0.0;
	
	local attacker = t.type.canAttack;
	local attacksAirborne = attacker and ((v.type.isSuborbitalPlane and t.type.trajectoryType == 'Missile') or t.type.trajectoryType == 'AAMissile');
	local detectesAirborne = v.type.isSuborbitalPlane or v.suborbitalScanningRange > 0;
	local attacksGroundUnits = attacker and
		(
		(not v.type.isSuborbitalPlane and t.type.trajectoryType ~= 'AAMissile')
		or (v.type.isSuborbitalPlane 
			and (t.type.trajectoryType == 'A2GMissile' or t.type.trajectoryType == 'VerticalBeam')
			)
		);
	local isAA = t.type.trajectoryType == 'AAMissile';
	local isBomber = v.type.name == 'Bomber';
	
	local interestedInAirborne = attacksAirborne or v.suborbitalScanningRange > 0 or (v.type.isSuborbitalPlane and not attacksGroundUnits);
	local interestedInGroundTargets = attacksGroundUnits or not v.type.isSuborbitalPlane or v.groundScanningRange >= 0.5 * v.scanningRange;

	if (attacker and t.ammoDemandPerShot > t.ammoAvailability) then
		Game.Log("Out of ammo: destroying vehicle "..v.niceName);
		v.Destroy();
		return;
	end;
	
	Game.Log(v.name.." "..tostring(v.health));

	v.ClearTasks();


	local haveMarker = false;
	
	if not isAA then
		for _,m in pairs(Game.myMarkers) do
			if (m.type == "Visited") then
				local distance = Game.Grid.GetDistance(m.location,v.location);
				if (distance < 5.0) then
					m.Touch();
					haveMarker = true;
				end
			end;
		end;

		if (not haveMarker) then
			Game.CreateMarker(v,'Visited');
		end;
	end;

	local explosionRadius = t.explosionRadius or 0;
	local expR2 = explosionRadius * explosionRadius;

	if (attacker and not isAA) then
		Game.Log("Processing attacker, non-AA "..v.niceName);
		local id = v.id;
		local attackTarget = {};

		Util.ForeachHostileIncludingGhosts(function(u)
			ProcessAttackTarget(v,u,attackTarget);
		end);
		
		--Game.Log("Got "..tostring(#attackTarget).." candidates");
		
		
		for i=1,#attackTarget do
			local a = attackTarget[i];
			if a.midflightExplosion then
				local ref = a.explosionAt;
				local conflict = false;
				for _,u in pairs(Game.myUnits) do
					local distance = QuadraticDistance(u.location.world,ref);
					if (distance < expR2) then
						conflict = true;
						break;
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
		for k,t in pairs(attackTarget) do
			targets[k] = t.target;
		end;
		
		v.Attack(targets);

		if #attackTarget > 0 and v.motionIgnoresAttack then
			return;
		end;
	end;

	local proximityPresence = 0;
	local radarProximity = 0;
	local aaProximity = 0;
	
	local primary = true;
	local primaryAA = true;
	
	for _,u in pairs(Game.myUnits) do
		if u ~= v and u.turret and QuadraticDistance(v.location.world,u.location.world) < 25.0 then
			if u.turret.trajectoryType == 'AAMissile' then
				if IsGreater(v,u) then
					primaryAA = false;
				end;
				aaProximity = aaProximity + 1;
			
			elseif u.turret.damageLevel > 0 then
				proximityPresence = proximityPresence + 1;
			else
				if IsGreater(v,u) then
					primary = false;
				end;
				radarProximity = radarProximity + 1;
			end;
		end;
	end;
	local overconfident = proximityPresence > 5 and not isAA;
	
	
	if not attacker and radarProximity > 0 and primary and math.random() < 0.3 then
		--too many radars here
		Game.Log("Too many radars near "..v.niceName..". Destroying radar");
		v.Destroy();
		return;
	end;

	if isAA and aaProximity > 0 and primaryAA and math.random() < 0.3 then
		--too many radars here
		Game.Log("Too many AAs near "..v.niceName..". Destroying AA");
		v.Destroy();
		return;
	end;
	
	

	local canIgnoreThreat = ((attacker and not isAA) or math.random() < 0.5) and not t.dealsEMPDamage;
	
	--local hunter = attacker and t.independentAttackAndMotion;
	local ctx = Context.New(v,canIgnoreThreat,overconfident);
	
	local referenceThreat = EstimateThreatFromAll(ctx, v.location,true);
	
	
	local reach = v.GetReach(BlockTest(overconfident,canIgnoreThreat,hunter,referenceThreat)
		,referenceSpeed,true,false,false,false);
	
	if (#reach == 0) then
		--no chance to get out of danger, so try to reduce threat level
		referenceThreat = EstimateThreatFromAll(ctx, v.location,false);
		local test = BlockTest(overconfident,canIgnoreThreat,hunter,referenceThreat);
		reach = v.GetReach(test,referenceSpeed,true,false,false,false);
		if (#reach == 0) then
			--ignore reversible
			reach = v.GetReach(test,referenceSpeed,false,false,false,false);
		end;
	end;

	Util.AddCurrentToReach(reach,v); --current location is now also in reach

	for _,n in pairs(reach) do
		local isCurrent = not n.predecessor;
		if not v.type.isSuborbitalPlane or isCurrent or n.location.isSuborbital then
		
		local goodWeight = 0;
		local badWeight = 0;
		if isAA then
			local aa = aaUnits[v.id];
			for _,prot in pairs(aa.protectedUnits) do
				if #prot.effectiveProtectors + #prot.theoreticalProtectors == 1 then --only me
					local range = QuadraticInterestDistance(Util.ToSuborbit(n.location),prot.target,true,true);
					if (range > aa.effectiveProtectionRangeSqr) then
						badWeight += 1000;
					end;
				end;
			end;
			
			for _,e in pairs(aaUnprotected) do
				local range = QuadraticInterestDistance(Util.ToSuborbit(n.location),e,true,true);
				if (range <= aa.effectiveProtectionRangeSqr) then
					goodWeight += 10.f;
				end;
			end;
		else
			Util.ForeachHostileIncludingGhosts(function(e)
				if (not e.location.isSuborbital or interestedInAirborne)
					and (e.location.isSuborbital or interestedInGroundTargets)
				then
					local distance = QuadraticInterestDistance(n.location,e,interestedInAirborne,interestedInGroundTargets);
					local fc = EstimateInterest(t,e.constructionCost);
					if (distance < expR2) then
						badWeight = badWeight + 20 / (1.0 + distance);
					else
						if not attacker then
							if not Util.IsAlive(e) then
								fc = fc * 2;
								if ofInterestToAttacker[e.id] ~= nil then
									fc = fc * 2;
								end;
							elseif ofInterestToAttacker[e.id] ~= nil then
								local vis = Util.CheckVisibility(v,e.location);
								if vis.isVisible and Game.Terrain.GetLowestSensorStrength(e.location,e.height) <= 1 then
									if distance <= vis.quadraticSensorRange then
										distance = 0;
										fc = fc * 10;		--only one seeing it. maintain visibility
									else
										fc = 0;
										badWeight = badWeight + 1000;
										return false;
									end;
								end;
							end;
						else
							ofInterestToAttacker[e.id] = true;
						end;
						goodWeight = goodWeight + fc / (1.0 + distance);
					end;
					
				end;
			end
			);
			
			for _,m in pairs(Game.myMarkers) do
				if m.type == 'Visited' then
					local distance = QuadraticDistance(m.location.world,n.location.world);
					badWeight = badWeight + 10.0 / (1.0 + (m.age) + distance * 0.2);
				end;
			end;
		
		end;
			
		if not attacker or isAA then
			for _,v2 in pairs(Work.processed) do
				if v2.turret and v2.turret.type == v.turret.type then
					badWeight = badWeight + SameTurretBadness(n.location,v2.location,v,v2,isAA);
				end;
				local directed = destHistory[v2.id];
				if directed then
					badWeight = badWeight + SameTurretBadness(n.location,directed,v,v2,isAA);
				end;
			end;
		
			Work.pending:Foreach(function(v2)
				badWeight = badWeight + SameTurretBadness(n.location,v2.location,v,v2,isAA);
			end);
		end;
			
		
		local score = (1.0 + goodWeight) / (1.0+badWeight);
		if not isAA then
			score = score / (1.0 + (math.abs(n.movementCostsFromRoot- minMotion * 3 / 2))*0.01);
		end;
		if not overconfident then
			local estimation = threatCache[n.location];
			if estimation and 
				ShouldBlockAtThreat(ctx, estimation, referenceThreat, true)
			then
				score = score + 3000 / (1.0 + n.movementCostsFromRoot);
			end;
		end;
		
		if not isAA then
			local extent = 1.0 + Sqr(Sqr(Sqr(math.random())));
			if (math.random() > 0.5) then
				extent = 1.0 / extent;
			end;
			score = score * extent;				
		end;
		
		
		if (score > bestScore) then
			best = n;
			bestScore = score;
		end;
		
		if isCurrent then
			current = n;
			currentScore = score;
		end;
	end;
	
	if (best ~= nil) then
		if not isAA or bestScore > currentScore * 2.0 then
			v.DirectTo(best);
			destHistory[v.id] = best.location;
			
			if attacker then
				for _,e in pairs(Game.hostileUnits) do
					local distance = QuadraticInterestDistance(best.location, e, interestedInAirborne, interestedInGroundTargets);
					if distance < Sqr(2.0 * v.turret.flatAttackRange) then
						ofInterestToAttacker[e.id] = true;
					end;
					distance = QuadraticInterestDistance(v.location, e, interestedInAirborne, interestedInGroundTargets);
					if distance < Sqr(2.0 * v.turret.flatAttackRange) then
						ofInterestToAttacker[e.id] = true;
					end;
				end;
			end;
		end;
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
	if (e.turret.type.canAttack and e.turret.damageLevel > 0) then
		local r = e.turret.flatAttackRange + e.turret.explosionRadius;
		local damagePotentialPerRound = 0.0;
		local participants = 0;
		for _,u in pairs(Game.myUnits) do
			if (u.type.isMobile 
				and u.turret.type.canAttack 
				and not u.turret.dealsEMPDamage
				and not u.turret.requiresStationaryVehicle
				and (
						(e.location.isSuborbital and CanAttackSuborbital(u))
					or 	(not e.location.isSuborbital and CanAttackGround(u,e.location))
					)
				and (
						(u.type.isSuborbitalPlane and CanAttackSuborbital(e))
					or	(not u.type.isSuborbitalPlane and CanAttackGround(e,u.location))
					)
				) 
				then
				
				local d2 = QuadraticInterestDistance(u.location,e,true,true);
				if (d2 < Sqr(r*1.2)) then	--might be okay anyway
					local p = GetDamagePotentialPerRound(u);
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


function	UpdateAAProtection()

	aaProtected = {};	--AAProtection[]
	aaUnprotected = {};	--Unit[]
	aaUnits = {};	--ID -> AA[]

	local allUnits = {};	--units that need protection
	
	for _,u in pairs(Game.myUnits) do
		if not u.turret then
			allUnits[#allUnits+1] = u;
		else
			if not u.type.isSuborbitalPlane then	-- ignore planes
				if u.turret.trajectoryType ~= 'AAMissile' then
					allUnits[#allUnits+1] = u;
				else
					aaUnits[u.id] = AA.New(u);
				end;
			end;
		end;
	end;
	
	for _,u in pairs(allUnits) do
		local p = nil;
		for _,aa in pairs(aaUnits) do
			local r = QuadraticInterestDistance(aa.subOrbitalCenter,u,true,true);
			if (r <= aa.effectiveProtectionRangeSqr) then
				if (not p) then
					p = AAProtected.New(u);
					Util.Push(aaProtected,p);
				end;
				if (aa.canAttack) then
					Util.Push(p.effectiveProtectors,aa);
				else
					Util.Push(p.theoreticalProtectors,aa);
				end;
				Util.Push(aa.protectedUnits,p);
			end;
		end;
		if not p then
			Util.Push(aaUnprotected,u);
		end;
	end;
end;













function ExportState()
	return nil;
end;

function Init(state)
end;


function Execute()

	ofInterestToAttacker = {};
	
	local bomberTurret = Game.Type.units['Bomber'].turret;
	maxHostileBomberRange = bomberTurret.flatAttackRange;
	for _,f in pairs(Game.factions) do
		if f.alliance ~= Game.myFaction.alliance then
			local r = bomberTurret.GetFlatAttackRange(f);
			maxHostileBomberRange = math.max(maxHostileBomberRange,r);
		end;
	end;
	Game.Log("Max hostile bomber range determined to be "..maxHostileBomberRange);
	UpdateAAProtection();
	

	for _,m in pairs(Game.myMarkers) do
		if (m.type == 'HostileContact') then
			if (m.age >= 10 and m.unitReference and m.unitReference.type.isMobile) then	--after 10 rounds, assume vehicle has moved
				Game.Log("Deleting marker "..m.id.." since hostile vehicle markers are considered outdated after 10 rounds (age="..tostring(m.age)..")");
				m.Destroy();
			end;
		elseif (m.type == 'EntityDeath') then
			local ctx = Context.New(m.unitReference,false,false);
			local estimation = EstimateThreatFromAll(ctx,m.location,false);
			if (estimation.damageThreat > 0) then	--knowing cause, dropping ghost
				Game.Log("Deleting marker "..m.id.." because cause of destruction is considered known");
				m.Destroy();
			end;
		end;
	end;
	Util.ForeachHostileIncludingGhosts(function(u)
		ProcessIgnorance(u);
	end);

	for _,m in pairs(Game.myMarkers) do
		if (m.type == 'Visited' and m.age > 100) then
			Game.Log("Deleting marker "..m.id..". Reached end-of-life after 100 rounds");
			m.Destroy();
		end;
	end;
	
	
	Work.Clear();

	for _,u in pairs(Game.myUnits) do
		if (u.type.isMobile) then
			Work.AddMobile(u);
		end;
	end;
	
	Work.pending:PushAllRight(Work.attackers);
	Work.pending:PushAllRight(Work.aa);
	Work.pending:PushAllRight(Work.scanners);

	while Work.pending:IsNotEmpty() do
		local u = Work.pending:PopLeft();
		ProcessMobile(u);
		Util.Push(Work.processed,u);
	end;
	
end;



