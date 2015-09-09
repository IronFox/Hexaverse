#include <global_root.h>
#include "systems.h"

namespace Objects
{
	void		BuildSystem(const Grid::LCellIdI&location, float hScale, float vScale, TMatrix4<>&system)
	{
		const TVec3<>&up = Grid::low.cells[location.stackID].cornerCenter;
		//TVec3<> y, x;
		float r = Grid::ToRadius(location.height)+0.1f;
		Mat::makeAxisSystem(up * r, up, 2, system);
		system.x *= hScale;
		system.y *= hScale;
		system.z *= vScale;

	}

	void		BuildSystem(const TVec3<>&location, float hScale, float vScale, TMatrix4<>&system)
	{
		TVec3<> up = location;
		Vec::normalize(up);
		//TVec3<> y, x;
		//float r = Grid::ToRadius(location.height);
		Mat::makeAxisSystem(location, up, 2, system);
		system.x *= hScale;
		system.y *= hScale;
		system.z *= vScale;

	}


	void		BuildStructureSystem(const Grid::LCellIdI&location, TMatrix4<>&system)
	{
		const auto&cell = Grid::low.cells[location.stackID];
		float hscale = std::min(cell.minCornerDistance, Grid::low.cells[0].minCornerDistance) * Grid::ToRadius(0) * 0.96f;
		float r = Grid::ToRadius(location.height);
		float vscale = Grid::ToRadius(location.height+1) -  r;
		//if (cell.numCorners < 6)
			hscale *= 0.9f;
		BuildSystem(location,hscale,vscale,system);
	}




}
