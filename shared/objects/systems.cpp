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
		float hscale = Grid::SingleMeanLayerHeight * 0.75f;
			//0.55f;
		float r = Grid::ToRadius(location.height);
		float vscale = Grid::ToRadius(location.height+1) -  r;

		auto scale = WorldEntity::GetScaleAtHeight(location.height);
		hscale *= scale;
		BuildSystem(location,hscale,vscale,system);
	}




}
