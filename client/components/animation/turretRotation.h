#pragma once
#include "abstractEvent.h"
#include <protocol.h>

namespace Animation
{
	class TurretRotation : public Event
	{
		typedef Event			Super;
	public:
		static const constexpr float ExplosionTime = 0.1f;

		/**/					TurretRotation(Protocol::EntityTurretRotation&);

		virtual void			Begin() override;
		virtual void			End() override;
		virtual void			Update(float x) override;
		virtual D::String		Describe() const override;
	private:
		M::TVec2<>				rotateTo,rotateFrom;
		M::TVec3<>				rotateToWorld;
		Objects::PWorldEntity	allocatedTarget;
	};

}

