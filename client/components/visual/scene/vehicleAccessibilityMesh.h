#ifdef HAS_WINDOW
#pragma once

#include <objects/types.h>
#include <engine/aspect.h>

/**
Renders a partially transparent terrain overlay that details where the selected unit can reach in 1, 2, 3 rounds
*/
namespace VehicleAccessibilityMesh
{


	void		Render(const Engine::Frustum<>&);



	/**
	Initializes component and installs event hooks
	*/
	void		Create();

	void		Refresh();

}





#endif
