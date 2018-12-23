#include "destroyEntity.h"
#include <expectedBehavior.h>
#include "delayedMobileInsertion.h"
#include "../visual/hud/hexGrid.h"
#include "../visual/scene/explosions.h"

namespace Animation
{
	static float GetExplosionSize(const Objects::PWorldEntity&target)
	{
		return !target->IsStructure() && !target->IsVehicle() ? 12.f : 4.f;
	}

	static float GetExplosionTime(const Objects::PWorldEntity&target)
	{
		return Objects::ExplosionTime(GetExplosionSize(target)/4.f);
		//target->IsSatellite() ? 4.f : 1.3f;
	}


	DestroyEntity::DestroyEntity(Protocol::DestroyEntity&de):Super(de.id)
	{
		explode = de.type == Objects::RemovalType::ControlledExplosion || de.type == Objects::RemovalType::UncontrolledExplosion;
		removalType.Load(de.type);
		eventTime = de.timestamp.atTime;
//		LogEvent(EventSignificance::Debug,nullptr,"Registered removal of entity "+ToString(de.id)+" ("+Objects::RemovalType::Reinterpret(de.type).ToString()+") at t="+ToString(time));
	}


	void DestroyEntity::Explode(const Objects::PWorldEntity&target)
	{
		#ifdef HAS_WINDOW
			Explosions::InitExplosion(target->system.w.xyz + Grid::low.cells[target->location.stackID].centerVertex.xyz * Grid::GetLayerHeight(target->location.height)*0.5f, false,GetExplosionSize(target), GetExplosionTime(target));
		#endif
	}
	void DestroyEntity::Explode(const Objects::PWorldEntity&target, const std::function<void()>&onQuarterDone)
	{
		#ifdef HAS_WINDOW
			Explosions::InitExplosion(target->system.w.xyz + Grid::low.cells[target->location.stackID].centerVertex.xyz *Grid::GetLayerHeight(target->location.height)*0.5f, false, GetExplosionSize(target), GetExplosionTime(target), onQuarterDone);
		#else
			onQuarterDone();
		#endif
	}

	/*virtual override*/ void DestroyEntity::Begin( bool instant )
	{
		allocatedTarget = Objects::GetEntity(targetEntityID);
		if (allocatedTarget)
		{
			if (explode)
				Explode(allocatedTarget);
//			LogEvent(EventSignificance::Common,allocatedTarget.get(),  "Removing entity during animation (@"+String(allocatedTarget.get())+", health "+String(allocatedTarget->health)+") at time "+String(currentTime)+"/"+ToString(this->time));
			//if (RuntimeConfig::phase == RuntimeConfig::Phase::AnimationPhase)
			{
				Objects::PVehicle v = To<Objects::Vehicle>::DynamicCast(allocatedTarget);
				if (v)
				{
					if (!insertIntoGridLater.Unset(v->id) && removalType != Objects::RemovalType::DestroyedInStorage)
						Objects::RemoveFromGrid(v,v->location);
				}
			}
			if (allocatedTarget->IsMobile() && allocatedTarget->owner->SharesSensorsWithMe())
			{
				Objects::PMobileEntity v = To<Objects::MobileEntity>::Cast(allocatedTarget);
				float t = 0.f;
				if (removalType != Objects::RemovalType::DestroyedInStorage)
					v->MoveSensoryToCurrentLocation(t);
			}

			float t = 0;
			Objects::Remove(allocatedTarget,Objects::RemovalType::Reinterpret(removalType),t);
			#ifdef HAS_WINDOW
				if (!allocatedTarget->isOwnedByMe)
					HUD::HexGrid::RefreshContacts();
				HUD::WindowStack::SignalSituationChange();
				if (removalType == Objects::RemovalType::UncontrolledExplosion)
					HUD::HexGrid::EventLog::RegisterEntityDestruction(eventTime, allocatedTarget->owner->IsAlliedWithMe(),allocatedTarget->GetWorldPosition(), allocatedTarget->id, allocatedTarget->GetNiceName()+" ("+allocatedTarget->owner->GetName()+")");

			#endif
		}
		else
		{
			LogEvent(EventClass::Preset::internalError,"Unable to allocate target for DestroyEntity "+ToString(targetEntityID));
			DBG_FATAL__("Unable to allocate target for DestroyEntity "+ToString(targetEntityID));
		}
	}

	/*virtual override*/ D::String		DestroyEntity::Describe() const
	{
		String e;
		if (allocatedTarget != nullptr)
			e = "@"+String(allocatedTarget.get())+", health "+String(allocatedTarget->health);
		else
			e = ToString(targetEntityID);
		return "DestroyEntity("+e+","+removalType.ToString()+")";
	}


}
