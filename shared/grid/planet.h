#pragma once

#include "volume.h"

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
		UINT32			innerCoreType = Content::Granite;	//type used beneath the lowest cell
		float4			atmosphereColor = float4(0.7,0.8,1.0,0.5);	//alpha=1.0 is really dense
		float4			sunPlaneNormal_Speed = float4(0,0,1,0.5f);	//fourth parameter is rotation speed in radians per second
		float4			sunColor = float4(1,1,1,1);	//fourth parameter is ignored

		void			Apply() const;
	};

	inline void	AssertIsValid(const Grid::LCellIdI&location)
	{
		ASSERT_LESS__(location.stackID,LowSetup::GridType::CellCount);
		ASSERT_LESS__(location.height,CellStack::HalfRange);
		ASSERT_GREATER_OR_EQUAL__(location.height,-CellStack::HalfRange);
	}



	#ifdef CLIENT
		#ifdef HAS_WINDOW
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
		#endif
	#endif

	class ExtendedPlanetConfig : public TSharedPlanetConfig, public SerializableObject
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
		UINT32		lastSeed = 0;

		virtual serial_size_t GetSerialSize(bool) const override
		{
			return sizeof(TSharedPlanetConfig) + sizeof(TLayerConfig)*2 + 4 + 8 + 4;
		}

		virtual bool HasFixedSize() const override
		{
			return true;
		}

		virtual	bool	Serialize(IWriteStream&outStream, bool) const override
		{
			return	outStream.WritePrimitive((const TSharedPlanetConfig&)*this)
					&&	outStream.WritePrimitive((const TLayerConfig&)extendedCore)
					&&	outStream.WritePrimitive((const TLayerConfig&)mountains)
					&&	outStream.WritePrimitive(type)
					&&	outStream.WritePrimitive(liquidCoverage)
					&&	outStream.WritePrimitive(extendedCoreType)
					&&	outStream.WritePrimitive(liquidOutline)
					&&	outStream.WritePrimitive(snowHeight)
					&&	outStream.WritePrimitive(liquidHeight)
					&&	outStream.WritePrimitive(lastSeed)
					;
		}

		virtual	bool	Deserialize(IReadStream&inStream, serial_size_t) override
		{
			return	inStream.ReadPrimitive((TSharedPlanetConfig&)*this)
					&&	inStream.ReadPrimitive((TLayerConfig&)extendedCore)
					&&	inStream.ReadPrimitive((TLayerConfig&)mountains)
					&&	inStream.ReadPrimitive(type)
					&&	inStream.ReadPrimitive(liquidCoverage)
					&&	inStream.ReadPrimitive(extendedCoreType)
					&&	inStream.ReadPrimitive(liquidOutline)
					&&	inStream.ReadPrimitive(snowHeight)
					&&	inStream.ReadPrimitive(liquidHeight)
					&&	inStream.ReadPrimitive(lastSeed)
					&&	ApplySeed()
					;
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
		void	RandomizeSeeds(Random&random);

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

	};
	
	extern TPlanetConfig	currentPlanetConfig;
	
}
