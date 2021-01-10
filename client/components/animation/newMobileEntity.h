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
		virtual void			Begin(bool instant) override;
		virtual D::String		Describe() const override;
		virtual bool			IsEntityCreation() const override {return true;}
	private:
		Protocol::NewMobileEntity	construct;
	};

}

