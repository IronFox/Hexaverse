#include <global_root.h>
#include "worldState.h"
#include <protocol.h>
#include "networkState.h"
#include "../networkChannels.h"

namespace World
{

	World::Data	spectatorWorldState;
	bool			terrainChanged=false;
	UINT16			empZonesLocked = 0, unlockedEMPZones = 0xFFFF;
	TUniqueID		worldID;



	namespace Config
	{
		Grid::WorldDiscovery	worldDiscovery;

		UINT32		uplinkBandwidth = 500;
		bool		empZones = false,
					simulateLiquids = true;


	}


	namespace Network
	{

		void			HandleSessionClosure()
		{
			terrainChanged = false;
			worldID.Generate();
		}









		bool	CheckCoverageSendTerrain(const Objects::PFaction&faction,const TCP::PPeer&peer)
		{
			static Protocol::WorldDelta	delta;

			BYTE numChanged = 0;
			for (BYTE family = 0; family < BaseGrid::NumFamilies; family++)
				if (faction->knownWorld.families[family].coverageChanged)
					numChanged++;

			if (numChanged)
			{
				BYTE at = 0;
				for (BYTE family = 0; family < BaseGrid::NumFamilies; family++)
					if (faction->knownWorld.families[family].coverageChanged)
					{
						faction->knownWorld.families[family].coverageChanged = false;
						at++;
						Protocol::GenerateWorldDelta(
							&faction->knownWorld,
							delta,family,
							RuntimeConfig::IsInAnyCommandPhase(),
							at == numChanged,
							peer ? 
								Protocol::WorldDeltaType::ProgressiveUpdate 
								: Protocol::WorldDeltaType::ProgressiveUpdateNoDelta
						);
						if (peer)
							Channels::updateWorld.SendTo(peer,delta);
					}
				return true;
			}
			return false;
		}




		void						SignalFamilyChangeToFaction(const Objects::PFaction&faction,BYTE familyId, bool last, bool rightNow)
		{
			static Protocol::WorldDelta	delta;
			GenerateWorldDelta(&faction->knownWorld,delta,familyId,rightNow,last,Protocol::WorldDeltaType::ProgressiveUpdate);

			::Network::PClient client;
			if (::Network::playerMap.Query(faction,client))
			{
				Channels::updateWorld.SendTo(client->peer.lock(),delta);
			}
		}

		void						SignalFamilyChange(BYTE familyId, bool last, bool rightNow)
		{
			if (::Network::spectators.IsNotEmpty())
			{
				static Protocol::WorldDelta	delta;
				GenerateWorldDelta(&spectatorWorldState,delta,familyId,rightNow,last,Protocol::WorldDeltaType::ProgressiveUpdate);
				for (const auto&s : ::Network::spectators)
				{
					Channels::updateWorld.SendTo(s->peer.lock(),delta);
				}
			}


			SignalFamilyChangeToFaction(Objects::Faction::nexus,familyId,last,rightNow);
			foreach (Objects::Faction::all,_f)
			{
				Objects::PFaction faction = _f->lock();
				if (faction)
					SignalFamilyChangeToFaction(faction,familyId,last,rightNow);
			}

			World::actual.families[familyId].changed = false;
		}


		
		void SendChangedFamilies()
		{
			BYTE changed = 0,counter=0;
			for (BYTE k = 0; k < BaseGrid::NumFamilies; k++)
			{
				if (World::actual.families[k].changed)
					changed ++;
			}
			if (changed)
			{
				for (BYTE k = 0; k < BaseGrid::NumFamilies; k++)
				{
					if (World::actual.families[k].changed)
						SignalFamilyChange(k,++counter == changed,RuntimeConfig::IsInAnyCommandPhase());
				}
				World::actual.structureChanged = false;
				terrainChanged = true;
			}
		}



	}




	void DebugCheckWorld(bool restart)
	{
		FATAL__("when is this called?");
		static Array<Protocol::WorldDelta>	deltas;
		if (restart)
			deltas.Clear();
		if (deltas.IsEmpty())
		{
			deltas.SetSize(BaseGrid::NumFamilies);
			for (BYTE k = 0; k < BaseGrid::NumFamilies; k++)
			{
				GenerateWorldDelta(nullptr, deltas[k], k, true, k+1==BaseGrid::NumFamilies, Protocol::WorldDeltaType::GlobalCopy);
			}
		}
		else
		{
			Protocol::WorldDelta delta;
			for (BYTE k = 0; k < BaseGrid::NumFamilies; k++)
			{
				bool isLast = k+1==BaseGrid::NumFamilies;
				GenerateWorldDelta(nullptr, delta, k, true, isLast, Protocol::WorldDeltaType::GlobalCopy);
				ASSERT__(delta.data0 == deltas[k].data0);
				ASSERT__(delta.familyAndLastFlag == deltas[k].familyAndLastFlag);
				ASSERT__(delta.timestamp.atTime == deltas[k].timestamp.atTime);

			}
			
		}
		deltas.SetSize(BaseGrid::NumFamilies);
		for (BYTE k = 0; k < BaseGrid::NumFamilies; k++)
		{
			std::cout<<(int)k<<": "<<deltas[k].GetSerialSize(false)<<std::endl;
		}
	}


	

	void	SimulateLiquidsIfEnabled()
	{
	
		
		if (Config::simulateLiquids)
		{
			Timer::Time t0 = timer.Now();

			static bool changed[BaseGrid::NumFamilies];
			memset(changed,false,sizeof(changed));
			struct TStack
			{
				BYTE	volumeCell[Grid::CellStack::Range];
			};
			static Array<TStack>	tempStacks;
			tempStacks.SetSize(Grid::HighSetup::GridType::CellCount);
			for (index_t i = 0; i < Grid::HighSetup::GridType::CellCount; i++)
			{
				TStack&target = tempStacks[i];
				const Grid::CellStack&source = World::actual.cellStack[i];
				for (index_t layer = 0; layer < Grid::CellStack::Range; layer++)
					target.volumeCell[layer] = source.volumeCell[layer].content;
			}

			Concurrency::parallel_for(index_t(0),Grid::HighSetup::GridType::CellCount,[](index_t stackIndex)
			{
				Grid::CellStack&stack = World::actual.cellStack[stackIndex];
				Grid::CellStack*nStacks[6] = {nullptr};
				const auto&neighborhood = Grid::high.grid.GetCell(stackIndex).neighborhood;
				DBG_ASSERT_LESS_OR_EQUAL__(neighborhood.numNeighbors,ARRAYSIZE(nStacks));
				for (int i = 0; i < neighborhood.numNeighbors; i++)
					nStacks[i] = World::actual.cellStack + neighborhood.neighborID[i];
				TStack&target = tempStacks[stackIndex];
				bool thisChanged = false;
				for (UINT32 layer = 0; layer < Grid::CellStack::Range; layer++)
				{
					if (stack.volumeCell[layer].content == Grid::Content::Clear)
					{
						BYTE fill = 
							layer+1 < Grid::CellStack::Range && Grid::CellStack::Cell::IsLiquid(stack.volumeCell[layer+1].content) ? stack.volumeCell[layer+1].content : 0;
						if (!fill)
							for (int i = 0; i < neighborhood.numNeighbors; i++)
								if (Grid::CellStack::Cell::IsLiquid(nStacks[i]->volumeCell[layer].content))
								{
									fill = nStacks[i]->volumeCell[layer].content;
									break;
								}

						if (fill)
						{
							target.volumeCell[layer] = fill;
							thisChanged = true;
						}
					}
				}
				if (thisChanged)
				{
					changed[Grid::high.grid.GetCell(stackIndex).family] = true;
					for (int i = 0; i < neighborhood.numNeighbors; i++)
						changed[Grid::high.grid.GetCell(neighborhood.neighborID[i]).family] = true;
				}
			});
			for (int i = 0; i < BaseGrid::NumFamilies; i++)
			{
				if (changed[i])
				{
					index_t offset = Grid::high.grid.GetFamilyOffset(i);
					count_t cnt = Grid::high.grid.CountFamilyMembers(i);
					for (index_t k = offset; k < offset+cnt; k++)
					{
						Grid::CellStack&target = World::actual.cellStack[k];
						const TStack&source = tempStacks[k];
						for (index_t layer = 0; layer < Grid::CellStack::Range; layer++)
							target.volumeCell[layer].content = source.volumeCell[layer];
					}
					World::actual.SignalFamilyStructureChange(i);
				}
			}

			static Buffer0<Objects::PWorldEntity>	waste;

			waste.Clear();

			Objects::ForeachWorldEntity([](const Objects::PWorldEntity&entity)
			{
				if (!changed[Grid::low.grid.GetCell(entity->location.stackID).family])
					return;

				const int h = entity->GetHeight();
				const bool broad = entity->IsBroad();

				if (entity->IsIndestructible())
				{
					for (int y = 0; y < h; y++)
					{
						if (broad)
							World::actual.Clear(entity->location + y,0);
						else
							World::actual.cellStack[entity->location.stackID].MakeLocalHole(entity->location.height+y,World::actual);
					}
				}
				else
				{
					for (int y = 0; y < h; y++)
					{
						if (broad)
						{
							if (!World::actual.ClearNonLava(entity->location + y,0))
							{
								waste << entity;
								break;
							}
						}
						else
						{
							if (World::actual.cellStack[entity->location.stackID].GetCellI(entity->location.height+y).content == Grid::Content::Lava)
							{
								waste << entity;
								break;
							}
							World::actual.cellStack[entity->location.stackID].MakeLocalHole(entity->location.height+y,World::actual);
						}
					}
				}
			},false);
			float t = timer.GetSecondsSince(t0);
			LogEvent(EventSignificance::PerformanceMeasurement,"Time needed to process water: "+String(t));

			for (const auto&e:waste)
			{
				float at = 0;
				Objects::Remove(e,Objects::RemovalType::UncontrolledExplosion,at,false);
			}
			waste.Clear();
		}

	}

	void			BuildConfiguredWorld(Random&random)
	{
		Grid::currentPlanetConfig.RandomizeSeeds(random);
		Grid::currentPlanetConfig.BuildWorld(World::actual);
		World::actual.ExportDiscovered(World::initial, Config::worldDiscovery);
		Grid::Resource::FillAll(random,Grid::currentCustomResourceDistribution);
		if (Objects::Faction::nexus)
			Objects::Faction::nexus->knownWorld = World::initial;

		if (RuntimeConfig::gameGoal == Game::Config::Goal::ConquerNexus)
		{
			LogEvent(EventSignificance::Common, "Spawning Nexus Ports (EMP fields="+String(RuntimeConfig::empZones)+")...");
			for (int pole = 0; pole < 12; pole++)
				if (RuntimeConfig::empZones || BaseGrid::IsIsolatedCorner(pole))
					Game::SpawnNexusPortAtPole(pole);

			LogEvent(EventSignificance::Debug, "Spawning Nexus bases...");
			if (Game::Config::Nexus::haveBases)
				for (int pole = 0; pole < 12; pole++)
					if (BaseGrid::IsIsolatedCorner(pole))
					{
						Game::SpawnNexusBaseAtPoleWithPort(pole);
					}
		}
		for (int i = 0; i < BaseGrid::NumFamilies; i++)
		{
			World::actual.SignalFamilyStructureChange(i);
		}
	}

	void			BuildConfiguredWorld(UINT32 seed)
	{
		Random random(seed);
		BuildConfiguredWorld(random);
	}




	index_t			GetFactionStartLocation(Random&random)
	{
		index_t best = InvalidIndex;
		float bestDist = 0;

		Objects::Component::VehicleBase *testBase = &Objects::Component::hoverbase;

		float biggestNDist = 0;

		count_t numTries = 0;
		while (best == InvalidIndex)
		{
			if (++numTries > 10)
			{
				return InvalidIndex;

			}

			const float r = Grid::ToRadius(0);
			//avoid 12 corner cells: make sure all candidates have 6 neighbors
			for (index_t i = 12; i < Grid::LowSetup::GridType::CellCount; i++)
			{
				if (RuntimeConfig::empZones)
				{
					if (!(unlockedEMPZones & Grid::low.grid.GetCell(i).cornerFlags))
						continue;

					const auto&n = Grid::low.grid.GetCell(i).neighborhood;
					bool broken = false;
					for (int k = 0; k < n.numNeighbors ; k++)
					{
						if (!(unlockedEMPZones & Grid::low.grid.GetCell(n.neighborID[k]).cornerFlags))
						{
							broken = true;
							break;
						}
					}
					if (broken)
						continue;
				}


				const int h0 = Objects::FindApplicableGround(Grid::LCellIdI((UINT32)i,0),true);
				if (h0 == Objects::badHeight || h0 == -Grid::CellStack::HalfRange || !testBase->CanStandOn(World::actual,Grid::LCellIdI((UINT32)i,h0-1),false,false))
					continue;
				{
					const auto&n = Grid::low.grid.GetCell(i).neighborhood;
					bool broken = false;
					for (int k = 0; k < n.numNeighbors && !broken; k++)
					{
						const int h1 = Objects::FindApplicableGround(Grid::LCellIdI(n.neighborID[k],0),true);
						if (h1 == Objects::badHeight || h1 == -Grid::CellStack::HalfRange || !testBase->CanStandOn(World::actual,Grid::LCellIdI(n.neighborID[k],h1-1),false,false))
							broken = true;
					}
					if (broken)
						continue;

				}


				const float3 ref = Grid::low.cells[i].centerVertex.xyz;// * r;
				float dist2 = std::numeric_limits<float>::max();
				float minNexusHubDist2 = std::numeric_limits<float>::max();
				Objects::ForeachWorldEntity([ref,&dist2,&minNexusHubDist2](const Objects::PWorldEntity&e)
				{
					float d2 = Vec::quadraticDistance(Grid::low.cells[e->location.stackID].centerVertex.xyz,ref);
					
					if (!e->IsStructure() || !To<Objects::NexusPort>::DynamicCast(e))
					{
						Objects::PTurretBase base = e->GetTurretBase();
						if (base)
							d2 = sqr(sqrt(d2) - base->turret->horizontalAttackRange(e->owner));

						dist2 = std::min(dist2, d2);
					}
					else
					{
						minNexusHubDist2 = std::min(minNexusHubDist2, d2);
					}
				},false);

				#ifdef LOWER_RESOLUTION_PLANET
					static const float MinimumNexusBaseDistance = 0.4f;	//minimum distance from any nexus building (nexus hub explicitly excluded)
				#else
					static const float MinimumNexusBaseDistance = 0.35f;
				#endif

				static const float PlacementVariance = 0.1f;	//should not exceed 0.1f

				if (Objects::Faction::all.IsEmpty())
					dist2 = dist2 > MinimumNexusBaseDistance ? sqr(random.NextFloat(1,2)) : 0.f;	//0 should imply to not take this location
				else
					dist2 *= pow(1.f - PlacementVariance,random.NextFloat(-1,1));	//vary randomly

				biggestNDist = std::max(biggestNDist,minNexusHubDist2);

				if (dist2 > bestDist && minNexusHubDist2 > 0.125f)
				{
					bestDist = dist2;
					//nbestDist = ndist2;
					best = i;

					//if (Objects::structures.IsEmpty() && Objects::vehicles.IsEmpty())
						//break;
				}

			}
		}
		return best;
	}


	




	void			UpdateEMPZones(count_t temporaryFactionCountBonus/*=0*/)
	{
		if (RuntimeConfig::empZones)
		{
			count_t numZones = Game::Config::CountAccessibleZones(temporaryFactionCountBonus);

			int unlocked = 0;
			for (index_t i = 0; i < numZones; i++)
			{
				if (Game::Config::Nexus::haveBases && BaseGrid::IsIsolatedCorner(unlocked) && RuntimeConfig::gameGoal == RuntimeConfig::GameGoal::ConquerNexus)
				{
					unlocked++;
				}
				unlocked++;
			}
				
			SetUnlockedEMPZones(unlocked);
		}
		else
		{
			SetUnlockedEMPZones(NumZones);
		}
	}


	void		SetUnlockedEMPZones(int unlocked)
	{
		const UINT16 newUnlocked = ((1<<unlocked) - 1);
		if (unlockedEMPZones == newUnlocked)
			return;
		Grid::empZonesUnlocked = unlockedEMPZones = newUnlocked;
		Grid::empZonesLocked = empZonesLocked = AllZones  & ~unlockedEMPZones;
		Channels::lockEmpZones.SendTo(::Network::server,empZonesLocked,Channels::FactionLevel);

		float t = 0;
		Game::UpdateResidualEMPDamage(t);

	}


	void		AppendAvailablePlanetInitFilesFrom(bool isFactoryDefault, FileSystem::Folder  folder, Buffer0<TPlanetIniFile,Swap>&outList)
	{
		if (!folder.IsValidLocation())
		{
			LogEvent(EventClass::Preset::remoteControlError,"Internal error: Unable to locate planets-directory '"+String(folder.GetLocation())+"'");
			return;
		}
		FileSystem::File file;
		folder.Rewind();
		while (folder.NextFile("ini",file))
		{
			outList.Append().name = String(file.GetInnerName());
			if (isFactoryDefault)
				outList.Last().factoryDefault.swap(file);
			else
				outList.Last().userDefined.swap(file);
		}

	}

	bool			IsPlanetIniFile(const String&filename)
	{
		if (!filename.endsWith(".ini"))
			return IsPlanetIniFile(filename+".ini");
		return FileSystem::Folder(RuntimeConfig::ConfigFile("config/myPlanets")).FindFile(PathString(filename)) != nullptr
				||
			FileSystem::Folder().FindFile("config/myPlanets/"+PathString(filename)) != nullptr;
	}


	void		GetAvailablePlanetIniFiles(Array<TPlanetIniFile>&rsOut)
	{
		Buffer0<TPlanetIniFile,Swap> files;
		
		AppendAvailablePlanetInitFilesFrom(false, RuntimeConfig::ConfigFile("config/myPlanets"),files);
		AppendAvailablePlanetInitFilesFrom(true,FileSystem::Folder().FindFolder("config/myPlanets"),files);

		GenericHashTable<String,TPlanetIniFile,StdHash,Swap,Swap>	merged;
		foreach (files,f)
		{
			
			auto*ptr = merged.QueryPointer(f->name);
			if (ptr)
			{
				if (f->factoryDefault.GetLocation().IsNotEmpty())
					ptr->factoryDefault.swap(f->factoryDefault);
				else
					ptr->userDefined.swap(f->userDefined);
			}
			else
				merged.Set(f->name).swap(*f);
		}

		merged.exportTo(rsOut);

		ByOperator::QuickSort(rsOut);
	}


	void			GetAvailablePlanetNames(Array<String>&rsOut)
	{
		Array<TPlanetIniFile>	files;
		GetAvailablePlanetIniFiles(files);
		rsOut.SetSize(files.Count());
		for (index_t i = 0; i < files.Count(); i++)
			rsOut[i].swap(files[i].name);
	}

}
