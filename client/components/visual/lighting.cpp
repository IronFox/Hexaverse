#include <global_root.h>
#include "lighting.h"
#include <grid/planet.h>
#include "../clientEvents.h"
#include "../clientRuntimeConfig.h"
#include "../cameraControl.h"


namespace Lighting
{
	namespace GPUResources
	{
		#ifdef CASCADING_SHADOW

			DX11::FBO						shadowMap[NumCascades];
			Engine::OrthographicAspect<>	shadowCamera[NumCascades];
			DX11::ObjectBuffer<M::TMatrix4<> >	shadowMatrix[NumCascades];
		#else
			DX11::FBO						shadowMap;
			Engine::OrthographicAspect<>	shadowCamera;
			DX11::ObjectBuffer<M::TMatrix4<> >	shadowMatrix;
		#endif
		DX11::ObjectBuffer<M::VecN<float,8> >	atmosphereConfig;
		DX11::ObjectBuffer<float4>			sunDirection,sunColor;

	}

	void Create()
	{
		
		#ifdef CASCADING_SHADOW
			for (index_t i = 0; i < NumCascades; i++)
			{
				GPUResources::shadowMap[i].Create(Resolution(1024,1024),nullptr,0,true);
				GPUResources::shadowMatrix[i].Create(M::Matrix<>::eye4);
			}
		#else
	//		shadowMap.Create(Resolution(4096,4096),&dbgFormat,1,true);
			GPUResources::shadowMap.Create(Resolution(4096,4096),nullptr,0,true);
			GPUResources::shadowMatrix.Create(M::Matrix<>::eye4);
		#endif

		GPUResources::atmosphereConfig.Create(M::VecN<float,8>());
		UpdateAtmosphere(Grid::currentPlanetConfig.atmosphereColor);

		GPUResources::sunDirection.Create(float4());
		GPUResources::sunColor.Create(float4());
		UpdateSun(Grid::currentPlanetConfig.sunPlaneNormal_Speed,Grid::currentPlanetConfig.sunColor);


		GlobalEvents::terrainFlush << ResetSunRotation;

		GlobalEvents::frameAfterCamera << []()
		{
			bool alwaysDay = RuntimeConfig::FromFile::alwaysDay && !Camera::autoRotation;

			if (alwaysDay)
				UpdateSunDirection(::sunDirection);
			else
				PerFrameUpdateSunDirection();
		};
	}

	void Destroy()
	{
		GPUResources::sunColor.Clear();
		GPUResources::atmosphereConfig .Clear();

	}



	namespace SunDetails
	{
		float4	plane_Speed = float4(0,0,1,0.5f);
		float4	color = float4(1);
		M::TMatrix3<>	sunSystem = M::Matrix<>::eye3;
		float3	sunDirection(1,0,0);
		double progression = 0.0;
	}

	const float3&	GetSunDirection()
	{
		return SunDetails::sunDirection;
	}

	void	UpdateSun(const float4&sunPlane_Speed,const float4&sunColor)
	{
		SunDetails::plane_Speed = sunPlane_Speed;
		SunDetails::color = sunColor;
		SunDetails::progression = 0.f;
		Mat::makeAxisSystem(sunPlane_Speed.xyz,2,SunDetails::sunSystem);

		GPUResources::sunColor.Update(sunColor);
		UpdateSunDirection(SunDetails::sunSystem.x,true);
	}


	void	PerFrameUpdateSunDirection()
	{
		SunDetails::progression += SunDetails::plane_Speed.w * Engine::timing.delta;


		float3 p = float3(cos(SunDetails::progression),sin(SunDetails::progression),0);
		Mat::Mult(SunDetails::sunSystem,p,SunDetails::sunDirection);
		UpdateSunDirection(SunDetails::sunDirection,true);
	}

	double	GetAcceleratedSunDirection(M::TVec3<>&outDir, float speedFactor/*=1.f*/)
	{
		double angle = SunDetails::progression*speedFactor;
		Vec::def(outDir,cos(angle),sin(angle),0);
		Mat::Mult(SunDetails::sunSystem,outDir);
		return angle;
	}


	const M::TMatrix3<>& GetSunSystem()
	{
		return SunDetails::sunSystem;
	}


	void	ResetSunRotation()
	{
		SunDetails::progression = 0;
	}




	void	UpdateSunDirection(const float3&p, bool shadows /*= true*/)
	{
		SunDetails::sunDirection = p;
		//#ifdef _DEBUG
		//	SunDetails::sunDirection = -p;
		//#endif
		GPUResources::sunDirection.Update(float4(SunDetails::sunDirection,shadows));



	}


	void	UpdateAtmosphere(const float4&fogColor)
	{
		UpdateAtmosphere(Grid::MeanPlanetRadius * 1.5f, fogColor);

	}

	void	UpdateAtmosphere(float radius, const float4&fogColor)
	{
		M::VecN<float,8>	atmosphereCfg;
		atmosphereCfg[0] = M::Sqr(radius);
		atmosphereCfg[1] = radius;
		atmosphereCfg[2] = 0;
		atmosphereCfg[3] = 0;
		atmosphereCfg[4] = fogColor.r;
		atmosphereCfg[5] = fogColor.g;
		atmosphereCfg[6] = fogColor.b;
		atmosphereCfg[7] = fogColor.a;

		GPUResources::atmosphereConfig.Update(atmosphereCfg);
	}


}