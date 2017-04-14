#include <global_root.h>
#include "mine.h"

namespace Objects
{
	/*virtual override*/	void			Mine::PrototypeSetup()
	{
		Super::PrototypeSetup();
	}

	

	bool Mine::LinkToCore(float atTime)
	{
		#ifdef CLIENT
			return isLinkedToCore;
		#else
			this->naturalResources = NaturalResourceVector();
			bool success = true;
			{
				#ifdef HYBRID_GRID
					const UINT32 higher = Grid::low.grid.GetCell(location.stackID).higherEquivalent;
				#else
					const UINT32 higher = location.cellID;
				#endif
				Grid::CellStack&stack = World::actual.cellStack[higher];

				UINT layer = Grid::CellStack::ToLayer(location.height);
				UINT i = 0;
				if (Grid::CellStack::bottom.content == Grid::Content::Clear)
					while (stack.volumeCell[i].content == Grid::Content::Clear && i < layer)
						i++;	//skip air if no ground
				if (i == layer)
					return false;	//this is REALLY weird, because the foundation would be missing

				for (UINT j = layer-1; j >= i && j < layer; j--)
				{
					int h = Grid::CellStack::ToHeight(j);
					if ((j == layer-1 || !stack.GetCellU(j).structureCounter) && !GetVehicle(Grid::LCellIdI(location.stackID,h)))
						stack.LocalOverwrite(h, Grid::Content::Foundation);
					else
					{
						success = false;
						break;
					}
				}
				World::actual.SignalChangeUpTo(location-1);
	//			stack.UpdateLighting();
			}
			InvalidateTerrain(location,atTime);

			if (success)
			{
				naturalResources.SampleWorld(location.stackID);
				naturalResources *= GetEfficiency();
			}
			if (success)
				RegisterChange(WorldEntity::CanEnableChanged,atTime);
			return success;
		#endif
	}






	


	bool	Mine::IsLinkedToCore()	const
	{
		#ifdef CLIENT
			return isLinkedToCore;
		#else
			#ifdef HYBRID_GRID
				const UINT32 higher = Grid::low.grid.GetCell(location.stackID).higherEquivalent;
			#else
				const UINT32 higher = location.cellID;
			#endif
			const Grid::CellStack&stack = World::actual.cellStack[higher];
			UINT layer = Grid::CellStack::ToLayer(location.height);
			UINT i = 0;
			if (Grid::CellStack::bottom.content == Grid::Content::Clear)
				while (stack.volumeCell[i].content == Grid::Content::Clear && i < layer)
					i++;	//skip air if no ground
			if (i == layer)
				return false;	//this is REALLY weird, because the foundation would be missing

			for (; i < layer; i++)
			{
				if (stack.GetCellU(i).content != Grid::Content::Foundation)
					return false;
			}
			return true;
		#endif
	}

	#ifdef CLIENT
		void	Mine::SetLinkedToCore(bool b, bool controlEnabled)
		{
			if (isLinkedToCore == b)
				return;
			isLinkedToCore = b;
		}
	#endif



	/*virtual override*/ void			Mine::DescribeType(BaseCommand::Label&label, UINT32 descFlags)
	{
		label.BeginBrief();
		Super::DescribeType(label,  descFlags);
		label.MarkBriefEnd();
		label << "Enabled mines extract any natural resources available in the planet-core beneath their location. They are essential in supplying other structures of the local compound."
		" Mines should be build only on known occurrences of natural resources."
		" They cannot be constructed atop each other, since they require a working link to the planet core in order to operate."
		//" Mines constructed on a height level of -8 or lower are twice as effective."
		" Mines, like many structures, require "+String(ResourceTag(Resource::Energy))+" from generators to operate."
		;
	}


}
