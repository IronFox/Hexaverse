#pragma once
#include "abstractEvent.h"
#include <protocol.h>

namespace Animation
{
	class StructureConstruction : public Event
	{
		typedef Event			Super;
	public:
		/**/					StructureConstruction(Protocol::StructureConstruction&, const Objects::PStructure&s);
		virtual void			Begin() override;
		virtual D::String		Describe() const override;
	private:
		Objects::PStructure		allocatedTarget;
	};

}

