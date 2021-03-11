#ifndef aiH
#define aiH

#include <objects.h>
#include <protocol.h>
#include <objects/pathfinding.h>

namespace AI
{
	void	ClearPending();
	void	AddVehicle(const Objects::PVehicle&v);
	void	SignalStart(const String&because, const TCodeLocation&caller);


	void	Process(float timeQuantum);

	bool	IsDone();

	/**
	(Re)initializes BlockTest() for the specified vehicle
	@param zeroThreatOnly Ignore routes that have any actual hostile threat level, regardless of whether the local unit is currently located in non-zero threat
	@param canIgnoreThreat True if the local vehicle should be able to ignore the threat posed by some chosen outnumbered hostile unit
	@param overConfident Set true to completely ignore hostile threats
	*/
	void	BeginBlockTest(const Objects::PWorldEntity&v, bool zeroThreatOnly, bool canIgnoreThreat, bool overConfident);
	/**
	The following cost function is used by AI mapping and can be used by other automating vehicles.
	It modifies the previously determined location cost to -1 if a blocking condition is determined.
	Must call BeginBlockTest() before using BlockTest()
	*/
	void	CostModifier( const Grid::LCellIdI&location, int&cost );
	
	void	AssemblePath(const Objects::Pathfinding::ReachabilityMapper&mapper, const Objects::Pathfinding::ReachabilityMapper::MapNode*current, BasicBuffer<Grid::LCellIdI>&outResult);
	void	IssuePath(const Objects::PVehicle&v, const Objects::Pathfinding::ReachabilityMapper&mapper, const Objects::Pathfinding::ReachabilityMapper::MapNode*current);

	void	SetMinRoundForHeavyVehicleBases(UINT32 minRoundForHeavyVehicleBases);
	void	SetMinRoundForPlanes(UINT32 minRoundForPlanes);
}



#endif
