#ifdef HAS_WINDOW
#include <global_root.h>
#include "cameraControl.h"


#include <math/matrix.h>
#include <engine/timing.h>

#include <grid/stack.h>
#include "clientRuntimeConfig.h"
#include <objects/satellite.h>
#include "visual/objectCommands.h"
#include "visual/terrain.h"

#include "timelapse.h"
#include "clientEvents.h"
#include "visual/lighting.h"


float3							sunDirection;

namespace Camera
{

	Engine::VectorCamera<>			scene;
	Engine::OrthographicAspect<>	hud;
	bool							autoRotation = false,
									locked = false;
	Observable<>						onZoom,onRotate,onDrag;

	namespace 
	{
		float							orbit = WorldScale,		//!< Current camera orbit
										targetOrbit = WorldScale,	//!< Targeted camera orbit (smooth animation target)
										groundTargetOrbit = 0.f;			//!< Copy of targetOrbit when switching to sky orbit
		float3							targetDirection;			//!< Target camera direction (of the focus position from planet core)
		float							cameraSpeed = 0;			//!< Direction animation camera speed. Starts off at 0 and accelerates as animation progresses
		bool							moveToTarget = false;		//!< Set true to animate scene camera to given target. Does not affect @a orbit / @a targetOrbit
		bool							skyGrid = false;
		bool							skyOrbitBase = false;
		float							orbitBaseFactor = 1.f;
	}


	static void							RealignSunDirection()
	{
		float3 axis = (sunDirection | scene.location).Normalized();
		TMatrix3<> mat;
		Mat::BuildRotationMatrix(SunOffAngle,axis,mat);
		Mat::Mult(mat,scene.location,sunDirection);
		sunDirection.Normalize0();
	}


	void						TargetSkyOrbit()
	{
		#ifndef NO_SATELLITES
			skyGrid = true;
			if (skyOrbitBase)
				return;
			skyOrbitBase = true;
			return;

			groundTargetOrbit = targetOrbit;

			float dist = std::numeric_limits<float>::max();
			targetOrbit = Grid::ToRadius(Objects::Satellite::BaseResidenceHeight);
			if (
				Obj::detectOpticalSphereIntersection(Vector<>::zero,targetOrbit,scene.GetAbsoluteLocation(),-scene.GetViewingDirection(),dist)
			
				&&
				Vec::angle(scene.GetAbsoluteLocation() - scene.GetViewingDirection() * dist, scene.GetAbsoluteLocation()) < 90.f
				)
			{
				float3 old = scene.location;
				scene.location = scene.GetAbsoluteLocation() - scene.GetViewingDirection() * dist;
				float3 axis;
				Vec::cross(old,scene.location,axis);
				float angle = Vec::angle(old,scene.location);
				if (angle > getError<float>())
				{

					orbit = Vec::length(scene.location);
					scene.upAxis = targetDirection = scene.location / orbit;
					scene.retraction.z = -dist;
					Rotate(float2());

					TMatrix3<> rot;
					Mat::BuildRotationMatrix(angle,axis.Normalized(),rot);
					Mat::Mult(rot, sunDirection);
					RealignSunDirection();

				}

			}
			orbitBaseFactor = 1.f / (1.f + (0.01f * fabs(targetOrbit - orbit)));
		#endif
	}

	void						TargetGroundOrbit()
	{
		skyGrid = false;
		if (!skyOrbitBase)
			return;
		skyOrbitBase = false;
		return;
		targetOrbit = groundTargetOrbit;
		orbitBaseFactor = 1.f / (1.f + (0.01f * fabs(targetOrbit - orbit)));
	}

	void						ToggleOrbit()
	{
		if (skyOrbitBase)
			TargetGroundOrbit();
		else
			TargetSkyOrbit();

	}

	bool						SkyGridIsVisible()
	{
		return skyGrid;
	}


	void						ChangeGroundRadius(float newOrbit)
	{
		targetOrbit = newOrbit;
		orbitBaseFactor = 1.f / (1.f + (0.01f * fabs(targetOrbit - orbit)));
	}


	float GetCurrentOrbit()
	{
		return orbit;
	}

	static const float MinCameraRetraction = 0.05f * WorldScale;
	static const float MaxCameraRetraction = 7.f * WorldScale;


	void Event::WheelAction(short param)					//handler for the mouse wheel
	{
		if (!locked)
		{
			//negative is wheel down
			scene.retraction.z /= pow(1.1f,param);
			scene.retraction.z = Math::clamp(scene.retraction.z,-MaxCameraRetraction,-MinCameraRetraction);
			scene.UpdateView();
			onZoom();
		}
	}


	void Event::ZoomIn()
	{
		if (!locked)
		{
			scene.retraction.z *= 0.95f;
			scene.retraction.z = Math::clamp(scene.retraction.z,-MaxCameraRetraction,-MinCameraRetraction);
			scene.UpdateView();
			onZoom();
		}
	}

	void Event::ZoomOut()
	{
		if (!locked)
		{
			scene.retraction.z /= 0.95f;
			scene.retraction.z = Math::clamp(scene.retraction.z,-MaxCameraRetraction,-MinCameraRetraction);
			scene.UpdateView();
			onZoom();
		}
	}

	void	Event::Zoom(float outSpeed)
	{
		scene.retraction.z *= pow(3.f,-outSpeed);
		scene.retraction.z = Math::clamp(scene.retraction.z,-MaxCameraRetraction,-MinCameraRetraction);
		scene.UpdateView();
		onZoom();
	}



	void Reset()
	{
		orbit = Grid::MeanPlanetRadius;
		targetOrbit = Grid::MeanPlanetRadius;
		orbitBaseFactor = 1.f;

		moveToTarget = false;
		skyOrbitBase = false;
		skyGrid = false;

		scene.retraction = float3(0, 0, -4 * WorldScale);
		scene.location = float3(0, orbit, 0);
		scene.direction = float3(0, 1, 1).normalized();
		scene.upAxis = scene.location;
		Vec::normalize0(scene.upAxis);

		scene.UpdateView();
	}


	namespace Cmd
	{

		Objects::CommandFlags Check(bool)
		{
			return Objects::CommandFlags::IsEnabled | 
					(Camera::SkyGridIsVisible() ? Objects::CommandFlags::IsPermaActive : Objects::CommandFlags::None);
		}
	}


	void Setup(float pixelAspect)
	{
		scene.UpdateProjection2(pixelAspect,0.1f,1000.f);

		Reset();
		//RebuildCamera();

		hud.UpdateProjection(0,0,1,1,-1,1);

		#ifndef NO_SATELLITES
		Objects::viewCommands->Add( new 
			Objects::GlobalCommand("Show Sky Grid", 
			&Camera::ToggleOrbit, &Cmd::Check));
		#endif
	}



	void							Rotate(const float2&angleDelta)
	{
		TMatrix3<> rot;
		Mat::BuildRotationMatrix(angleDelta.x, scene.upAxis, rot);
		Mat::Mult(rot, scene.direction);
		TVec3<> x;
		Vec::cross(scene.upAxis, scene.direction, x);
		Vec::normalize0(x);
		Mat::BuildRotationMatrix(angleDelta.y, x, rot);
		Mat::Mult(rot, scene.direction);

		float angle = Vec::angle(scene.direction, scene.upAxis);
		if (angle > 90.f)
		{
			Vec::cross(x, scene.upAxis, scene.direction);
			Vec::normalize0(scene.direction);
		}

		if (angle < 20.f)
		{
			Mat::BuildRotationMatrix(-(20.f - angle), x, rot);
			Mat::Mult(rot, scene.direction);
		}


		scene.UpdateView();
		onRotate();
		//moveToTarget = false;	//leave on if on
	}

	void							TranslateTo(const float3&direction)
	{
		if (locked)
			return;
		moveToTarget = true;
		targetDirection = direction.Normalized0();
		cameraSpeed = 0;
	}

	void							AbortTargetedTranslation()
	{
		moveToTarget = false;
	}

	
	void	ToggleAutoRotation()
	{
		if (!autoRotation)
		{
			if (!locked)
			{
				autoRotation = true;
				Lighting::ResetSunRotation();
			}
		}
		else
			StopAutoRotation();
	}

	void							StopAutoRotation()
	{
		if (autoRotation)
		{
			autoRotation = false;
			Reset();
			scene.UpdateView();
		}
	}

	void							ProgressRotation()
	{
		if (autoRotation)
		{
			static const count_t CameraRotations = 2;
			static const count_t SunRotations = 5;
			TVec3<> dir;
			double at = Lighting::GetAcceleratedSunDirection(dir,-float(CameraRotations) / SunRotations);
			if (fabs(at) >= M_PI*2.0 * CameraRotations)
			{
				StopAutoRotation();
				return;
			}
			//scene.location = dir * Grid::ToRadius(180);
			scene.location = float3();
			scene.retraction.z = -WorldScale * 3.75f;
			scene.direction = -dir;
			scene.upAxis = Lighting::GetSunSystem().z;
			scene.UpdateView();

			return;
		}


		if (fabs(targetOrbit - orbit) > 0.1f)
		{
			DebugCheckCamera();
			float fc = orbitBaseFactor;
			orbit += (targetOrbit - orbit) * (1.f - pow(0.5f * fc, Engine::timing.delta));
			//float delta = targetOrbit - orbit;
			//orbit += float(sign(delta)) * std::min(fabs(delta), Engine::timing.delta*10.f);

			Vec::setLen(scene.location, orbit);
			DebugCheckVector(scene.location,-10000,10000);

			scene.UpdateView();
			DebugCheckCamera();
		}

		if (!moveToTarget)
			return;
		TVec3<> axis;
		Vec::cross(scene.location,targetDirection,axis);
		DebugCheckVector(scene.location,-10000,10000);
		float angle = Vec::angle(scene.location,targetDirection);
		if (angle < 0.01f)
		{
			moveToTarget = false;
			DebugCheckCamera();
			return;
		}
		cameraSpeed += Engine::timing.delta;
		cameraSpeed = std::min(1.f,cameraSpeed);
		float speed = cameraSpeed * sqrt(smoothstep(0.f,45.f,angle));
		angle = std::min(5.f*speed,angle);
		//angle *= 1.f - pow(0.2f,Engine::timing.delta );
		
		Vec::normalize0(axis);
		DebugCheckVector(axis,-2,2);
		TMatrix3<> rot;
		Mat::BuildRotationMatrix(-angle,axis,rot);
		ASSERT_IS_CONSTRAINED__(angle,-1000,1000);
		DebugCheckVector(rot.x,-2,2);

		Mat::Mult(rot, scene.location);
		Mat::Mult(rot, scene.direction);
		if (RuntimeConfig::FromFile::alwaysDay)
		{
			Mat::Mult(rot, sunDirection);
			RealignSunDirection();
		}
		DebugCheckVector(scene.location,-10000,10000);
		DebugCheckCamera();


		UINT32 stackID = Grid::high.Locate(scene.location);
		{
			int top;
			if (World::actual.GetStack(stackID).FindTopOccupied(top))
				ChangeGroundRadius(Grid::ToRadius(top+2));
		}
		if (VisualTerrain::SeeThrough::IsEnabled())
			ChangeGroundRadius(std::min(Grid::ToRadius(VisualTerrain::SeeThrough::GetFocusPoint().height+2), targetOrbit));
		ASSERT__(orbit > 0);
		DebugCheckVector(scene.location,-10000,10000);
		Vec::setLen0(scene.location, orbit);
		DebugCheckVector(scene.location,-10000,10000);
		scene.upAxis = scene.location / orbit;
		DebugCheckVector(scene.upAxis,-2,2);

		DebugCheckCamera();



		scene.UpdateView();
		DebugCheckCamera();

	}

	void						ChangeGroundRadiusToFocusedTerrain()
	{
		UINT32 stackID = Grid::high.Locate(scene.location);
		{
			int top;
			if (World::actual.GetStack(stackID).FindTopOccupied(top))
				ChangeGroundRadius(Grid::ToRadius(top+2));
		}
	}


	void							Translate(const float2&angleDelta)
	{
		//moveToTarget;

		AbortTargetedTranslation();

		TVec3<> x, y;
		float3 up = scene.GetAbsoluteLocation();
		Vec::cross(up, scene.direction, x);
		Vec::normalize0(x);
		Vec::cross(x, up, y);
		Vec::normalize0(y);


		TMatrix3<> rot;
		Mat::BuildRotationMatrix(angleDelta.y, x, rot);
		Mat::Mult(rot, scene.location);
		Mat::Mult(rot, scene.direction);
		Mat::BuildRotationMatrix(angleDelta.x, y, rot);
		Mat::Mult(rot, scene.location);
		Mat::Mult(rot, scene.direction);
		if (RuntimeConfig::FromFile::alwaysDay)
		{
			Mat::Mult(rot, sunDirection);
			RealignSunDirection();
		}



		ChangeGroundRadiusToFocusedTerrain();

		//targetOrbit = Grid::ToRadius(World::actual.cellStack[stackID].FindTopOccupied());
		if (VisualTerrain::SeeThrough::IsEnabled())
			ChangeGroundRadius(std::min(Grid::ToRadius(VisualTerrain::SeeThrough::GetFocusPoint().height+2), targetOrbit));


		Vec::setLen0(scene.location, orbit);
		scene.upAxis = scene.location / orbit;




		scene.UpdateView();
		onDrag();

	}

	void						Create()
	{
		GlobalEvents::frameBegin << ProgressRotation;
	}

}

#endif
