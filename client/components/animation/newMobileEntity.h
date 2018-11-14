#pragma once
#include "abstractEvent.h"
#include <protocol.h>

namespace Animation
{
	class NewMobileEntity : public Event
	{
		typedef Event			Super;
	public:
		/**/					NewMobileEntity(Protocol::NewMobileEntity&);
		virtual void			Begin() override;
		virtual D::String		Describe() const override;
	private:
		Protocol::NewMobileEntity	construct;
	};

}

