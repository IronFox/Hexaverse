#include <global_root.h>
#include "systems.h"
#include "entity.h"

namespace Objects
{
	void		BuildSystem(const Grid::LCellIdI&location, float hScale, float vScale, M::TMatrix4<>&system)
	{
		const M::TVec3<>&up = Grid::low.cells[location.stackID].cornerCenter;
		//M::TVec3<> y, x;
		float r = Grid::ToRadius(location.height)+0.1f;
		Mat::makeAxisSystem(up * r, up, 2, system);
		system.x *= hScale;
		system.y *= hScale;
		system.z *= vScale;

	}

	void		BuildSystem(const M::TVec3<>&location, float hScale, float vScale, M::TMatrix4<>&system)
	{
		M::TVec3<> up = location;
		Vec::normalize(up);
		//M::TVec3<> y, x;
		//float r = Grid::ToRadius(location.height);
		Mat::makeAxisSystem(location, up, 2, system);
		system.x *= hScale;
		system.y *= hScale;
		system.z *= vScale;

	}


	void		BuildStructureSystem(const Grid::LCellIdI&location, M::TMatrix4<>&system)
	{
		const auto&cell = Grid::low.cells[location.stackID];
		float hscale = std::min(cell.minCornerDistance, Grid::low.cells[0].minCornerDistance) * Grid::ToRadius(0) * 0.96f;
		float r = Grid::ToRadius(location.height);
		float vscale = Grid::ToRadius(location.height+1) -  r;
		//if (cell.numCorners < 6)
			hscale *= 0.9f;
		hscale *= WorldEntity::GetScaleAtHeight(location.height);
		BuildSystem(location,hscale,vscale,system);
	}




}
