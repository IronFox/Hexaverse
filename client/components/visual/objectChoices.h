#pragma once
#ifdef HAS_WINDOW


#include <grid/stack.h>
#include <objects/types.h>

namespace Objects
{
	struct Choice;

	//extern bool		controlIsEnabled /*= false*/;
	extern Buffer0<Choice>			movementChoices;
	extern TChoiceVisualization		choiceVis;
	extern Buffer0<Grid::HCellIdI>	cellSelection;	//actually selected cells in either build or remove block mode
	extern Buffer0<Grid::HCellIdU>	buildCandidates, removeCandidates;	//possible cells to build or remove - used for visualization purposes only



	void				UpdateChoices(bool controlEnabled);
	/**
	Detects the ray-intersection with currently visible choices (in movementChoices)
	@return Index of the intersected choices, or InvalidIndex, if none was detected
	*/
	index_t				DetectChoiceIntersection(const M::TVec3<>&b, const M::TVec3<>&d, float&distance);
	/**
	Detects and handles any ray-choice-intersection
	@return true, if a choices intersection was determined (and handled), false otherwise
	*/
	bool				HandleChoiceIntersection(const M::TVec3<>&b, const M::TVec3<>&d, float distance);
	/**
	Resets the cursor mode to idle.
	Updates choices and commands if necessary.
	@param controlEnabled Pass true if player controls are currently enabled (not disabled due to wrong phase, or open modal dialogs)
	@return true, if any modes were disabled
	*/
	bool			DisableModes(bool controlEnabled);
}



#endif
