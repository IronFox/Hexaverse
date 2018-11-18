#pragma once
#include "abstractEvent.h"
#include <protocol.h>
#include <objects/components.h>
#include <math/vector.h>
#include "../visual/display.h"
#ifdef HAS_WINDOW
	#include <al/openal.h>
	#include <objects/geometry.h>
	#include "../visual/scene/objectRendering.h"
#endif

namespace Animation
{
	#ifdef HAS_WINDOW
		namespace DX11 = Engine::DX11;
		namespace OpenAL = Engine::OpenAL;
	#endif
	class Trajectory : public Event
	{
		typedef Event			Super;
	public:
		static void				StaticCreate();
		static void				StaticDestroy();

		/**/					Trajectory(Protocol::PreciseAttackTrajectory&);
		/**/					Trajectory(Protocol::VagueAttackTrajectory&);

		virtual void			Begin(bool instant) override;
		virtual void			End() override;
		virtual void			Update(float x) override;
		#ifdef HAS_WINDOW
			virtual void		Render() const override;
		#endif
		virtual D::String		Describe() const override;
	private:
		#ifdef HAS_WINDOW
			typedef Buffer0<M::TVec4<> >	TTrajectory;
			struct TTrajectoryAnimation
			{
				TTrajectory					activeTrajectory;
				Objects::Geometry			trajectoryGeometry;
				DX11::InstructionBlock32	instruction;
				float						maxProjectileProjection,projectileAt=0, beamHeight;
				M::TVec3<>					beamBase;
				OpenAL::Source				localizedSoundSource;
			};
		#endif

		Objects::Component::Turret*	attackerTurret;	//reference targetEntityID to retrieve the identity of the attacker (precise attack only)
		float						attackRange,attackAngle;
		M::TVec3<>					attackerPosition;
		bool						isAutoTarget;
		M::TVec3<>					explosionCenter;
		float						explosionRadius;
		float						eventTime;
		#ifdef HAS_WINDOW
			TTrajectoryAnimation	trajectoryAnimation;
		#endif

	};

}
