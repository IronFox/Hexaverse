#pragma once
#include "abstractEvent.h"
#include <protocol.h>

namespace Animation
{
	class DestroyEntity : public Event
	{
		typedef Event			Super;
	public:
		/**/					DestroyEntity(Protocol::DestroyEntity&);
		virtual void			Begin(bool instant) override;
		virtual D::String		Describe() const override;

		static void				Explode(const Objects::PWorldEntity&target);
		static void				Explode(const Objects::PWorldEntity&target, const std::function<void()>&onQuarterDone);

	private:
		bool					explode;
		Objects::RemovalType	removalType;
		Objects::PWorldEntity	allocatedTarget;
		float					eventTime;
	};

}

