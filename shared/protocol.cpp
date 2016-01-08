#include <global_root.h>
#include "protocol.h"

namespace Protocol
{

	template <typename T0, typename T1>
		static void Compress(const Array<T0>&dts, Array<T1>&dest)
		{
			static Buffer<T1>	blocks;
			blocks.Clear();
			foreach(dts,dt)
			{
				if (blocks.IsEmpty() || blocks.last().value != *dt)
				{
					auto&b = blocks.Append();
					b.value = *dt;
					b.repitition = 1;
				}
				else
					blocks.last().repitition++;
			}
			blocks.CopyToArray(dest);
		}
	template <typename T0, typename T1>
		static void Compress(const Array<T0>&dts, Array<T1>&dest, bool assertNotNull)
		{
			static Buffer<T1>	blocks;
			blocks.Clear();
			foreach(dts,dt)
			{
				#ifndef CLIENT
					if (assertNotNull)
						ASSERT__(dt->IsSet());
				#endif
				if (blocks.IsEmpty() || blocks.last().value != *dt)
				{
					auto&b = blocks.Append();
					b.value = *dt;
					b.repitition = 1;
				}
				else
					blocks.last().repitition++;
			}
			blocks.CopyToArray(dest);
		}


	void			GenerateWorldDelta(Grid::VisibilityState*vs, Grid::WorldData*ws, Protocol::WorldDelta&delta, BYTE familyID, bool rightNow, bool last, WorldDeltaType::delta_type_t deltaType)
	{
			const index_t offset = Grid::high.grid.GetFamilyOffset(familyID);
			const count_t length = Grid::high.grid.CountFamilyMembers(familyID);

			const bool writeOutput = deltaType != WorldDeltaType::ProgressiveUpdateNoDelta;

			if (writeOutput)
			{
				delta.timestamp.atTime = std::max(Grid::high.family[familyID].outdatedAt,vs?vs->families[familyID].lastChange : 0.f);
				delta.timestamp.rightNow = rightNow;
				delta.familyAndLastFlag = familyID;
				if (last)
					delta.familyAndLastFlag.value |= 0x80;
			}

			const Grid::CellStack::Cell empty(Grid::Content::Reinterpret(0),0);
			Array<Grid::CellStack::TTypeDelta> tdts(writeOutput ? length * Grid::CellStack::Range : 0);
			//Array<Grid::CellStack::TTechniteFactionDelta> fdts(length * Grid::CellStack::Range);
			//Array<bool> fdtsValid(length * Grid::CellStack::Range);

			if (vs)
				vs->families[familyID].Reset();
			
			Concurrency::parallel_for(index_t(0),length*Grid::CellStack::Range,
			[&tdts,/*&fdts,&fdtsValid,*/length,offset,empty,deltaType,vs,ws](index_t i)
			{
				//index_t stackId = offset + (i / Grid::CellStack::Range);
				//index_t layer = i % Grid::CellStack::Range;
				index_t stackID = offset + (i % length);
				index_t layer = i / length;
				switch (deltaType)
				{
					#ifndef CLIENT
						case WorldDeltaType::ProgressiveUpdate:
						{
							auto&cell =  vs->cellStack[stackID].volumeCell[layer];
							bool canSee = cell.visibility > 0 || cell.temporarilyVisible;
							if (canSee)
							{
								Grid::actualWorld.cellStack[stackID].volumeCell[layer].
								GetDeltaFromAndUpdate(ws->cellStack[stackID].volumeCell[layer],tdts[i]/*,fdts[i],fdtsValid[i]*/);
							}
							else
							{
								tdts[i].newType = 0;
							}
							cell.temporarilyVisible = false;
						}
						break;
						case WorldDeltaType::ProgressiveUpdateNoDelta:
						{
							auto&cell =  vs->cellStack[stackID].volumeCell[layer];
							bool canSee = cell.visibility > 0 || cell.temporarilyVisible;
							if (canSee)
							{
								ws->cellStack[stackID].volumeCell[layer] = Grid::actualWorld.cellStack[stackID].volumeCell[layer];
								cell.temporarilyVisible = false;
							}
						}
						break;
						case WorldDeltaType::ProgressiveInit:
						{
							auto&cell =  vs->cellStack[stackID].volumeCell[layer];
							bool canSee = cell.visibility > 0 || cell.temporarilyVisible;
							const Grid::CellStack::Cell& source = canSee ? Grid::actualWorld.cellStack[stackID].volumeCell[layer] : Grid::initialWorld.cellStack[stackID].volumeCell[layer];
							source.GetDeltaFrom(empty,tdts[i]/*,fdts[i],fdtsValid[i]*/);
							ws->cellStack[stackID].volumeCell[layer] = source;
							cell.temporarilyVisible = false;
						}
						break;
						case WorldDeltaType::GlobalInitialCopy:
						{
							const Grid::CellStack::Cell& source = Grid::initialWorld.cellStack[stackID].volumeCell[layer];
							source.GetDeltaFrom(empty,tdts[i]/*,fdts[i],fdtsValid[i]*/);
						}
						break;
					#endif
					case WorldDeltaType::LocalCopy:
					{
						const Grid::CellStack::Cell& source = ws->cellStack[stackID].volumeCell[layer];
						source.GetDeltaFrom(empty,tdts[i]/*,fdts[i],fdtsValid[i]*/);
					}
					break;
					case WorldDeltaType::GlobalCopy:
					{
						const Grid::CellStack::Cell& source = Grid::actualWorld.cellStack[stackID].volumeCell[layer];
						source.GetDeltaFrom(empty,tdts[i]/*,fdts[i],fdtsValid[i]*/);
					}
					break;
				}
			});

			if (writeOutput)
			{
				Compress(tdts,delta.data0, /*deltaType == FactionCopy || */deltaType == WorldDeltaType::GlobalCopy/* || deltaType == GlobalInitialCopy*/);
				//{
				//	BYTE lastSet = 0;
				//	for (index_t i = 0; i < fdts.Count(); i++)
				//	{
				//		if (!fdtsValid[i])
				//			fdts[i].newTechniteFaction = lastSet;
				//		else
				//			lastSet = fdts[i].newTechniteFaction;
				//	}
				//	Compress(fdts,delta.data2);
				//}
			}

	}

}	



