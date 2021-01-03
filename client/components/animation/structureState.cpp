#include "structureState.h"
#include "../visual/objectSelection.h"
#include "../visual/objectChoices.h"
#include "../visual/scene/objectComponent.h"

#include "../network.h"
#include "../playerControlBegin.h"

#include <globalEvents.h>

#include <objects/structure/tower.h>
#include <objects/structure/mine.h>
#include <objects/packable.h>

namespace Animation
{
	/*static*/ void			StructureState::Apply(const Objects::PStructure&s, Protocol::StructureState&vs)
	{
		Objects::PWorkingStructure w = To<Objects::WorkingStructure>::DynamicCast(s);
		if (w)
		{
			w->SetResourceStorage(vs.resourceStorage);
			w->ApplyEnableChangeFromServer((vs.flags & Protocol::StructureFlags::Enabled) != 0);
			#ifndef NO_HIBERNATION
				w->Hibernate((vs.flags & Protocol::StructureFlags::Hibernating) != 0,0);
			#endif
	

			Objects::PMobileEntityProducer p = To<Objects::MobileEntityProducer>::DynamicCast(s);


			if (p)
			{
				const Protocol::ProducerState*state = vs.selective.Get1();
				if (state)
				{
					p->queue.Clear();
					p->productionDone = state->productionDone;
					if (state->gatheringPoint != Grid::LCellIdI::Invalid)
						Grid::AssertIsValid(state->gatheringPoint,CLOCATION);
					p->gatheringPoint.Set(state->gatheringPoint,Objects::IsSelected(s));
					
					foreach (state->productionTasks,t)
					{
						p->AppendTask(t->id,t->type);
					}
				}
			}

			Objects::PTower t = To<Objects::Tower>::DynamicCast(s);
			if (t)
			{
				const Protocol::TowerState*state = vs.selective.Get3();
				if (state)
				{
					t->attackActionLeft = state->attackActionLeft;
					t->attackQueue.Clear();
					foreach (state->attackTasks,at)
					{
						Grid::AssertIsHorizontallyValid(*at,CLOCATION);
						if (at->height != Objects::SubOrbital::Height)
							Grid::AssertIsVerticallyValid(*at,CLOCATION);
 						auto&task = t->AppendTask(*at);
						#ifdef HAS_WINDOW
							Objects::TowerAttackVisualization::Allocate(task).Recompile(*t,*at);
						#endif
					}
				}
			}
			else
			{
				Objects::NexusPort*n = dynamic_cast<Objects::NexusPort*>(s.get());
				if (n)
				{
					const Protocol::NexusPortState*state = vs.selective.Get2();
					if (state)
						n->resistance = *state;
				}
				else
				{
					Objects::Mine*m = dynamic_cast<Objects::Mine*>(s.get());
					if (m)
					{
						const Protocol::MineState*state = vs.selective.Get0();
						if (state)
						{
							m->SetLinkedToCore(state->linkedToCore,Network::ControlEnabled());
							m->naturalResources = state->naturalResources;
							#ifdef HAS_WINDOW
								if (Objects::selectedEntity == s)
									Objects::UpdateChoices(Network::ControlEnabled());
							#endif
						}
					}
				}
			
			}
		}
		{
			Objects::Packable*se = dynamic_cast<Objects::Packable*>(s.get());
			if (se)
			{
				se->UnpackFromArray(vs.packed);
			}
		}
		s->unstable = (vs.entityState.entityStateFlags & Protocol::EntityStateFlags::IsUnstable) != 0;
		s->ApplySleepChangeFromServer( (vs.entityState.entityStateFlags & Protocol::EntityStateFlags::IsSleeping) != 0 );
		s->ApplyUpgradingChangeFromServer( (vs.entityState.entityStateFlags & Protocol::EntityStateFlags::IsUpgrading) != 0 );


		s->health = vs.entityState.health;
		s->UpdateEMPDamage(vs.entityState.empDamageLevel);


		if (vs.entityState.t2State.IsNotEmpty())
		{
			if (!s->t2)
				s->t2.reset(new Objects::T2::Properties());
			s->t2->UnpackFromArray(vs.entityState.t2State);
		}
		else
			s->t2.reset();
		s->UpdateNiceName();

		if (vs.entityState.location != Grid::LCellIdI::Invalid && vs.entityState.location != s->location)
		{
			s->location = vs.entityState.location;	//very unusual
			s->UpdateAutomaticSystem();
			Objects::RefreshCompoundCoverage(0);
		}

		#ifndef NO_SATELLITES
			if (s->IsSkyPort())
			{
				Objects::PSatellite sat = Objects::GetSatellite(s->location.stackID);
				if (sat && sat->IsSkyHub())
				{
					Objects::PSkyHub hub = To<Objects::SkyHub>::Cast(sat);
					hub->UpdateConnections();
				}
			}
		#endif

		if (w && w->isInserted)
		{
			#ifndef ONE_COMPOUND_PER_FACTION
				if (s->compoundID = 0xFFFFFFFF)
					Objects::RefreshCompoundCoverage(0);
			#endif
			s->GetCompound().RefreshState(0);
		}

		GlobalEvents::invalidateObjectCache();	//could be affecting selected network
		#ifdef HAS_WINDOW
			if (Objects::IsSelected(s))
			{
				Objects::UpdateChoices(Network::ControlEnabled());
			}
			//HUD::HexGrid::Configure(Objects::selectedEntity);
		#endif
		if (RuntimeConfig::IsInAnyCommandPhase())
			PlayerControlBegin::CheckAutoNextRound();
	}


	StructureState::StructureState(Protocol::StructureState&s):Super(s.entityState.id)
	{
		state.swap(s);
	}

	/*virtual override*/ void StructureState::Begin( bool instant )
	{
		auto allocatedTarget = Objects::GetEntity(targetEntityID);
		if (allocatedTarget) //probably got whooped otherwise
		{
			ASSERT1__(allocatedTarget,targetEntityID);
			Apply(To<Objects::Structure>::DynamicCast(allocatedTarget),state);
		}
		else
			LogEvent(EventSignificance::NonFatalError,"Unable to update structure "+ToString(targetEntityID)+" at t="+String(currentTime));
	}

	/*virtual override*/ D::String		StructureState::Describe() const
	{
		String e;
			e = ToString(targetEntityID);
		return "StructureState("+e+")";

	}
}

