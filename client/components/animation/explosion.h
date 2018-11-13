#pragma once
#include "abstractEvent.h"
#include <protocol.h>

namespace Animation
{
	//namespace DX11 = Engine::DX11;
	//namespace OpenAL = Engine::OpenAL;
	class Explosion : public Event
	{
		typedef Event			Super;
	public:
		static const constexpr float ExplosionTime = 0.1f;

		/**/					Explosion(Protocol::Explosion&);

		virtual void			Begin() override;
		virtual void			End() override;
		virtual D::String		Describe() const override;
	private:
		M::TVec3<>				explosionCenter;
		float					explosionRadius;
		bool					empExplosion;
	};

}

