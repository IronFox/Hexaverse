#pragma once

#include "stack.h"
#include "worldData.h"

namespace Grid
{

	struct TSharedPlanetConfig;
	#ifndef CLIENT
		class ExtendedPlanetConfig;
		typedef ExtendedPlanetConfig	TPlanetConfig;
	#else
		typedef TSharedPlanetConfig	TPlanetConfig;
	#endif


	struct TSharedPlanetConfig
	{
		typedef TSharedPlanetConfig Self;

		UINT32			innerCoreType = Content::Granite;	//type used beneath the lowest cell
		float4			atmosphereColor = float4(0.7,0.8,1.0,0.5);	//alpha=1.0 is really dense
		float4			sunPlaneNormal_Speed = float4(0,0,1,0.5f);	//fourth parameter is rotation speed in radians per second
		float4			sunColor = float4(1,1,1,1);	//fourth parameter is ignored


		friend void		Deserialize(IReadStream&s, Self&v)
		{
			using Serialization::Deserialize;
			s.ReadSize(v.innerCoreType);
			Deserialize(s,v.atmosphereColor);
			Deserialize(s,v.sunPlaneNormal_Speed);
			Deserialize(s,v.sunColor);
		}
		friend void		Serialize(IWriteStream&s, const Self&v)
		{
			using Serialization::Serialize;
			s.WriteSize(v.innerCoreType);
			Serialize(s,v.atmosphereColor);
			Serialize(s,v.sunPlaneNormal_Speed);
			Serialize(s,v.sunColor);
		}
	};




	class ExtendedPlanetConfig : public TSharedPlanetConfig
	{
		void	RandomizeSun(Random&random)
		{
			sunColor = float4(1);
			//float4(random.NextFloat(0.8,1.05f),random.NextFloat(0.8,1.05f),random.NextFloat(0.8,1.05f),1);

			Vec::stretch(sunColor.rgb,atmosphereColor.rgb,atmosphereColor.rgb);
			//sunColor.rgb = atmosphereColor.rgb;


			sunPlaneNormal_Speed = float4(
					random.NextFloat(-1,1),random.NextFloat(-1,1),random.NextFloat(-1,1),
					0.07f
					//random.NextFloat(-0.2,0.2)
				);
			Vec::normalize0(sunPlaneNormal_Speed.xyz);

		}
	public:
		typedef ExtendedPlanetConfig Self;



		struct TSeedSet
		{
			int64_t	seed[5],	//for fractal levels 0 to 5
					productSeed,	//for the final product
					depressionSeed0,
					depressionSeed1,
					depressionSeed2,
					depressionSeed3;


			void	Randomize(Random&random)
			{
				for (index_t i = 0; i < ARRAYSIZE(seed); i++)
					seed[i] = random.NextSeed();
				depressionSeed0 = random.NextSeed();
				depressionSeed1 = random.NextSeed();
				depressionSeed2 = random.NextSeed();
				depressionSeed3 = random.NextSeed();
				productSeed = random.NextSeed();
			}
		};

		struct NoiseSet
		{
			SimplexNoise	fractal[5],	//for fractal levels 0 to 5
							product,	//for the final product
							depression0,
							depression1,
							depression2,
							depression3;

			/**/			NoiseSet(const TSeedSet&set):
								fractal
									{
										set.seed[0],
										set.seed[1],
										set.seed[2],
										set.seed[3],
										set.seed[4]
									},
								product(set.productSeed),
								depression0(set.depressionSeed0),
								depression1(set.depressionSeed1),
								depression2(set.depressionSeed2),
								depression3(set.depressionSeed3)
			{}
		};


		struct TLayerConfig
		{
			int		dampening = 2;
			int		baseHeight = 0;	//center terrain height
			float	heightScale = 1.f;	//vertical scale of noise.
			//1.0f is pretty much the most you should set with normal noise frequency
			float	frequency = 1.f;	//noise frequency factor
		};

		struct TLayer : public TSeedSet, public TLayerConfig
		{};
		 
		static const size_t TLayer_size = sizeof(TLayer);
		static const size_t TSeedSet_size = sizeof(TSeedSet);


		static_assert(sizeof(TLayer)==sizeof(TSeedSet)+16,"packing issue?");

		TLayer		extendedCore,mountains;
		BYTE		type = Content::Unknown,
					liquidCoverage = Content::Water,
					extendedCoreType = Content::Granite,
					liquidOutline = Content::Sand;
		INT32		snowHeight = 5,	//height past which blocks will be covered in snow. Set to >=HalfRange(16) to disable
					liquidHeight = 0;
		UINT32		lastSeed = 0,
					versionCounter = 0;



		friend	void	Serialize(IWriteStream&outStream, const Self&v)
		{
			outStream.WritePrimitive((const TSharedPlanetConfig&)v);
			outStream.WritePrimitive((const TLayerConfig&)v.extendedCore);
			outStream.WritePrimitive((const TLayerConfig&)v.mountains);
			outStream.WritePrimitive(v.type);
			outStream.WritePrimitive(v.liquidCoverage);
			outStream.WritePrimitive(v.extendedCoreType);
			outStream.WritePrimitive(v.liquidOutline);
			outStream.WritePrimitive(v.snowHeight);
			outStream.WritePrimitive(v.liquidHeight);
			outStream.WritePrimitive(v.lastSeed);
			outStream.WritePrimitive(v.versionCounter);
		}

		friend	void	Deserialize(IReadStream&inStream, Self&v)
		{
			inStream.ReadPrimitive((TSharedPlanetConfig&)v);
			inStream.ReadPrimitive((TLayerConfig&)v.extendedCore);
			inStream.ReadPrimitive((TLayerConfig&)v.mountains);
			inStream.ReadPrimitive(v.type);
			inStream.ReadPrimitive(v.liquidCoverage);
			inStream.ReadPrimitive(v.extendedCoreType);
			inStream.ReadPrimitive(v.liquidOutline);
			inStream.ReadPrimitive(v.snowHeight);
			inStream.ReadPrimitive(v.liquidHeight);
			inStream.ReadPrimitive(v.lastSeed);
			inStream.ReadPrimitive(v.versionCounter);
			v.ApplySeed();
		}
		
		/**
		Reapplies @a lastSeed, and fills random parameters.
		As long as @a lastSeed is not modified, the resulting values will not either
		*/
		bool	ApplySeed();
		/**
		Updates @a lastSeed to the given parameter, and fills random parameters.
		If @a lastSeed does not actually change, the resulting values will not either
		*/
		void	SetSeed(UINT32 seed);
		/**
		Copies the current seed of @a random to @a lastSeed, and fill random parameters using the specified
		random variable.
		Contrary to SetSeed(random.currentSeed()), @a random will actually be advanced by the operation.
		Using a random variable with current seed equal to @a lastSeed will produce identical results
		*/
		void	RandomizeSeeds(TwisterRandom&random);

		void	GenerateAtmosphereColorForType(BYTE t)
		{
			if (t == Content::Water)
			{
				atmosphereColor.rgb = float3(0.7,0.8,1.0);
				return;
			}
			if (t == Content::Unknown)
			{
				GenerateAtmosphereColorForType(Content::Grass);
				return;
			}
			atmosphereColor.rgb = CellStack::Cell::typeColor[t];
			Vec::abs(atmosphereColor);
		}

		void	GenerateAtmosphereColor()
		{
			if (liquidCoverage != Content::Clear)
			{
				GenerateAtmosphereColorForType(liquidCoverage);
					
				return;
			}
			if (innerCoreType != Content::Clear)
				GenerateAtmosphereColorForType(Content::Reinterpret(innerCoreType));
			else
				GenerateAtmosphereColorForType(type);
		}

		void	InitUnknown(CellStack&stack, float2 heights) const;
		void	FillTypes(CellStack&stack) const;

		void	BuildWorld(World::Data&world) const;

	};
	
	extern TPlanetConfig	currentPlanetConfig;
	
}
