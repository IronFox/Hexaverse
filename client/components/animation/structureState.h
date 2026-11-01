#pragma once
#include "abstractEvent.h"
#include <protocol.h>

namespace Animation
{
	class StructureState : public Event
	{
		typedef Event			Super;
	public:
		/**/					StructureState(Protocol::StructureState&);
		virtual void			Begin(bool instant) override;
		virtual void			End() override	{};
		virtual void			Update(float x) override	{};
		virtual D::String		Describe() const override;
		virtual bool			IsStructureState() const override {return true;}


		static void				Apply(const Objects::PStructure&, Protocol::StructureState&);

	private:
		Protocol::StructureState	state;
	};

}

