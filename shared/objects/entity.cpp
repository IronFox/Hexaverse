#include <global_root.h>
#include "entity.h"
#include "turretBase.h"

#include "../runtimeConfig.h"

#ifdef HAS_WINDOW
	#include "visualCommands.h"
	//#include "gridAndChoices.h"
	#include "satellite.h"
	#include "faction.h"
#endif
#ifndef CLIENT
	#include "faction.h"
	#include "gridAndChoices.h"
#endif
#include "vehicle.h"
#include "structure.h"


namespace Objects
{
	#ifdef CLIENT
		Buffer0<PWorldEntity>		waste;
	#endif


	GenericHashTable<TUniqueID,PWorldEntity,TUniqueID,Strategy::POD,Strategy::Swap>	idMap;




	#ifdef HAS_WINDOW


		float GetLowLighting(const Grid::LCellIdI&location)
		{
			return std::min(1.f, 
					2.f * 
					float(Grid::actualWorld.cellStack[
						#ifdef HYBRID_GRID
							Grid::low.grid.GetCell(location.stackID).higherEquivalent
						#else
							location.stackID

						#endif
						].GetCellI(location.height).intLightLevel) 
					/ 255.f)/**0.75f + 0.25f*/;	//quite the expression, no :P
		}




	#endif

	#ifdef CLIENT
	void				WorldEntity::UpdateEMPDamage(int damageLevel, bool isCreated)
	{
		if (damageLevel == empDamageLevel)
			return;
		float t=0.f;

		if (isMine && isCreated)
			UnregisterSensory(t);

		empDamageLevel = damageLevel;

		if (isMine && isCreated)
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

	TurretTrajectory		WorldEntity::TraceTrajectory(const Grid::WorldData&world, const Grid::LCellIdI&dest, Buffer0<TVec4<> >&outSteps, TVec3<>&impactAt, Grid::HCellIdI&impactCell) const
	{
		float zOffset;
		PConstTurretBase b = GetTurretBase(zOffset);
		if (!b)
		{
			outSteps.Clear();
			return TurretTrajectory::InvalidTrajectory;
		}
		return Component::Turret::TraceTrajectory(b->turret->trajectoryType,b->GetFlatAttackRange(),world,location,zOffset,dest,outSteps,impactAt,impactCell);
	}

	TurretTrajectory		WorldEntity::TraceTrajectory(const Grid::WorldData&world, const Grid::LCellIdI&dest, TVec3<>&impactAt) const
	{
		float zOffset;
		PConstTurretBase b = GetTurretBase(zOffset);
		if (!b)
		{
			return TurretTrajectory::InvalidTrajectory;
		}
		return Component::Turret::TraceTrajectory(b->turret->trajectoryType,b->GetFlatAttackRange(),world,location,zOffset,dest,impactAt);
	}


	

	TurretTrajectory		WorldEntity::TraceTrajectoryFrom(const Grid::WorldData&world, const Grid::LCellIdI&source, const Grid::LCellIdI&dest, Buffer0<TVec4<> >&outSteps, TVec3<>&impactAt, Grid::HCellIdI&impactCell) const
	{
		float zOffset;
		PConstTurretBase b = GetTurretBase(zOffset);
		if (!b)
		{
			outSteps.Clear();
			return TurretTrajectory::InvalidTrajectory;
		}
		return Component::Turret::TraceTrajectory(b->turret->trajectoryType,b->GetFlatAttackRange(),world,source,zOffset,dest,outSteps,impactAt,impactCell);
	}

	TurretTrajectory		WorldEntity::TraceTrajectoryFrom(const Grid::WorldData&world, const Grid::LCellIdI&source, const Grid::LCellIdI&dest, TVec3<>&impactAt) const
	{
		float zOffset;
		PConstTurretBase b = GetTurretBase(zOffset);
		if (!b)
		{
			return TurretTrajectory::InvalidTrajectory;
		}
		return Component::Turret::TraceTrajectory(b->turret->trajectoryType,b->GetFlatAttackRange(),world,source,zOffset,dest,impactAt);
	}

	bool						WorldEntity::CanHitFrom(const Grid::LCellIdI&origin, const TVec3<>&dest) const
	{
		float zOffset;
		PConstTurretBase b = GetTurretBase(zOffset);
		if (!b)
		{
			return false;
		}
		return Component::Turret::CanHit(b->turret->trajectoryType,b->GetFlatAttackRange(),origin,zOffset,dest);
	}

	bool						WorldEntity::CanHitFrom(const Grid::LCellIdI&origin, const Grid::LCellIdI&dest) const
	{
		float zOffset;
		PConstTurretBase b = GetTurretBase(zOffset);
		if (!b)
		{
			return false;
		}
		return Component::Turret::CanHit(b->turret->trajectoryType,b->GetFlatAttackRange(),origin,zOffset,dest);
	}

	bool						WorldEntity::CanHit(const TVec3<>&dest) const
	{
		return CanHitFrom(location,dest);
	}
	bool						WorldEntity::CanHit(const Grid::LCellIdI&dest) const
	{
		return CanHitFrom(location,dest);
	}



	float		WorldEntity::EMPModify(float raw, float min) const
	{
		if (raw <= min)
			return raw;
		const float window = raw - min;
		return min + window * Grid::powi(0.25f,(UINT)empDamageLevel);
	}

	float		WorldEntity::GetScanningRange() const
	{
		return EMPModify(GetMaxScanningRange(), 2.f);
	}


	//bool		WorldEntity::CanSee(const PWorldEntity&other) const
	//{
	//	return Vec::quadraticDistance(system.w.xyz,other->system.w.xyz) <= sqr(GetScanningRange());
	//}

	//bool		WorldEntity::CanSee(const TVec3<>&center, float radius)	const
	//{
	//	return Vec::quadraticDistance(system.w.xyz,center) <= sqr(GetScanningRange()+radius);
	//}



	/*virtual*/ void	WorldEntity::OnConstructionFinished( float atTime)
	{
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
					elif (IsSatellite())
						cnt = Satellite::CountTurretOccurrences(overrideOwner,t);
					else
						FATAL__("Unexpected mobile entity type: "+GetName());
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

				return " Deployed: "+String(cnt)+"/"+String(limit);

			}
			return "";
		}

		void WorldEntity::AppendDeploymentCount(BaseCommand::Label&outResult, const OwnerParameter&overrideOwner)	const
		{
			if (RuntimeConfig::UnitLimits::AnySet())
				outResult << CompileDeploymentCount(overrideOwner);
		}

		void WorldEntity::DescribeType(BaseCommand::Label&outResult, UINT32 descFlags)
		{
			if (descFlags & DescIncludeName)
				outResult << GetTypeName();
			if (IsIndestructible())
				outResult << " Indestructible";
			else
			{
				if (descFlags & DescIncludeMaxHealth)
				{
					if ((descFlags & DescIncludeCurrentHealth))
						outResult << " Health: " +String(this->health)+"/"+String( this->GetMaxHealth() );
					else
						outResult << " Health: " +String( this->GetMaxHealth() );
				}
				else
					if ((descFlags & DescIncludeCurrentHealth))
						outResult << " Health: " +String(this->health);
			}
			if (descFlags & DescIncludePrice)
				outResult << " Price: "+String(ResourceTag(Resource::Minerals)) + String(" ") + String(this->GetConstructionCost());
			if ((descFlags & DescIncludeUnitCount))
			{
				AppendDeploymentCount(outResult,owner);
			}

			outResult << " Scanning range: "+String(GetMaxScanningRange());
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
					outResult << " Hrz. Attack range: "+String(b->GetFlatAttackRange()) + "+"+String(b->turret->explosionRadius);
				outResult << "";
				b->turret->DescribeType(outResult,true, IsSatellite() ? "Satellite" : (IsVehicle() ? "Vehicle":"Tower"),owner);
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
		//		LogEvent(EventType::InternalFault,e.get(),"Trying to call MapVisibilityBetween() on a dead entity.");
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
		/*virtual*/ void WorldEntity::RegisterWorldSensory(const Grid::HighSetup::TVolumeDescriptor&desc)
		{
			Grid::actualWorld.InsertSensor(desc);

		}

		/*virtual*/ void WorldEntity::UnregisterWorldSensory(const Grid::HighSetup::TVolumeDescriptor&desc)
		{
			Grid::actualWorld.RemoveSensor(desc);
		}
	#endif


	void					WorldEntity::MoveSensory(const Grid::LCellIdI&from, const Grid::LCellIdI&to, float&atTime, bool checkChangesNow/*=true*/)
	{
		if (from == to)
			return;
		RegisterSensory(to,atTime,false);
		UnregisterSensory(from,atTime,checkChangesNow);
	}

	/*virtual override*/	void WorldEntity::RegisterSensory(const Grid::LCellIdI&loc, float&atTime, bool checkChangesNow/*=true*/, Buffer0<PWorldEntity>*newContacts /*= nullptr*/)
	{
		
		LogEvent(EventType::MinorEntityProcess,this,"Registering sensory ("+loc.ToString()+", "+String(GetScanningRange())+")");
		#ifdef CLIENT
			if (isMine)
			{
				Grid::HighSetup::TVolumeDescriptor desc;
				Grid::high.GenerateVolume(Sphere<>(Grid::ToWorld(loc),GetScanningRange()),desc);
				RegisterWorldSensory(desc);
				#ifdef THOROUGLY_DEBUG_SENSORY
					VerifySensory();
				#endif
			}

		#else
			owner->RegisterSensorCreation(Sphere<>(Grid::ToWorld(loc),GetScanningRange()),IsSatellite(), atTime,checkChangesNow, newContacts);
		#endif
	}

	/*virtual override*/	void					WorldEntity::UnregisterSensory(const Grid::LCellIdI&loc, float&atTime, bool checkChangesNow/*=true*/)
	{
		LogEvent(EventType::MinorEntityProcess,this,"Unregistering sensory ("+loc.ToString()+", "+String(GetScanningRange())+") at time "+String(atTime));
		
		#ifndef CLIENT
			owner->RegisterSensorRemoval(Sphere<>(Grid::ToWorld(loc), GetScanningRange()),IsSatellite(),atTime,checkChangesNow);
		#endif

		#ifdef CLIENT
			if (isMine)
			{
				Grid::HighSetup::TVolumeDescriptor desc;
				Grid::high.GenerateVolume(Sphere<>(Grid::ToWorld(loc),GetScanningRange()),desc);
				UnregisterWorldSensory(desc);
				#ifdef THOROUGLY_DEBUG_SENSORY
					VerifySensory();
				#endif
			}
		#endif


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
			localRandom.setSeed(seed);
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

