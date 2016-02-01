#include <global_root.h>
#include "core.h"
#include <objects/gridAndChoices.h>
#include <objects/mobileEntity.h>
#include <objects/faction.h>
#include "../state.h"
#include <objects/orbitalStructure.h>
#include <objects/orbitalVehicle.h>

namespace Evolver
{


	namespace CoreDetails
	{
		/**
		Like vehicles, towers have an attack action floation point value. Unlike vehicles, they are not part of the
		movement phase. Therefore, once per round, this function must be called to reset their attack action.
		*/
		void			ResetTowerAttackAction(float atTime);


		void CheckMobileSupply(float&t);


		struct TActionReference
		{
			Objects::WMobileEntity	ref;
			float			initiativeFactor;

			/**/		TActionReference(const Objects::PMobileEntity&e):ref(e)	{initiativeFactor = e->entityRandom.getFloat(0.95f,1.f); /*std::cout << e->GetName()<<": "<< initiativeFactor << std::endl;*/}

			bool		IsAlive() const {return ref.lock() && ref.lock()->health >= 0;}

		};

	//	/*static*/ Random	TActionReference::random;


		struct TAttackReference
		{
			Objects::WTurretBase		tref;
			Objects::WWorldEntity			eref;
			float			initiativeFactor;

			/**/		TAttackReference(const Objects::PWorldEntity&we):eref(we)	{initiativeFactor = we->entityRandom.getFloat(0.95f,1.f);/*std::cout << we->GetName()<<": "<< initiativeFactor << std::endl;*/}

			bool		IsAlive() const {Objects::PWorldEntity e = eref.lock(); return e && e->health >= 0;}

			float		GetInitiative() const {return IsAlive() ? initiativeFactor * float(tref.lock()->GetAttackInitiative()) : -1.f;}

			bool		operator>(const TAttackReference&other) const {return GetInitiative() > other.GetInitiative();}
			bool		operator<(const TAttackReference&other) const {return GetInitiative() < other.GetInitiative();}
		};

		Buffer<TAttackReference>	attackingEntities;
		float						attackTime = 0.f;




			
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


		Buffer<TMovementReference>	movingVehicles,movingVehicles2;

		void		SignalMotionPhase()
		{
			Objects::ForeachMobileWorldEntity([](const Objects::PMobileEntity&v)
			{
				v->BeginMotionPhase(attackTime);
				if (v->IsBuilding() || v->NextTaskIsMotion())
					movingVehicles.Append(TMovementReference(v));
			});
		}

		bool		ProcessNextVehicleAttack()
		{
			if (attackingEntities.IsEmpty())
			{
				SignalMotionPhase();
				return false;
			}
			std::sort(attackingEntities.begin(),attackingEntities.end());

			while (attackingEntities.IsNotEmpty() &&!attackingEntities.last().IsAlive())
				attackingEntities.EraseLast();
			if (attackingEntities.IsEmpty())
			{
				SignalMotionPhase();
				return false;
			}
			Objects::PTurretBase b = attackingEntities.last().tref.lock();
			b->Attack(attackTime);
			//PWorldEntity e = attackingEntities.last().eref.lock();

			while (attackingEntities.IsNotEmpty())
			{
				b = attackingEntities.last().tref.lock();
				Objects::PWorldEntity e = attackingEntities.last().eref.lock();
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

			while (movingVehicles.IsNotEmpty() && !movingVehicles.last().IsAlive())
				movingVehicles.EraseLast();
			if (movingVehicles.IsEmpty())
			{
				return false;
			}
			Objects::PMobileEntity v = movingVehicles.last().ref.lock();
			bool hadMoved = v->hasMoved;
			if (!v->PerformNonAttack() && !hadMoved)
				movingVehicles2.Append(TMovementReference(v));
			movingVehicles.EraseLast();

			return movingVehicles.IsNotEmpty() || movingVehicles2.IsNotEmpty();
		}

		void ProcessEMPState(float atTime, const std::function<bool(const Objects::PWorldEntity&)>&doHeal)
		{
			Objects::ForeachWorldEntity([doHeal,&atTime](const Objects::PWorldEntity&e)
			{
				if (doHeal(e))
					e->HealEMPDamage(atTime);
			});
		}


		void	BeginRoundProcessing()
		{
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

			float t = 0.f;
			CheckMobileSupply(t);



			Objects::ForeachMobileWorldEntity([](const Objects::PMobileEntity&v)
			{
				if (v->IsHibernating())
					return;
				attackTime = 0;
				v->BeginAttackPhase();

				Objects::PTurretBase b = v->GetTurretBase();
				if (!b || !b->turret->CanAttack() || !b->CountPossibleAttacks())
					return;

				//if (v->IsSatellite())
				//{
				////	Objects::PSatellite s = To<Objects::Satellite>::Cast(v);

				//	if (v->myTasks.IsEmpty())
				//		v->FillAttackQueue();
				//}


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
						LogEvent(EventType::CommandRejected,v.get(),"Insufficient ammunition. Attacks cancelled");
						v->EraseFirstTask();
						v->RegisterChange(Objects::WorldEntity::TasksChanged,v->timeAt);
					}
					//ref.initiative = v->GetAttackInitiative();
					//ref.attacksLeft = v->CountPossibleAttacks();
				}

			}
			);
			Objects::ForeachFaction([](const Objects::PFaction&f)
			{
				foreach (f->towers,_t)
				{
					Objects::PTower t = _t->lock();
					if (!t || t->IsHibernating())
						continue;
					attackTime = 0;
					t->BeginAttackPhase();
					//t->BeginAttackPhase();
					if (t->turret->CanAttack())
					{
						if (t->attackQueue.IsEmpty() && !t->FillAttackQueue())
							continue;
						if (!t->HasAmmoForShot(0))
						{
							LogEvent(EventType::CommandRejected,t.get(),"Insufficient ammunition. Attacks cancelled");
							t->attackQueue.Clear();
							t->RegisterChange(Objects::WorldEntity::TasksChanged,t->timeAt);
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
			//#ifdef CONNECTOR_LESS
				Objects::RefreshCompoundCoverageIfNeeded(atTime);
				Objects::ForeachCompound([atTime](Objects::Compound&c)
				{
					c.TickEnd(atTime);
				});
			//#endif

		}




		void CheckOrbitalSensory(float&t, bool terrainChanged)
		{
			if (terrainChanged)
				LogEvent(EventType::Debug,nullptr,"Terrain changed, updating satellite ground-sensory");
			for (const auto&sat : Objects::Orbital::ships)
			{
				t+=Objects::timeEpsilon;

				if (terrainChanged)
					sat->RefreshGroundSensor(t);
			}
			for (const auto&sat : Objects::Orbital::stations)
			{
				t+=Objects::timeEpsilon;

				if (terrainChanged)
					sat->RefreshGroundSensor(t);
			}
		};

		int GetDamageDueToMissingUpkeep(const Objects::ResourceVector&missing)
		{
			return missing.SumPositive();
		}


		void CheckMobileSupply(float&t)
		{
			Objects::RefreshSkyHubConnections(t);
			Objects::ResourceVector missing;
			Buffer0<Objects::PWorldEntity>	died;
			Objects::ForeachMobileWorldEntity([&missing,&t,&died](const Objects::PMobileEntity&e)
			{
				t+=Objects::timeEpsilon;

				if (!e->TrySelfMaintain(t,missing))
				{
					LogEvent(EventType::EntityProcess,e.get(),"Insufficient resources to self-maintain (missing "+missing.ToString()+"). Unit took damage.");
					int damage = GetDamageDueToMissingUpkeep(missing);
					e->SetStable(false,t);
					e->TakeDamage(damage,t);
					if (e->health <= 0)
						died << e;
				}
				else
					e->SetStable(true,t);
			});

			for (const auto&d : died)
				Objects::Remove(d,d->IsOrbital() ? Objects::RemovalType::UncontrolledOrbitalDecline : Objects::RemovalType::UncontrolledExplosion,t,true);
			
			t+=Objects::timeEpsilon;
		}

		void CheckStructureSupply(float&t)
		{
			Objects::RefreshSkyHubConnections(t);
			Objects::ResourceVector missing;
			Buffer0<Objects::PWorldEntity>	died;
			for (const auto&e : Objects::structures)
			{
				if (e->IsIndestructible() || e->IsMaintained())
					return;

				t+=Objects::timeEpsilon;

				e->GetUpkeep(missing);
				e->TakeDamage(GetDamageDueToMissingUpkeep(missing),t);
				if (e->health <= 0)
					died << e;
			};

			for (const auto&d : died)
				Objects::Remove(d,d->IsOrbital() ? Objects::RemovalType::UncontrolledOrbitalDecline : Objects::RemovalType::UncontrolledExplosion,t,true);
			
			t+=Objects::timeEpsilon;
		}




		bool ContinueRoundProcessing(float&animationLength)
		{
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
						Objects::ForeachMobileEntityIncludingStored([&animationLength](const Objects::PMobileEntity&m)
						{
							animationLength = std::max(m->timeAt,animationLength);
						});
						Game::PerformNewRoundLogic(animationLength);
						ResetTowerAttackAction(animationLength);
						CheckOrbitalSensory(animationLength,World::terrainChanged);
						World::terrainChanged = false;
						CheckStructureSupply(animationLength);
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
		RuntimeConfig::roundNumber++;

		Game::Network::ChangePhase(RuntimeConfig::Phase::CommandPhase,Game::Config::roundTimer > 0 && !Game::roundTimerPaused ? Game::Config::roundTimer : -1);
	}

}
