#ifndef objectSystemsH
#define objectSystemsH

#include <math/vector.h>
#include <math/matrix.h>
#include "../grid/stack.h"

namespace Objects
{
	void		BuildSystem(const M::TVec3<>&location, float hScale, float vScale, M::TMatrix4<>&system);
	void		BuildSystem(const Grid::LCellIdI&location, float hScale, float vScale, M::TMatrix4<>&system);
	void		BuildStructureSystem(const Grid::LCellIdI&location, M::TMatrix4<>&system);



}


#endif
