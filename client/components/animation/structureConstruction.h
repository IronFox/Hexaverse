#pragma once
#include "abstractEvent.h"
#include <protocol.h>

namespace Animation
{
	class StructureConstruction : public Event
	{
		typedef Event			Super;
	public:
		/**/					StructureConstruction(Protocol::StructureConstruction&);
		virtual void			Begin(bool instant) override;
		virtual D::String		Describe() const override;
	private:
		Protocol::StructureConstruction construct;
	};

}

