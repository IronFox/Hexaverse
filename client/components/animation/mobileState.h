#pragma once
#include "abstractEvent.h"
#include <protocol.h>

namespace Animation
{
	class MobileState : public Event
	{
		typedef Event			Super;
	public:
		static void				StaticCreate();
		static void				StaticDestroy();

		/**/					MobileState(Protocol::MobileState&);
		virtual void			Begin(bool instant) override;
		virtual void			End() override	{};
		virtual void			Update(float x) override	{};
		virtual D::String		Describe() const override;

		virtual bool			IsMobileState() const override {return true;}

		static void				UpdateAutomation(const Objects::PVehicle&v);
		static void				Apply(const Objects::PMobileEntity&v, Protocol::MobileState&vs, bool isNew, bool isNewForEMP);

	private:
		Protocol::MobileState	state;
	};

}

