#include <global_root.h>
#include "plantation.h"
#include "../gridAndChoices.h"

namespace Objects
{
	/*virtual override*/	void			Plantation::PrototypeSetup()
	{
		Super::PrototypeSetup();
	}


	#ifndef CLIENT
	/*virtual override*/ void				Plantation::Tick(Resource::Vector&v)
	{
		Super::Tick(v);	//get resource vector first, then change landscape

		auto seq = World::actual.BeginChangeSequence(entityTime);

		for (index_t i = 0; i+1 < plantStackIDs.Count(); i++)
		{
			index_t j = i+ entityRandom.NextIndex(plantStackIDs.Count()-i);
			if (i != j)
				swp(plantStackIDs[i],plantStackIDs[j]);
		}

		count_t treesHarvested = 0;
		count_t crystalsHarvested = 0;
		count_t planted = 0;
		const int bottom = std::max(Grid::CellStack::BottomHeight,location.height-3);
		const int top = std::min(Grid::CellStack::TopHeight,location.height+3);
		foreach (plantStackIDs, c)
		{
			seq.SelectStack(*c);
			for (int h = bottom; h <= top; h++)
			{
				const auto&cell = seq.GetCellI(h);
				const auto&upper = seq.GetCellI(h+1);
				if (upper.content != Grid::Content::Clear)
					continue;
				if (upper.structureCounter > 0)
					continue;
				switch (cell.content)
				{
					case Grid::Content::CrystalForest:
						if (crystalsHarvested < MaxHarvestPerRound)
						{
							seq.Change(h,Grid::Content::Crystals);
							crystalsHarvested++;
						}
						break;
					case Grid::Content::Forest:
						if (treesHarvested < MaxHarvestPerRound)
						{
							seq.Change(h,Grid::Content::Trees);
							treesHarvested++;
						}
						break;
					case Grid::Content::Granite:
						if (planted < MaxPlantingPerRound)
						{
							seq.Change(h,Grid::Content::Crystals);
							planted++;
						}
						break;
					case Grid::Content::Grass:
						if (planted < MaxPlantingPerRound)
						{
							seq.Change(h,Grid::Content::Trees);
							planted++;
						}
						break;
					case Grid::Content::Earth:
					case Grid::Content::Sand:
						if (planted < MaxPlantingPerRound)
						{
							seq.Change(h,Grid::Content::Grass);
							planted++;
						}
						break;
					case Grid::Content::Lava:
						if (planted < MaxPlantingPerRound)
						{
							seq.Change(h,Grid::Content::Granite);
							planted++;
						}
						break;
				}
			}
		}
		CheckPresence(0);
	}


	void Plantation::CheckPresence(float t)
	{
		int d2 = GetPresence(Grid::Content::CrystalForest);
		int t2 = GetPresence(Grid::Content::Forest);
		if (d2 != crystalPresence || t2 != treePresence)
		{
			crystalPresence = d2;
			treePresence = t2;
			SignalChangeByExternalCause(WorldEntity::ResourceAvailabilityChanged,t);
		}
	}

	UINT32 Plantation::GetPresence(Grid::Content type) const
	{
		UINT32 rs = 0;
		foreach (plantStackIDs, c)
		{
			for (int h = std::max(Grid::CellStack::BottomHeight,location.height-3); h <= std::min(Grid::CellStack::TopHeight,location.height+3); h++)
			{
				const auto&cell = World::actual.GetCell(Grid::HCellIdI(*c,h));
				const auto&upper = World::actual.GetCell(Grid::HCellIdI(*c,h+1));
				if (upper.content != Grid::Content::Clear)
					continue;
				if (upper.structureCounter > 0)
					continue;
				if (cell.content == type)
				{
					rs ++;
					if (rs >= MaxHarvestPerRound)
						return MaxHarvestPerRound;
				}
			}
		}
		return rs;
	}
	#endif

	/*virtual override*/ void Plantation::Pack(D::IWriteStream&s,Packable::Target target) const
	{
		if (target != Packable::Target::Hostile)
		{
			s.WriteSize(crystalPresence);
			s.WriteSize(treePresence);
		}
		else
		{
			s.WriteSize(0);
			s.WriteSize(0);
		}
		s.WriteFlagSet8(plantTrees,plantCrystals);
	}

	/*virtual override*/ void Plantation::Unpack(D::IReadStream&s)
	{
		s.ReadSize(crystalPresence);
		s.ReadSize(treePresence);
		s.ReadFlagSet8(plantTrees,plantCrystals);
	}


	/*virtual override*/ void Plantation::ToSavegame(D::IWriteStream&s) const
	{
		s.WriteFlagSet8(plantTrees,plantCrystals);
	}

	/*virtual override*/ void Plantation::FromSavegame(D::IReadStream&s)
	{
		s.ReadFlagSet8(plantTrees,plantCrystals);
	}



	#ifdef HAS_WINDOW
		/*virtual override*/ void			Plantation::DescribeType(Command::Base::Label&label, UINT32 descFlags, const OwnerParameter&owner)
		{
			label.BeginBrief();
			Super::DescribeType(label,  descFlags, owner);
			label.MarkBriefEnd();
			label << "Enabled plantations plant and harvest any growing natural resources on surrounding surfaces."
			" Plantations should be build near grass, earth, sand, lava, granite, or existing trees/crystals."
			" Plantations, like many structures, require "+Resource::Energy.GetTagStr()+" from generators to operate."
			;
		}
	#endif

}
