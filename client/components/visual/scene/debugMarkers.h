#pragma once
#ifdef HAS_WINDOW
#include <global_string.h>
#include <grid/stack.h>
#include <math/vector.h>

namespace Objects
{
	namespace DebugMarkers
	{
		namespace D = DeltaWorks;
		namespace M = D::Math;

		index_t		Create(const Grid::LCellIdI&loc, const M::TVec3<>&color, const D::String&label);
		void		Delete(index_t);
		void		FlushAll();
		void		Render();
		void		RenderText();
	}
}



#endif
