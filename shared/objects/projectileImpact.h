#pragma once

#include <general/enumeration.h>
#include <math/vector.h>
#include <global_string.h>
#include "../grid/stack.h"

namespace Objects
{

	CONSTRUCT_ENUMERATION3(
		TraceResult,
			InvalidTrajectory,
			ReachesTarget,
			MidFlightImpact
	);
	
	namespace D = DeltaWorks;

	struct ImpactDetails
	{

		TraceResult			result;
		D::M::TVec3<>		impactAt;	//!< World location of any detected impact. Valid only if result!=TrajectoryResult::InvalidTrajectory
		Grid::HCellIdI		impactCell;	//!< High resolution cell location of any detected impact. Valid only if result!=TrajectoryResult::InvalidTrajectory
		float				artilleryAngle=0;	//!< Used artillery angle for this impact. Valid only if result!=TrajectoryResult::InvalidTrajectory, and used weapon is artillery
		/**
		Geometric distance to the given destination.
		Valid only if result != TrajectoryResult::InvalidTrajectory
		*/
		float				distanceToTarget=0;

		friend D::String	ToString(const ImpactDetails&det);

		operator TraceResult() const {return result;}
		bool				operator==(const TraceResult&rs) const {return result == rs;}
		bool				operator!=(const TraceResult&rs) const {return result != rs;}

		bool				operator!=(const ImpactDetails&other) const	{return !operator==(other);}
		bool				operator==(const ImpactDetails&other) const;

		void				AssertEquality(const ImpactDetails&other) const;
	};
}
