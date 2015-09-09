#include <global_root.h>
#include "entity.h"
#include "turretBase.h"
#ifdef HAS_WINDOW
	#include "visualCommands.h"
	#include "gridAndChoices.h"
	#include "satellite.h"
	#include "faction.h"
#endif
#ifndef CLIENT
	#include "faction.h"
	#include "structure.h"
#endif
#include "vehicle.h"


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
			UnregisterSensory(t,false);

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
		#ifdef _DEBUG
			return GetTypeName() + " " + id.ToString()+" @"+String((void*)this);
		#else
			return GetTypeName() + " " + id.ToString();
		#endif
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


	bool		WorldEntity::CanSee(const PWorldEntity&other) const
	{
		return Vec::quadraticDistance(system.w.xyz,other->system.w.xyz) <= sqr(GetScanningRange());
	}

	bool		WorldEntity::CanSee(const TVec3<>&center, float radius)	const
	{
		return Vec::quadraticDistance(system.w.xyz,center) <= sqr(GetScanningRange()+radius);
	}



	/*virtual*/ void	WorldEntity::OnConstructionFinished( float atTime)
	{
	}

	#ifdef HAS_WINDOW

		void WorldEntity::AppendDeploymentCount(BaseCommand::Label&outResult, const OwnerParameter&overrideOwner)	const
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

				outResult << " Deployed: "+String(cnt)+"/"+String(limit);

			}
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


		/*static*/ void WorldEntity::MapVisibilityBetween(const PWorldEntity&e, const PWorldEntity&other, float&atTime)
		{
			if (e == other || e->owner == other->owner)
				return;
			if (e->CanSee(other))
			{
				//seeing:
				/*bool seeingNewEntity = true;
				foreach (other->seenBy,s)
					if ((*s)->owner == e->owner)
					{
						seeingNewEntity = false;
						break;
					}*/
				e->seeing << other;
				other->seenBy << e;
				//if (seeingNewEntity)
				{
					//SignalAppearanceToHostileFaction(e->owner,other,atTime);
					e->owner->SignalAppearance(other,atTime);
				}
			}

			if (other->CanSee(e))
			{
				//beeing seen:
				//bool beingSeenByNewOwner = true;
				//foreach (e->seenBy,s)
				//	if ((*s)->owner == other->owner)
				//	{
				//		beingSeenByNewOwner = false;
				//		break;
				//	}
				e->seenBy << other;
				other->seeing << e;
				//if (beingSeenByNewOwner)
				{
					//SignalAppearanceToHostileFaction(other->owner,e,atTime);
					other->owner->SignalAppearance(e,atTime);

				}
			}
		}


	#endif
	

	#ifdef HAS_WINDOW
		/*virtual*/ void WorldEntity::RegisterWorldSensory()
		{
			Grid::actualWorld.InsertSensor(location,GetScanningRange());

		}

		/*virtual*/ void WorldEntity::UnregisterWorldSensory()
		{
			Grid::actualWorld.RemoveSensor(location,GetScanningRange());
		}

		/*virtual*/ void	WorldEntity::MoveWorldSensory(const Grid::LCellIdI&from, const Grid::LCellIdI&to)
		{
			Grid::actualWorld.MoveSensor(from,to,GetScanningRange());
		}


	#endif


	#ifndef CLIENT
		/*virtual*/ void		WorldEntity::RegisterFactionSensory(float atTime)
		{
			owner->RegisterSensorCreation(location,GetScanningRange(), atTime);
		}

		/*virtual*/ void		WorldEntity::UnregisterFactionSensory(float atTime)
		{
			owner->RegisterSensorRemoval(location,GetScanningRange(), atTime);
		}
		/*virtual*/ void		WorldEntity::MoveFactionSensory(const Grid::LCellIdI&from, const Grid::LCellIdI&to, float atTime)
		{
			owner->RegisterSensorMotion(from,to,GetScanningRange(), atTime);
		}

	#endif

	void WorldEntity::RegisterSensory(float&atTime)
	{
		LogEvent(EventType::MinorEntityProcess,this,"Registering sensory ("+String(GetScanningRange())+")");
		#ifndef CLIENT
			ForeachWorldEntity([this,&atTime](const PWorldEntity&other)
			{
				MapVisibilityBetween(shared_from_this(),other,atTime);
			});
		#endif

		#ifdef CLIENT
			#ifdef HAS_WINDOW
				if (isMine)
				{
					RegisterWorldSensory();
					#ifdef THOROUGLY_DEBUG_SENSORY
						VerifySensory();
					#endif
				}
			#endif

		#else
			RegisterFactionSensory(atTime);
		#endif
	}

	void					WorldEntity::UnregisterSensory(float&atTime, bool isFinal)
	{
		LogEvent(EventType::MinorEntityProcess,this,"Unregistering sensory ("+String(GetScanningRange())+") at time "+String(atTime));
		
		PWorldEntity _this = shared_from_this();
		#ifndef CLIENT
			if (isFinal)
			{
				foreach (seenBy,s)
				{
					(*s)->owner->canSee.Unset(_this);
					ASSERT__((*s)->seeing.FindAndErase(_this));
				}
			}
			foreach (seeing,s)
			{
				//SignalDisappearance(shared_from_this(),timestamp,false);
				ASSERT__((*s)->seenBy.FindAndErase(_this));
				PWorldEntity other = *s;
				bool found = false;
				foreach (other->seenBy,s2)
				{
					if ((*s2)->owner == owner)
					{
						found = true;
						break;
					}
				}
				if (!found)
				{
					PMobileEntity v = To<MobileEntity>::DynamicCast(other);
					if (v)
						atTime = std::max(atTime,v->timeAt);
					owner->SignalDisappearance(other,atTime);
				}

			}
			if (isFinal)
				seenBy.Clear(0);
			seeing.Clear(0);
		#endif

		#ifdef CLIENT
			#ifdef HAS_WINDOW
				if (isMine)
				{
					UnregisterWorldSensory();
					#ifdef THOROUGLY_DEBUG_SENSORY
						VerifySensory();
					#endif
				}
			#endif

		#else
			UnregisterFactionSensory(atTime);
		#endif


	}


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

