#ifdef HAS_WINDOW
#include "operationLacksResources.h"
#include <expectedBehavior.h>
#include "../visual/scene/objectIcons.h"

namespace Animation
{
	OperationLacksResources::OperationLacksResources(Protocol::OperationLacksResources&op0):Super(op0.entityID)
	{
		op = std::move(op0);
	}

	/*virtual override*/ String			OperationLacksResources::Describe() const
	{
		return "OperationLacksResources("+ToString(op.entityID)+","+String(op.resourceIndices.Count())+")";
	}


	/*virtual override*/ void OperationLacksResources::Begin()
	{
		auto lockedTarget = Objects::GetEntity(targetEntityID);
		EXPECT1(lockedTarget,targetEntityID);
		
		Objects::Icons::AddAnimatedFloater(lockedTarget,op.resourceIndices);
	}


}


#endif
