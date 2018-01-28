#ifndef aiH
#define aiH

#include <objects.h>
#include <protocol.h>
#include <objects/pathfinding.h>

namespace AI
{
	extern Buffer0<Objects::PVehicle>	vehicles;

	void	Process(float timeQuantum);
	void	SignalStart(const String&because, const TCodeLocation&caller);


	/**
	The following block function is used by AI mapping and can be used by other automating vehicles
	@return true, if cell is 'blocked' and should not be passed
	*/
	bool	BlockTest(const Grid::LCellIdI&location);

	void	AssemblePath(const Objects::Pathfinding::ReachabilityMapper&mapper, const Objects::Pathfinding::ReachabilityMapper::MapNode*current, BasicBuffer<Grid::LCellIdI>&outResult);
	void	IssuePath(const Objects::PVehicle&v, const Objects::Pathfinding::ReachabilityMapper&mapper, const Objects::Pathfinding::ReachabilityMapper::MapNode*current);

}



#endif
