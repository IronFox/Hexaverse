#pragma once
#ifdef HAS_WINDOW

#include <grid/worldData.h>
#include <grid/sensorCoverage.h>
#include <math/matrix.h>

namespace Grid
{

	index_t	TestLowGridIntersection(TVec3<> b, TVec3<> d, float radius, float&distance);



}

#endif
