#pragma once
#ifdef HAS_WINDOW

#include "display.h"


#define CASCADING_SHADOW


namespace Lighting
{
	static const count_t NumCascades = 2;
	namespace DX11 = Engine::DX11;

	namespace GPUResources
	{

		#ifdef CASCADING_SHADOW
			extern DX11::FBO						shadowMap[NumCascades];
			extern Engine::OrthographicAspect<>		shadowCamera[NumCascades];
			extern DX11::ObjectBuffer<TMatrix4<> >	shadowMatrix[NumCascades];
		#else
			extern DX11::FBO						shadowMap;
			extern Engine::OrthographicAspect<>		shadowCamera;
			extern DX11::ObjectBuffer<TMatrix4<> >	shadowMatrix;
		#endif
		
		extern DX11::ObjectBuffer<VecN<float,8> >	atmosphereConfig;
		extern DX11::ObjectBuffer<float4>			sunDirection,sunColor;


	}


	void	UpdateAtmosphere(const float4&fogColor);
	void	UpdateAtmosphere(float radius, const float4&fogColor);
	void	UpdateSun(const float4&sunPlane_Speed,const float4&sunColor);


	const float3&	GetSunDirection();
	void	PerFrameUpdateSunDirection();
	void	UpdateSunDirection(const float3&, bool shadows = true);
	/**
	Retrieves a directional vector of the sun at an altered rotation speed.
	Given speed-factor 1, the result exactly matches the current direction of the sun.
	@param[out] outDir Resulting sun direction. Normalized.
	@param speedFactor Factor to accelerate (or slow down) regular sun speed by. Any non-NaN, non-INF value is good.
	@return Angle used to compute the resulting outDir. Depending on game progression and @a speedFactor,
			this value can be large.
	*/
	double	GetAcceleratedSunDirection(TVec3<>&outDir, float speedFactor=1.f);
	void	ResetSunRotation();
	/**
	Retrieves the orientation system of the sun.
	The z-axis is the rotation axis, the other two axes are orthogonal to each other and the z-axis but otherwise arbitrary.
	*/
	const TMatrix3<>& GetSunSystem();


	void	Create();
	void	Destroy();

}

#endif
