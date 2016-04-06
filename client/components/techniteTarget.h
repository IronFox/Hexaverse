#ifndef componentTechniteTargetH
#define componentTechniteTargetH
#include <uniqueID.h>
#include <sphereGrid.h>

namespace TechniteTarget
{
	void	SignalSelection(const Grid::HCellIdU&techniteID);
	void	FlushAll();
}

#endif