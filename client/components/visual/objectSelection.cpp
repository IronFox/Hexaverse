#ifdef HAS_WINDOW
#include <global_root.h>
#include "objectSelection.h"
#include "objectChoices.h"
#include <objects/interface.h>
#include <objects/gridAndChoices.h>
#include <objects/vehicle.h>
#include <objects/faction.h>
#include <objects/structure/mine.h>
#include <objects/structure/laboratory.h>
#include <globalEvents.h>

#include "hud/hexGrid.h"

namespace Objects
{

	PVehicle				selectedVehicle;
	PWorldEntity			selectedEntity;
	PMobileEntity			selectedMobileEntity;
	PMobileEntity			deployingMobileEntity;
	Observable<>				onSelectionChanged;
	bool					shiftPressed = false,shiftMeansQueue = true;



	

	count_t		CountIterateableEntities()
	{
		return
			Objects::vehicles.Count() 
			#ifndef NO_SATELLITES
				+ Objects::satellites.Count() 
			#endif
			+ Objects::structures.Count();
	}

	Objects::PWorldEntity	GetIterateableEntity(index_t i)
	{
		Objects::PWorldEntity rs;
		GetIterateableEntity(rs,i,Objects::vehicles);
		#ifndef NO_SATELLITES
			GetIterateableEntity(rs,i,Objects::satellites);
		#endif
		GetIterateableEntity(rs,i,Objects::structures);
		return rs;
	}
	
	index_t	FindIterateableEntity(const Objects::PWorldEntity&e)
	{
		index_t rs = InvalidIndex, offset=0;
		FindIterateableEntity(offset,rs,e,Objects::vehicles);
		#ifndef NO_SATELLITES
			FindIterateableEntity(offset,rs,e,Objects::satellites);
		#endif
		FindIterateableEntity(offset,rs,e,Objects::structures);
		return rs;

	}


	bool	CanHit(const Objects::PMobileEntity&m, const Grid::LCellIdI&loc)
	{
		return m->CanHit(Objects::TrajectoryEndPoint( loc ));
	//			if (source.ExtCanHit(source.owner, Objects::TrajectoryEndPoint( source.location,source.base->height),Objects::TrajectoryEndPoint( candidate.location,0 ),target))

	}


	bool	IsIdle(const Objects::PMobileEntity&v)
	{
		//okay, clarification:
		//0: if dead: NOT IDLE (duh)
		if (v->IsDeadOrGhost())
			return false;	//yes, this can happen if badly called
		//1: if hibernating: NOT IDLE, EVER
		if (v->IsHibernating())
			return false;
		//2: if automated, then some other logic takes care of everything. NOT IDLE
		if (v->isAutomated)
			return false;

		//3: if it already IS doing something, anything really, then it is NOT IDLE
		if (v->myTasks.IsNotEmpty())	//if it already is doing normal vehicle stuff, then it's not IDLE (duh)
			return false;
		const Objects::MobileEntityProducer*p =  v->CastToProducer();
		if (p && p->queue.IsNotEmpty())	//if it's a factory and has any task, then it's not IDLE
			return false;


		//4: if it's SLEEPING then it's not idle, UNLESS the local entity can attack, and has something to attack:
		if (v->IsSleeping())
		{
			#if 0
			if (v->CanAttack() && v->HasAmmoForShot() && v->CountPossibleAttacks())	//check that its turret can attack, there's ammo for the shot, and it has any shots left this round
			{
				bool emp = v->turret->dealsEMPDamage;
				if (v->IsVehicle())
				{
					bool canAttackAny = false;
					Objects::ForeachWorldEntity([emp,&canAttackAny,v](const Objects::PWorldEntity&other)
					{
						if (!canAttackAny && (!emp || !other->empDamageLevel))
							if (!other->isOwnedByMe && !other->IsIndestructible() && CanHit(v,other->location))
								canAttackAny = true;
					},false);
					if (canAttackAny)
						return true;
				}
				else
					#ifndef NO_SATELLITES
					if (v->IsSatellite())
					{
						for (auto other : Objects::satellites)
							if (!other->isOwnedByMe && !other->IsIndestructible() && CanHit(v,other->location))
								if (!emp || !other->empDamageLevel)
									return true;
					}
					else
					#endif
					{
						FATAL__("Unknown mobile entity type "+v->GetTypeName());
					}
			}
			#endif
			return false;	//cannot attack or has nothing to attack: NOT IDLE since it is SLEEPING
		}

		//not sleeping, no tasks, not producing, not automated, not hibernating:
		return true;	//yes, IDLE
	}


	bool	IsIdle(const Objects::PStructure&s)
	{
		if (s->IsDeadOrGhost())
			return false;
		#ifndef NO_HIBERNATION
			if (s->ToHibernating()->IsHibernating())
				return false;
		#endif

		if (!s->IsWorkingStructure() || s->IsSleeping())
			return false;
		Objects::PWorkingStructure ws = To<Objects::WorkingStructure>::Cast(s);

		
		//Objects::PVehiclePlant vp = To<Objects::VehiclePlant>::DynamicCast(s);
		if (s->CastToProducer())
			return (s->AwaitsUserInteraction() || /*vp->queue.IsEmpty() || */
			(!ws->IsEnabled()
			&&
			ws->CouldEnable(true)
			)
			);
		Objects::PMine mine = To<Objects::Mine>::DynamicCast(s);
		if (mine)
			return !mine->IsEnabled() && mine->CouldEnable();

		auto lab = To<Objects::Laboratory>::DynamicCast(s);
		if (lab)
		{
			if ((lab->IsEnabled() || lab->CouldEnable()) && lab->owner->researchState.IsIdle())
				return true;
			return !lab->IsEnabled() && lab->CouldEnable();
		}

		#ifndef NO_SATELLITES
			if (s->IsSkyLinkStructure())
			{
				Objects::PSkyLinkStructure st = To<Objects::SkyLinkStructure>::DynamicCast(s);	//around a inheritance corner, prolly required to use dynamic cast here
				if (!st->connectedToHub.expired())
				{
					return !To<Objects::WorkingStructure>::Cast(s)->IsEnabled();
				}
				if (s->IsSkyPort())
				{
					Objects::PSkyPort p = To<Objects::SkyPort>::Cast(s);
					if ((p->queue.IsEmpty() || !p->IsEnabled()) && p->CouldEnable(true))
						return true;
				}
			}
		#endif
		return false;

	}

	bool	IsIdle(const Objects::PWorldEntity&e)
	{
		if (e->IsStructure())
			return IsIdle(To<Objects::Structure>::Cast(e));
		if (e->IsMobile())
			return IsIdle(To<Objects::MobileEntity>::Cast(e));
		return false;
	}


	void	SelectAndFocusNextIdle(bool controlsEnabled)
	{
		if (HUD::Dialog::ConfirmCurrent(true))
			return;

		const count_t total = CountIterateableEntities();
		if ((total == 0) || !Faction::mine)
			return;
		index_t from = FindIterateableEntity(Objects::selectedEntity);
		if (from == InvalidIndex)
			from = 0;

		index_t to = from;
		do
		{
			to ++;
			if (to >= total)
				to = 0;

			Objects::PWorldEntity v = GetIterateableEntity(to);
			if (v->isOwnedByMe && IsIdle(v))
			{
				Select(v,controlsEnabled);
				return;
			}
		}
		while (to != from);

		HUD::HexGrid::RefreshButtonState(); //just in case
	}




	
	void	SelectAndFocusPrev(bool controlsEnabled)
	{
		const count_t total = CountIterateableEntities();
		if ((total == 0) || !Faction::mine)
			return;
		index_t from = FindIterateableEntity(Objects::selectedEntity);
		if (from == InvalidIndex)
			from = 0;
			
		index_t to = from;
		do
		{
			if (to > 0)
			{
				to --;
			}
			else
				to = total-1;

			Objects::PWorldEntity v = GetIterateableEntity(to);

			if (v->owner == Objects::Faction::mine)
			{
				Select(v,controlsEnabled);
				return;
			}
		}
		while (to != from);
	}

	void	SelectAndFocusNext(bool controlsEnabled)
	{
		const count_t total = CountIterateableEntities();
		if ((total == 0) || !Faction::mine)
			return;
		index_t from = FindIterateableEntity(Objects::selectedEntity);
		if (from == InvalidIndex)
			from = 0;

		index_t to = from;
		do
		{
			to ++;
			if (to >= total)
				to = 0;

			Objects::PWorldEntity v = GetIterateableEntity(to);

			if (v->owner == Objects::Faction::mine)
			{
				Select(v,controlsEnabled);
				return;
			}
		}
		while (to != from);
	}




	void					SetShiftMeansQueue(bool b)
	{
		shiftMeansQueue = b;
	}

	bool					ShiftMeansQueue()
	{
		return shiftMeansQueue;
	}

	void					SignalShiftPressed()
	{
		bool n = !shiftPressed;
		shiftPressed = true;
		if (shiftMeansQueue && n)
			HUD::HexGrid::RefreshButtonState();
	}

	void					SignalShiftReleased()
	{
		bool n = shiftPressed;
		shiftPressed = false;
		if (shiftMeansQueue && n)
			HUD::HexGrid::RefreshButtonState();
	}

	bool					ShouldQueueCommands()
	{
		return shiftPressed || !shiftMeansQueue;
	}


	void					Select(const PWorldEntity&entity, bool controlEnabled)
	{
		deployingMobileEntity.reset();
		movementChoices.Clear();
//		bool changed = selectedEntity != entity;
		PWorldEntity e = selectedEntity;
		selectedEntity = entity;
		selectedMobileEntity = To<MobileEntity>::DynamicCast(entity);
		selectedVehicle = To<Vehicle>::DynamicCast(selectedMobileEntity);
		activeCursorMode = CursorMode::Idle;

		if (e != entity)
		{
			if (e)
				e->OnDeselect();
			if (entity)
				entity->OnSelect();
		}
		
		if (e != entity)
			onSelectionChanged();
		GlobalEvents::invalidateObjectCache();
		UpdateChoices(controlEnabled);
	}

	


	void					Deselect(bool controlEnabled)
	{
		deployingMobileEntity.reset();
		movementChoices.Clear();
		PWorldEntity e = selectedEntity;
		selectedEntity.reset();	
		selectedVehicle.reset();
		selectedMobileEntity.reset();
		activeCursorMode = CursorMode::Idle;

		if (e)
			e->OnDeselect();
		HUD::HexGrid::RefreshButtonState();
		if (e)
			onSelectionChanged();
		GlobalEvents::invalidateObjectCache();
	}


	PWorldEntity					GetRayIntersection(const M::TVec3<>&b, const M::TVec3<>&d, float&distance)
	{
		PWorldEntity newSel;
		ForeachWorldEntity([b,d,&distance,&newSel](const Objects::PWorldEntity&e)
		{
			if (e->TestIntersection(b, d, distance))
			{
				newSel = e;
			}
		},true);

		return newSel;
	}

	bool					Select(const M::TVec3<>&b, const M::TVec3<>&d, float&distance,bool controlEnabled)
	{
		//if (Compound::Transfer::isActive)
			//return false;

		//selectedEntity.reset();
		PWorldEntity newSel = GetRayIntersection(b,d,distance);

		//bool success = !!selectedEntity;
		if (newSel)
		{
			Select(newSel,controlEnabled);
		}
		#ifdef HAS_WINDOW
			else
				HUD::HexGrid::RefreshButtonState();	//in UpdateChoices() otherwise
		#endif
		return !!newSel;
	}
}

#endif
