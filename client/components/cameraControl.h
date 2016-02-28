#ifdef HAS_WINDOW
#ifndef cameraControlComponentH
#define cameraControlComponentH

#include <math/vclasses.h>
#include <engine/aspect.h>

#define DebugCheckVector(T, M, MX)
#define DebugCheckCamera()

//inline void			DebugCheckVector(const TVec3<>&test, float min, float max)
//{
//	ASSERT_IS_CONSTRAINED3__(test.v,min,max);
//	ASSERT__(!isinf(test.x));
//	ASSERT__(!isinf(test.y));
//	ASSERT__(!isinf(test.z));
//	ASSERT__(!isnan(test.x));
//	ASSERT__(!isnan(test.y));
//	ASSERT__(!isnan(test.z));
//
//
//}
//void			DebugCheckCamera()
//{
//	const TVec3<>&test = scene.GetViewingDirection();
//	DebugCheckVector(test,-1,1);
//
//}

extern float3					sunDirection;
static const float				SunOffAngle = 35.f;	//divergence between vertical axis and sun direction. 0 would be cenit


/**
Constrols most aspects around cameras. Manages instances, and rotates them as necessary
*/
namespace Camera
{
	extern Engine::VectorCamera<>	scene;
	extern Engine::OrthographicAspect<>	hud;

	extern bool					autoRotation;


	/**
	Initiates cameras (must be called only once during program start)
	*/
	void						Setup(float pixelAspect);
	/**
	Resets camera configuration to initial setup
	*/
	void						Reset();

	/**
	Rotates camera viewing direction by the given angle deltas
	*/
	void						Rotate(const float2&angleDelta);
	/**
	Rotates the focus position around the planet center by the given angle delta.
	Aborts any ongoing targeted translation
	*/
	void						Translate(const float2&angleDelta);

	/**
	Initiates targeted translation (smoothly animated rotation of focus point around planet center)
	@param direction Direction from planet center to smoothly translate to. Normalized automatically
	*/
	void						TranslateTo(const float3&direction);
	/**
	Stops any ongoing targeted translation
	*/
	void						AbortTargetedTranslation();

	/**
	Smoothly updates ongoing camera animations. Must be called once per frame
	*/
	void						ProgressRotation();

	void						StopAutoRotation();
	/**
	Queries the current camera orbit radius
	*/
	float						GetCurrentOrbit();

	/**
	Set a new target orbit to smoothly interpolate to
	*/
	void						TargetOrbit(float targetOrbit);


	/**
	Switches to sky orbit
	*/
	void						TargetSkyOrbit();
	/**
	Switches to ground orbit
	*/
	void						TargetGroundOrbit();
	/**
	Toggles between sky and ground orbit
	*/
	void						ToggleOrbit();

	bool						SkyGridIsVisible();



	/**
	Input event handlers
	*/
	namespace Event
	{
		void					WheelAction(short param);
		void					ZoomIn();
		void					ZoomOut();
		void					Zoom(float outSpeed);
	}

}


#endif
#endif
