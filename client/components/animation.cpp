#include <global_root.h>
#include "animation.h"
#include "network.h"
#include "ui.h"
#include "world.h"
#include "clientRuntimeConfig.h"
#include "ai.h"
#ifdef HAS_WINDOW
	#include "explosions.h"
#endif


void			SignalMarkerDeletion(TUniqueID markerID);
void			OnPlayerControlBegin();


namespace Animation
{
	typedef Buffer0<TVec4<> >	Trajectory;
	Trajectory					activeTrajectory;
	float						projectileAt;	//[0,1], specifies where, along @a activeTrajectory, the projectile currently is
	bool						projectileActive=false;

	#ifdef HAS_WINDOW
		Objects::Geometry			trajectoryGeometry;
		DX11::InstructionBlock32	trajectoryVisualization;
		DX11::ObjectBuffer<float4>	trajectoryProgression,trajectoryTint;
		OpenAL::Source				/*trajectoryFireSource[1],*/missileHissSource;
		float						maxProjectileProjection;
		OpenAL::Wave				artilleryShot,missileHiss,constructSound;
	#endif

	float						atTime = 0.f;




	bool NoBlock(const Grid::LCellIdI&cell)
	{
		return false;
	}

	bool EstimateThreatLevel(const Objects::PWorldEntity&self,   Grid::LCellIdI&location, float&closestCompetition, float&closestAlly, float&closestGhost)
	{
		const TVec3<>&reference = Grid::low.cells[location.stackID].centerVertex.xyz;
		TVec3<> world = reference * Grid::ToRadius(location.height);

		bool canDo = true;
		Objects::ForeachWorldEntity([&canDo,world,reference,self,&closestCompetition,&closestAlly,&closestGhost](const Objects::PWorldEntity&e)
		{
			if (!canDo)
				return;
			if (e == self)
				return;

			if (!e->isMine)
			{
				float d2 = Vec::quadraticDistance(world,e->system.w.xyz);
				if (d2 < sqr(e->GetScanningRange()))
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


			if (e->IsMobile() && e->health > 0)
			{
				Objects::PMobileEntity o = To<Objects::MobileEntity>::Cast(e);
				bool surveyor = o->IsVehicle() && To<Objects::Vehicle>::Cast(o)->IsAutomatedResourceDetector();
				bool constructor = o->GetOperationSpeed(Objects::Component::Construct) > 0;
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

			if (e->health <= 0)
			{
				float d = Vec::quadraticDistance(e->system.w.xyz,world);
				if (d < closestGhost)
					closestGhost = d;
				if (d < 2.f)
					canDo = false;
			}
		});
		return canDo;
	}




	void			UpdateAutomation(const Objects::PVehicle&v)
	{
		static const float maxGhost = 30.f;

		ASSERT__(v);
		ASSERT__(v->isMine);
		ASSERT__(v->turret->detectsResources);
		if (v->IsHibernating())
			return;
		if (v->myTasks.IsNotEmpty())
		{
			if (v->myTasks.Peek().type == Objects::Vehicle::Task::MoveTo && v->myTasks.Count() == 1)
			{
				Grid::LCellIdI last = v->GetLastPointOfOrigin();
				if (!Grid::Resource::all.first().known[last.stackID])
				{
					float competition = 100.f;
					float ally = 100.f;
					float ghost = sqr(maxGhost);
					bool canDo = true;
					foreach (v->myTasks.GetNewest().path,node)
					{
						if (!EstimateThreatLevel(v,node->cell,competition,ally,ghost) || !Objects::NothingBlockingHere(node->cell,true))
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
		SignalDropAllTasks(v);
		v->myTasks.Clear();	//will happen anyways

		int referenceSpeed = v->GetMovementStepsPerRound()-1;

	//	static Buffer<Objects::MapNode>	nodes;
		static Objects::ReachabilityMapper mapper(false);
		Objects::Component::VehicleBase::canStandOnUnknown = false;
		mapper.Map(v.get(),v->location,AI::BlockTest);
		Objects::Component::VehicleBase::canStandOnUnknown = true;


		float closestDistance = std::numeric_limits<float>::max();
		const Objects::ReachabilityMapper::MapNode*closest = nullptr;
		//Grid::LCellIdI closest = Grid::LCellIdI::Invalid;
		//float closestCompetingDistance = 0;

		const bool*known = Grid::Resource::all.first().known;

	
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
				float ghost = sqr(maxGhost);


				if (!(Grid::low.cells[node->cell.stackID].cornerFlags & Grid::empZonesUnlocked))
					continue;


				bool canDo = EstimateThreatLevel(v,node->cell,competition,ally,ghost);


				if (!competition || !canDo || !ghost)
					continue;
				//const auto &n = Grid::low.grid.GetCell(node->cell.stackID).neighborhood;
				int score = 0;
				Objects::ReachabilityMapper::MapNode*at = node;
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
						score += CeilDiv(at->customScore,2);	//consider half
					else
						score += CeilDiv(at->customScore,3);	//consider one third
					if (at->predecessor == InvalidIndex)
						break;
					at = mapper.result + at->predecessor;
				}
				float ghostFear = sqr(maxGhost) / ((ghost));
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



	/**
	This method is called as the first instruction possible during command mode
	*/
	void UpdateAutomation(const TCodeLocation&loc)
	{
	//	ASSERT_NOT_EQUAL__(RuntimeConfig::roundNumber,Network::dbgRoundNumber);
		//Network::dbgRoundNumber = RuntimeConfig::roundNumber;



		if (RuntimeConfig::aiMode)
		{
			//LogEvent(EventSignificance::Debug,nullptr,"Automation issued by "+String(loc.ToString()));
			AI::vehicles.Clear();
		}
		foreach(Objects::vehicles, _v)
		{
			Objects::PVehicle v = *_v;
			if (v->isMine)
			{
				if (v->isAutomated && v->turret->detectsResources)
					UpdateAutomation(v);
				else
					if (RuntimeConfig::aiMode)
					{
						AI::vehicles << v;
					}
			}
		}
		if (RuntimeConfig::aiMode)
		{
			DBG_ASSERT__(!Network::newRoundRequested);
			AI::SignalStart("UpdateAutomation",loc);
		}
	}




	bool						LocateProjectile(float3&outPosition, float3&out);


	void						Create()
	{
		#ifdef HAS_WINDOW
			trajectoryTint.Create(float4(1));
			trajectoryProgression.Create(float4(1));
			DX11::Shader::Binding bindings[] =
			{
				DX11::Shader::Binding(trajectoryTint, "Tint"),
				DX11::Shader::Binding(trajectoryProgression, "Progression"),
			};
			trajectoryVisualization.binding = Objects::stdVehicleShader.binding;
			trajectoryVisualization.shader.LoadFromFile("shaders/projectilePath.hlsl", "vsMain", "psMain", bindings, ARRAYSIZE(bindings));
			trajectoryVisualization.UpdateLayout();
			trajectoryVisualization.SetTriangles();

			artilleryShot = OpenAL::Load("resources/objectSounds/artilleryShot.wav");
			//for (index_t i = 0; i < ARRAYSIZE(trajectoryFireSource); i++)
			//{
			//	trajectoryFireSource[i].Create();
			//	trajectoryFireSource[i].SetPitch(1.f - 0.01f * i);
			//	trajectoryFireSource[i].SetReferenceDistance(10.f);
			//}

			constructSound = OpenAL::Load("resources/objectSounds/construct.wav");
			missileHiss = OpenAL::Load("resources/objectSounds/missileHiss.wav");
			missileHissSource.Create();
			#ifdef _DEBUG
				missileHissSource.name = "missileHiss";
			#endif
			missileHissSource.SetReferenceDistance(10.f,0.5f);
			missileHissSource.SetWaveBuffer(missileHiss);
			missileHissSource.SetLooping(true);

		#endif
	}

	void						Destroy()
	{
		#ifdef HAS_WINDOW
			trajectoryVisualization = DX11::InstructionBlock32();
			trajectoryProgression.Clear();
			trajectoryTint.Clear();
			//for (index_t i = 0; i < ARRAYSIZE(trajectoryFireSource); i++)
			//	trajectoryFireSource[i].Destroy();
			missileHissSource.Destroy();
			OpenAL::Discard(missileHiss);
			OpenAL::Discard(constructSound);
			OpenAL::Discard(artilleryShot);
		#endif
	}


	bool	Equals(const Objects::PVehicleJob&a, const Objects::PVehicleJob&b)
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

	//a=serverVersion, b = localVersion
	bool	Mergeable(const Objects::MobileEntity::Task&a, const Objects::MobileEntity::Task&b)
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

	void	Merge(const Objects::MobileEntity&parent, const Grid::LCellIdI&pointOfOrigin, Objects::MobileEntity::Task&serverVersion, Objects::MobileEntity::Task&localVersion, Objects::MobileEntity::Task&outMerged)
	{
		ASSERT__(Mergeable(serverVersion,localVersion));


		if (serverVersion.foci == localVersion.foci)
		{
			outMerged.swap(localVersion);
			return;
		}
		outMerged.mode = localVersion.mode;
		outMerged.job = localVersion.job;
		#ifdef HAS_WINDOW
			outMerged.localizableShapes.adoptData(localVersion.localizableShapes);
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


	
	void	TranslateTask(const Objects::MobileEntity&e, const Protocol::MobileTask&sourceTask, Objects::MobileEntity::Task&t1)
	{
		foreach (sourceTask.foci,f)
			t1.foci.Push().target = *f;
		t1.mode = (Objects::Vehicle::mode_t) sourceTask.mode.value;
		t1.name = sourceTask.name;
		t1.type = (Objects::Vehicle::Task::type_t) sourceTask.type.value;
		if (t1.type == Objects::Vehicle::TaskData::Job)
		{
			if (sourceTask.buildPath.IsNotEmpty())
			{
				std::shared_ptr<Objects::VehicleJob::DummyPathJob>	job(new Objects::VehicleJob::DummyPathJob(e));
				job->path.Clear(sourceTask.buildPath.length());
				foreach (sourceTask.buildPath,p)
					job->path << *p;
				t1.job = job;
			}
			else
			{
				std::shared_ptr<Objects::VehicleJob::DummyJob>	job(new Objects::VehicleJob::DummyJob(e));
				t1.job = job;
			}
			t1.job->movesToFocusWhenDone = sourceTask.visualizePathToFocus;
			t1.job->roundsToJobCompletion = sourceTask.roundsToJobCompletion;
			t1.job->mineralCostLeft = sourceTask.mineralCostLeft;
			#ifdef HAS_WINDOW
				if (sourceTask.foci.Count())
				{
					Grid::LCellIdI loc = sourceTask.foci.first();
					if (sourceTask.visualizationStructureName != ConstName::Empty && e.IsSatellite())
					{
						int h = Objects::SkyHub::GetStructureBuildHeight(loc);
						if (h != Objects::badHeight)
							loc.height = h;
					}

					t1.SetLocalizableShape(sourceTask.visualizationStructureName,loc);
				}
			#endif
		}
		//t1.Recompile(v->GetPrevPointOfOrigin(),v.get());
	}


	static GenericHashSet<TUniqueID,TUniqueID> insertIntoGridLater;

	void ApplyState(const Objects::PMobileEntity&v, Protocol::MobileState&vs, bool isNew, bool isNewForEMP)
	{
		ASSERT__(v);
		int oldHeight = v->location.height;

		bool locationChanged = v->storedIn.expired() 
								&& v->location != vs.entityState.location 
								&& vs.entityState.location != Grid::LCellIdI::Invalid;
		if (locationChanged)
		{
			LogEvent(EventClass::Preset::debug,v.get(), "ApplyState(): Updating location: "+v->location.ToString()+"->"+vs.entityState.location.ToString());
			ASSERT3__(v->preAnimationLocation == v->location, v->GetName(), v->preAnimationLocation.ToString(), v->location.ToString());
			v->Relocate(vs.entityState.location,false);	//this moves sensors. not good during animation phase. however, this is a rare and isolated process (not part of a sequence of updates)
		}
		v->health = vs.entityState.health;
		v->unstable = (vs.entityState.entityStateFlags & Protocol::EntityStateFlag::IsUnstable) != 0;
		v->ApplySleepChangeFromServer( (vs.entityState.entityStateFlags & Protocol::EntityStateFlag::IsSleeping) != 0 );

		v->UpdateEMPDamage(vs.entityState.empDamageLevel, !isNewForEMP);	//must remove visible sensory, must be linked to destructor
		//v->roundsDisabled = vs.entityState.disabledRounds;

		#ifndef NO_HIBERNATION
			bool hibernating = (vs.flags.value & Protocol::VehicleFlags::Hibernating) != 0;
			v->Hibernate(hibernating,0);
		#endif

		bool automationChange = false;
		if (v->IsVehicle())
		{
			Objects::PVehicle&veh = To<Objects::Vehicle>::Cast(v);
			bool newAutomated = (vs.flags.value & Protocol::VehicleFlags::Automated) != 0;
			automationChange = newAutomated != veh->isAutomated;
			veh->isAutomated = newAutomated;
		}

		Objects::PVehicleStore storedIn;
		auto info = vs.myMobileEntity.Get0();
		if (info)
		{
			#ifndef ONE_COMPOUND_PER_FACTION
				v->currentStorage = info->resourceVector;
			#endif
			v->constructionRoundCounter = info->constructionRoundCounter;

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
							currentTasks.erase(index_t(0));
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
				storedIn = To<Objects::VehicleStore>::DynamicCast(Objects::GetEntity(id->value));
				ASSERT1__(storedIn,id->value.ToString());

				
			}

			Objects::PMobileEntityProducer p = To<Objects::MobileEntityProducer>::DynamicCast(v);
			if (p)
			{
				const Protocol::ProducerState*state = info->producerState.Get0();
				if (state)
				{
					p->queue.Clear();
					p->productionDone = state->productionDone;
					p->gatheringPoint.Set(state->gatheringPoint,v);
					
					foreach (state->productionTasks,t)
					{
						switch (t->parameters.Count())
						{
							case 1:
								p->AppendTask(t->id,t->parameters.first(),ConstName::Empty);
							break;
							case 2:
								p->AppendTask(t->id,t->parameters.first(),t->parameters.last());
							break;
							default:
								FATAL__("Unexpected parameter count: "+String(t->parameters.Count())
											+" while decoding task "+t->id.value.ToString()
											+" of "+v->GetName());
						}
						//ASSERT_EQUAL__(t->parameters.Count(),2);
							
					}
				}
			}




		}
		else
		{
			//v->currentStorage
		}
		Objects::PVehicleStore currentlyStoredIn = To<Objects::VehicleStore>::DynamicCast(v->storedIn.lock());
		if (storedIn != currentlyStoredIn)
		{
			LogEvent(EventSignificance::Debug,v.get(),"StoredIn-change ("+currentlyStoredIn->GetName()+" -> "+storedIn->GetName()+")");
			float f = 0;

			Objects::PVehicle veh = To<Objects::Vehicle>::DynamicCast(v);
			ASSERT1__(veh,v->GetName());

			if (storedIn)
			{
				if (currentlyStoredIn)
					currentlyStoredIn->Lose(veh);
				else
					insertIntoGridLater.Unset(v->id);
				storedIn->StoreLocally(veh,f);
				ASSERT1__(!v->isInsertedIntoGrid,v->GetName());

				#ifdef HAS_WINDOW
					if (!currentlyStoredIn)
						UI::UpdateTraffic();
				#endif

			}
			else
			{
				ASSERT1__(vs.entityState.location != Grid::LCellIdI::Invalid,v->GetName());
				ASSERT1__(!v->isInsertedIntoGrid,v->GetName());
				currentlyStoredIn->LocalCheckout(veh,vs.entityState.location,f);
				LogEvent(EventSignificance::Debug,v.get(),"Checked out to "+vs.entityState.location.ToString());
				ASSERT1__(v->location == vs.entityState.location,v->GetName());
				ASSERT1__(v->isInserted,v->GetName());
				ASSERT1__(Objects::vehicles.Contains(v),v->GetName());
				v->UpdateSystem();
				#ifdef HAS_WINDOW
					UI::UpdateTraffic();
				#endif
			}
			UI::HexGrid::RefreshContacts();
		}
		//else
			//LogEvent(EventSignificance::Debug,v.get(),"StoredIn unchanged ("+currentlyStoredIn->GetName()+")");

		v->attackActionLeft = vs.attackActionLeft;
		v->movementActionLeft = vs.movementActionLeft;
		if (locationChanged || isNew)
			v->UpdateSystem();

		#ifdef HAS_WINDOW
			if (v == Objects::selectedEntity)
			{
				OnSelectionChange(true);
				Objects::UpdateChoices(Network::ControlEnabled());
			}
		#endif

		if (automationChange && v->isMine && v->IsVehicle() && To<Objects::Vehicle>::Cast(v)->IsAutomatedResourceDetector())
		{
			UpdateAutomation(To<Objects::Vehicle>::Cast(v));
		}

		#ifdef HAS_WINDOW
			if (storedIn != currentlyStoredIn)
			{
				Objects::VehicleStore::StaticSignalDeployment();
				UI::HexGrid::Configure(Objects::selectedEntity);
			}


			if (!v->turret->CanAttack())
			{
				if (v->myTasks.IsNotEmpty() && v->myTasks.Peek().IsJob() && !v->IsSleeping())
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
						v->activitySound.SetPitch(1.f + random.getFloat(-0.05f,0.05f));
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

	}

	void	UpdateState(const Objects::PStructure&s,Protocol::StructureState&vs)
	{
		Objects::PWorkingStructure w = To<Objects::WorkingStructure>::DynamicCast(s);
		if (w)
		{
			w->SetResourceStorage(vs.resourceVector);
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
					p->gatheringPoint.Set(state->gatheringPoint,s);
					
					foreach (state->productionTasks,t)
					{
						switch (t->parameters.Count())
						{
							case 1:
								p->AppendTask(t->id,t->parameters.first(),ConstName::Empty);
							break;
							case 2:
								p->AppendTask(t->id,t->parameters.first(),t->parameters.last());
							break;
							default:
								FATAL__("Unexpected parameter count: "+String(t->parameters.Count())
											+" while decoding task "+t->id.value.ToString()
											+" of "+s->GetName());
						}
						//ASSERT_EQUAL__(t->parameters.Count(),2);
							
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
						//ASSERT_EQUAL__(t->parameters.Count(),2);
						t->AppendTask(*at);
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
						n->resistence = *state;
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
						}
					}
				}
			
			}
		}
		s->unstable = (vs.entityState.entityStateFlags & Protocol::EntityStateFlag::IsUnstable) != 0;
		s->ApplySleepChangeFromServer( (vs.entityState.entityStateFlags & Protocol::EntityStateFlag::IsSleeping) != 0 );


		s->health = vs.entityState.health;
		s->empDamageLevel = vs.entityState.empDamageLevel;

		if (vs.entityState.location != Grid::LCellIdI::Invalid && vs.entityState.location != s->location)
		{
			s->location = vs.entityState.location;	//very unusual
			s->UpdateAutomaticSystem();
			Objects::RefreshCompoundCoverage(0);
		}

		if (s->IsSkyPort())
		{
			Objects::PSatellite sat = Objects::GetSatellite(s->location.stackID);
			if (sat && sat->IsSkyHub())
			{
				Objects::PSkyHub hub = To<Objects::SkyHub>::Cast(sat);
				hub->UpdateConnections();
			}
		}


		if (w && w->isInserted)
		{
			#ifndef ONE_COMPOUND_PER_FACTION
				ASSERT__(s->compoundID != Objects::Compound::InvalidNetwork);
			#endif
			s->GetCompound().RefreshState(0);
		}

		#ifdef HAS_WINDOW
			OnSelectionChange(true);	//could be affecting selected network
			if (s == Objects::selectedEntity)
			{
				Objects::UpdateChoices(Network::ControlEnabled());
			}
			//UI::HexGrid::Configure(Objects::selectedEntity);
		#endif

	}

	float GetExplosionSize(const Objects::PWorldEntity&target)
	{
		return target->IsSatellite() ? 12.f : 4.f;
	}

	float GetExplosionTime(const Objects::PWorldEntity&target)
	{
		return Objects::ExplosionTime(GetExplosionSize(target)/4.f);
		//target->IsSatellite() ? 4.f : 1.3f;
	}

	void ExplodeEntity(const Objects::PWorldEntity&target)
	{
		#ifdef HAS_WINDOW
			Explosions::InitExplosion(target->system.w.xyz + Grid::low.cells[target->location.stackID].centerVertex.xyz * Grid::SingleLayerHeight*0.5f ,false,GetExplosionSize(target), GetExplosionTime(target));
		#endif
	}
	void ExplodeEntity(const Objects::PWorldEntity&target, const std::function<void()>&onQuarterDone)
	{
		#ifdef HAS_WINDOW
			Explosions::InitExplosion(target->system.w.xyz + Grid::low.cells[target->location.stackID].centerVertex.xyz * Grid::SingleLayerHeight*0.5f,false, GetExplosionSize(target), GetExplosionTime(target), onQuarterDone);
		#else
			onQuarterDone();
		#endif
	}


	void					Primitive::Init(Protocol::WorldDelta&delta)
	{
		type = TerrainChange;
		time.SetMinAndMax(delta.timestamp.atTime);
		worldDelta.reset(new DeltaField());
		worldDelta->Append().swap(delta);
		//data.adoptData(delta.data);
		//worldDelta->family = delta.family;
		//worldDelta->timestamp = delta.timestamp;
	}

	void					Primitive::Init(Protocol::StructureConstruction&sc, const Objects::PStructure&s)
	{
		type = Construction;
		allocatedTarget = s;
		time.SetMinAndMax(sc.state.entityState.timestamp.atTime/*-0.01f*/);
	}

	void					Primitive::Init(Protocol::NewMobileEntity&sc, const Objects::PMobileEntity&v)
	{
		type = NewMobileEntity;
		allocatedTarget = v;
		time.SetMinAndMax(sc.state.entityState.timestamp.atTime/*-0.01f*/);
	}
	void					Primitive::Init(Protocol::MobileState&vs)
	{
		type = MobileState;
		//targetID = vs.entityState.id;	//is in vehicleState
		vehicleState.reset(new Protocol::MobileState(vs));
		targetEntityID = vs.entityState.id;
//			target = Objects::GetEntity(vs.entityState.id,true);
		time.SetMinAndMax(vs.entityState.timestamp.atTime);
	}
	void					Primitive::Init(Protocol::StructureState&vs)
	{
		type = StructureState;
		structureState.reset(new Protocol::StructureState(vs));
		targetEntityID = vs.entityState.id;
		//target = Objects::GetEntity(vs.entityState.id,true);
		//ASSERT1__(target,vs.entityState.id.value.ToString());
		time.SetMinAndMax(vs.entityState.timestamp.atTime);
	}
	void					Primitive::Init(Protocol::MobileEntityMotion&motion/*, const Objects::PVehicle&v*/)
	{
		type = MovementStream;
		//target = v;
		path.reset(new Path());
		path->rawPath.Append(std::make_pair(motion.fromLocation,motion.timestamp.atTime));
		path->rawPath.Append(std::make_pair(motion.toLocation,motion.timestamp.atTime + motion.timeNeeded));
		path->changed = true;
		targetEntityID = motion.id;
		//LogEvent(EventSignificance::Debug,nullptr,"Initiating movement stream at time "+String(motion.timestamp.atTime.value));
		time.Set(motion.timestamp.atTime, motion.timestamp.atTime + motion.timeNeeded);
	}
	void				Primitive::Append(Protocol::MobileEntityMotion&motion)
	{
		ASSERT__(path);
		ASSERT__(path->rawPath.last().first == motion.fromLocation);
		ASSERT2__(targetEntityID == motion.id,targetEntityID.ToString(),motion.id.value.ToString());
		path->rawPath.Append(std::make_pair(motion.toLocation,motion.timestamp.atTime + motion.timeNeeded));
		path->changed = true;
		time.Include(FloatRange<float>(motion.timestamp.atTime,motion.timestamp.atTime + motion.timeNeeded));
	}
	void					Primitive::Init(Protocol::DestroyEntity&e)
	{
		type = DestroyEntity;
		targetEntityID = e.id;
		//target = Objects::GetEntity(e.id,true);
		explode = e.type == Objects::RemovalType::ControlledExplosion || e.type == Objects::RemovalType::UncontrolledExplosion;
		removalType = e.type;
		//ASSERT__(target);
		time.SetMinAndMax(e.timestamp.atTime/*+0.01f*/);
		LogEvent(EventSignificance::Debug,nullptr,"Registered removal of entity "+e.id.value.ToString()+" ("+Objects::RemovalType::Reinterpret(e.type.value).ToString()+") at t="+time.ToString());
	}
	void					Primitive::Init(Protocol::VagueAttackTrajectory&a)
	{
		type = Attack;
		targetEntityID = TUniqueID::Empty();
		attackerTurret = Objects::Component::GetTurretByName(a.turretType);
		ASSERT_NOT_NULL1__(attackerTurret,(int)a.turretType.value);
		attackRange = a.turretRange;
		attackerPosition = a.sourcePosition;
		explosionCenter = Grid::ToWorld(a.traceTo);
		isAutoTarget = false;

		location = a.traceTo;
		time.Set(a.timestamp.atTime,a.timestamp.atTime+a.flightTime);
	}
	void					Primitive::Init(Protocol::PreciseAttackTrajectory&a)
	{
		type = Attack;
		targetEntityID = a.attacker;
		attackerTurret = nullptr;	//later
		explosionCenter = Grid::ToWorld(a.traceTo);

		isAutoTarget = a.isAutoAttack;
		location = a.traceTo;
		time.Set(a.timestamp.atTime,a.timestamp.atTime+a.flightTime);
	}
	void					Primitive::Init(Protocol::Explosion&ex)
	{
		type = Explosion;
		explosionCenter = ex.center.value;
		explosionRadius = ex.radius;
		empExplosion = ex.isEMP;
		time.min = ex.timestamp.atTime;
		time.max = time.min + ExplosionTime;
	}

	void					Primitive::Init(Protocol::EntityTurretRotation&rot)
	{
		type = TurretRotation;
//			Objects::PWorldEntity e = Objects::GetEntity(rot.id,true);
		//Objects::PVehicle v = To<Objects::Vehicle>::DynamicCast(e);
		//Objects::PVehicle t = To<Objects::Vehicle>::DynamicCast(e);
//		float dummy;
	//	ASSERT__(e->GetTurretBase(dummy));
		//target = e;
		targetEntityID = rot.id;
		time.min = rot.timestamp.atTime;
		time.max = time.min + rot.rotationTime;
		rotateToWorld = rot.toWorld;

	}

	void					Primitive::Init(Protocol::ResourceUpdate&up)
	{
		type = ResourceUpdate;
		resourceUpdate.reset(new Protocol::ResourceUpdate());
		resourceUpdate->cells.adoptData(up.cells);
		resourceUpdate->resourceName.adoptData(up.resourceName);
		//Protocol::ResourceUpdate&u
		time.min = up.timestamp.atTime;
		time.max = time.min;
	}


	#ifdef _DEBUG
		float dbgLastRelative;
	#endif

	void				Primitive::Begin()
	{
		#ifdef _DEBUG
			dbgLastRelative = 0.f;
		#endif
		switch (type)
		{
			case Construction:
			{
				LogEvent(EventSignificance::Common,allocatedTarget.get(),  "Placing structure entity during animation at time "+String(Animation::atTime)+"/"+this->time.ToString());
				Objects::Place(To<Objects::Structure>::DynamicCast(allocatedTarget));
				if (!allocatedTarget->isMine)
					UI::HexGrid::RefreshContacts();
			}
			break;
			case NewMobileEntity:
			{
				Objects::PMobileEntity v = To<Objects::MobileEntity>::DynamicCast(allocatedTarget);
				if (v)
				{
					LogEvent(EventSignificance::Common,allocatedTarget.get(),  "Placing mobile entity during animation at time "+String(Animation::atTime)+"/"+this->time.ToString());
					//MoveSensor(v->preAnimationLocation,v->location,v->turret->scanningRange);
					v->preAnimationLocation = v->location;
					Objects::PlaceMobileEntity(v,false);
					if (v->IsVehicle())	//only vehicles get to be inserted anyhow
						insertIntoGridLater.Set(v->id);
					if (!allocatedTarget->isMine)
						UI::HexGrid::RefreshContacts();
				}
			}
			break;
			case DestroyEntity:
			{
				allocatedTarget = Objects::GetEntity(targetEntityID);
				ASSERT1__(allocatedTarget,targetEntityID.ToString());
				if (explode)
					ExplodeEntity(allocatedTarget);
				bool removed = false;
				LogEvent(EventSignificance::Common,allocatedTarget.get(),  "Removing entity during animation at time "+String(Animation::atTime)+"/"+this->time.ToString());
				//if (RuntimeConfig::phase == RuntimeConfig::Phase::AnimationPhase)
				{
					Objects::PVehicle v = To<Objects::Vehicle>::DynamicCast(allocatedTarget);
					if (v)
					{
						if (!insertIntoGridLater.IsSet(v->id) && removalType != Objects::RemovalType::DestroyedInStorage)
							Objects::RemoveVehicleFromGrid(v,v->preAnimationLocation);
						removed = true;	//is set anyway. placeVehicleLater indicates it is already removed
					}
				}
				if (allocatedTarget->IsMobile())
				{
					Objects::PMobileEntity v = To<Objects::MobileEntity>::Cast(allocatedTarget);
					if (v->preAnimationLocation != v->location)
					{
						float t = 0.f;
						v->MoveSensory(v->preAnimationLocation,v->location, t);
						//v->RegisterSensory(t);
					}
				}


				Objects::Remove(allocatedTarget,Objects::RemovalType::Reinterpret(removalType),0,Network::ControlEnabled(),removed);
				if (!allocatedTarget->isMine)
					UI::HexGrid::RefreshContacts();
				#ifdef HAS_WINDOW
					Dialog::SignalSituationChange();
					if (removalType == Objects::RemovalType::UncontrolledExplosion)
						UI::EventLog::RegisterEntityDestruction(this->time.min, allocatedTarget->isMine,allocatedTarget->GetWorldPosition(), allocatedTarget->id, allocatedTarget->GetNiceName());

				#endif
			}
			break;
			case MovementStream:
			{
				allocatedTarget = Objects::GetEntity(targetEntityID);
				LogEvent(EventSignificance::Debug, allocatedTarget.get() ,"Starting movement stream at "+String(Animation::atTime)+"/"+this->time.ToString());
				ASSERT1__(allocatedTarget,targetEntityID.ToString());
				Objects::PMobileEntity v = To<Objects::MobileEntity>::DynamicCast(allocatedTarget);
				ASSERT1__(v,allocatedTarget->GetName());

				if (!insertIntoGridLater.IsSet(v->id))
				{
					ASSERT2__(v->preAnimationLocation == v->location ,v->preAnimationLocation.ToString(),v->location.ToString());
					//#ifdef HAS_WINDOW
					//	if (v->isMine && v->IsVehicle())
					//		ASSERT1__(Grid::actualWorld.HasSensor(v->location,v->GetScanningRange()),v->id.ToString());
					//#endif
					v->preAnimationLocation = v->location;
					if (v->IsVehicle())
						Objects::RemoveVehicleFromGrid(To<Objects::Vehicle>::Cast(v),v->location);
					insertIntoGridLater.Set(v->id);
				}
				path->Rebuild(v);
			}
			break;
			case TurretRotation:
			{
				//Objects::PVehicle v = To<Objects::Vehicle>::DynamicCast(target);
				//Objects::PTower t = To<Objects::Tower>::DynamicCast(target);
				//ASSERT__(v);
				allocatedTarget = Objects::GetEntity(targetEntityID);
				if (allocatedTarget)
				{
					float dummy;
					Objects::PTurretBase b = allocatedTarget->GetTurretBase(dummy);
					ASSERT__(b);

					#ifdef HAS_WINDOW
						if (!b->activitySound.IsCreated())
						{
							b->activitySound.Create();
							b->activitySound.SetLooping(true);
							#ifdef _DEBUG
								b->activitySound.name = allocatedTarget->GetName()+".turretRotate";
							#endif
						}
						b->activitySound.SetWaveBuffer(Objects::Renderer::GetAmbientSoundOf("turretRotate",ConstName::Turret_Artillery));
						b->activitySound.SetPosition(allocatedTarget->GetWorldPosition());
						b->activitySound.SetPitch(0.7f);
						b->activitySound.Play();
					#endif

					float3 dir = rotateToWorld;
					float3 innerDir;
					Mat::rotate(allocatedTarget->inverse,dir,innerDir);
					rotateTo.x = Vec::angle2PI(innerDir.xy) + M_PIF/2.f;
					rotateTo.y = -(90.f - Vec::angle(rotateToWorld,Grid::low.cells[allocatedTarget->location.stackID].centerVertex.xyz)) * M_PIF / 180.f;

					rotateFrom = float2(b->turretAngleX,b->turretAngleY);
					while (rotateFrom.x - rotateTo.x > M_PIF) rotateFrom.x -= M_PIF*2.f;
					while (rotateFrom.x - rotateTo.x < -M_PIF) rotateFrom.x += M_PIF*2.f;
				}
				else
				{
					LogEvent(EventClass::Preset::error,nullptr,"Warning: Unable to identify entity "+targetEntityID.ToString()+" for turret rotation");
					/*let's ignore this for now. this event is caused, when visibility of
					its target has been determined but at a latter time by some vehicle.
					during the motion phase of a different vehicle an automated attack is caused.
					since visualization time and processing time are not identical, this can lead
					to a temporal distortion. should be minor security violation. the player is
					notified of a turret rotation that they can't actually see yet.
					the same goes for attacks, i guess
					*/
				}
			}
			break;
			case Attack:
			{
			//	allocatedTarget = Objects::GetEntity(targetEntityID);
				//if (allocatedTarget)
				bool isFriendly = false;
				bool doRegister = true;
				#ifdef HAS_WINDOW
					TUniqueID attackerID = TUniqueID::Empty();
				#endif
				if (!attackerTurret)
				{
					Objects::PWorldEntity attacker = Objects::GetEntity(targetEntityID);
					ASSERT2__(attacker,targetEntityID.ToString(),this->time.ToString());
					float zOffset;
					Objects::PTurretBase base= attacker->GetTurretBase(zOffset);
					ASSERT2__(base,attacker->GetName(),this->time.ToString());
					attackerTurret = base->turret;
					ASSERT_NOT_NULL1__(attackerTurret,attacker->GetName());
					attackRange = base->GetFlatAttackRange();
					attackerPosition = attacker->GetWorldPosition();
					attackerPosition += Grid::low.cells[attacker->location.stackID].cornerCenter * zOffset;
					#ifdef HAS_WINDOW
						attackerID = attacker->id;
					#endif
					doRegister = !attacker->isMine || isAutoTarget;
					isFriendly = attacker->isMine;
				}

				LogEvent(EventSignificance::Debug, allocatedTarget.get() ,"Starting attack ("+attackerTurret->typeName+",range="+String(attackRange)+",loc="+location.ToString()+") at "+String(Animation::atTime)+"/"+String(this->time.min));
				
				{
					Objects::Component::Turret::TraceTrajectory(attackerTurret->trajectoryType,attackRange, attackerPosition,Grid::ToWorld(location),activeTrajectory);
					explosionRadius = attackerTurret->explosionRadius;
					projectileAt = 0.f;
					projectileActive = true;

					#ifdef HAS_WINDOW
						trajectoryGeometry.Clear();
						#ifdef _DEBUG
							LogEvent(EventSignificance::Debug, allocatedTarget.get() ,"Compiling trajectory with "+String(activeTrajectory.Count())+" steps, starting from "+Vec::toString(activeTrajectory.first())+", ending "+Vec::toString(activeTrajectory.last()));
						#endif
						maxProjectileProjection = Objects::Component::Turret::CompileTrajectory(activeTrajectory,Objects::Component::Turret::Animation,trajectoryGeometry,0.5f);
						trajectoryVisualization.vbo.CreateFromStructs(trajectoryGeometry.GetLOD(0).vertices);
						trajectoryVisualization.ibo.Create(trajectoryGeometry.GetLOD(0).indices);


						index_t smokeTrailIndex = Objects::Renderer::RegisterSmokeTrail(activeTrajectory);
						if (doRegister)
						{
							UI::EventLog::RegisterAttack(this->time.min, isFriendly,attackerPosition, attackerID, smokeTrailIndex);
						}

						switch (attackerTurret->trajectoryType)
						{
							case Objects::TrajectoryType::Artillery:
							{
								OpenAL::PlayOnce(attackerPosition,artilleryShot,1.f,10.f);
								//for (index_t i = 0; i < ARRAYSIZE(trajectoryFireSource); i++)
								//{
								//	OpenAL::Source&s = trajectoryFireSource[i];
								//	s.SetPosition(attackerPosition);
								//	s.SetWaveBuffer(artilleryShot);
								//	s.Play();
								//}
							}
							break;
							case Objects::TrajectoryType::Missile:
							{
								missileHissSource.SetPosition(attackerPosition);
								missileHissSource.Play();
							}
						}

						

					#endif


				}
			}
			break;
			case Explosion:
			{
				#ifdef HAS_WINDOW
					Explosions::InitExplosion(explosionCenter,empExplosion,explosionRadius*4.f,Objects::ExplosionTime(explosionRadius));
				#endif
			}
			break;
		}

	}

	void			Primitive::End()
	{
		switch (type)
		{
			case  MovementStream:
			{
				LogEvent(EventSignificance::Debug, allocatedTarget.get() ,"Ending movement stream at "+String(Animation::atTime)+"/"+this->time.ToString());

				Objects::PMobileEntity v = To<Objects::MobileEntity>::DynamicCast(allocatedTarget);
				ASSERT__(v);
				path->ApplyTo(v,path->rawPath.last().second);
				//v->Relocate(path->rawPath.last().first,false);
				v->location = path->rawPath.last().first;
				//Objects::InsertVehicleIntoGrid(v,v->location);	will be done later
				if (v->isMine && v->preAnimationLocation != v->location)
				{
					float t = 0.f;
					v->MoveSensory(v->preAnimationLocation,v->location,t);

					//v->MoveWorldSensory(v->preAnimationLocation,v->location);
					//Grid::actualWorld.MoveSensor(v->preAnimationLocation,v->location,v->GetScanningRange());
				}
				v->preAnimationLocation = v->location;
				allocatedTarget.reset();	//for debugging purposes
				#ifdef HAS_WINDOW
					if (v->myTasks.IsNotEmpty())
					{
						auto&task = v->myTasks.Peek();
						while (task.foci.IsNotEmpty() && this->path->Contains(task.foci.Peek().target))
							task.foci.Pop();
						//this->path->nodes
						v->myTasks.Peek().Replot(v->location,v.get(),Objects::FocusChangeType::FirstSegmentChanged,true);
					}
				#endif
				//v->UpdateSystem();	//might bite me in the arse later
			}
			break;
			case Attack:
				LogEvent(EventSignificance::Debug, allocatedTarget.get() ,"Ending attack at "+String(Animation::atTime)+"/"+String(this->time.max));
				projectileActive = false;
				for (index_t i = 0; i < Objects::Marker::all.Count(); i++)
				{
					const Objects::Marker&m = Objects::Marker::all[i];
					if (m.type != Objects::Marker::Type::HostileContact)
						continue;
					if (Vec::quadraticDistance(Grid::ToWorld(m.location),explosionCenter) < sqr(explosionRadius))
					{
						LogEvent(EventSignificance::Common,nullptr,m.GetName()+ " considered near occuring explosion. Dropping marker");
						SignalMarkerDeletion(m.id);
						i--;
					}
				}
				#ifdef HAS_WINDOW
					missileHissSource.Stop();
				#endif
			break;
			case Explosion:
				for (index_t i = 0; i < Objects::Marker::all.Count(); i++)
				{
					const Objects::Marker&m = Objects::Marker::all[i];
					if (m.type != Objects::Marker::Type::HostileContact)
						continue;
					if (Vec::quadraticDistance(Grid::ToWorld(m.location),explosionCenter) < sqr(explosionRadius))
					{
						LogEvent(EventSignificance::Common,nullptr,m.GetName()+ " considered near occuring explosion. Dropping marker");
						SignalMarkerDeletion(m.id);
						i--;
					}
				}

				//auto-closed by Explosions-module
			break;
			case TurretRotation:
			{
				if (allocatedTarget)
				{
					float dummy;
					Objects::PTurretBase b = allocatedTarget->GetTurretBase(dummy);
					b->turretAngleX = rotateTo.x;
					b->turretAngleY = rotateTo.y;

					#ifdef HAS_WINDOW
						b->activitySound.Stop();
					#endif

				}
			}
			break;
		}
		path.reset();
	}

	Primitive::result_t			Primitive::Update(float t)
	{
		switch (type)
		{
			case MobileState:
				allocatedTarget = Objects::GetEntity(targetEntityID);
				if (allocatedTarget) //probably got whooped otherwise
				{
					ASSERT1__(allocatedTarget,targetEntityID.ToString());
					ApplyState(To<Objects::MobileEntity>::DynamicCast(allocatedTarget),*vehicleState,false,false);
				}
				else
					LogEvent(EventSignificance::NonFatalError,"Unable to update mobile entity "+targetEntityID.ToString()+" at t="+String(t));
			return Done;
			case StructureState:
				allocatedTarget = Objects::GetEntity(targetEntityID);
				if (allocatedTarget) //probably got whooped otherwise
				{
					ASSERT1__(allocatedTarget,targetEntityID.ToString());
					UpdateState(To<Objects::Structure>::DynamicCast(allocatedTarget),*structureState);
				}
				else
					LogEvent(EventSignificance::NonFatalError,"Unable to structure "+targetEntityID.ToString()+" at t="+String(t));
			return Done;
			case Construction:
			return Done;
			case NewMobileEntity:
			return Done;
			case DestroyEntity:
			return Done;
			case ResourceUpdate:
				Grid::UpdateResourceMap(resourceUpdate->resourceName,resourceUpdate->cells);
			return Done;
			case TerrainChange:
			{
				foreach (*worldDelta,block)
				{
					BYTE family = block->familyAndLastFlag & 0x7F;
					//bool last = (block->family & 0x80) != 0;
					Grid::actualWorld.ApplyFamilyDelta(family,block->data0,block->data1,false,true);
				}
				#ifdef HAS_WINDOW
					World::allowRecompilation = true;
				#endif
			}
			return Done;
		}
		float x = time.Relativate(t);

		//DBG_ASSERT_GREATER_OR_EQUAL__(x,dbgLastRelative );
		//#ifdef _DEBUG
		//	dbgLastRelative = x;
		//#endif

		if (x > 1.f)
			return Done;

		switch (type)
		{
			case Explosion:
				//explosion->progression = x;
			break;
			case Attack:
			{
				float y = x;//sqr((x - 0.5f) * 2.f) * sign(x-0.5f) * 0.5f + 0.5f;
				projectileAt = y;
				#ifdef HAS_WINDOW
					trajectoryProgression.Update(float4(y * maxProjectileProjection,1.f,5.f,1.f));

					index_t at = std::min(index_t(x * activeTrajectory.GetLength()),activeTrajectory.GetLength()-1);
					const TVec4<>&v = activeTrajectory[at];
					missileHissSource.SetPosition(v.xyz * v.w);
					float volume = sqr(sqr(1.f - x))*0.75f + 0.25f;
					//missileHissSource.SetGain(volume);
					missileHissSource.SetPitch(0.75f + x);

				#endif
			}
			break;
			case MovementStream:
				//LogEvent(EventSignificance::Debug,allocatedTarget.get(),"Animation stream updated at "+String(t));
				path->ApplyTo(To<Objects::Vehicle>::Cast(allocatedTarget),t);
				ASSERT_LESS_OR_EQUAL__(t,time.max);
			break;
			case TurretRotation:
			{
				if (allocatedTarget)
				{
					float dummy;
					Objects::PTurretBase b = allocatedTarget->GetTurretBase(dummy);
					TVec2<> angles;
					Vec::interpolate(rotateFrom,rotateTo,x,angles);
					//Objects::PVehicle v = To<Objects::Vehicle>::Cast(target);
					b->turretAngleX = angles.x;
					b->turretAngleY = angles.y;
					//b->rotationSound.SetPitch(0.5f + 0.3f * (sqrt((x))));

				}
			}
			break;
			
		}
			
		return Continue;
	}
	

	void Primitive::Path::Rebuild(const Objects::PMobileEntity&target)
	{
		changed = false;

		nodes.SetSize(rawPath.Count());
		for (index_t i = 0; i < rawPath.Count(); i++)
		{
			const Grid::LCellIdI&a = rawPath[i].first;
			nodes[i].position = Grid::low.cells[a.stackID].centerVertex.xyz * Grid::ToRadius(a.height);
		}

		for (index_t i = 0; i+1 < rawPath.Count(); i++)
		{
			TNode	&na = nodes[i],
					&nb = nodes[i+1];

			const Grid::LCellIdI&a = rawPath[i].first,
								&b = rawPath[i+1].first;
			const float3&p0 = na.position;
			const float3&p1 = nb.position;
			TVec3<> n,d = p1 - p0;
			Vec::cross(p0,d,n);
			na.normal += n;
			nb.normal += n*0.9f;
			na.dir += d;
			nb.dir += d*0.9f;
		}
		ASSERT__(nodes.IsNotEmpty());	//this shouldn't be possible
		{
			//Vec::makeNormalTo(nodes.first().dir,nodes.first().position);
			Vec::makeNormalTo(nodes.last().dir,nodes.last().position);
		}
		if (!target->IsSatellite())
			nodes.first().dir = -target->system.y.xyz;
		foreach (nodes, n)
		{
			Vec::setLen0(n->dir,Objects::vehicleScale);
			Vec::setLen0(n->normal,Objects::vehicleScale);
		}
	}

	void Primitive::Path::ApplyTo(const Objects::PMobileEntity&target, float t)
	{
		ASSERT__(!changed);


		//String out;
		//foreach (rawPath,p)
			//out += ","+String(p->second);
		//LogEvent(EventSignificance::Debug,target.get()," Applying at "+String(t)+" in {"+out+"}");

		for (index_t i = 0; i+1 < rawPath.Count(); i++)
		{
			//if (t < rawPath[i].second)
			//{
				//return;
			if (t > rawPath[i+1].second)
				continue;

			TNode	&na = nodes[i],
					&nb = nodes[i+1];

			const Grid::LCellIdI&a = rawPath[i].first,
								&b = rawPath[i+1].first;
			const TVec3<>&d0 = Grid::low.cells[a.stackID].cornerCenter; //centerVertex.xyz;
			float3 p0 = d0  * Grid::ToRadiusf(float(a.height));
			const TVec3<>&d1 = Grid::low.cells[b.stackID].cornerCenter;// centerVertex.xyz;
			float3 p1 = d1 * Grid::ToRadiusf(float(b.height));

			float3 c0 = p0 + na.dir;
			float3 c1 = p1 - nb.dir;

			float relative = std::max(0.f,(t - rawPath[i].second) / (rawPath[i+1].second - rawPath[i].second));

			//LogEvent(EventSignificance::Debug,target.get()," Relative ("+String(i)+"/"+String(rawPath.Count())+")="+String(relative));
			//!((i+1)% stepsPerRound);

			//TVec3<> d = p1 - p0;
			//if (a.height == b.height)

			
			float fj = relative;

			TVec3<> p;
			Vec::resolveBezierCurvePoint(p0,c0,c1,p1,fj,p);

			TMatrix4<>&sys = target->system;
			#ifdef HAS_WINDOW
				const TVec3<> prev = sys.w.xyz;
			#endif
			if (target->IsSatellite())
			{
				sys.z.xyz = p;
				Vec::normalize0(sys.z.xyz);

				Vec::cross(sys.y.xyz,sys.z.xyz,sys.x.xyz);
				Vec::normalize0(sys.x.xyz);
				Vec::cross(sys.z.xyz,sys.x.xyz,sys.y.xyz);
				Vec::normalize0(sys.y.xyz);
			}
			else
			{
				TVec3<> n,d,axis;
				Vec::resolveBezierCurveAxis(p0,c0,c1,p1,fj,axis);
				Vec::normalize0(axis);
				axis = -axis;

				Vec::cross(axis,p,n);
				Vec::normalize0(n);
				//target->Relocate(rawPath[i].first,false);
				target->location = rawPath[i].first;
				sys.x.xyz = n;
				sys.y.xyz = axis;
				Vec::cross(n,axis,sys.z.xyz);
				Vec::normalize0(sys.z.xyz);
			}
			#ifdef HAS_WINDOW
				target->ambientSound.SetPosition(sys.w.xyz);
				target->ambientSound.SetVelocity((sys.w.xyz - prev) / Engine::timing.delta);
				float x = t / rawPath.last().second;
				float activity = 1.f - (sqr(((x - 0.5f)*2.f)));
				target->ambientSound.SetPitch(1.f + activity * 0.5f);
				target->ambientSound.SetGain(std::min(target->GetSoundLevel() + activity,1.f));
				target->activitySound.SetPosition(sys.w.xyz);
				target->activitySound.SetVelocity((sys.w.xyz - prev) / Engine::timing.delta);
			#endif

			target->system.w.xyz = p;
			Mat::resetBottomRow(target->system);
			Mat::invertSystem(target->system, target->inverse);
			return;
		}
	}



	Buffer0<Primitive>		primitives,actives,whenDone;

	
	void Update()
	{
	
		if (!AllDone())
		{
			if ( RuntimeConfig::phase == RuntimeConfig::Phase::AnimationPhase || RuntimeConfig::phase == RuntimeConfig::Phase::CommandPhase)
			{
				//allowWorldRecompilation = false;
				#ifndef HAS_WINDOW
					float increase = 0.1f * Network::animationTimeFactor;
				#else
					float increase = Engine::timing.delta * Network::animationTimeFactor;
				#endif
				//LogEvent(EventSignificance::Debug,nullptr,"Animation::Update(@"+String(Animation::atTime)+"+"+String(increase)+")");
				Animation::atTime += increase;


				struct TSortItem
				{
					index_t	elementIndex;
					float	atTime;
			
					bool operator<(const TSortItem&other) const
					{
						return atTime < other.atTime;
					}
				};


				for (index_t i = 0; i < Animation::actives.Count(); i++)	//complete before adding new ones
				{
					if (Animation::actives[i].Update(Animation::atTime) == Animation::Primitive::Done)
					{
						Animation::actives[i].End();
						Animation::actives.Erase(i);
						i--;
					}
				}

				static Buffer0<TSortItem>	items;
				items.Clear();
				for (index_t i = 0; i < Animation::primitives.Count(); i++)
				{
					if (Animation::atTime >= Animation::primitives[i].time.min)
					{
						items.Append().elementIndex = i;
						items.last().atTime = Animation::primitives[i].time.min;
					}
				}


				std::sort(items.begin(),items.end());

				foreach (items,it)
				{
					Animation::actives << Animation::primitives[it->elementIndex];
					Animation::actives.last().Begin();
					if (Animation::actives.last().Update(Animation::atTime) == Animation::Primitive::Done)
					{
						Animation::actives.last().End();
						Animation::actives.EraseLast();
					}
					Animation::primitives.Erase(it->elementIndex);
					auto nxt = it;
					nxt++;
					for (;nxt != items.end(); ++nxt)
					{
						if (nxt->elementIndex > it->elementIndex)
							nxt->elementIndex--;
					}
				}



				//for (index_t i = 0; i < Animation::actives.Count(); i++)
				//{
				//	if (Animation::actives[i].Update(Animation::atTime) == Animation::Primitive::Done)
				//	{
				//		Animation::actives[i].End();
				//		Animation::actives.Erase(i);
				//		i--;
				//	}
				//}
			}

			if (Animation::AllDone())
			{
				if (whenDone.IsNotEmpty())
				{
					LogEvent(EventSignificance::Debug,nullptr,"Executing "+String(whenDone.Count())+" post-animation event(s)");
					for (auto&entry : whenDone)
					{
						entry.time.min = 0.f;
						entry.time.max = 1.f;

						entry.Begin();
						entry.Update(1.1f);
						entry.End();
					}

					whenDone.Clear();
				}


				#ifdef HAS_WINDOW
					World::allowRecompilation = true;
					Objects::UpdateChoices(Network::ControlEnabled());
				#endif
				Animation::atTime = 0;

				Objects::ForeachMobileWorldEntity([](const Objects::PMobileEntity&e)
				{
					//if ((*v)->preAnimationLocation != (*v)->location)
						//Objects::RemoveVehicleFromGrid(*v,(*v)->preAnimationLocation);
					if (e->isMine && !Animation::insertIntoGridLater.IsSet(e->id))
					{
						float t = 0.f;
						e->MoveSensory(e->preAnimationLocation,e->location,t);
//							Grid::actualWorld.MoveSensor((*v)->preAnimationLocation,(*v)->location,(*v)->GetScanningRange());
					}
				});


				Animation::insertIntoGridLater.VisitAllKeys([](const TUniqueID&key)
				{
					Objects::PMobileEntity v = To<Objects::MobileEntity>::Cast(Objects::GetEntity(key));
					if (v)
					{
						if (v->isMine)
						{
							float t = 0.f;
							v->MoveSensory(v->preAnimationLocation,v->location,t);
						}
						if (v->IsVehicle())
							Objects::InsertVehicleIntoGrid(To<Objects::Vehicle>::Cast(v),v->location);
					}
				});
				Animation::insertIntoGridLater.Clear();

				foreach (Objects::vehicles, v)
					(*v)->preAnimationLocation = (*v)->location;
				foreach (Objects::satellites, v)
					(*v)->preAnimationLocation = (*v)->location;

				if (RuntimeConfig::phase == RuntimeConfig::Phase::CommandPhase && Network::gameHasBegun)
				{
					OnPlayerControlBegin();
				}
			}

	
		}
	}

}
