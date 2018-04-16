#include <global_root.h>
#include "entity.h"
#include "turretBase.h"
#include "../grid/worldData.h"

#include "../runtimeConfig.h"

#ifdef HAS_WINDOW
	//#include "gridAndChoices.h"
	#include "satellite.h"
#endif
#ifndef CLIENT
	#include "faction.h"
	#include "gridAndChoices.h"
#endif
#include "vehicle.h"
#include "structure.h"
#include "faction.h"


namespace Objects
{


	Ctr::GenericHashTable<TUniqueID,PWorldEntity>	idMap;




	#ifdef HAS_WINDOW


		float GetLowLighting(const Grid::LCellIdI&location)
		{
			return std::min(1.f, 
					2.f * 
					float(World::actual.GetStack(
						#ifdef HYBRID_GRID
							Grid::low.grid.GetCell(location.stackID).higherEquivalent
						#else
							location.stackID

						#endif
						).GetCellI(location.height).intLightLevel) 
					/ 255.f)/**0.75f + 0.25f*/;	//quite the expression, no :P
		}






	#endif

	#ifdef CLIENT
	void				WorldEntity::UpdateEMPDamage(int damageLevel, bool isCreated)
	{
		if (damageLevel == empDamageLevel)
			return;
		float t=0.f;

		if (isCreated && isInsertedIntoGrid)
			UnregisterSensory(t);

		empDamageLevel = damageLevel;

		if (isCreated && isInsertedIntoGrid)
			RegisterSensory(t);

	}
	#endif


	String	ToString(const BasicBuffer<World::Sensor>&sensors)
	{
		static StringBuffer buffer;
		buffer.Clear();
		buffer << "["<<sensors.Count()<<"]{";
		bool first = true;
		foreach (sensors,sensor)
		{
			if (!first)
				buffer << ',';
			first = false;
			buffer << ToString(sensor->gridCenter)<<"_r="<<sensor->radius;
		}
		buffer << '}';
		return buffer.ToStringRef();
	}

	WorldEntity::~WorldEntity()
	{
		//#ifdef CLIENT
		//	unplacedIDMap.Unset(id);
		//#endif
	}

	/*virtual*/ bool				WorldEntity::IsIndestructible() const
	{
#ifndef CLIENT
		return RuntimeConfig::tutorialMode;
#else
		return false;
#endif
	};


	#ifdef HAS_WINDOW
		bool				WorldEntity::TestIntersection(const Geometry&geo, const M::TVec3<>&_b, const M::TVec3<>&_d, float & distance) const
		{
			M::TVec3<> b,d;//,test;
			Mat::Transform(inverse,_b,b);
			Mat::rotate(inverse,_d,d);
			//Vec::normalize0(d);
			return geo.TestIntersection(b,d,distance);
		}
	#endif


	
	String						BaseEntity::GetName() const
	{
		if (!this)
			return "<NULL>";
		//#ifdef _DEBUG
		//	return GetTypeName() + " " + id.ToString()+" @"+String((void*)this);
		//#else
			return GetTypeName() + " " + ToString(id);
//		#endif
	}

	String						WorldEntity::GetNameAndOwner() const
	{
		if (!this)
			return "<NULL>";
		return GetName()+"["+(owner ? owner->GetName() : "<NULL>")+"]";
	}


	#ifdef CLIENT
		void					BaseEntity::UpdateNiceName()
		{
			cachedNiceName = 
				#ifdef _DEBUG
					GetTypeName() + " '" + id.ToNiceString()+"' @"+String((void*)this);
				#else
					GetTypeName() + " '" + id.ToNiceString()+'\'';
				#endif
			
		}
	#endif


	#ifdef CLIENT
		const String&
	#else
		String
	#endif
						BaseEntity::GetNiceName() const
	{
		static const String null = "<NULL>";
		if (!this)
			return null;
		#ifdef CLIENT
			return cachedNiceName;
		#else
			#ifdef _DEBUG
				return GetTypeName() + " '" + id.ToNiceString()+"' @"+String((void*)this);
			#else
				return GetTypeName() + " '" + id.ToNiceString()+'\'';
			#endif
		#endif
	}

	ImpactDetails		WorldEntity::TraceTrajectory(const TrajectoryEndPoint&dest, bool traceInActualWorld, Ctr::Buffer0<M::TVec4<> >&outSteps) const
	{
		float zOffset;
		PConstTurretBase b = GetTurretBase(zOffset);
		if (!b)
		{
			outSteps.Clear();
			return ImpactDetails{};
		}

		const World::Data&visibleWorld =
		#ifndef CLIENT
			owner->knownWorld
		#else
			World::actual
		#endif
		;

		return Component::Turret::TraceTrajectory(b->turret->trajectoryType,b->GetFlatAttackRange(),visibleWorld, World::actual,traceInActualWorld, TrajectoryEndPoint( location,zOffset ),dest,outSteps);
	}

	ImpactDetails		WorldEntity::TraceTrajectory(const TrajectoryEndPoint&dest, bool traceInActualWorld) const
	{
		float zOffset;
		PConstTurretBase b = GetTurretBase(zOffset);
		if (!b)
		{
			return ImpactDetails{};
		}
		const World::Data&visibleWorld =
		#ifndef CLIENT
			owner->knownWorld
		#else
			World::actual
		#endif
		;

		return Component::Turret::TraceTrajectory(b->turret->trajectoryType,b->GetFlatAttackRange(),visibleWorld, World::actual,traceInActualWorld, TrajectoryEndPoint(location,zOffset),dest);
	}


	

	ImpactDetails		WorldEntity::TraceTrajectoryFrom(const Grid::LCellIdI&source, const TrajectoryEndPoint&dest, bool traceInActualWorld, Ctr::Buffer0<M::TVec4<> >&outSteps) const
	{
		float zOffset;
		PConstTurretBase b = GetTurretBase(zOffset);
		if (!b)
		{
			outSteps.Clear();
			return ImpactDetails{};
		}
		const World::Data&visibleWorld =
		#ifndef CLIENT
			owner->knownWorld
		#else
			World::actual
		#endif
		;
		return Component::Turret::TraceTrajectory(b->turret->trajectoryType,b->GetFlatAttackRange(),visibleWorld, World::actual, traceInActualWorld,TrajectoryEndPoint(source,zOffset),dest,outSteps);
	}

	ImpactDetails		WorldEntity::TraceTrajectoryFrom(const Grid::LCellIdI&source, const TrajectoryEndPoint&dest, bool traceInActualWorld) const
	{
		float zOffset;
		PConstTurretBase b = GetTurretBase(zOffset);
		if (!b)
		{
			return ImpactDetails{};
		}
		const World::Data&visibleWorld =
		#ifndef CLIENT
			owner->knownWorld
		#else
			World::actual
		#endif
		;
		return Component::Turret::TraceTrajectory(b->turret->trajectoryType,b->GetFlatAttackRange(),visibleWorld, World::actual,traceInActualWorld, TrajectoryEndPoint(source,zOffset),dest);
	}

	bool						WorldEntity::CanHitFrom(const Grid::LCellIdI&origin, const TrajectoryEndPoint&dest, float&outAngle) const
	{
		float zOffset;
		PConstTurretBase b = GetTurretBase(zOffset);
		if (!b)
		{
			return false;
		}
		return Component::Turret::CanHit(b->turret->trajectoryType,b->GetFlatAttackRange(),TrajectoryEndPoint(origin,zOffset),dest,outAngle);
	}

	bool						WorldEntity::CanHitFrom(const Grid::LCellIdI&origin, const TrajectoryEndPoint&dest) const
	{
		float zOffset;
		PConstTurretBase b = GetTurretBase(zOffset);
		if (!b)
		{
			return false;
		}
		return Component::Turret::CanHit(b->turret->trajectoryType,b->GetFlatAttackRange(),TrajectoryEndPoint(origin,zOffset),dest);
	}


	bool						WorldEntity::CanHit(const TrajectoryEndPoint&dest, float&outAngle) const
	{
		return CanHitFrom(location,dest,outAngle);
	}
	bool						WorldEntity::CanHit(const TrajectoryEndPoint&dest) const
	{
		return CanHitFrom(location,dest);
	}


	/*static*/ float				WorldEntity::StaticEMPModify(float raw, float min, int empDamageLevel)
	{
		if (raw <= min)
			return raw;
		const float window = raw - min;
		return min + window * Grid::powi(0.25f,(UINT)empDamageLevel);
	}
	

	float		WorldEntity::EMPModify(float raw, float min) const
	{
		return StaticEMPModify(raw,min,empDamageLevel);
	}

	/*virtual*/ float		WorldEntity::GetScanningRange() const
	{
		return EMPModify(GetTable()->maxScanningRange.GetValue(GetResearchState()),2);
	}

	/*virtual*/ float3		WorldEntity::GetScanningCenter(const Grid::LCellIdI&loc) const
	{
		return Grid::low.cells[loc.stackID].cornerCenter * Grid::ToRadiusf(0.5f + loc.height);
	}

	/*virtual*/ int			WorldEntity::GetConstructionCost() const
	{
		return GetTable()->constructionCost.GetValue(GetResearchState());
	}

	/*virtual*/ int			WorldEntity::GetMaxHealth() const
	{
		return GetTable()->maxHealth.GetValue(GetResearchState());
	}

	/*virtual*/ String		WorldEntity::DescribeMaxHealth() const
	{
		return GetTable()->maxHealth.Describe(GetResearchState());
	}




	const Research::Attributes::Table& WorldEntity::GetResearchState() const
	{
		return owner ? owner->researchState.attributeTable : Research::globalState.zeroTable;
	}


	/*virtual*/ String		WorldEntity::DescribeScanningRange() const
	{
		return GetTable()->maxScanningRange.Describe(GetResearchState());
	}

	//bool		WorldEntity::CanSee(const PWorldEntity&other) const
	//{
	//	return Vec::quadraticDistance(system.w.xyz,other->system.w.xyz) <= M::Sqr(GetScanningRange());
	//}

	//bool		WorldEntity::CanSee(const M::TVec3<>&center, float radius)	const
	//{
	//	return Vec::quadraticDistance(system.w.xyz,center) <= M::Sqr(GetScanningRange()+radius);
	//}



	/*virtual*/ void	WorldEntity::OnInsert( float&atTime, bool freshConstruction )
	{
		atTime += timeEpsilon;
	}

	#ifdef HAS_WINDOW

		String	WorldEntity::CompileDeploymentCount(const OwnerParameter&overrideOwner)	const
		{
			if (RuntimeConfig::UnitLimits::AnySet())
			{
				count_t cnt = 0, limit = 0;
				if (IsMobile())
				{
					Component::Turret* t = ((const MobileEntity*)this)->turret;
					if (IsVehicle())
						cnt = Vehicle::CountTurretOccurrences(overrideOwner,t);
					#ifndef NO_SATELLITES
						elif (IsSatellite())
							cnt = Satellite::CountTurretOccurrences(overrideOwner,t);
					#endif
					else
						FATAL__("Unexpected mobile entity type: "+GetTypeName());
					limit = RuntimeConfig::UnitLimits::Get(t->constName);
				}
				elif (IsStructure())
				{
					const EntityType cname = GetType();
					cnt = Structure::CountOccurrences(overrideOwner,cname);
					limit = RuntimeConfig::UnitLimits::Get(cname);
				}
				else
					FATAL__("Unexpected entity type: "+GetName());
				if (limit == 0xffff)
					return "";

				return " Deployed: "+String(cnt)+"/"+String(limit);

			}
			return "";
		}

		void WorldEntity::AppendDeploymentCount(Description&outResult, const OwnerParameter&overrideOwner)	const
		{
			if (RuntimeConfig::UnitLimits::AnySet())
			{
				String deployment = CompileDeploymentCount(overrideOwner);
				if (deployment.IsNotEmpty())
					outResult << deployment;
			}
		}

		void WorldEntity::DescribeType(Description&outResult, UINT32 descFlags)
		{
			if (descFlags & DescIncludeName)
				outResult << GetTypeName();
			if (IsIndestructible())
				outResult << "Indestructible";
			else
			{
				if (descFlags & DescIncludeMaxHealth)
				{
					if ((descFlags & DescIncludeCurrentHealth))
						outResult << "Health: " +String(this->health)+"/"+this->DescribeMaxHealth();
					else
						outResult << "Health: " + this->DescribeMaxHealth();
				}
				else
					if ((descFlags & DescIncludeCurrentHealth))
						outResult << "Health: " +String(this->health);
			}
			
			if (!owner && Faction::mine && IsAlive())	//prototype
			{
				if (!this->IsUnlockedAtState(Faction::mine->researchState.attributeTable))
				{
					outResult << "[More Research Required]";
				}
			}
			

			if (descFlags & DescIncludePrice)
				outResult << "Price: "+String(ResourceTag(Resource::Minerals)) + String(" ") + String(this->GetConstructionCost());
			if ((descFlags & DescIncludeUnitCount))
			{
				AppendDeploymentCount(outResult,owner);
			}
			outResult << "Scanning range: "+DescribeScanningRange();
			#ifndef NO_HIBERNATION
					outResult << " "+String(TrafficResourceTag()) +String(GetUplinkTraffic());

				const Hibernating*h = ToHibernating();
				if (h && h->GetUplinkPriority())
					outResult.lines.Last() += ", Priority: "+String(h->GetUplinkPriority());
			#endif

			PTurretBase b = this->GetTurretBase();
			if (b)
			{
				if (b->turret->CanAttack())
				{
					outResult << "Hrz. Attack range: "+b-> DescribeFlatAttackRange()+" ("+b->turret->trajectoryType.ToString()+")";
				}
				b->turret->DescribeType(outResult,true,  false,  IF_SATELLITES( IsSatellite() ? "Satellite" : ) (IsVehicle() ? "Vehicle":"Tower"),owner);
			}
		}
	#endif

	bool			WorldEntity::ClearOccupiedTerrain(World::Data&world, float atTime) const
	{
		#ifdef HYBRID_GRID
			const UINT32 higher = Grid::low.grid.GetCell(location.stackID).higherEquivalent;
			const auto&n = Grid::high.grid.GetCell(higher).neighborhood;
		#else
			const UINT32 higher = location.stackID;
		#endif
		auto seq = world.BeginChangeSequence(atTime);
		using StringConversion::ToString;
		const int height = GetHeight();
		const bool forceRemoveAll = this->IsIndestructible();
		#ifdef HYBRID_GRID
			for (int j = 0; j < n.numNeighbors; j++)
			{
				seq.SelectStack(n.neighborID[j]);
				for (int a = 0; a < height; a++)
				{
					if (!forceRemoveAll && seq.GetCellI(location.height+a).content == Grid::Content::Lava)
					{
						LogEvent(EventSignificance::Common,this,"Lava encountered in cell "+ToString(Grid::HCellIdI(n.neighborID[j],location.height+a)));
						return false;
					}
					if (seq.Clear(location.height+a))
						LogEvent(EventSignificance::Common,this,"Clearing cell "+ToString(Grid::HCellIdI(n.neighborID[j],location.height+a)));
				}
			}
		#endif
		seq.SelectStack(higher);
		for (int a = 0; a < height; a++)
		{
			if (!forceRemoveAll && seq.GetCellI(location.height+a).content == Grid::Content::Lava)
			{
				LogEvent(EventSignificance::Common,this,"Lava encountered in cell "+ToString(Grid::HCellIdI(higher,location.height+a)));
				return false;
			}
			if (seq.Clear(location.height+a))
				LogEvent(EventSignificance::Common,this,"Clearing cell "+ToString(Grid::HCellIdI(higher,location.height+a)));
		}
		return true;
	}


	bool			WorldEntity::ClearOccupiedTerrain(float atTime) const
	{
		return ClearOccupiedTerrain(World::actual,atTime);
	}



	#ifndef CLIENT

	
	

		///*static*/ void WorldEntity::MapVisibilityBetween(const PWorldEntity&e, const PWorldEntity&other, float&atTime)
		//{
		//	if (e == other || e->owner == other->owner)
		//		return;
		//	if (e->IsDeadOrGhost())
		//	{
		//		LogEvent(EventClass::Preset::error,e.get(),"Trying to call MapVisibilityBetween() on a dead entity.");
		//		//this happens due to some rare cascading events where the destruction of some structure leads to the ownership change of a nexus port, which in turn redetects visibility,
		//		// but the destroyed structure has not yet been removed from lists. moving OnDestroy down now but this fix remains until it does no longer occur
		//		return;
		//	}
		//	if (e->CanSee(other))
		//	{
		//		//seeing:
		//		/*bool seeingNewEntity = true;
		//		foreach (other->seenBy,s)
		//			if ((*s)->owner == e->owner)
		//			{
		//				seeingNewEntity = false;
		//				break;
		//			}*/
		//		e->seeing << other;
		//		other->seenBy << e;
		//		//if (seeingNewEntity)
		//		{
		//			//SignalAppearanceToHostileFaction(e->owner,other,atTime);
		//			e->owner->SignalAppearance(other,atTime);
		//		}
		//	}

		//	if (other->CanSee(e))
		//	{
		//		//beeing seen:
		//		//bool beingSeenByNewOwner = true;
		//		//foreach (e->seenBy,s)
		//		//	if ((*s)->owner == other->owner)
		//		//	{
		//		//		beingSeenByNewOwner = false;
		//		//		break;
		//		//	}
		//		e->seenBy << other;
		//		other->seeing << e;
		//		//if (beingSeenByNewOwner)
		//		{
		//			//SignalAppearanceToHostileFaction(other->owner,e,atTime);
		//			other->owner->SignalAppearance(e,atTime);

		//		}
		//	}
		//}
		void		WorldEntity::OverrideSetRegisteredSensorToCurrentLocation()
		{
			registeredSensors.Clear();
			VisitSensors(location, [this](const World::Sensor&sensor)
			{
				registeredSensors << sensor;
			});
		}

	#endif
	

	void		WorldEntity::MoveSensoryToCurrentLocation(float&atTime, Ctr::Buffer0<PWorldEntity>*newContacts/* = nullptr*/)
	{
		#ifdef CLIENT
			if (!isOwnedByMe && !owner->SharingSensorWith(Faction::mine))
				return;
		#endif
		index_t at = 0;
		bool anyChanged = registeredSensors.IsEmpty();
		if (!anyChanged)
			VisitSensors(location,[this,&at,&anyChanged](const World::Sensor&s)
			{
				if (anyChanged)
					return;

				anyChanged = at >= registeredSensors.Count() || s != registeredSensors[at];
				at++;
			});
		if (!anyChanged)
			return;
		
		#ifdef CLIENT
			this->UnregisterSensory(registeredSensors,atTime);
			this->RegisterSensory(registeredSensors,location,atTime,newContacts);
		#else
			RegisteredSensorStorage newSensors;
			this->RegisterSensory(newSensors,location,atTime,newContacts);
			this->UnregisterSensory(registeredSensors,atTime);
			registeredSensors.swap(newSensors);
		#endif
	}


	/*virtual*/ void			WorldEntity::VisitSensors(const Grid::LCellIdI&loc, const std::function<void(const World::Sensor&)>&visitor, bool appearance /*= false*/) const
	{
		float range = GetScanningRange();
		static const float MinSolid = 2.f * Grid::UnitScale;
		const auto center = GetScanningCenter(loc);
		if (range > MinSolid && !appearance)
		{
			visitor(World::Sensor(center, MinSolid, World::Sensor::SolidSphere));
			visitor(World::Sensor(center, range, World::Sensor::ShadedSphere));
		}
		else
			visitor(World::Sensor(center, range, World::Sensor::SolidSphere));
	}


	void					WorldEntity::InsertSensoryInto(World::Data&target, float&atTime)
	{
		this->SyncTime(atTime);
		#ifdef CLIENT
			if (isOwnedByMe || owner->SharingSensorWith(Faction::mine))
		#endif
		VisitSensors(location, [&target, &atTime,this](const World::Sensor&sensor)
		{
			sensor.InsertInto(target, atTime);
		});
	}

	void					WorldEntity::ReapplySensory(float&atTime)
	{
		if (registeredSensors.IsEmpty())
		{
			LogEvent(EventClass::Preset::debug,this,"Cannot reapply entity sensory: None known. Registering new");
			//DBG_FATAL__("Cannot reapply entity sensory: None known");	//this happens during loading
			RegisterSensory(atTime);
			return;
		}

		this->SyncTime(atTime);
		LogEvent(EventSignificance::Common,this,"Reapplying sensory (#="+String(registeredSensors.Count())+") at time "+String(atTime));

		#ifdef CLIENT
			if (!isOwnedByMe && !owner->SharingSensorWith(Faction::mine))
				return;
		#endif
		foreach (registeredSensors,sensor)
		{
			#ifdef CLIENT
				sensor->InsertInto(World::actual, atTime);
			#else
				owner->RegisterSensorCreation(*sensor, location.height, atTime, nullptr);
			#endif
		};

	}


	void WorldEntity::RegisterSensory(RegisteredSensorStorage&sensors, const Grid::LCellIdI&loc, float&atTime, Ctr::Buffer0<PWorldEntity>*newContacts /*= nullptr*/)
	{
		#ifdef CLIENT	//special constraint applied by client: strict register->unregister->register... order
			if (!isOwnedByMe && !owner->SharingSensorWith(Faction::mine))
				return;
			if (registeredSensors.IsNotEmpty())
			{
				LogEvent(EventClass::Preset::debug,this,"Double sensor registration detected");
				DBG_FATAL__("Double sensor registration detected");
				UnregisterSensory(registeredSensors,atTime);
			}
		#endif
		sensors.Clear();

		
		this->SyncTime(atTime);
		LogEvent(EventSignificance::Debug,this,"Registering sensory ("+ToString(loc)+", "+String(GetScanningRange())+") at time "+String(atTime));
		VisitSensors(loc, [&atTime, newContacts,&sensors,this](const World::Sensor&sensor)
		{
			#ifdef CLIENT
				sensor.InsertInto(World::actual, atTime);
			#else
				if (owner->alliance->config.shareSensors)
				{
					owner->alliance->ForeachMember([&atTime,newContacts,this,sensor](const PFaction&f)
					{
						f->RegisterSensorCreation(sensor, location.height, atTime, f == owner ? newContacts : nullptr);
					},true);
				}
				else
					owner->RegisterSensorCreation(sensor, location.height, atTime, newContacts);
			#endif
			sensors << sensor;
		});
		LogEvent(EventSignificance::Debug,this,"Registered sensory ("+ToString(sensors)+") at time "+String(atTime));

	}

	void					WorldEntity::UnregisterSensory(RegisteredSensorStorage&sensors, float&atTime)
	{
		#ifdef CLIENT
			if (!isOwnedByMe && !owner->SharingSensorWith(Faction::mine))
				return;
		#endif
		if (sensors.IsEmpty())
		{
			DBG_FATAL__("Double sensor unregistration detected (now="+String(atTime)+"): "+ToString(id));
			return;	//graceful exit
		}
		#ifdef CLIENT
			if (&sensors != &registeredSensors)
			{
				DBG_FATAL__("Bad sensor unregistration detected");
				UnregisterSensory(registeredSensors,atTime);
				return;
			}
		#endif
		
		this->SyncTime(atTime);	

		LogEvent(EventClass::Preset::common,this,"Unregistering sensory ("+ToString(sensors)+") at time "+String(atTime));
		foreach (sensors,sensor)
		{
			#ifdef CLIENT
				sensor->RemoveFrom(World::actual, atTime);
			#else
				if (owner->alliance->config.shareSensors)
				{
					owner->alliance->ForeachMember([&atTime,this,sensor](const PFaction&f)
					{
						f->RegisterSensorRemoval(*sensor, location.height, atTime);
					},true);
				}
				else
					owner->RegisterSensorRemoval(*sensor, location.height, atTime);
	
			#endif
		};
		sensors.Clear();
	}

	#ifndef CLIENT
		void					WorldEntity::ReSeedRandom()
		{
			CRC32::Sequence s;
			s.AppendPOD(location);
			s.AppendPOD(RuntimeConfig::roundNumber);
			EntityType cName = GetType();
			s.AppendPOD(cName);
			UINT32 seed = s.Finish();
			//std::cout << "Reseeding "<<GetName()<<" to "<<seed<< std::endl;
			entityRandom.seed(seed);
		}
	#endif

	PWorldEntity			GetEntity(const TUniqueID&id)
	{
		PWorldEntity*p = idMap.QueryPointer(id);
		if (p)
			return *p;
		//if (includeUnplaced)
		//{
		//	WWorldEntity*w = unplacedIDMap.QueryPointer(id);
		//	if (w)
		//		return w->lock();
		//}
		return PWorldEntity();
	}

}

