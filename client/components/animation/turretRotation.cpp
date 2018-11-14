#include "turretRotation.h"
#include "../audio/objectSounds.h"

namespace Animation
{

	TurretRotation::TurretRotation(Protocol::EntityTurretRotation&rot):Super(rot.id)
	{
		rotateToWorld = rot.toWorld;
	}

	/*virtual override*/ void TurretRotation::Begin()
	{
		allocatedTarget = Objects::GetEntity(targetEntityID);
		if (allocatedTarget)
		{
			float dummy;
			Objects::PTurretBase b = allocatedTarget->GetTurretBase(dummy);
			ASSERT__(b);

			#ifdef HAS_WINDOW
				if (!b->activitySound.IsCreated())
				{
					b->activitySound.Create();
					b->activitySound.SetLooping(true);
					#ifdef _DEBUG
						b->activitySound.name = allocatedTarget->GetName()+".turretRotate";
					#endif
				}
				b->activitySound.SetWaveBuffer(Objects::GetAmbientSoundOf("turretRotate",ConstName::Turret_Artillery));
				b->activitySound.SetPosition(allocatedTarget->GetWorldPosition());
				b->activitySound.SetPitch(0.7f);
				b->activitySound.Play();
			#endif

			float3 dir = rotateToWorld;
			float3 innerDir;
			Mat::rotate(allocatedTarget->inverse,dir,innerDir);
			rotateTo.x = Vec::angle2PI(innerDir.xy) + M_PIF/2.f;
			rotateTo.y = -(90.f - Vec::angle(rotateToWorld,Grid::low.cells[allocatedTarget->location.stackID].centerVertex.xyz)) * M_PIF / 180.f;

			rotateFrom = float2(b->turretAngleX,b->turretAngleY);
			while (rotateFrom.x - rotateTo.x > M_PIF) rotateFrom.x -= M_PIF*2.f;
			while (rotateFrom.x - rotateTo.x < -M_PIF) rotateFrom.x += M_PIF*2.f;
		}
		else
		{
			LogEvent(EventClass::Preset::error,nullptr,"Warning: Unable to identify entity "+ToString(targetEntityID)+" for turret rotation");
			/*let's ignore this for now. this event is caused when visibility of
			its target has been determined at a latter time by some (other) vehicle.
			during the motion phase of a different vehicle, an automated attack is caused.
			since visualization time and processing time are not identical, this can lead
			to a temporal distortion. should be minor security violation. the player is
			notified of a turret rotation that they can't actually see yet.
			the same goes for attacks, i guess
			*/
		}
	}

	/*virtual override*/ void TurretRotation::Update(float x)
	{
		if (allocatedTarget)
		{
			float dummy;
			Objects::PTurretBase b = allocatedTarget->GetTurretBase(dummy);
			M::TVec2<> angles;
			Vec::interpolate(rotateFrom,rotateTo,x,angles);
			//Objects::PVehicle v = To<Objects::Vehicle>::Cast(target);
			b->turretAngleX = angles.x;
			b->turretAngleY = angles.y;
			//b->rotationSound.SetPitch(0.5f + 0.3f * (sqrt((x))));
		}
	}


	/*virtual override*/ void TurretRotation::End()
	{
		if (allocatedTarget)
		{
			float dummy;
			Objects::PTurretBase b = allocatedTarget->GetTurretBase(dummy);
			b->turretAngleX = rotateTo.x;
			b->turretAngleY = rotateTo.y;

			#ifdef HAS_WINDOW
				b->activitySound.Stop();
			#endif
		}
	}

	/*virtual override*/ String TurretRotation::Describe() const
	{
		return "TurretRotation("+ToString(targetEntityID)+": ->"+ToString(rotateToWorld)+")";
	}
}

