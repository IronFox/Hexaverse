#include <global_root.h>
#include "galaxy.h"
#include <engine/display.h>
#include "../client/ExtRenderer/DirectX11.h"
#include <general/random.h>
#include <image/image.h>


extern Engine::Display<Engine::DirectX11>	display;


namespace Galaxy
{
	
	/*
	Concept for later:
	*/
	class Generation
	{
	public:
		FloatImage		dustDensity;

		void			swap(Generation&other)
		{
			dustDensity.swap(other.dustDensity);

		}

		void			Init(Random&random)
		{
			dustDensity.setDimensions(6,1,1);

			dustDensity.setChannel(0,0,0,1.f);
			dustDensity.setChannel(1,0,0,0.f);
			dustDensity.setChannel(2,0,0,0.f);
			dustDensity.setChannel(3,0,0,1.f);
			dustDensity.setChannel(4,0,0,0.f);
			dustDensity.setChannel(5,0,0,0.f);
		}

		void			Evolve(const Generation&parent, Random&random)
		{
			dustDensity.setDimensions(parent.dustDensity.width()+1,1,1);
			for (UINT32 x = 0; x < dustDensity.width(); x++)
			{
				float v= parent.dustDensity.sampleChannelAt(float(x)/float(dustDensity.width()),0,0,true);
				v += random.getFloat(-0.01f,0.01f);
				dustDensity.setChannel(x,0,0,v);
			}
		}

	};

	
	Buffer0<Generation,Swap>	generations;


	void	Build(UINT32 seed)
	{
		Random random(seed);

		#if 0
		//for later:
		generations.Clear();
		generations.Append().Init(random);
		for (index_t i = 0; i < 512; i++)
		{
			generations.Append().Evolve(generations.fromEnd(1),random);
		}
		#endif /*0*/





	}


	#ifdef HAS_WINDOW
	void	Render(const Engine::Aspect<>&camera, const Engine::Frustum<>&frustum)
	{
		display.StoreRendererState();
			display.SetRasterizer(true,false);
			display.SetDepthTest(false,false);
			display.SetBlendMode(Engine::BlendMode::FlareBlend);




		display.RestoreRendererState();

	}
	#endif

}

