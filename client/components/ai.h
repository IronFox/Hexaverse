#ifndef aiH
#define aiH

#include "../../shared/objects.h"
#include "../../shared/protocol.h"

namespace AI
{
	extern Buffer0<Objects::PVehicle>	vehicles;

	void	Process(float timeQuantum);
	void	SignalStart();


	/**
	The following block function is used by AI mapping and can be used by other automating vehicles
	@return true, if cell is 'blocked' and should not be passed
	*/
	bool	BlockTest(const Grid::LCellIdI&location);

	void	IssuePath(const Objects::PVehicle&v, const Objects::ReachabilityMapper&mapper, const Objects::ReachabilityMapper::MapNode*current);

}



#endif
