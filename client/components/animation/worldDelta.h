#pragma once
#include "abstractEvent.h"
#include <protocol.h>

namespace Animation
{
	class WorldDelta : public Event
	{
		typedef Event			Super;
	public:
		/**/					WorldDelta(Protocol::WorldDelta&);
		virtual void			Begin() override;
		virtual D::String		Describe() const override;

		virtual bool			IsWorldDelta() const override {return true;}

	//private:
		Buffer0<Protocol::WorldDelta,Swap>	deltas;
		bool					isComplete=false;
	};

}

