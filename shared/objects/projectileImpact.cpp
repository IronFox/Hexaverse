#include "projectileImpact.h"

namespace Objects
{


	bool				ImpactDetails::operator==(const ImpactDetails&other) const
	{
		return result == other.result
				&& Vec::equal(impactAt,other.impactAt)
				&& artilleryAngle == other.artilleryAngle
				&& distanceToTarget == other.distanceToTarget;
	}

	void				ImpactDetails::AssertEquality(const ImpactDetails&other) const
	{
		ASSERT2__(result == other.result,result.ToString(),other.result.ToString());
		ASSERT2__(Vec::equal(impactAt,other.impactAt),impactAt,other.impactAt);
		ASSERT_EQUAL__(artilleryAngle,other.artilleryAngle);
		ASSERT_EQUAL__(distanceToTarget,other.distanceToTarget);
	}
	

	D::String	ToString(const ImpactDetails&det)
	{
		if (det.result == TraceResult::InvalidTrajectory)
			return "No Impact";
		return String(det.result.ToString())+": @"+ToString(det.impactAt)+"/"+ToString(det.impactCell)+", dist="+String(det.distanceToTarget);
	}
}
