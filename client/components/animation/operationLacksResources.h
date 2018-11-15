#pragma once

#ifdef HAS_WINDOW
#include "abstractEvent.h"

namespace Animation
{
	class OperationLacksResources : public Event
	{
		typedef Event			Super;
	public:

		/**/					OperationLacksResources(Protocol::OperationLacksResources&);

		virtual void			Begin() override;
		virtual String			Describe() const override;
	private:
		Protocol::OperationLacksResources	op;
	};
}






#endif
