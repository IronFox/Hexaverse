#include <global_root.h>
#include "core.h"
#include <objects/gridAndChoices.h>
#include <objects/mobileEntity.h>
#include <objects/faction.h>
#include "../state.h"
#include <objects/subOrbital.h>

namespace Evolver
{


	namespace CoreDetails
	{
		Ctr::Queue<TExplosion>	pendingExplosions;

		/**
		Like vehicles, towers have an attack action floation point value. Unlike vehicles, they are not part of the
		movement phase. Therefore, once per round, this function must be called to reset their attack action.
		*/
		void			ResetTowerAttackAction(float atTime);
		/**
		Updates what satellites can see on the ground (if terrainChanged is set true), and their fuel supply.
		Unsupplied satellites may be destroyed.
		Must be called once per round.
		@param t Server time
		@param terrainChanged Set true if the terrain changed since the last invocation of this function. If in doubt pass true. 
		*/
		void			CheckOrbitalSupplyAndSensory(float&t, bool terrainChanged);




		struct TActionReference
		{
			Objects::WMobileEntity	ref;
			float			initiativeFactor;

			/**/		TActionReference(const Objects::PMobileEntity&e):ref(e)	{initiativeFactor = e->entityRandom.NextFloat(0.95f,1.f); /*std::cout << e->GetName()<<": "<< initiativeFactor << std::endl;*/}

			bool		IsAlive() const {return ref.lock() && ref.lock()->health >= 0;}

		};

	//	/*static*/ Random	TActionReference::random;


		struct TAttackReference
		{
			Objects::WTurretBase		tref;
			Objects::WWorldEntity			eref;
			float			initiativeFactor;

			/**/		TAttackReference(const Objects::PWorldEntity&we):eref(we)	{initiativeFactor = we->entityRandom.NextFloat(0.95f,1.f);/*std::cout << we->GetName()<<": "<< initiativeFactor << std::endl;*/}

			bool		IsAlive() const {Objects::PWorldEntity e = eref.lock(); return e && e->health >= 0;}

			float		GetInitiative() const {return IsAlive() ? initiativeFactor * float(tref.lock()->GetAttackInitiative()) : -1.f;}

			bool		operator>(const TAttackReference&other) const {return GetInitiative() > other.GetInitiative();}
			bool		operator<(const TAttackReference&other) const {return GetInitiative() < other.GetInitiative();}
		};

		Ctr::Buffer<TAttackReference>	attackingEntities;
		//float						attackTime = 0.f;




			
		struct TMovementReference : public TActionReference
		{
			/**/		TMovementReference(const Objects::PMobileEntity&e):TActionReference(e)	{}
			float		GetInitiative() const {return IsAlive() ? initiativeFactor * float(ref.lock()->GetMovementInitiative()) : -1.f;}

			bool		operator>(const TMovementReference&other) const {return GetInitiative() > other.GetInitiative();}
			bool		operator<(const TMovementReference&other) const {return GetInitiative() < other.GetInitiative();}
		};

		
		void			ResetTowerAttackAction(float atTime)
		{
			Objects::ForeachFaction([&atTime](const Objects::PFaction&f)
			{
				foreach (f->towers,_t)
				{
					Objects::PTower t = _t->lock();
					if (t->attackActionLeft != 1.f)
					{
						t->attackActionLeft = 1.f;
						t->SignalChange(Objects::WorldEntity::ActionChanged,atTime);
					}
				}
			});
		}


		Ctr::Buffer<TMovementReference>	movingVehicles,movingVehicles2;

		void		SignalMotionPhase()
		{
			float attackTime = 0;
			Objects::ForeachWorldEntity([&attackTime](const Objects::PWorldEntity&e)
			{
				attackTime = std::max(attackTime,e->entityTime);

			},false);

			Objects::ForeachMobileWorldEntity([attackTime](const Objects::PMobileEntity&v)
			{
				v->BeginMotionPhase(attackTime);
				if (v->IsBuilding() || v->NextTaskIsMotion())
					movingVehicles.Append(TMovementReference(v));
			},false);
		}

		bool		ProcessNextVehicleAttack()
		{
			if (attackingEntities.IsEmpty())
			{
				SignalMotionPhase();
				return false;
			}
			std::sort(attackingEntities.begin(),attackingEntities.end());

			while (attackingEntities.IsNotEmpty() &&!attackingEntities.Last().IsAlive())
				attackingEntities.EraseLast();
			if (attackingEntities.IsEmpty())
			{
				SignalMotionPhase();
				return false;
			}
			Objects::PWorldEntity e = attackingEntities.Last().eref.lock();
			Objects::PTurretBase b = attackingEntities.Last().tref.lock();
			b->Attack(e->entityTime);
			//PWorldEntity e = attackingEntities.Last().eref.lock();

			while (attackingEntities.IsNotEmpty())
			{
				b = attackingEntities.Last().tref.lock();
				if (!b || !b->NextTaskIsAttack() || b->CountPossibleAttacks() <= 0 || !b->HasAmmoForShot() || !e || e->empDamageLevel)
					attackingEntities.EraseLast();	//happened
				else
					break;
			}
	//			PVehicle v = To<Vehicle>::DynamicCast(e);
			if (attackingEntities.IsEmpty())
				SignalMotionPhase();

			return attackingEntities.IsNotEmpty();
		}


		bool ProcessVehicleMotion()
		{

			if (movingVehicles.IsEmpty())
			{
				if (movingVehicles2.IsEmpty())
					return false;
				
				movingVehicles2.swap(movingVehicles);
			}
			std::sort(movingVehicles.begin(),movingVehicles.end());

			while (movingVehicles.IsNotEmpty() && !movingVehicles.Last().IsAlive())
				movingVehicles.EraseLast();
			if (movingVehicles.IsEmpty())
			{
				return false;
			}
			Objects::PMobileEntity v = movingVehicles.Last().ref.lock();
			//bool hadMoved = v->hasMoved;
			auto rs = v->PerformNonAttack();
			switch (rs)
			{
				case Objects::MobileEntity::TaskResult::AllDoneForThisRound:
				break;
				case Objects::MobileEntity::TaskResult::FinishedCanDoMore:
				case Objects::MobileEntity::TaskResult::TryingSomething:	//this case may need special handling. let's see how it goes first
					movingVehicles2.Append(TMovementReference(v));
				break;
				default:
					FATAL__("Unexpected return value");
			}
			if (movingVehicles2.IsEmpty())
			{
				bool brk = true;
			}
			movingVehicles.EraseLast();

			return movingVehicles.IsNotEmpty() || movingVehicles2.IsNotEmpty();
		}

		void ProcessEMPState(float atTime, const std::function<bool(const Objects::PWorldEntity&)>&doHeal)
		{
			Objects::ForeachWorldEntity([doHeal,&atTime](const Objects::PWorldEntity&e)
			{
				if (doHeal(e))
					e->HealEMPDamage(atTime);
			},false);
		}


		void	BeginRoundProcessing()
		{
			Objects::Compound::lastDisabled = nullptr;
			Objects::ForeachFaction([](const Objects::PFaction&f)
			{
				f->DropPerRoundData();
			});



			Objects::RefreshCompoundCoverageIfNeeded(0);


			Objects::ForeachCompound([](Objects::Compound&c)
			{
				c.RefreshState(0);
				c.TickBegin();
				c.RefreshState(0);
			});
			Objects::ForeachFaction([](const Objects::PFaction&f)
			{
				f->statistics.SignalCompoundProcessingEnd();
				f->researchState.FinalizeRound(f);
			});

			if (Objects::Faction::nexus && Game::Config::Nexus::roundsPerResearchPoint)
			{
				if (((RuntimeConfig::roundNumber.main+1)%Game::Config::Nexus::roundsPerResearchPoint)==0)
				{
					LogEvent(EventSignificance::Common,Objects::Faction::nexus,"Advancing Nexus research state");
					Objects::Faction::nexus->researchState.AdvanceNexusResearch();
				}
			}


			Objects::ForeachWorldEntityIncludingStored([](const Objects::PWorldEntity&e)
			{
				e->ResetEntityTime();
			},false);

			Objects::ForeachMobileWorldEntity([](const Objects::PMobileEntity&v)
			{
				if (v->IsHibernating())
					return;
				v->BeginAttackPhase();

				Objects::PTurretBase b = v->GetTurretBase();
				if (!b || !b->turret->CanAttack() || !b->CountPossibleAttacks())
					return;

				//if (v->IsSentry())
				{
					if (v->myTasks.IsEmpty() || (b->turret->ignoresVehicleMotion && !v->NextTaskIsAttack()))
					{
						Objects::TurretBase::TargetChoice choice;
						while (v->FrontQueueAutoAttack(choice));
					}
				}

				if (v->NextTaskIsAttack())
				{
					if (b->HasAmmoForShot())
					{
						TAttackReference&ref = attackingEntities.Append(TAttackReference(v));
	//					ref.eref = v;
						ref.tref = b;
					}
					else
					{
						LogEvent(EventClass::Preset::commandRejected,v.get(),"Insufficient ammunition. Attacks cancelled");
						v->EraseFirstTask();
						v->RegisterChange(Objects::WorldEntity::TasksChanged,v->entityTime);
					}
					//ref.initiative = v->GetAttackInitiative();
					//ref.attacksLeft = v->CountPossibleAttacks();
				}

			},false
			);
			Objects::ForeachFaction([](const Objects::PFaction&f)
			{
				foreach (f->towers,_t)
				{
					Objects::PTower t = _t->lock();
					if (!t || t->IsHibernating())
						continue;
					t->BeginAttackPhase();

					if (t->turret->CanAttack())
					{
						if (t->attackQueue.IsEmpty())
						{
							bool any = false;

							Objects::TurretBase::TargetChoice choice;
							while (t->FrontQueueAutoAttack(choice))
								any = true;

							if (!any)
								continue;
						}
						if (!t->HasAmmoForShot())
						{
							LogEvent(EventClass::Preset::commandRejected,t.get(),"Insufficient ammunition. Attacks cancelled");
							t->attackQueue.Clear();
							t->RegisterChange(Objects::WorldEntity::TasksChanged,t->entityTime);
							continue;
						}


						TAttackReference&ref = attackingEntities.Append(TAttackReference(t));
						//ref.eref = t;
						ref.tref = t;
					}
				}
			});

			ProcessEMPState(0,[](const Objects::PWorldEntity&e)->bool
			{
				return !Grid::AffectedByEMP(e->location);
			});

			World::SimulateLiquidsIfEnabled();

			Game::Network::ChangePhase(RuntimeConfig::Phase::AttackPhase);
		}

		void ProcessStructures1(float atTime)
		{
			//#ifdef ONE_COMPOUND_PER_FACTION
				Objects::RefreshCompoundCoverageIfNeeded(atTime);
				Objects::ForeachCompound([atTime](Objects::Compound&c)
				{
					c.TickEnd(atTime);
				});
			//#endif

		}




		void CheckOrbitalSupplyAndSensory(float&t, bool terrainChanged)
		{
			#ifndef NO_SATELLITES
				Objects::RefreshSkyHubConnections();
			
				static Buffer0<Objects::PSatellite>	waste;
				for (index_t i = 0; i < Objects::satellites.Count(); i++)	//must be an integer loop (see ProcessRound())
				{
					Objects::PSatellite sat = Objects::satellites[i];
					t+=Objects::timeEpsilon;

					if (terrainChanged)
						sat->RefreshGroundSensor(t);

					if (!sat->ProcessRound(t))	//this method can insert new satellites, hence a int loop, and not a foreach loop
					{
						if (sat->location.height <= Objects::Satellite::DeclinedResidenceHeight)
						{
							LogEvent(EventClass(EventSignificance::Important, EventFlavor::Loss),sat.get(),"Insufficient fuel supply. Satellite lost");
							//t+= 0.025f;
							sat->motionTime+=Objects::timeEpsilon;
							waste << sat;
						}
						else
						{
							LogEvent(EventClass(EventSignificance::Important, EventFlavor::Loss),sat.get(),"Insufficient fuel supply. Satellite is declining");
							Grid::LCellIdI newLocation = sat->location;
							newLocation.height = Objects::Satellite::DeclinedResidenceHeight;
							//t+= 0.025f;
							sat->HandleNow(sat->PlaceExactlyAt(newLocation, 0.1f, 0.f));
						}
					}
					else
						if (sat->location.height < Objects::Satellite::BaseResidenceHeight)
						{
							LogEvent( EventClass(EventSignificance::Uncommon, EventFlavor::Gain),sat.get(),"Satellite height restored");
							Grid::LCellIdI newLocation = sat->location;
							newLocation.height = Objects::Satellite::BaseResidenceHeight;
							sat->HandleNow(sat->PlaceExactlyAt(newLocation, 0.1f, 0.f));
						}
				}
				//satellitesAreLocked = false;

				for (const auto&sat : waste)
					Objects::Remove(sat, Objects::RemovalType::UncontrolledExplosion, sat->motionTime, true);
				waste.Clear();
			#endif
		};





		bool ContinueRoundProcessing(float&animationLength)
		{
			static TExplosion exp;
			while (pendingExplosions >> exp)
			{
				ApplicationInterface::TAttack at;
				at.attackFrom = exp.causedBy;
				Objects::Explode(at,Grid::ToHigh(exp.center),Grid::ToWorld(exp.center),exp.radius,exp.damageLevel,false,exp.atTime);
			}

			switch (RuntimeConfig::phase)
			{
				case RuntimeConfig::Phase::AttackPhase:
					if (!ProcessNextVehicleAttack())
						Game::Network::ChangePhase(RuntimeConfig::Phase::MovementPhase);
					//anythingChanged = true;
				break;
				case RuntimeConfig::Phase::MovementPhase:
					if (!ProcessVehicleMotion())
					{
						Game::ProcessChanges(true);
						#ifndef NO_HIBERNATION
							State::UpdateHibernation();
						#endif

						animationLength = 0.f;
						Objects::ForeachWorldEntityIncludingStored([&animationLength](const Objects::PWorldEntity&m)
						{
							animationLength = std::max(m->entityTime,animationLength);
						},false);
						Game::PerformNewRoundLogic(animationLength);
						ResetTowerAttackAction(animationLength);
						CheckOrbitalSupplyAndSensory(animationLength,World::terrainChanged);
						World::terrainChanged = false;
						ProcessStructures1(animationLength);//clears overproduction: must be called after CheckSatelliteSupply()
						return false;
					}
				break;
			}
			return true;
		}
	}


	/*virtual override*/ void	Core::BeginRoundProcessing()
	{
		CoreDetails::BeginRoundProcessing();
	}

	/*virtual override*/ bool	Core::ContinueRoundProcessing()
	{
		return CoreDetails::ContinueRoundProcessing(animationLength);

	}

	/*virtual override*/ void Core::FinishRoundProcessing()
	{
		RuntimeConfig::roundNumber.main++;
		
		Objects::Statistics::SignalNewRoundBegin(true);

		Game::Network::ChangePhase(RuntimeConfig::Phase::CommandPhase,Game::Config::roundTimer > 0 && !Game::roundTimerPaused ? Game::Config::roundTimer : -1);
	}

	/*virtual override*/ void		Core::HandleChainReactionExplosion(const TExplosion&explosion)
	{
		CoreDetails::pendingExplosions << explosion;

	}

}
