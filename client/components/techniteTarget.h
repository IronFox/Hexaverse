#ifndef componentTechniteTargetH
#define componentTechniteTargetH
#include <uniqueID.h>
#include <sphereGrid.h>

namespace TechniteTarget
{
	void	Set(const TUniqueID&techniteID, const Grid::LCellIdI&);
	void	Unset(const TUniqueID&techniteID);

	void	SignalSelection(const TUniqueID&techniteID);
	void	FlushAll();
}

#endif