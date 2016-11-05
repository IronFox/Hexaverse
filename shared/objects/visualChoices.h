#ifdef HAS_WINDOW

#ifndef visualChoicesH
#define visualChoicesH

#include "../sphereGrid.h"
#include "types.h"

namespace Objects
{
	struct Choice;

	extern bool		controlIsEnabled /*= false*/;
	extern Buffer0<Choice>			movementChoices;
	extern bool						hasArrowCellTarget,
									showArrow;
	extern float3					arrowFrom;	//arrow visualization point of origin. Might be the location of a vehicle, or any of the locations in its task queue. Only relevant if showArrow is true
	extern Grid::LCellIdI			arrowFromCell;
	extern TrajectoryEndPoint		arrowCellTarget;
	extern Buffer0<Grid::HCellIdI>	cellSelection;	//actually selected cells in either build or remove block mode
	extern Buffer0<Grid::HCellIdU>	buildCandidates, removeCandidates;	//possible cells to build or remove - used for visualization purposes only



	void				UpdateChoices(bool controlEnabled);
	/**
	Detects the ray-intersection with currently visible choices (in movementChoices)
	@return Index of the intersected choices, or InvalidIndex, if none was detected
	*/
	index_t				DetectChoiceIntersection(const TVec3<>&b, const TVec3<>&d, float&distance);
	/**
	Detects and handles any ray-choice-intersection
	@return true, if a choices intersection was determined (and handled), false otherwise
	*/
	bool				HandleChoiceIntersection(const TVec3<>&b, const TVec3<>&d, float distance);

}



#endif
#endif
