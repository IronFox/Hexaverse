#include "mobileState.h"
#include "../visual/scene/objectComponent.h"
#include "../visual/objectSelection.h"
#include "../visual/hud/hexGrid.h"
#include "../visual/hud/resourceGauges.h"
#include "../visual/objectChoices.h"
#include "../network.h"
#include "delayedMobileInsertion.h"
#include "../ai.h"
#include "../playerControlBegin.h"
#include <expectedBehavior.h>
#include <globalEvents.h>
#ifdef HAS_WINDOW
	#include <al/openal.h>
#endif

namespace Animation
{
	#ifdef HAS_WINDOW
		Engine::OpenAL::Wave				constructSound;
	#endif

	/*static*/ void MobileState::StaticCreate()
	{
		#ifdef HAS_WINDOW
			constructSound = Engine::OpenAL::Load("resources/objectSounds/construct.wav");
		#endif
	}

	/*static*/ void MobileState::StaticDestroy()
	{
		#ifdef HAS_WINDOW
			Engine::OpenAL::Discard(constructSound);
		#endif
	}

	static bool	Equals(const Objects::PVehicleJob&a, const Objects::PVehicleJob&b)
	{
		if (!a && !b)
			return true;
		if (!a || !b)
			return false;

		
		std::shared_ptr<Objects::VehicleJob::DummyPathJob> pj0 = To<Objects::VehicleJob::DummyPathJob>::DynamicCast(a);
		std::shared_ptr<Objects::VehicleJob::DummyPathJob> pj1 = To<Objects::VehicleJob::DummyPathJob>::DynamicCast(b);

		std::shared_ptr<Objects::VehicleJob::DummyJob> j0 = To<Objects::VehicleJob::DummyJob>::DynamicCast(a);
		std::shared_ptr<Objects::VehicleJob::DummyJob> j1 = To<Objects::VehicleJob::DummyJob>::DynamicCast(b);

		if ((!pj0 == !pj1) && (!j0 == !j1))
		{
			if (pj0)
			{
				return pj0->path == pj1->path;
			}
			else
				return true;
		}
		else
			return false;

	}

	
	
	static void	TranslateTask(const Objects::MobileEntity&e, const Protocol::MobileTask&sourceTask, Objects::MobileEntity::Task&t1)
	{
		foreach (sourceTask.foci,f)
			t1.foci.Push().target = *f;
		t1.mode = (Objects::Vehicle::mode_t) sourceTask.mode;
		t1.name = sourceTask.name;
		t1.type = (Objects::Vehicle::Task::type_t) sourceTask.type;
		if (t1.type == Objects::Vehicle::TaskData::Job)
		{
			if (sourceTask.buildPath.IsNotEmpty())
			{
				std::shared_ptr<Objects::VehicleJob::DummyPathJob>	job(new Objects::VehicleJob::DummyPathJob(e));
				job->path.Clear(sourceTask.buildPath.GetLength());
				foreach (sourceTask.buildPath,p)
					job->path << *p;
				t1.job = job;
			}
			else
			{
				std::shared_ptr<Objects::VehicleJob::DummyJob>	job(new Objects::VehicleJob::DummyJob(e,(Objects::Component::Operation)sourceTask.jobAffinity));
				t1.job = job;
				job->delayedBySleep = (sourceTask.taskFlags & Protocol::TaskFlags::DelayedBySleep) != 0;
			}
			t1.job->movesToFocusWhenDone = (sourceTask.taskFlags & Protocol::TaskFlags::VisualizePathToFocus) != 0;
			t1.job->continuationsToJobCompletion = sourceTask.continuationsToJobCompletion;
			t1.job->resourceCostLeft = sourceTask.resourceCostLeft;
			
			#ifdef HAS_WINDOW
				if (sourceTask.foci.Count())
				{
					Grid::LCellIdI loc = sourceTask.foci.First();
					#ifndef NO_SATELLITES
						if (sourceTask.visualizationName != ConstName::Empty && e.IsSatellite())
						{
							int h = Objects::SkyHub::GetStructureBuildHeight(loc);
							if (h != Objects::badHeight)
								loc.height = h;
						}
					#endif
					
					Objects::TaskAttachment::Allocate(t1).SetLocalizableShape(sourceTask.visualizationName,loc,e.GetResearchState());
				}
			#endif
		}
		//t1.Recompile(v->GetPrevPointOfOrigin(),v.get());
	}


	//a=serverVersion, b = localVersion
	static bool	Mergeable(const Objects::MobileEntity::Task&a, const Objects::MobileEntity::Task&b)
	{
		if (a.name != b.name || a.mode != b.mode || a.type != b.type)
			return false;
		if (a.foci.IsNotEmpty())
		{
			if (b.foci.IsEmpty())
				return false;

			if (a.foci.GetNewest() != b.foci.GetNewest() && a.foci.GetOldest() != b.foci.GetOldest())
				return false;
			if (b.path.IsEmpty())
				return false;
		}
		else
			if (b.foci.IsNotEmpty())
				return false;
		return Equals(a.job,b.job);
	}

	static void	Merge(const Objects::MobileEntity&parent, const Grid::LCellIdI&pointOfOrigin, Objects::MobileEntity::Task&serverVersion, Objects::MobileEntity::Task&localVersion, Objects::MobileEntity::Task&outMerged)
	{
		ASSERT__(Mergeable(serverVersion,localVersion));


		if (serverVersion.foci == localVersion.foci)
		{
			localVersion.job = serverVersion.job;
			outMerged.swap(localVersion);
			return;
		}
		outMerged.mode = localVersion.mode;
		outMerged.job = localVersion.job;
		#ifdef HAS_WINDOW
			outMerged.visualization.swap(localVersion.visualization);
		#endif
		outMerged.name.adoptData(localVersion.name);
		outMerged.type = localVersion.type;
		#ifdef HAS_WINDOW
			outMerged.visualization = localVersion.visualization;
		#endif
		if (serverVersion.foci.GetOldest() == localVersion.foci.GetOldest())
		{
			auto it0 = serverVersion.foci.begin();
			auto it1 = localVersion.foci.begin();
			outMerged.path.Clear();
			auto pit = localVersion.path.begin();
			while (it0 != serverVersion.foci.end() && it1 != localVersion.foci.end() && *it0 == *it1)
			{
				while (pit->cell != it0->target && pit != localVersion.path.end())
				{
					outMerged.path << *pit;
					++pit;
				}
				if (pit == localVersion.path.end())
				{
					outMerged = serverVersion;
					outMerged.Replot(pointOfOrigin,&parent,Objects::FocusChangeType::FullChange,true);
					return;
				}
				outMerged.foci << *it0;
				++it0;
				++it1;
			}
			outMerged.path << *pit;
			while (it0 != serverVersion.foci.end())
			{
				outMerged.foci << *it0;

				outMerged.Replot(pointOfOrigin,&parent,Objects::FocusChangeType::FocusAppended,false);

				++it0;
			}
			outMerged.Replot(pointOfOrigin,&parent,Objects::FocusChangeType::NoChange,true);
			return;
		}
		else if (serverVersion.foci.GetNewest() == localVersion.foci.GetNewest())
		{
			if (serverVersion.foci.Count() < localVersion.foci.Count())	//first n foci deleted
			{
				auto pit = localVersion.path.begin();
				while (pit != localVersion.path.end() && pit->cell != /*serverVersion.foci.Peek().target*/ parent.location)
					pit++;
				if (pit == localVersion.path.end())
				{
					outMerged.swap(serverVersion);
					outMerged.Replot(pointOfOrigin,&parent,Objects::FocusChangeType::FullChange,true);
					return;
				}
				while (localVersion.foci.IsNotEmpty() && localVersion.foci.Peek() != serverVersion.foci.Peek())
					localVersion.foci.Pop();
				auto it0 = localVersion.foci.begin();
				auto it1 = serverVersion.foci.begin();
				while (it0 != localVersion.foci.end() && it1 != serverVersion.foci.end() && *it0 == *it1)
				{
					++it0;
					++it1;
				}

				if (it0 != localVersion.foci.end() || it1 != serverVersion.foci.end())
				{
					outMerged.swap(serverVersion);
					outMerged.Replot(pointOfOrigin,&parent,Objects::FocusChangeType::FullChange,true);
					return;
				}
				while (pit != localVersion.path.end())
				{
					outMerged.path << *pit;
					++pit;
				}
				outMerged.foci.swap(serverVersion.foci);
				outMerged.Replot(pointOfOrigin,&parent,Objects::FocusChangeType::NoChange,true);
				return;
			}
			//foci added out front ...? probably all mismatched. ignoring this case

		}

		outMerged.swap(serverVersion);
		outMerged.Replot(pointOfOrigin,&parent,Objects::FocusChangeType::FullChange,true);
	}


	static void AppendTask(const Objects::PMobileEntityProducer&p,const TUniqueID&id, Objects::EntityType type)
	{
		p->AppendTask(id,type);
	}


	static void StoreLocally(const Objects::PHangar&h,const Objects::PVehicle&v,float&t,bool setHealthToFull)
	{
		if (!h->StoreLocally(v,t,setHealthToFull,h))
			throw Except::Program::DataConsistencyFault(CLOCATION,"Failed to store "+v->GetName()+" in hangar "+h->ToEntity().GetName()+" at "+ToString(h->ToEntity().location));
	}

	
	static bool NoBlock(const Grid::LCellIdI&cell)
	{
		return false;
	}

	static bool EstimateThreatLevel(const Objects::PWorldEntity&self,   Grid::LCellIdI&location, float&closestCompetition, float&closestAlly, float&closestGhost)
	{
		const M::TVec3<>&reference = Grid::low.cells[location.stackID].centerVertex.xyz;
		M::TVec3<> world = reference * Grid::ToRadius(location.height);

		bool canDo = true;
		Objects::ForeachWorldEntity([&canDo,world,reference,self,&closestCompetition,&closestAlly,&closestGhost](const Objects::PWorldEntity&e)
		{
			if (!canDo)
				return;
			if (e == self)
				return;

			if (!e->isOwnedByMe)
			{
				float d2 = Vec::quadraticDistance(world,e->system.w.xyz);
				if (d2 < M::Sqr(e->GetScanningRange()))
				{
					canDo = false;
					return;
				}
				if (e->CanHit(world))
				{
					canDo = false;
					return;
				}
				return;
			}


			if (e->IsMobile() && e->IsAlive())
			{
				Objects::PMobileEntity o = To<Objects::MobileEntity>::Cast(e);
				bool surveyor = o->IsGroundBoundVehicle() && To<Objects::Vehicle>::Cast(o)->IsAutomatedResourceDetector();
				bool constructor = o->GetOperationSpeed(Objects::Component::Operation::Construct) > 0;
				if (!surveyor && !constructor)
					return;
				const Grid::LCellIdI&loc =o->GetLastPointOfOrigin();
				float d = Vec::quadraticDistance(Grid::low.cells[loc.stackID].centerVertex.xyz,reference);

				if (surveyor)
				{
					if (d < closestCompetition)
						closestCompetition = d;
				}
				else
				{
					if (d < closestAlly)
						closestAlly = d;
				}
				return;
			}

			if (e->IsDeadOrGhost())
			{
				float d = Vec::quadraticDistance(e->system.w.xyz,world);
				if (d < closestGhost)
					closestGhost = d;
				if (d < 2.f)
					canDo = false;
			}
		},Objects::IncludeGhosts::Living);
		return canDo;
	}



	static float sensorRange =0;
	static Buffer0<UINT32> stacks;
	static void SearchCostModifier( const Grid::LCellIdI&loc, int&cost )
	{
		AI::CostModifier(loc,cost);
		if (cost == -1)
			return;
		Grid::ResourceDiscovery::DetermineCoverage(loc,sensorRange,stacks);
		int step = M::CeilDiv(cost,3);
		foreach (stacks, st)
			if (Grid::ResourceDiscovery::client.resourcesDiscovered[*st])
				cost += step;
	}

	
	/*static*/ void			MobileState::UpdateAutomation( const Objects::PVehicle&v )
	{
		static const float maxGhost = 30.f;

		ASSERT__(v);
		ASSERT__(v->isOwnedByMe);
		ASSERT__(v->turret->detectsResources);
		if (v->IsHibernating())
			return;

		sensorRange = v->GetScanningRange();

		if (v->myTasks.IsNotEmpty())
		{
			if (v->myTasks.Peek().type == Objects::Vehicle::Task::MoveTo && v->myTasks.Count() == 1)
			{
				Grid::LCellIdI last = v->GetLastPointOfOrigin();
				if (!Grid::ResourceDiscovery::client.resourcesDiscovered[last.stackID])
				{
					float competition = 100.f;
					float ally = 100.f;
					float ghost = M::Sqr(maxGhost);
					bool canDo = true;
					foreach (v->myTasks.GetNewest().path,node)
					{
						if (node->cell == v->location)
							continue;
						const bool threatless = EstimateThreatLevel(v,node->cell,competition,ally,ghost);
						const auto blocking = Objects::GetBlockingHere(node->cell,true);
						if (!threatless || blocking)
						{
							canDo = false;
							break;
						}
					}
					if (sqrt(ghost) < 16.f)
						canDo = false;
				
					if (canDo)
						return;	//looks okay
				}
			}
		}
		ApplicationInterface::SignalDropAllTasks(v);
		v->myTasks.Clear();	//will happen anyways

		int referenceSpeed = v->GetMovementStepsPerRound()-1;

	//	static Buffer<Objects::MapNode>	nodes;
		static Objects::Pathfinding::ReachabilityMapper mapper(false);


		AI::BeginBlockTest(v,false,false,false);

		using namespace Objects::Pathfinding;

		mapper.Map(*v,v->location,RoutingFlags::ForceRevertible,Objects::Component::GroundFlags::DisallowUnknownGround,referenceSpeed*5, SearchCostModifier);

		float dummy = 0;
		const bool originGood = EstimateThreatLevel(v,v->location,dummy,dummy,dummy);;


		float closestDistance = std::numeric_limits<float>::max();
		const Objects::Pathfinding::ReachabilityMapper::MapNode*closest = nullptr;
		//Grid::LCellIdI closest = Grid::LCellIdI::Invalid;
		//float closestCompetingDistance = 0;

		const auto&known = Grid::ResourceDiscovery::client.resourcesDiscovered;

	
		for (int retries = 0; retries < 2; retries++)
		{
			foreach(mapper.result,node)
			{
				if (known[node->cell.stackID])
					continue;
				if (retries == 0 && node->movementCostsFromRoot < referenceSpeed)
					continue;

				float competition = 100.f;
				float ally = 100.f;
				float ghost = M::Sqr(maxGhost);


				if (!(Grid::low.cells[node->cell.stackID].cornerFlags & Grid::empZonesUnlocked))
					continue;


				bool canDo = !originGood || EstimateThreatLevel(v,node->cell,competition,ally,ghost);


				if (!competition || !canDo || !ghost)
					continue;
				//const auto &n = Grid::low.grid.GetCell(node->cell.stackID).neighborhood;
				int score = 0;
				Objects::Pathfinding::ReachabilityMapper::MapNode*at = node;
				for (;;)
				{
					if (at->customScore < 0)
					{
						at->customScore = 0;
						const auto &n = Grid::low.grid.GetCell(at->cell.stackID).neighborhood;
						for (int i = 0; i < n.numNeighbors; i++)
							if (!known[n.neighborID[i]])
							{
								at->customScore++;
							}
					}
					if (at == node || at->predecessor == InvalidIndex)	//first or last
						score += M::CeilDiv(at->customScore,2);	//consider half
					else
						score += M::CeilDiv(at->customScore,3);	//consider one third
					if (at->predecessor == InvalidIndex)
						break;
					at = mapper.result + at->predecessor;
				}
				float ghostFear = M::Sqr(maxGhost) / ((ghost));
				float dist = float(node->movementCostsFromRoot) * sqrt(ally) / sqrt(competition) / float(score+1) * ghostFear;
				if (dist > closestDistance)
					continue;
				closestDistance = dist;
				closest = node;
			}
			if (closest)
				break;
		}
		if (closest)
		{
			AI::IssuePath(v,mapper,closest);
	//		DirectTo(v,closest,false,Objects::Vehicle::Idle);
		}
	}




	/*static*/ void					MobileState::Apply(const Objects::PMobileEntity&v, Protocol::MobileState&vs, bool isNew, bool isNewForEMP)
	{
		ASSERT__(v);
		DBG_ASSERT1__(v->storedIn.expired() || v->myTasks.IsEmpty(),v->GetName());
		int oldHeight = v->location.height;

		const bool locationChanged = v->storedIn.expired() 
								&& v->location != vs.entityState.location 
								&& vs.entityState.location != Grid::LCellIdI::Invalid;
		const auto info = vs.myMobileEntity.Get0();
		const bool relocateToStorage = info && info->storedIn.Get0();

		if (locationChanged && !relocateToStorage)
		{
			LogEvent(EventClass::Preset::debug,v.get(), "ApplyState(): Updating location: "+ToString(v->location)+"->"+ToString(vs.entityState.location));
			/*
			This moves sensors. May occur during animation phase (e.g. Move->*Store)
			*/
			if (!insertIntoGridLater.IsSet(v->id))
			{
				/*
				No motion has happened during the local animation phase, or currently not in any phase
				*/
				ASSERT2__(!v->isInMovementStream, v->GetName(), v->location);
				v->Relocate(vs.entityState.location,false);
			}
			else
				v->location = vs.entityState.location;	//Motioned happend before. Just update location
		}
		v->health = vs.entityState.health;
		v->unstable = (vs.entityState.entityStateFlags & Protocol::EntityStateFlags::IsUnstable) != 0;
		v->ApplySleepChangeFromServer( (vs.entityState.entityStateFlags & Protocol::EntityStateFlags::IsSleeping) != 0 );

		v->UpdateEMPDamage(vs.entityState.empDamageLevel, !isNewForEMP);	//must remove visible sensory, must be linked to destructor
		//v->roundsDisabled = vs.entityState.disabledRounds;

		#ifndef NO_HIBERNATION
			bool hibernating = (vs.flags & Protocol::VehicleFlags::Hibernating) != 0;
			v->Hibernate(hibernating,0);
		#endif

		bool automationChange = false;
		if (v->IsGroundBoundVehicle())
		{
			Objects::PVehicle&veh = To<Objects::Vehicle>::Cast(v);
			bool newAutomated = (vs.flags & Protocol::VehicleFlags::Automated) != 0;
			automationChange = newAutomated != veh->isAutomated;
			veh->isAutomated = newAutomated;
		}

		Objects::PWorldEntity storeIn;
		Objects::PHangar storeInHangar;
		if (info)
		{
			#ifndef ONE_COMPOUND_PER_FACTION
				v->currentStorage = info->resourceVector;
			#endif
			v->constructionActionCounter = info->constructionActionCounter;

			Array<Objects::MobileEntity::Task>	currentTasks,newTasks(info->tasks.Count());
			v->myTasks.MoveToArray(currentTasks);

			for (index_t i = 0; i < info->tasks.Count(); i++)
				TranslateTask(*v, info->tasks[i],newTasks[i]);
			if (newTasks.IsEmpty())
			{
			}
			else
			{
				bool fullRebuild = false;
//				if (v->myTasks.Count() >= newTasks.Count())
				{
					if (currentTasks.IsNotEmpty() && Mergeable(newTasks.First(),currentTasks.First()))
					{
						Grid::LCellIdI last = v->location;
						for (index_t i = 0; i < newTasks.Count(); i++)
						{
							if (i < currentTasks.Count() && Mergeable(newTasks[i],currentTasks[i]))
								Merge(*v, last,newTasks[i],currentTasks[i],v->myTasks.Push());
							else
							{
								v->myTasks.Push().swap(newTasks[i]);
								v->myTasks.GetNewest().Replot(last,v.get(),Objects::FocusChangeType::FullChange,true);
							}
							last = v->GetLastPointOfOrigin();
						}
					}
					elif (currentTasks.IsNotEmpty() && Mergeable(newTasks.Last(),currentTasks.Last()))
					{
						while (currentTasks.IsNotEmpty() && !Mergeable(newTasks.First(),currentTasks.First()))
							currentTasks.Erase(0);
						if (currentTasks.IsEmpty())
							fullRebuild = true;
						else
						{
							Grid::LCellIdI last = v->location;
							for (index_t i = 0; i < newTasks.Count(); i++)
							{
								if (i < currentTasks.Count() && Mergeable(newTasks[i],currentTasks[i]))
									Merge(*v, last,newTasks[i],currentTasks[i],v->myTasks.Push());
								else
								{
									v->myTasks.Push().swap(newTasks[i]);
									v->myTasks.GetNewest().Replot(last,v.get(),Objects::FocusChangeType::FullChange,true);
								}
								last = v->GetLastPointOfOrigin();
							}
						}
					}
					else
						fullRebuild = true;
				}

				if (fullRebuild)
				{
					Grid::LCellIdI last = v->location;
					for (index_t i = 0; i < newTasks.Count(); i++)
					{
						v->myTasks.Push().swap(newTasks[i]);
						v->myTasks.GetNewest().Replot(last,v.get(),Objects::FocusChangeType::FullChange,true);
						last = v->GetLastPointOfOrigin();
					}
				}

			}


			//v->myTasks.Clear();
			//foreach (info->tasks,t)
			//{
			//	AppendTask(v,t);
			//}

			auto id = info->storedIn.Get0();
			if (id)
			{
				storeIn = Objects::GetEntity(*id);
				storeInHangar = To<Objects::Hangar>::DynamicCast(storeIn);
				ASSERT1__(storeIn,*id);
				ASSERT1__(storeInHangar,storeIn->GetName());
			}

			Objects::PMobileEntityProducer p = To<Objects::MobileEntityProducer>::DynamicCast(v);
			if (p)
			{
				const Protocol::ProducerState*state = info->producerState.Get0();
				if (state)
				{
					p->queue.Clear();
					p->productionDone = state->productionDone;
					p->gatheringPoint.Set(state->gatheringPoint, Objects::IsSelected(v));

					
					foreach (state->productionTasks,t)
						AppendTask(p,t->id,t->type);
				}
			}




		}
		else if (!v->isOwnedByMe)
		{
			const bool building = (vs.flags & Protocol::VehicleFlags::Building) != 0;
			if (building)
			{
				if (v->myTasks.IsEmpty())
				{

					Objects::PVehicleJob job(new Objects::VehicleJob::DummyJob(*v,Objects::Component::Operation::Construct));
					job->continuationsToJobCompletion = 1;
					job->movesToFocusWhenDone = false;
					job->resourceCostLeft = 1;

					auto&t = v->myTasks.Push();
					t.name = "Building";
					t.job = job;
					t.type = Objects::MobileEntity::Task::Job;
					#ifdef HAS_WINDOW
						Objects::TaskAttachment::Allocate(t);
					#endif
				}
			}
			else
				v->myTasks.Clear();
			//v->currentStorage
		}
		Objects::PHangar currentlyStoredIn = v->storedIn.lock();
		if (info != nullptr && storeInHangar != currentlyStoredIn)
		{
			if (currentlyStoredIn)
				LogEvent(EventSignificance::Debug,v.get(),"StoredIn-change ("+currentlyStoredIn->ToEntity().GetName()+" -> "+storeIn->GetName()+")");
			else
				LogEvent(EventSignificance::Debug,v.get(),"StoredIn-change ( -> "+storeIn->GetName()+")");
			float f = 0;

			Objects::PVehicle veh = To<Objects::Vehicle>::DynamicCast(v);
			ASSERT1__(veh,v->GetName());

			if (storeIn)
			{
				if (currentlyStoredIn)
					currentlyStoredIn->Lose(veh);
				else
				{
					insertIntoGridLater.Unset(v->id);
				}
				StoreLocally(storeInHangar,veh,f,false);
				ASSERT1__(!v->isInsertedIntoGrid,v->GetName());	//should never happen. If failed, StoreLocally would have thrown an exception

				#ifdef HAS_WINDOW
					if (!currentlyStoredIn)
						HUD::ResourceGauges::UpdateTraffic();
				#endif

			}
			else
			{
				ASSERT1__(vs.entityState.location != Grid::LCellIdI::Invalid,v->GetName());
				ASSERT1__(!v->isInsertedIntoGrid,v->GetName());
				currentlyStoredIn->LocalCheckout(veh,vs.entityState.location,f);
				LogEvent(EventSignificance::Debug,v.get(),"Checked out to "+ToString(vs.entityState.location));
				ASSERT1__(v->location == vs.entityState.location,v->GetName());
				ASSERT1__(v->isInserted,v->GetName());
				ASSERT1__(Objects::vehicles.Contains(v) || Objects::SubOrbital::planes.Contains(v),v->GetName());
				v->UpdateSystem();
				#ifdef HAS_WINDOW
					HUD::ResourceGauges::UpdateTraffic();
				#endif
			}
			#ifdef HAS_WINDOW
				HUD::HexGrid::RefreshContacts();
			#endif
		}

		v->attackActionLeft = vs.attackActionLeft;
		v->movementActionLeft = vs.movementActionLeft;
		if (locationChanged || isNew)
			v->UpdateSystem();

		#ifdef HAS_WINDOW
			if (v == Objects::selectedEntity)
			{
				GlobalEvents::invalidateObjectCache();
				Objects::UpdateChoices(Network::ControlEnabled());
			}
		#endif

		if (automationChange && v->isOwnedByMe && v->IsGroundBoundVehicle() && To<Objects::Vehicle>::Cast(v)->IsAutomatedResourceDetector())
		{
			UpdateAutomation(To<Objects::Vehicle>::Cast(v));
		}

		#ifdef HAS_WINDOW
			if (storeInHangar != currentlyStoredIn)
			{
				Objects::deployingMobileEntity.reset();
				HUD::HexGrid::Configure(Objects::selectedEntity);
			}


			if (!v->turret->CanAttack())
			{
				if (v->myTasks.IsNotEmpty() && v->myTasks.Peek().IsJob() && (!v->IsSleeping() || !v->myTasks.Peek().job->DelayedBySleep()))
				{
					if (!v->activitySound.IsCreated())
					{
						v->activitySound.Create();
						#ifdef _DEBUG
							v->activitySound.name = v->GetName()+".activity";
						#endif
						v->activitySound.SetLooping(true);
						v->activitySound.SetReferenceDistance(3.f,1.f);
						v->activitySound.EnqueueWaveBuffer(constructSound);
						static Random random;
						v->activitySound.SetPitch(1.f + random.NextFloat(-0.05f,0.05f));
						v->activitySound.SetGain(0.5f);
					}
					if (!v->activitySound.IsPlaying())
						v->activitySound.Play();
					v->activitySound.SetPosition(v->GetWorldPosition());
					v->activitySound.SetGain((v->GetSoundLevel()));
				}
				else
					if (v->activitySound.IsPlaying())
						v->activitySound.Stop();
			}

		#endif

		GlobalEvents::invalidateObjectCache();
		if (RuntimeConfig::IsInAnyCommandPhase())
			PlayerControlBegin::CheckAutoNextRound();
	}


	/*virtual override*/ D::String		MobileState::Describe() const
	{
		return "MobileState("+ToString(targetEntityID)+")";
	}


	/**/					MobileState::MobileState(Protocol::MobileState&s):Super(s.entityState.id)
	{
		//state.swap(s);
		state = std::move(s);
	}


	/*virtual override*/ void			MobileState::Begin( bool instant )
	{
		auto allocatedTarget = Objects::GetEntity(targetEntityID);
		if (allocatedTarget) //probably got whooped otherwise
		{
			EXPECT1(allocatedTarget,targetEntityID);
			Apply(To<Objects::MobileEntity>::DynamicCast(allocatedTarget),state,false,false);
		}
		else
			LogEvent(EventSignificance::NonFatalError,"Unable to update mobile entity "+ToString(targetEntityID)+" at t="+String(currentTime));
	}


}
