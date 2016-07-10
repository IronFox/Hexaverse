#include <global_root.h>
#include "worldState.h"
#include <protocol.h>
#include "networkState.h"
#include "../networkChannels.h"

namespace World
{

	Grid::WorldData	spectatorWorldState;
	bool			terrainChanged=false;
	UINT32			empZonesLocked = 0, unlockedEMPZones = 0xFFFF;
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
				if (faction->visibility.families[family].coverageChanged)
					numChanged++;

			if (numChanged)
			{
				BYTE at = 0;
				for (BYTE family = 0; family < BaseGrid::NumFamilies; family++)
					if (faction->visibility.families[family].coverageChanged)
					{
						faction->visibility.families[family].coverageChanged = false;
						at++;
						Protocol::GenerateWorldDelta(&faction->visibility,
							&faction->currentState,
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
			GenerateWorldDelta(&faction->visibility,&faction->currentState,delta,familyId,rightNow,last,Protocol::WorldDeltaType::ProgressiveUpdate);

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
				GenerateWorldDelta(nullptr,&spectatorWorldState,delta,familyId,rightNow,last,Protocol::WorldDeltaType::ProgressiveUpdate);
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


			Grid::high.family[familyId].normal.outdated = false;
		}


		
		void SendChangedFamilies()
		{
			BYTE changed = 0,counter=0;
			for (BYTE k = 0; k < BaseGrid::NumFamilies; k++)
			{
				if (Grid::high.family[k].normal.outdated)
					changed ++;
			}
			if (changed)
			{
				for (BYTE k = 0; k < BaseGrid::NumFamilies; k++)
				{
					if (Grid::high.family[k].normal.outdated)
						SignalFamilyChange(k,++counter == changed,RuntimeConfig::IsInAnyCommandPhase());
				}
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
				GenerateWorldDelta(nullptr,nullptr, deltas[k], k, true, k+1==BaseGrid::NumFamilies, Protocol::WorldDeltaType::GlobalCopy);
			}
		}
		else
		{
			Protocol::WorldDelta delta;
			for (BYTE k = 0; k < BaseGrid::NumFamilies; k++)
			{
				bool isLast = k+1==BaseGrid::NumFamilies;
				GenerateWorldDelta(nullptr, nullptr, delta, k, true, isLast, Protocol::WorldDeltaType::GlobalCopy);
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
				const Grid::CellStack&source = Grid::actualWorld.cellStack[i];
				for (index_t layer = 0; layer < Grid::CellStack::Range; layer++)
					target.volumeCell[layer] = source.volumeCell[layer].content;
			}

			Concurrency::parallel_for(index_t(0),Grid::HighSetup::GridType::CellCount,[](index_t stackIndex)
			{
				Grid::CellStack&stack = Grid::actualWorld.cellStack[stackIndex];
				Grid::CellStack*nStacks[6] = {nullptr};
				const auto&neighborhood = Grid::high.grid.GetCell(stackIndex).neighborhood;
				DBG_ASSERT_LESS_OR_EQUAL__(neighborhood.numNeighbors,ARRAYSIZE(nStacks));
				for (int i = 0; i < neighborhood.numNeighbors; i++)
					nStacks[i] = Grid::actualWorld.cellStack + neighborhood.neighborID[i];
				TStack&target = tempStacks[stackIndex];
				bool thisChanged = false;
				for (UINT32 layer = 0; layer < Grid::CellStack::Range; layer++)
				{
					if (stack.volumeCell[layer].content == Grid::Content::Clear)
					{
						BYTE Fill = 
							layer+1 < Grid::CellStack::Range && Grid::CellStack::Cell::IsLiquid(stack.volumeCell[layer].content) ? stack.volumeCell[layer].content : 0;
						if (!Fill)
							for (int i = 0; i < neighborhood.numNeighbors; i++)
								if (Grid::CellStack::Cell::IsLiquid(nStacks[i]->volumeCell[layer].content))
								{
									Fill = nStacks[i]->volumeCell[layer].content;
									break;
								}

						if (Fill)
						{
							target.volumeCell[layer] = Fill;
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
						Grid::CellStack&target = Grid::actualWorld.cellStack[k];
						const TStack&source = tempStacks[k];
						for (index_t layer = 0; layer < Grid::CellStack::Range; layer++)
							target.volumeCell[layer].content = source.volumeCell[layer];
					}
					Grid::low.family[i].SetOutdated(0);
					Grid::high.family[i].SetOutdated(0);
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
							Grid::actualWorld.Clear(entity->location + y,0);
						else
							Grid::actualWorld.cellStack[entity->location.stackID].MakeLocalHole(entity->location.height+y);
					}
				}
				else
				{
					for (int y = 0; y < h; y++)
					{
						if (broad)
						{
							if (!Grid::actualWorld.ClearNonLava(entity->location + y,0))
							{
								waste << entity;
								break;
							}
						}
						else
						{
							if (Grid::actualWorld.cellStack[entity->location.stackID].GetCellI(entity->location.height+y).content == Grid::Content::Lava)
							{
								waste << entity;
								break;
							}
							Grid::actualWorld.cellStack[entity->location.stackID].MakeLocalHole(entity->location.height+y);
						}
					}
				}
			});
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
		Grid::actualWorld.BuildWorld(Grid::currentPlanetConfig);
		Grid::actualWorld.ExportDiscovered(Grid::initialWorld, Config::worldDiscovery);
		Grid::Resource::FillAll(random);
		if (Objects::Faction::nexus)
			Objects::Faction::nexus->currentState = Grid::initialWorld;

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
			Grid::high.family[i].SetOutdated(0);
			Grid::low.family[i].SetOutdated(0);
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
				if (h0 == Objects::badHeight || h0 == -Grid::CellStack::HalfRange || !testBase->CanStandOn(Grid::actualWorld,Grid::LCellIdI((UINT32)i,h0-1),false,false))
					continue;
				{
					const auto&n = Grid::low.grid.GetCell(i).neighborhood;
					bool broken = false;
					for (int k = 0; k < n.numNeighbors && !broken; k++)
					{
						const int h1 = Objects::FindApplicableGround(Grid::LCellIdI(n.neighborID[k],0),true);
						if (h1 == Objects::badHeight || h1 == -Grid::CellStack::HalfRange || !testBase->CanStandOn(Grid::actualWorld,Grid::LCellIdI(n.neighborID[k],h1-1),false,false))
							broken = true;
					}
					if (broken)
						continue;

				}


				float3 ref = Grid::low.cells[i].centerVertex.xyz;// * r;
				float dist2 = std::numeric_limits<float>::max();
				float ndist2 = std::numeric_limits<float>::max();
				foreach (Objects::structures, s)
				{
					float d2 = Vec::quadraticDistance(Grid::low.cells[(*s)->location.stackID].centerVertex.xyz,ref);
					if (!To<Objects::NexusPort>::DynamicCast(*s))
					{
						Objects::PTurretBase base = (*s)->GetTurretBase();
						if (base)
							d2 = sqr(sqrt(d2) - base->GetFlatAttackRange());	//fair enough here
						if (Grid::AffectedByEMP((*s)->location))
							d2 *= 4.f;
						if ((*s)->owner != Objects::Faction::nexus)
							d2 *= 0.25f;
						dist2 = std::min(dist2, d2);
					}
					else
					{
						ndist2 = std::min(ndist2, d2);
					}
				}
				foreach (Objects::vehicles, v)
				{
					float d2 = Vec::quadraticDistance(Grid::low.cells[(*v)->location.stackID].centerVertex.xyz,ref);
					dist2 = std::min(dist2, d2);
				}
				if (Objects::Faction::all.IsEmpty())
					dist2 = dist2 > 0.35f ? sqr(random.getFloat(1,2)) : 0.f;
				else
					dist2 *= random.getFloat(0.9f,1.f / 0.9f);

				biggestNDist = std::max(biggestNDist,ndist2);

				if (dist2 > bestDist && ndist2 > 0.25f)	//actual
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
			count_t numZones = Game::mostFactionsAlive+temporaryFactionCountBonus;	//not counting nexus faction, but counting new faction(s)
			numZones = (numZones/3) + !!(numZones % 3);

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

			Channels::lockEmpZones.SendTo(::Network::server,empZonesLocked,2);
		}

	}


	void		SetUnlockedEMPZones(int unlocked)
	{
		Grid::empZonesUnlocked = unlockedEMPZones = ((1<<unlocked) - 1);
		Grid::empZonesLocked = empZonesLocked = ((1 << 12)-1)  & ~unlockedEMPZones;
	}

}
