#pragma once

#include <container/array.h>
#include "suborbital.h"

namespace Objects
{
	namespace D = DeltaWorks;	

	template <typename T>
		class VolumeGrid
		{
		public:

			static const count_t Height = Grid::CellStack::Range
							+1	//sub-orbital
						;


			static index_t	EntityHeightToLayer(int h)
			{
				if (h == SubOrbital::Height)
					return Grid::CellStack::Range;

				return Grid::CellStack::ToLayer(h);
			}

			static int		LocalLayerToEntityHeight(index_t layer)
			{
				if (layer >= Grid::CellStack::Range)
					return SubOrbital::Height;

				return Grid::CellStack::ToHeight(layer);
			}


			/**/		VolumeGrid():entries(Grid::LowSetup::GridType::CellCount * Height)
			{}


			index_t		CellToGridIndex(const Grid::LCellIdI&location) const
			{
				index_t rs = EntityHeightToLayer(location.height) * Grid::LowSetup::GridType::CellCount + location.stackID;
				ASSERT_LESS__(rs,entries.GetLength());
				return rs;
			}

			const T&	operator[](const Grid::LCellIdI&loc)	const
			{
				return entries[CellToGridIndex(loc)];
			}


			T&			operator[](const Grid::LCellIdI&loc)
			{
				return entries[CellToGridIndex(loc)];
			}

			void		Fill(const T&pattern)
			{
				entries.Fill(pattern);
			}


			size_t		GetStructureSize() const
			{
				return entries.GetContentSize() + sizeof(*this);
			}

		protected:
			D::Array<T>	entries;
		};


}
