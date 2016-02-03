#ifndef objectSystemsH
#define objectSystemsH

#include <math/vector.h>
#include <math/matrix.h>
#include "../sphereGrid.h"

namespace Objects
{
	void		BuildNormalizedSystem(const Grid::LCellIdI&location, TMatrix4<>&system);
	void		BuildSystem(const TVec3<>&location, float hScale, float vScale, TMatrix4<>&system);
	void		BuildSystem(const Grid::LCellIdI&location, float hScale, float vScale, TMatrix4<>&system);
	void		BuildStructureSystem(const Grid::LCellIdI&location, TMatrix4<>&system);



}


#endif
