--[[ API:

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
		
	
]]



function Sqr(x)
	return x*x;
end;

function QuadraticDistance(u,v)
	return Sqr(u.x - v.x) + Sqr(u.y - v.y) + Sqr(u.z - v.z);
end;


function ProcessVehicle(v)
	
	local t = v.turret;
	if t == nil then
		return;
	end;
	
	if (t.detectsResources or t->can["Construct"]) then
		Game.Log("AI: Skipping unsupported vehicle type "..v.niceName);
		return;
	end;

	local referenceSpeed = v.movementSpeed*3;
	local minMotion = v.movementSpeed;
	local attacker = t.canAttack;
	local bestScore = 0.f;
	
	//Objects::Pathfinding::ReachabilityMapper::MapNode*best = nullptr;

	if (attacker and t.ammoDemandPerShot > t.ammoAvailability) then
		Game.Log("AI: Out of ammo: destroying vehicle "..v.niceName);
		v.Destroy();
		return;
	end;

	v.ClearTasks();


	local haveMarker = false;
	

	for _,m in ipairs(Game.myMarkers) do
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

		for _,u in ipairs(Game.units) do
			ProcessAttackTarget(v,u,attackTarget);
		end;
		
		for _,m in ipairs(Game.myMarkers) do
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
		for _,t in ipairs(attackTarget) do
			targets[#targets] = t.target;
		end;
		
		v.ScheduleAttacks(targets);

		if #attackTarget > 0 and v.motionIgnoresAttack then
			return;
		end;
	end;

	local proximityPresence = 0;
	local radarProximity = 0;
	for _,u in ipairs(Game.units) do
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
	BeginBlockTest(v,true,canIgnoreThreat,overconfident);	//force current to have no threat level, limiting motion to areas with zero threat
	
	local reach = Game.GetUnitReach(v,BlockTest,referenceSpeed,true,false,false,false);
	
	if (#reach == 0) then
		--no chance to get out of danger, so try to reduce threat level
		BeginBlockTest(v,false,canIgnoreThreat,overconfident);
		reach = Game.GetUnitReach(v,BlockTest,referenceSpeed,true,false,false,false);
		if (#reach == 0) then
			reach = Game.GetUnitReach(v,BlockTest,referenceSpeed,false,false,false,false);
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
		
		for _,m in ipairs(Game.myMarkers) do
			if m.type == 'HostileContact' then
				local distance = QuadraticDistance(e.location.world,reference);
				local fc = EstimateInterest(t,e.location.height,e.constructionCost,distance);
				if (distance < expR2)
					badWeight = badWeight + 20 / (1.f + distance);
				else
					goodWeight = goodWeight + fc / (1.f + distance);
			end;
		end;
		
		for _,m in ipairs(Game.myMarkers) do
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
		for _,u in ipairs(Game.myUnits) do
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
	f

	for (index_t i = 0; i < Objects::Marker::all.Count(); i++)
	{
		const Objects::Marker&m = Objects::Marker::all[i];
		switch (m.type)
		{
			case Objects::Marker::Type::HostileContact:
				if (m.age >= 10 && m.visualization && m.visualization->IsMobile())	//after 10 rounds, assume vehicle has moved
				{
					LogEvent(EventSignificance::Debug,nullptr,"AI: Deleting marker "+m.GetName()+" since hostile vehicle markers are considered outdated after 10 rounds (age="+String(m.age)+")");
					ObjectHelper::SignalMarkerDeletion(m.id);
					i--;
				}
			break;
			case Objects::Marker::Type::EntityDeath:
			{
				TThreatEstimation estimation;
				EstimateThreatFromAll(Grid::ToWorld(m.location),false,false,estimation);
				if (estimation.damageThreat > 0)	//knowing cause, dropping ghost
				{
					LogEvent(EventSignificance::Debug,nullptr,"AI: Deleting marker "+m.GetName()+" because cause of destruction is considered known");
					ObjectHelper::SignalMarkerDeletion(m.id);
					i--;
				}
			}
			break;
		}
	}


	Objects::ForeachWorldEntity([](const Objects::PWorldEntity&e)
	{
		ProcessIgnorance(*e);
	},Objects::IncludeGhosts::Living);


}

void	Process(float timeQuantum)
{
	if (done)
		return;
	LogEvent(EventSignificance::Debug,nullptr,"AI: Resuming processing ("+String(timeQuantum)+")");
	tempMarkers.Clear();
	Timer::Time started = timer.Now();
	for (index_t i = vehicles.Count() - 1; i < vehicles.Count(); i--)
	{
		ProcessVehicle(vehicles[i]);
		vehicles.EraseLast();
		if (timer.GetSecondsSince(started) > timeQuantum)
			return;
	}
	
	
	

	if (vehicles.IsEmpty())
	{
		LogEvent(EventSignificance::Debug,nullptr,"AI: Vehicle processing completed");
		
		if (!ObjectHelper::MarkerCheck::DoneThisRound())
		{
			//we could be more strict, enforcing that markers be checked before processing units, but in practice the difference should be minimal
			const bool painting = World::actual.PaintingSensorsConcurrently();
			LogEvent(EventSignificance::Debug,nullptr,"AI: Markers processing NOT completed (painting="+String(painting)+")");
			return;
		}

		done = true;
		for (index_t i = 0; i < Objects::Marker::all.Count(); i++)
		{
			const Objects::Marker&m = Objects::Marker::all[i];
			if (m.type == Objects::Marker::Type::Visited && m.age > 100)
			{
				LogEvent(EventSignificance::Debug,nullptr,"AI: Deleting marker "+m.GetName()+". Reached end-of-life after 100 rounds");

				ObjectHelper::SignalMarkerDeletion(m.id);
				i--;
			}
		}

		ASSERT__(done);
		LogEvent(EventSignificance::Debug,nullptr,"AI: all done. requesting new round");
		ApplicationInterface::SignalRoundEnd();
		
	}

}

