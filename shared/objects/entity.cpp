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
	#ifdef CLIENT
		Buffer0<PWorldEntity>		waste;
	#endif


	GenericHashTable<TUniqueID,PWorldEntity,TUniqueID,Strategy::POD,Strategy::Swap>	idMap;

	/*static*/ index_t			BaseAttributeCache::versionCounter = 0;



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

		if (isOwnedByMe && isCreated)
			UnregisterSensory(t);

		empDamageLevel = damageLevel;

		if (isOwnedByMe && isCreated)
			RegisterSensory(t);

	}
	#endif

	WorldEntity::~WorldEntity()
	{
		//#ifdef CLIENT
		//	unplacedIDMap.Unset(id);
		//#endif
	}

	#ifdef HAS_WINDOW
		bool				WorldEntity::TestIntersection(const Geometry&geo, const TVec3<>&_b, const TVec3<>&_d, float & distance) const
		{
			TVec3<> b,d;//,test;
			Mat::transform(inverse,_b,b);
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
			return GetTypeName() + " " + id.ToString();
//		#endif
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

	TurretTrajectory		WorldEntity::TraceTrajectory(const TrajectoryEndPoint&dest, Buffer0<TVec4<> >&outSteps, TVec3<>&impactAt, Grid::HCellIdI&impactCell, float&outAngle) const
	{
		float zOffset;
		PConstTurretBase b = GetTurretBase(zOffset);
		if (!b)
		{
			outSteps.Clear();
			return TurretTrajectory::InvalidTrajectory;
		}

		const World::Data&visibleWorld =
		#ifndef CLIENT
			owner->knownWorld
		#else
			World::actual
		#endif
		;

		return Component::Turret::TraceTrajectory(b->turret->trajectoryType,b->GetFlatAttackRange(),visibleWorld, World::actual,TrajectoryEndPoint( location,zOffset ),dest,outSteps,impactAt,impactCell,outAngle);
	}

	TurretTrajectory		WorldEntity::TraceTrajectory(const TrajectoryEndPoint&dest, TVec3<>&impactAt) const
	{
		float zOffset;
		PConstTurretBase b = GetTurretBase(zOffset);
		if (!b)
		{
			return TurretTrajectory::InvalidTrajectory;
		}
		const World::Data&visibleWorld =
		#ifndef CLIENT
			owner->knownWorld
		#else
			World::actual
		#endif
		;

		return Component::Turret::TraceTrajectory(b->turret->trajectoryType,b->GetFlatAttackRange(),visibleWorld, World::actual,TrajectoryEndPoint(location,zOffset),dest,impactAt);
	}


	

	TurretTrajectory		WorldEntity::TraceTrajectoryFrom(const Grid::LCellIdI&source, const TrajectoryEndPoint&dest, Buffer0<TVec4<> >&outSteps, TVec3<>&impactAt, Grid::HCellIdI&impactCell, float&outAngle) const
	{
		float zOffset;
		PConstTurretBase b = GetTurretBase(zOffset);
		if (!b)
		{
			outSteps.Clear();
			return TurretTrajectory::InvalidTrajectory;
		}
		const World::Data&visibleWorld =
		#ifndef CLIENT
			owner->knownWorld
		#else
			World::actual
		#endif
		;
		return Component::Turret::TraceTrajectory(b->turret->trajectoryType,b->GetFlatAttackRange(),visibleWorld, World::actual,TrajectoryEndPoint(source,zOffset),dest,outSteps,impactAt,impactCell,outAngle);
	}

	TurretTrajectory		WorldEntity::TraceTrajectoryFrom(const Grid::LCellIdI&source, const TrajectoryEndPoint&dest, TVec3<>&impactAt) const
	{
		float zOffset;
		PConstTurretBase b = GetTurretBase(zOffset);
		if (!b)
		{
			return TurretTrajectory::InvalidTrajectory;
		}
		const World::Data&visibleWorld =
		#ifndef CLIENT
			owner->knownWorld
		#else
			World::actual
		#endif
		;
		return Component::Turret::TraceTrajectory(b->turret->trajectoryType,b->GetFlatAttackRange(),visibleWorld, World::actual,TrajectoryEndPoint(source,zOffset),dest,impactAt);
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
		return cache.maxScanningRange.GetEMPModified(*this,GetTable()->maxScanningRange);
	}

	/*virtual*/ float3		WorldEntity::GetScanningCenter(const Grid::LCellIdI&loc) const
	{
		return Grid::low.cells[loc.stackID].centerVertex.xyz * Grid::ToRadiusf(0.5f + loc.height);
	}

	/*virtual*/ int			WorldEntity::GetConstructionCost() const
	{
		return cache.constructionCost.Get(GetResearchState(), GetTable()->constructionCost,Component::IFunc(),1);
	}

	/*virtual*/ int			WorldEntity::GetMaxHealth() const
	{
		return cache.maxHealth.Get(GetResearchState(),GetTable()->maxHealth,Component::IFunc(),1);
		//return GetTable()->maxHealth.GetValue(GetResearchState());
	}

	/*virtual*/ String		WorldEntity::DescribeMaxHealth() const
	{
		return GetTable()->maxHealth.Describe(GetResearchState());
	}




	Lua::Reference WorldEntity::GetResearchState() const
	{
		return owner ? owner->researchState.researchState : Lua::Reference();
	}


	/*virtual*/ String		WorldEntity::DescribeScanningRange() const
	{
		return GetTable()->maxScanningRange.Describe(GetResearchState());
	}

	//bool		WorldEntity::CanSee(const PWorldEntity&other) const
	//{
	//	return Vec::quadraticDistance(system.w.xyz,other->system.w.xyz) <= sqr(GetScanningRange());
	//}

	//bool		WorldEntity::CanSee(const TVec3<>&center, float radius)	const
	//{
	//	return Vec::quadraticDistance(system.w.xyz,center) <= sqr(GetScanningRange()+radius);
	//}



	/*virtual*/ void	WorldEntity::OnConstructionFinished(float&atTime)
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
					ConstName cname = GetCTypeName();
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
					outResult.lines.last() += ", Priority: "+String(h->GetUplinkPriority());
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

	#ifndef CLIENT


		///*static*/ void WorldEntity::MapVisibilityBetween(const PWorldEntity&e, const PWorldEntity&other, float&atTime)
		//{
		//	if (e == other || e->owner == other->owner)
		//		return;
		//	if (e->health <= 0)
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


	#endif
	

	#ifdef CLIENT
		void					WorldEntity::MoveSensory(const Grid::LCellIdI&from, const Grid::LCellIdI&to, float&atTime, bool checkChangesNow/*=true*/)
		{
			if (from == to)
				return;
			UnregisterSensory(from,atTime);
			RegisterSensory(to,atTime);	//MUST BE IN THIS ORDER, or sky sensory of satellites and planes breaks
		}
	#else
		/*virtual*/ void		WorldEntity::MoveSensoryToCurrentLocation(const Grid::LCellIdI&oldLocation, float&atTime, Buffer0<PWorldEntity>*newContacts/* = nullptr*/)
		{
			this->RegisterSensory(location,entityTime,newContacts);
			this->UnregisterSensory(oldLocation,entityTime);
		}

	#endif


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


	void WorldEntity::RegisterSensory(const Grid::LCellIdI&loc, float&atTime, Buffer0<PWorldEntity>*newContacts /*= nullptr*/)
	{
		this->SyncTime(atTime);	
		LogEvent(EventSignificance::Common,this,"Registering sensory ("+loc.ToString()+", "+String(GetScanningRange())+") at time "+String(atTime));
		VisitSensors(loc, [&atTime, newContacts,this](const World::Sensor&sensor)
		{
			#ifdef CLIENT
				if (isOwnedByMe)
				{
					sensor.InsertInto(World::actual, atTime);
				}
			#else
				owner->RegisterSensorCreation(sensor, location.height, atTime, newContacts);
			#endif
		});
	}

	void					WorldEntity::UnregisterSensory(const Grid::LCellIdI&loc, float&atTime)
	{
		this->SyncTime(atTime);	
		LogEvent(EventClass::Preset::common,this,"Unregistering sensory ("+loc.ToString()+", "+String(GetScanningRange())+") at time "+String(atTime));
		VisitSensors(loc, [&atTime, this](const World::Sensor&sensor)
		{
		#ifdef CLIENT
			if (isOwnedByMe)
			{
				sensor.RemoveFrom(World::actual, atTime);
			}
		#else
			owner->RegisterSensorRemoval(sensor, location.height, atTime);
		#endif
		});
	}

	#ifndef CLIENT
		void					WorldEntity::ReSeedRandom()
		{
			CRC32::beginSequence();
			CRC32::calcNext(&location,sizeof(location));
			CRC32::calcNext(&RuntimeConfig::roundNumber,sizeof(RuntimeConfig::roundNumber));
			ConstName cName = GetCTypeName();
			CRC32::calcNext(&cName,sizeof(ConstName));
			UINT32 seed = CRC32::endSequence();
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

