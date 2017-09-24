#include <global_root.h>
#include "worldState.h"
#include <protocol.h>
#include "networkState.h"
#include "../networkChannels.h"

namespace World
{

	World::Data	spectatorWorldState;
	UINT16			empZonesLocked = 0, unlockedEMPZones = 0xFFFF;
	TUniqueID		worldID;
	bool	 terrainChanged = false;



	namespace Config
	{
		Grid::WorldDiscovery	worldDiscovery;

		UINT32		uplinkBandwidth = 500;
		bool		empZones = false,
					simulateLiquids = true;


	}
	


	void					DeltaContainer::CompressState(World::Data&sourceWorld,const Protocol::Timestamp&t)
	{
			
		for (BaseGrid::family_t family = 0; family < BaseGrid::NumFamilies; family++)
		{
			auto&d = delta[family];
			sourceWorld.CompressFamilyState(family,d.data0);
			d.familyAndLastFlag = family;
			d.timestamp = t;
		}
		numDeltas = BaseGrid::NumFamilies;
		delta[numDeltas-1].familyAndLastFlag |= 0x80;
	}


	bool					DeltaContainer::Process(World::Data&clientData,bool noDelta/*=false*/)
	{
		numDeltas = 0;
		Ctr::Array<Grid::CellStack::TTypeDeltaBlock>	blocks;
			
		float t = 0;
		const bool rightNow = RuntimeConfig::IsInAnyCommandPhase();
		for (BaseGrid::family_t family = 0; family < BaseGrid::NumFamilies; family++)
		{
			if (clientData.ProgressiveUpdate(family,blocks,t,noDelta))
			{
				auto&d = delta[numDeltas++];
				d.data0.swap(blocks);
				d.familyAndLastFlag = family;
				d.timestamp.rightNow = rightNow;
			}
		}

		if (numDeltas)
		{
			if (!rightNow)
				for (index_t i = 0; i < numDeltas; i++)
					delta[i].timestamp.atTime = t;
			delta[numDeltas-1].familyAndLastFlag |= 0x80;

			return true;
		}
		return false;
	}


	namespace Network
	{

		void			HandleSessionClosure()
		{
			terrainChanged = false;
			worldID.Generate();
		}



		/*static*/ DeltaContainer	staticContainer;


		bool	CheckCoverageSendTerrain(const Objects::PFaction&faction,const TCP::PPeer&peer)
		{
			faction->CheckSensory();
			if (!staticContainer.Process(faction->knownWorld,!peer))
				return false;

			for (index_t i = 0; i < staticContainer.numDeltas; i++)
				Channels::updateWorld.SendTo(peer,staticContainer.delta[i]);

			return true;
		}

		bool	CheckCoverageSendTerrain(const Objects::PFaction&faction)
		{
			TCP::PPeer peer;
			::Network::PClient client;
			if (::Network::playerMap.Query(faction,client))
				peer = client->peer.lock();
			return CheckCoverageSendTerrain(faction,peer);
		}




		void						SignalFamilyChangeToFaction(const Objects::PFaction&faction,BaseGrid::family_t familyId, bool last, bool rightNow)
		{
			static Protocol::WorldDelta	delta;
			delta.timestamp.atTime = 0;
			if (faction->knownWorld.ProgressiveUpdate(familyId,delta.data0,delta.timestamp.atTime))
			{
				delta.timestamp.rightNow = rightNow;
				delta.familyAndLastFlag = familyId;
				if (last)
					delta.familyAndLastFlag |= 0x80;

				::Network::PClient client;
				if (::Network::playerMap.Query(faction,client))
				{
					Channels::updateWorld.SendTo(client->peer.lock(),delta);
				}
			}
		}

		void						CheckAllFactionCoverageAndSendAsNeeded()
		{
			if (World::actual.FlushAllChanges())
				terrainChanged = true;



			if (::Network::spectators.IsNotEmpty())
			{
				if (spectatorWorldState.SensoryIsInvalid())
					spectatorWorldState.FlushAndValidateSensory(1);

				if (staticContainer.Process(spectatorWorldState))
					for (index_t i = 0; i < staticContainer.numDeltas; i++)
						for (const auto&s : ::Network::spectators)
						{
							Channels::updateWorld.SendTo(s->peer.lock(),staticContainer.delta[i]);
						}
			}

			CheckCoverageSendTerrain(Objects::Faction::nexus);
			foreach (Objects::Faction::all,_f)
			{
				const Objects::PFaction&faction = *_f;
				CheckCoverageSendTerrain(faction);
			}
		}



	}



	

	void	SimulateLiquidsIfEnabled()
	{
	
		
		if (Config::simulateLiquids)
		{
			Timer::Time t0 = timer.Now();

			struct TStack
			{
				BYTE	volumeCell[Grid::CellStack::Range];
			};
			static Ctr::Array<TStack>	tempStacks;
			tempStacks.SetSize(Grid::HighSetup::GridType::CellCount);
			for (UINT32 i = 0; i < Grid::HighSetup::GridType::CellCount; i++)
			{
				TStack&target = tempStacks[i];
				const Grid::CellStack&source = World::actual.GetStack(i);
				for (index_t layer = 0; layer < Grid::CellStack::Range; layer++)
					target.volumeCell[layer] = source.volumeCell[layer].content;
			}

			BaseGrid::FamiliesChanged changed;

			Concurrency::parallel_for(UINT32(0),Grid::HighSetup::GridType::CellCount,[&changed](UINT32 stackIndex)
			{
				const Grid::CellStack&stack = World::actual.GetStack(stackIndex);
				const Grid::CellStack*nStacks[6] = {nullptr};
				const auto&neighborhood = Grid::high.grid.GetCell(stackIndex).neighborhood;
				DBG_ASSERT_LESS_OR_EQUAL__(neighborhood.numNeighbors,ARRAYSIZE(nStacks));
				for (int i = 0; i < neighborhood.numNeighbors; i++)
					nStacks[i] = &World::actual.GetStack(neighborhood.neighborID[i]);
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
					changed.Set(Grid::high.grid.GetCell(stackIndex).family);
					for (int i = 0; i < neighborhood.numNeighbors; i++)
						changed.Set(Grid::high.grid.GetCell(neighborhood.neighborID[i]).family);
				}
			});
			auto seq = World::actual.BeginChangeSequence(0);
			foreach (changed,ch)
			{
				UINT32 offset = Grid::high.grid.GetFamilyOffset(*ch);
				count_t cnt = Grid::high.grid.CountFamilyMembers(*ch);
				for (UINT32 k = offset; k < offset+cnt; k++)
				{
					auto access = seq.AccessStack(k);
					Grid::CellStack&target = access.GetStack();
					const TStack&source = tempStacks[k];
					for (index_t layer = 0; layer < Grid::CellStack::Range; layer++)
						target.volumeCell[layer].content = source.volumeCell[layer];
				}
			}

			Objects::ForeachWorldEntity([changed,&seq](const Objects::PWorldEntity&entity)
			{
				if (!changed.IsSet(Grid::low.grid.GetCell(entity->location.stackID).family))
					return;

				const int h = entity->GetHeight();
				const bool broad = entity->IsBroad();

				if (entity->IsIndestructible())
				{
					for (int y = 0; y < h; y++)
					{
						if (broad)
							seq.Clear(entity->location + y);
						else
							seq.Clear(Grid::ToHigh(entity->location+y));
					}
				}
				else
				{
					bool kill = false;
					for (int y = 0; y < h; y++)
					{
						if (broad)
						{
							if (!seq.ClearNonLava(entity->location + y))
							{
								kill = true;
								break;
							}
						}
						else
						{
							const Grid::HCellIdI loc = Grid::ToHigh(entity->location+y);
							if (!seq.ClearNonLava(loc))
							{
								kill = true;
								break;
							}
						}
					}
					if (kill)
					{
						float t = 0;
						Objects::Remove(entity,Objects::RemovalType::UncontrolledExplosion,t);
					}
				}
			},false);
			float t = timer.GetSecondsSince(t0);
			LogEvent(EventSignificance::PerformanceMeasurement,"Time needed to process liquids: "+String(t));
		}

	}

	void			BuildConfiguredWorld(TwisterRandom&random)
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
	}

	void			BuildConfiguredWorld(UINT32 seed)
	{
		TwisterRandom random(seed);
		BuildConfiguredWorld(random);
	}




	index_t			GetFactionStartLocation(TwisterRandom&random)
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
							d2 = M::sqr(sqrt(d2) - base->turret->horizontalAttackRange(e->owner));

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
					dist2 = dist2 > MinimumNexusBaseDistance ? M::sqr(random.NextFloat(1,2)) : 0.f;	//0 should imply to not take this location
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


	void		AppendAvailablePlanetInitFilesFrom(bool isFactoryDefault, FileSystem::Folder  folder, Ctr::Buffer0<TPlanetIniFile,Swap>&outList)
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
		if (!filename.EndsWith(".ini"))
			return IsPlanetIniFile(filename+".ini");
		return FileSystem::Folder(RuntimeConfig::ConfigFile("config/myPlanets")).FindFile(PathString(filename)) != nullptr
				||
			FileSystem::Folder().FindFile("config/myPlanets/"+PathString(filename)) != nullptr;
	}


	void		GetAvailablePlanetIniFiles(Ctr::Array<TPlanetIniFile>&rsOut)
	{
		Ctr::Buffer0<TPlanetIniFile,Swap> files;
		
		AppendAvailablePlanetInitFilesFrom(false, RuntimeConfig::ConfigFile("config/myPlanets"),files);
		AppendAvailablePlanetInitFilesFrom(true,FileSystem::Directory().FindDirectory("config/myPlanets"),files);

		Ctr::GenericHashTable<String,TPlanetIniFile,Ctr::StdHash,Swap,Swap>	merged;
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

		merged.ExportTo(rsOut);

		Sorting::ByOperator::QuickSort(rsOut);
	}


	void			GetAvailablePlanetNames(Ctr::Array<String>&rsOut)
	{
		Ctr::Array<TPlanetIniFile>	files;
		GetAvailablePlanetIniFiles(files);
		rsOut.SetSize(files.Count());
		for (index_t i = 0; i < files.Count(); i++)
			rsOut[i].swap(files[i].name);
	}

}
