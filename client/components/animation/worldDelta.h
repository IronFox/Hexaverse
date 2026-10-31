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
		virtual void			Begin(bool instant) override;
		virtual D::String		Describe() const override;

		virtual bool			IsWorldDelta() const override {return true;}

		void					Append(Protocol::WorldDelta&&delta);
	private:
		Vector<Protocol::WorldDelta,Swap>	deltas;
		bool					isComplete=false;
	};

}

