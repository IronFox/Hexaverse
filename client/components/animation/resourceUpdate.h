#pragma once
#include "abstractEvent.h"
#include <protocol.h>

namespace Animation
{
	class ResourceUpdate : public Event
	{
		typedef Event			Super;
	public:
		/**/					ResourceUpdate(Protocol::ResourceUpdate&);
		virtual void			Begin(bool instant) override;
		virtual D::String		Describe() const override;
	private:
		Protocol::ResourceUpdate	up;
	};

}

