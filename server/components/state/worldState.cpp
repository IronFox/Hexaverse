#include <global_root.h>
#include "worldState.h"
#include <protocol.h>
#include "networkState.h"
#include "../networkChannels.h"
#include <objects/nexus/nexusPresence.h>

namespace World
{

	World::Data	spectatorWorldState;
	UINT16			empZonesLocked = 0, unlockedEMPZones = 0xFFFF;
	TUniqueID		worldID;
	bool	 terrainChanged = false;



	namespace Config
	{
		Grid::WorldDiscovery	worldDiscovery;

		#ifndef NO_HIBERNATION
			UINT32	uplinkBandwidth = 500;
		#endif
		bool		empZones = false,
					simulateLiquids = true;
		TPlantGrowth	crystals,
						trees;
		float		grassGrowthProbability = 0.2f;
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


	class ParallelTerrainChanger
	{
	
	public:
		struct TStack
		{
			UINT32	hash;
			BYTE	volumeCell[Grid::CellStack::Range];
		};

		void		Begin()
		{
			t0 = timer.Now();

			tempStacks.SetSize(Grid::HighSetup::GridType::CellCount);
			for (UINT32 i = 0; i < Grid::HighSetup::GridType::CellCount; i++)
			{
				TStack&target = tempStacks[i];
				const Grid::CellStack&source = World::actual.GetStack(i);
				for (index_t layer = 0; layer < Grid::CellStack::Range; layer++)
					target.volumeCell[layer] = source.volumeCell[layer].content;
			}
		}

		void		End()
		{
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

			float t = timer.GetSecondsSince(t0);
			LogEvent(EventSignificance::PerformanceMeasurement,"Time needed to process terrain change: "+String(t));
		}

		void		ForeachStack(const std::function<void(UINT32 stackIndex, TStack&stack)>&f)
		{
			Concurrency::parallel_for(UINT32(0),Grid::HighSetup::GridType::CellCount,[&f,this](UINT32 stackIndex)
			{
				f(stackIndex,tempStacks[stackIndex]);
			});
		}

		void		SetStackChanged(UINT32 stackIndex)
		{
			changed.Set(Grid::high.grid.GetCell(stackIndex).family);
		}

		UINT32		MakeRandomSeed()
		{
			Concurrency::parallel_for(UINT32(0),Grid::HighSetup::GridType::CellCount,[this](UINT32 stackIndex)
			{
				TStack&stack = tempStacks[stackIndex];
				CRC32::Sequence seq;
				seq.Append(stack.volumeCell,sizeof(stack.volumeCell));
				stack.hash = seq.Finish();
			});

			CRC32::Sequence seq;
			seq.AppendPOD(RuntimeConfig::roundNumber);
			foreach (tempStacks,stack)
				seq.AppendPOD(stack->hash);
			return seq.Finish();
		}


	private:
		Ctr::Array<TStack>	tempStacks;
		Timer::Time t0;
		BaseGrid::FamiliesChanged changed;
	};

	static ParallelTerrainChanger	changer;



	static void RandomPlantTransition(float p, Grid::Content from, Grid::Content to, Random&randomSource)
	{
		if (p <= 0)
			return;

		const auto seed = randomSource.NextSeed();
		changer.ForeachStack([seed,from,to,p](UINT32 stackIndex, ParallelTerrainChanger::TStack&stack)
		{
			Random localRandom(CRC32::Sequence().AppendPOD(stackIndex).AppendPOD(seed).Finish());
			for (UINT layer = 0; layer+1 < ARRAYSIZE(stack.volumeCell); layer++)
			{
				if (	stack.volumeCell[layer+1] == Grid::Content::Clear 
					&&	stack.volumeCell[layer] == from 
					&&	localRandom.NextFloat() < p)
				{
					stack.volumeCell[layer] = to;
					changer.SetStackChanged(stackIndex);
				}
			}
		});
	}

	static void EnvironmentalRandomTransition(float p, Grid::Content from, Grid::Content to, Grid::Content neighbor, Random&randomSource)
	{
		if (p <= 0)
			return;

		const auto seed = randomSource.NextSeed();
		changer.ForeachStack([seed,from,to,neighbor,p](UINT32 stackIndex, ParallelTerrainChanger::TStack&stack)
		{
			Random localRandom(CRC32::Sequence().AppendPOD(stackIndex).AppendPOD(seed).Finish());
			const Grid::CellStack*nStacks[6] = {nullptr};
			const auto&neighborhood = Grid::high.grid.GetCell(stackIndex).neighborhood;
			DBG_ASSERT_LESS_OR_EQUAL__(neighborhood.numNeighbors,ARRAYSIZE(nStacks));
			for (int i = 0; i < neighborhood.numNeighbors; i++)
				nStacks[i] = &World::actual.GetStack(neighborhood.neighborID[i]);

			for (UINT layer = 0; layer+1 < ARRAYSIZE(stack.volumeCell); layer++)
			{
				const UINT lower = layer > 0 ? layer -1 : layer;
				const UINT upper = layer+1 < ARRAYSIZE(stack.volumeCell) ? layer+1 : layer;

				if (stack.volumeCell[layer+1] == Grid::Content::Clear && stack.volumeCell[layer] == from)
				{
					count_t numFound = 0;
					for (int i = 0; i < neighborhood.numNeighbors; i++)
					{
						for (UINT l2 = lower; l2 <= upper; l2++)
							if (nStacks[i]->volumeCell[l2].content == neighbor)
								numFound++;
					}
					if (numFound > 0 && localRandom.NextFloat() < 1.f - M::PowU(1.f - p,numFound))
					{
						stack.volumeCell[layer] = to;
						changer.SetStackChanged(stackIndex);
					}
				}
			}
		});
	}

	void	SimulatePlantGrowth(Random&randomSource)
	{
		RandomPlantTransition(Config::crystals.deathProbability, Grid::Content::CrystalForest, Grid::Content::Granite, randomSource);
		RandomPlantTransition(Config::crystals.growthProbability, Grid::Content::Crystals, Grid::Content::CrystalForest, randomSource);

		RandomPlantTransition(Config::trees.deathProbability, Grid::Content::Forest, Grid::Content::Grass, randomSource);
		RandomPlantTransition(Config::trees.growthProbability, Grid::Content::Trees, Grid::Content::Forest, randomSource);

		RandomPlantTransition(Config::grassGrowthProbability, Grid::Content::Earth, Grid::Content::Grass, randomSource);

		EnvironmentalRandomTransition(Config::trees.spawnProbability, Grid::Content::Grass, Grid::Content::Trees, Grid::Content::Forest, randomSource);
		EnvironmentalRandomTransition(Config::crystals.spawnProbability, Grid::Content::Granite, Grid::Content::Crystals, Grid::Content::CrystalForest, randomSource);
	}


	void	SimulateLiquidsIfEnabled()
	{
		changer.Begin();
		{
			Random plantRandom = Random(changer.MakeRandomSeed());
			SimulatePlantGrowth(plantRandom);
		}
		
		if (Config::simulateLiquids)
		{
			changer.ForeachStack([](UINT32 stackIndex, ParallelTerrainChanger::TStack&target)
			{
				const Grid::CellStack&stack = World::actual.GetStack(stackIndex);
				const Grid::CellStack*nStacks[6] = {nullptr};
				const auto&neighborhood = Grid::high.grid.GetCell(stackIndex).neighborhood;
				DBG_ASSERT_LESS_OR_EQUAL__(neighborhood.numNeighbors,ARRAYSIZE(nStacks));
				for (int i = 0; i < neighborhood.numNeighbors; i++)
					nStacks[i] = &World::actual.GetStack(neighborhood.neighborID[i]);
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
					changer.SetStackChanged(stackIndex);
					for (int i = 0; i < neighborhood.numNeighbors; i++)
						changer.SetStackChanged(neighborhood.neighborID[i]);
				}
			});
		}

		changer.End();

	}

	
	static void _BuildConfiguredWorld(TwisterRandom&random)
	{
		random.discard(20);



		Grid::currentPlanetConfig.BuildWorld(World::actual);
		World::actual.ExportDiscovered(World::initial, Config::worldDiscovery);
		Grid::Resource::FillAll(random,Grid::currentCustomResourceDistribution);
		if (Objects::Faction::nexus)
			Objects::Faction::nexus->knownWorld = World::initial;

		Game::Config::Nexus::allPorts.havePlaced = Game::Config::Nexus::allPorts.doPlace;
		Game::Config::Nexus::bases.havePlaced = Game::Config::Nexus::bases.doPlace;
		Game::Config::Nexus::satellitesAtopBases.havePlaced = Game::Config::Nexus::satellitesAtopBases.doPlace;
		NexusPresence::Create(Game::Config::Nexus::bases.havePlaced,Game::Config::Nexus::allPorts.havePlaced,Game::Config::Nexus::satellitesAtopBases.havePlaced);

	}

	void			BuildConfiguredWorld(UINT32 seed)
	{
		//Important: Must NOT call RandomizeSeeds() here, or interactive configuration mode gets caught generating new seeds with each change!
		Grid::currentPlanetConfig.SetSeed(seed);
		TwisterRandom random(seed);
		_BuildConfiguredWorld(random);
	}
	
	void			BuildConfiguredWorld(TwisterRandom&rnd)
	{
		Grid::currentPlanetConfig.RandomizeSeeds(rnd);
		_BuildConfiguredWorld(rnd);
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


				const int h0 = Objects::FindApplicableGround(Grid::LCellIdI((UINT32)i,0),true, true);	//make sure we don't end up IN the lava
				if (h0 == Objects::badHeight || h0 == Grid::CellStack::BottomHeight || !testBase->CanStandOn(World::actual,Grid::LCellIdI((UINT32)i,h0-1)))
					continue;
				{
					const auto&n = Grid::low.grid.GetCell(i).neighborhood;
					bool broken = false;
					for (int k = 0; k < n.numNeighbors && !broken; k++)
					{
						const int h1 = Objects::FindApplicableGround(Grid::LCellIdI(n.neighborID[k],0),true, true);
						if (h1 == Objects::badHeight || h1 == Grid::CellStack::BottomHeight || !testBase->CanStandOn(World::actual,Grid::LCellIdI(n.neighborID[k],h1-1)))
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
							d2 = M::sqr(sqrt(d2) - base->turret->horizontalAttackRange.GetValue(e->GetResearchState()));

						dist2 = std::min(dist2, d2);
					}
					else
					{
						minNexusHubDist2 = std::min(minNexusHubDist2, d2);
					}
				});

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
			const count_t numZones = Game::Config::CountTheoreticallyAccessibleZones(temporaryFactionCountBonus);
			const count_t unlocked = Game::Config::CountActuallyAccessibleZones(temporaryFactionCountBonus);
			LogEvent(EventSignificance::Common,"Re-counted necessary EMP Zones (bonus="+String(temporaryFactionCountBonus)+"): raw="+String(numZones)+", final="+String(unlocked));
			SetUnlockedEMPZones(unlocked);
		}
		else
		{
			LogEvent(EventSignificance::Common,"Clearing all EMP Zones");
			SetUnlockedEMPZones(NumZones);
		}
	}


	void			CorrectResidualEMPDamage(float&atTime, UINT16 oldEmpZonesUnlocked, UINT16 newEmpZonesUnlocked)
	{
		Objects::ForeachWorldEntity([&atTime,oldEmpZonesUnlocked,newEmpZonesUnlocked](const Objects::PWorldEntity&e)
		{
			if (Grid::AffectedByEMP(e->location,oldEmpZonesUnlocked) && !Grid::AffectedByEMP(e->location,newEmpZonesUnlocked))
			{
				e->HealAllEMPDamage(atTime);
				LogEvent(EventSignificance::Common,e.get(),"EMP-Zone has liften. All EMP damage removed");
			}
			else
				if (!Grid::AffectedByEMP(e->location,oldEmpZonesUnlocked) && Grid::AffectedByEMP(e->location,newEmpZonesUnlocked))
					Game::ApplyEMPZoneDamage(e,atTime);
		});
	}

	void		SetUnlockedEMPZones(count_t unlocked)
	{
		const UINT16 newUnlocked = (UINT16)(((1<<unlocked) - 1));
		if (unlockedEMPZones == newUnlocked)
			return;
		LogEvent(EventSignificance::Common,"Updating unlocked EMP Zones (unlocked="+String(unlocked)+"): "+Bin2Str::Convert(&unlockedEMPZones,sizeof(unlockedEMPZones))+"->"+Bin2Str::Convert(&newUnlocked,sizeof(newUnlocked)));
		float t = 0;
		CorrectResidualEMPDamage(t,unlockedEMPZones,newUnlocked);
		Grid::empZonesUnlocked = unlockedEMPZones = newUnlocked;
		Grid::empZonesLocked = empZonesLocked = AllZones  & ~unlockedEMPZones;
		Channels::lockEmpZones.SendTo(::Network::server,empZonesLocked,Channels::Level::Faction);


	}


	void		AppendAvailablePlanetInitFilesFrom(bool isFactoryDefault, FileSystem::FolderScan folder, Ctr::Buffer0<TPlanetIniFile,Swap>&outList)
	{
		if (!folder.IsValidLocation())
		{
			LogEvent(EventClass::Preset::remoteControlError,"Internal error: Unable to locate planets-directory '"+String(folder.GetLocation())+"'");
			return;
		}
		FileSystem::File file;
		while (folder.NextFile("ini",file))
		{
			outList.Append().name = String(file.GetInnerName());
			if (isFactoryDefault)
				outList.Last().factoryDefault.swap(file);
			else
				outList.Last().userDefined.swap(file);
		}

	}

	bool			IsPlanet(const String&name)
	{
		if (name.BeginsWith('*'))
			return FileSystem::IsFile(RuntimeConfig::sharedConfig.Lookup("config/myPlanets/"+name.SubString(1)+".ini"));
		return FileSystem::Folder().FindFile("../predefinedPlanets/"+PathString(name)+".ini") != nullptr;
	}


	void		GetAvailablePlanetIniFiles(Ctr::Array<TPlanetIniFile>&rsOut)
	{
		Ctr::Buffer0<TPlanetIniFile,Swap> files;
		
		AppendAvailablePlanetInitFilesFrom(false, FileSystem::Folder(RuntimeConfig::sharedConfig.Lookup("config/myPlanets")),files);
		AppendAvailablePlanetInitFilesFrom(true, FileSystem::Directory().FindDirectory("../predefinedPlanets"),files);

		HashTable<String,TPlanetIniFile,Ctr::StdHash,Swap,Swap>	merged;
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
