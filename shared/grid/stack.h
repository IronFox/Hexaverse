#pragma once


#include <math/matrix.h>
//#include <engine\aspect.h>
//#include "base.h"
#include "../stringTable.h"
#include "../logEvent.h"
#include <general/random.h>
#include <math/SimplexNoise.h>
#include "types.h"
#include "../fileVersion.h"
#include <container/hashtable.h>
#include <math/vclasses.h>

#ifdef LOWER_RESOLUTION_PLANET
	static const constexpr float WorldScale = 20.f;
#else
	static const constexpr float WorldScale = 40.f;
#endif

namespace Grid
{
	using namespace DeltaWorks::Math;

	struct TVisibility
	{
		float value = 0.f;

		/**/	TVisibility(float value):value(value)	{}
	};


	CONSTRUCT_ENUMERATION4(WorldDiscovery,
		None,
		Water,
		Liquids,
		FullTerrain
	)

	static const int BadHeight = 0x1FFFFFFF;	//keep equal to gridAndChoices.h Objects::badHeight

	class ExtendedPlanetConfig;


	static const int NoClipHeight = 0x7FFFFFFF;	//max positive int
	typedef int(*FClipHeight)(index_t);	//!< Clipping function for baking operations

	inline bool	IsClipped(FClipHeight f, index_t cell, int h)	{ return f(cell)<=h; }



	static const constexpr float RadiusDeltaRatio = 0.033f;	//mean layer height relative to planet mean radius

	static const constexpr float MeanPlanetRadius = WorldScale;

#ifdef GAME_BLOB_SYSTEM
	Buffer<Volume::Blob>	blobs;
#endif

	static const constexpr float SingleMeanLayerHeight = RadiusDeltaRatio * WorldScale;

	static const constexpr float MobileBaseScale = Grid::SingleMeanLayerHeight * 1.5f;

	inline float ToRadiusf(float height)
	{
		//ASSERT__(layer < 1000.f);
		return height >= 0 ?
			height * SingleMeanLayerHeight + MeanPlanetRadius
			:
			MeanPlanetRadius * pow(1.f + RadiusDeltaRatio,height);
			;
	}
	inline float HeightFromRadiusf(float radius)
	{
		return radius >= MeanPlanetRadius ?
			(radius - MeanPlanetRadius) / SingleMeanLayerHeight
			:
			-log(MeanPlanetRadius/radius)/log(1.f + RadiusDeltaRatio)
			;
	}

	inline float ToRadius(int height)
	{
		return ToRadiusf(height);
	}

	inline int HeightFromRadius(float radius)
	{
		return (int)M::Round(HeightFromRadiusf(radius));
	}


	inline int HeightFloorFromRadius(float radius)
	{
		return (int)floor(HeightFromRadiusf(radius));
	}


	inline float GetLayerHeight(int height)
	{
		return height < 0 ? RadiusDeltaRatio * ToRadius(height) : SingleMeanLayerHeight;
	}



	inline float powi(float f, UINT exponent)
	{
		if (exponent == 0)
			return 1.f;
		if (exponent == 1)
			return f;
		float rs = powi(f,exponent>>1);
		rs *= rs;
		if (exponent % 2)
			rs *= f;
		return rs;
	}


	



	class CellStack
	{
		static UINT		ToLayer(UINT layer)	{ FATAL__("bad call"); }
		static int		ToHeight(int height) { FATAL__("bad call"); }
	public:
		//float topHeight;

		static const unsigned Range = 32;	//range up and down. effective range is thus [-Range/2,+Range/2]
		static const int HalfRange = (Range >> 1);
		static const constexpr int	TopHeight = HalfRange-1,
									BottomHeight = -HalfRange;


		struct TTypeDelta
		{
			typedef TTypeDelta Self;

			BYTE		newType;	//0 = Bedrock = no change. Bedrock cannot be applied to normal cells

			bool		operator!=(const TTypeDelta&other) const
			{
				return newType != other.newType;
			}
			bool		operator==(const TTypeDelta&other) const
			{
				return newType == other.newType;
			}
			bool		IsSet() const {return newType != 0;}

			friend void Serialize(IWriteStream&s, const Self&v)
			{
				using Serialization::Serialize;
				Serialize(s,v.newType);
			}
			friend void Deserialize(IReadStream&s, Self&v)
			{
				using Serialization::Deserialize;
				Deserialize(s,v.newType);
			}
		};

		static_assert(sizeof(TTypeDelta) == 1, "Struct-alignment out of whack");

		struct TTypeDeltaBlock
		{
			typedef TTypeDeltaBlock Self;

			UINT32				repitition;
			TTypeDelta			value;
			bool		operator!=(const TTypeDeltaBlock&other) const
			{
				return repitition != other.repitition || value != other.value;
			}

			friend void Serialize(IWriteStream&s, const Self&v)
			{
				s.WriteSize(v.repitition);
				using Serialization::Serialize;
				Serialize(s,v.value);
			}
			friend void Deserialize(IReadStream&s, Self&v)
			{
				s.ReadSize(v.repitition);
				using Serialization::Deserialize;
				Deserialize(s,v.value);
			}
		};

		class CompactCell
		{
		public:
			CONSTRUCT_ENUMERATION13(Content,
				Unknown,
				Granite,
				Earth,
				Grass,
				Rock,
				Sand,
				Snow,
				Foundation,
				Road,
				Lava,
				Technite,
				Water,
				Clear
			)


			BYTE	content;
			BYTE	structureCounter;	//number of structures (density) that occupy this block

			/**/	CompactCell() :content(Content::Clear), structureCounter(0) {}
			/**/	CompactCell(Content t) :content(t), structureCounter(0)
			{}



			void	GetDeltaFrom(const CompactCell&other, TTypeDelta&tout)	const
			{
				tout.newType = content != other.content ? content+1 : 0;
			}
			void	GetDeltaFromAndUpdate(CompactCell&other, TTypeDelta&tout) const
			{
				GetDeltaFrom(other,tout);
				other.content = this->content;
			}


			bool	ApplyDelta(const TTypeDelta&tdelta)
			{
				if (tdelta.newType)
				{
					content = tdelta.newType-1;
					DBG_ASSERT_LESS__(content,Content::NumberOfPossibilities);
					return true;
				}
				return false;
			}
		};

		class Cell : public CompactCell
		{
		public:
			static BYTE Degrade(BYTE level, BYTE by)
			{
				if (level <= by)
					return 0;
				return level - by;
			}


			//bool	occupied;	//placeholder for more stuff later on
			#ifdef HAS_WINDOW
				BYTE	intLightLevel;
				BYTE	submergence;	//levels of submergence under water.
				#ifdef THOROUGLY_DEBUG_SENSORY
					BYTE	dbgVisibility;	//used to check visibility visualization is correct
				#endif
			#endif

			static const count_t TypeCount = 13;

			static float3	typeColor[TypeCount];
			static float	typeStructureFactor[TypeCount];
			static bool		typeColorMixes[TypeCount];
			static int		typeStrength[TypeCount];


			short	visibility=0;
			bool	temporarilyVisible=false;

			#ifdef HAS_WINDOW
				Grid::TVisibility	GetVisibility() const {return Grid::TVisibility(visibility>0);}
			#endif

			void	AlterVisibility(short change)
			{
				visibility += change;
				DBG_ASSERT__(visibility >= 0);
				if (change >= 0)	//==0 is a special case triggering temporary visibility but leaving actual visibility unchanged
				{
					temporarilyVisible = true;
				}
			}

			/**
			Checks whether or not the terrain contents of the local cell are currently visible,
			either because it is currently in range, or has been in range at any given time during
			the last processing round.
			Use this method for terrain contents only. Not to be used for entity visibility.
			Use IsCurrentlyVisible() instead!
			*/
			bool	TerrainIsVisible() const
			{
				return visibility > 0 || temporarilyVisible;
			}

			/**
			Checks whether or not this cell is currently visible.
			Should be used for entity visibility.
			*/
			bool	IsCurrentlyVisible() const
			{
				return visibility > 0;
			}


			#ifdef HAS_WINDOW
			/**/	Cell() :CompactCell(Content::Clear), intLightLevel(255), submergence(0) {}
			/**/	Cell(Content t, BYTE lightLevel) :CompactCell(t), intLightLevel(lightLevel), submergence(0)	{}
			#else
			/**/	Cell() :CompactCell(Content::Clear) {}
			/**/	Cell(Content t, BYTE /*lightLevel*/) :CompactCell(t)	{}
			#endif
			bool	SetToIfSolid(BYTE cnt)	{if (IsSolid() && content != cnt) {content = cnt; return true;} return false;}
			bool	IsWater() const { return content == Content::Water; }
			bool	IsTechnite() const {return content == Content::Technite; }
			//bool	WaterSurfaceBelow() const { return content == Content::Clear; }
			bool	IsOccupied() const
			{
				return content != Content::Clear || structureCounter > 1;	//one is mild structure, 2+ is heavy
			}
			bool	IsClearOrWater() const
			{
				return structureCounter == 0 && (content == Content::Clear || content == Content::Water);
			}
			bool	IsClearOrWaterIgnoreStructure() const
			{
				return (content == Content::Clear || content == Content::Water);
			}

			bool	IsClearOrLiquidIgnoreStructure() const
			{
				return (content == Content::Clear || IsLiquid(content));
			}

			static bool IsDead(BYTE cnt)
			{
				return cnt < Content::Technite;
			}
			static bool IsAlive(BYTE cnt)
			{
				return cnt == Content::Technite;
			}
			static bool IsSolid(BYTE cnt)
			{
				return cnt < Content::Water && cnt != Content::Lava;
			}
			static bool IsOpaque(BYTE cnt)
			{
				return cnt < Content::Technite;
			}
			static bool IsLiquid(BYTE cnt)
			{
				return cnt == Content::Water || cnt == Content::Lava;
			}

			bool	IsOpaque() const {return IsOpaque(content);}
			bool	IsLiquid() const {return IsLiquid(content);}

			bool	IsSolid() const
			{
				//if (content == Content::Foundation)
					//ShowMessage("break");
				return IsSolid(content);
				//return content < Content::Water;
			}
			bool	IsSolidDead() const	{return IsDead(content);}
			int		GetStrength() const
			{
				return typeStrength[content];
			}


			#ifdef HAS_WINDOW
				bool	Mixes()	const{ return typeColorMixes[content]; }

				float	GetLightLevel() const;
				float	GetRawLightLevel() const	{return float(intLightLevel) / 255.f;}
				void	SetLightLevel(float v);
				void	SetIntLightLevel(int v);
				void	SetSubmergence(UINT sub);
				UINT	GetIncreasedSubmergence() const;
				float	GetStructureFactor() const	{ return typeStructureFactor[content]; }
			#endif
		};

		Cell			volumeCell[Range];		//All contained cells
		static Cell		bottom, top;	//extremes below the bottom most, and above the top most
		#ifdef HAS_WINDOW
			BYTE			lowestOpenCell=0,
							highestOpaqueCell=0,
							lightingChangedTo=0;
			M::TIntRange<BYTE>	waterRange;
		#endif


		static UINT		ToLayer(int height)	{return (UINT)(height < -HalfRange ? 0 : height + HalfRange);}
		static int		ToHeight(UINT layer) {return int(layer) - CellStack::HalfRange;}

		/**
		Finds the top-most clear block that is located above a non-clear cell.
		@param stopOnStructureCount If set search will stop on the first occurrence of a structure count >0
		@param failOnBlockingStructureAtDestination If set, will cause the method to fail if the located clear cell has a >1 struct-count. Only checks any matching cell
		@return Height level that is within the valid range, clear, and above a non-clear block. Returns BadHeight on failure
		*/
		int				FindTopClearOverNonClear(bool stopOnStructureCount, bool failOnBlockingStructureAtDestination)	const;
		
		/**
		Finds the bottom-most clear block that is located beneath a non-clear cell, starting at the specified layer
		@param stopOnStructureCount If set search will stop on the first occurrence of a structure count >0
		@param failOnBlockingStructureAtDestination If set, will cause the method to fail if the located clear cell has a >1 struct-count. Only checks any matching cell
		@return Height level that is within the valid range, clear, and beneath a non-clear block. Returns BadHeight on failure
		*/
		int				FindNextClearBeneathNonClear(bool stopOnStructureCount, bool failOnBlockingStructureAtDestination, UINT startLayer) const;

		#ifdef HAS_WINDOW
		bool			HasWater() const { return waterRange.IsNotEmpty(); }


		void			DebugCheckRange()
		{
			//for (BYTE i = 0; i < lowestOpenCell; i++)
			//	ASSERT__(volumeCell[i].IsSolid());
			//for (BYTE i = highestOpaqueCell+1; i < Range; i++)
			//	ASSERT__(!volumeCell[i].IsSolid());
		}

		void			DbgCheckWaterRange() const
		{
			#ifdef _DEBUG
				if (waterRange.IsNotEmpty())
				{
					ASSERT_LESS_OR_EQUAL__(waterRange.end, Range);
				}
				else
				{
					ASSERT2__(
						(waterRange.start == 0xFF && waterRange.end==0x0)
						||
						(waterRange.start == 0x0 && waterRange.end==0x0)
						,waterRange.start,waterRange.end
					);
				}
			#endif
		}

		void			UpdateRange()
		{
			waterRange.start = 0xFF;
			waterRange.end = 0x0;
			lowestOpenCell = 0xFF;
			highestOpaqueCell = 0;
			for (unsigned i = 0; i < Range; i++)
				if (volumeCell[i].IsOpaque())
					highestOpaqueCell = (BYTE)i;
				else
				{
					lowestOpenCell = std::min(lowestOpenCell, (BYTE)i);
					if (volumeCell[i].IsWater())
						waterRange.Include(i);
				}
		}
		#endif

		const Cell&		GetCellU(unsigned layer)	const;
		const Cell&		GetCellI(int height)	const;
		Cell&			GetMutableCellI(int height);
		//#ifdef HAS_WINDOW

		//	TLitColor	GetDownColorU(unsigned layer, Cell::Content oppositeContent, bool&mixes, Cell::Content&outType) const;
		//	TLitColor	GetColorU(unsigned layer, Cell::Content oppositeContent, bool&mixes, Cell::Content&outType) const;
		//	//float3			GetSelectiveColorU(unsigned layer, const float3&colorIfNonSolid, bool&mixes) const;
		//	TWeightedColor	GetMixColorU(unsigned layer, Cell::Content oppositeContent, float&sumBrightnessTo, int&sumVisibilityTo, Cell::Content&outContent) const;
		//	TWeightedColor	GetDownMixColorU(unsigned layer, Cell::Content oppositeContent, float&sumBrightnessTo, int&sumVisibilityTo, Cell::Content&outContent) const;
		//#endif
		void			ChangeStructureCounterI(int height, int by);


		//bool			PutLightStructureI(int height);
		//bool			ClearLightStructureI(int height);

		int				FindTopOccupied() const;
		bool			FindTopOccupied(int&outHeight) const;


		void			ZeroInit(bool resetVisibility);
		/**
		Resets structure counters (and potentially visibility) to 0
		@param resetVisibility Set true to also reset visibility
		@return true if anything changed, false otherwise
		*/
		bool			ResetStructureCounters(bool resetVisibility);
		#ifdef HAS_WINDOW

			#ifdef THOROUGLY_DEBUG_SENSORY
				void	ClearDbgSensory()	{for (index_t i = 0; i < ARRAYSIZE(cell); i++) volumeCell[i].dbgVisibility = 0;}
				void	CheckDbgSensory()	{for (index_t i = 0; i < ARRAYSIZE(cell); i++) ASSERT_EQUAL1__(volumeCell[i].dbgVisibility,volumeCell[i].visible,i);}
			#endif
		#endif


		/**
		Attempts to find the most applicable ground relative to the given location.
		A ground location is a cell that is not occupied but above an occupied cell.
		If the specified cell is occupied, any applicable ground must be ab
		*/
		int				FindMostApplicableGround(int h, bool canStandOnWater, bool canStandOnLava) const;
	};

	typedef CellStack::Cell::Content Content;

	CONSTRUCT_ENUMERATION5(PlaceholderType, Clear, LiquidMountains, LiquidExtendedCore, Mountains, ExtendedCore);

	class Resource;

	struct TDistanceRange : public M::TFloatRange<>
	{
		typedef M::TFloatRange<>	Super;

		float	sum=0;
		count_t	numSamples=0;

		void	Include(float distance)
		{
			Super::Include(distance);
			sum += distance;
			numSamples ++;
		}
		float	GetAverage() const
		{
			return numSamples ? sum / numSamples : 1;
		}
	};

	//extern DX11::ObjectBuffer<float4>	terrainColor;


	
	static const UINT32			CutoffRadius = 20;

	
	
	inline float Get(const SimplexNoise&n, const M::TVec3<>&p)
	{
		return (float)n.Get(p.x,p.y,p.z);
	}
	inline float GetContinuous(const SimplexNoise&n, const M::TVec3<>&p)
	{
		return (float)n.GetUniform(p.x,p.y,p.z);
	}




	///**
	//Visualizes the outlines of the spcified low-resolution stackID and all high-resolution cells that intersect it.
	//*/
	//void	RenderNeighborhood(index_t cellID, float radius);

	///**
	//Calculates a fractal random height for the given center direction
	//*/
	//float	RandomHeight(const M::TVec3<>&center0);



	/**
	Clipping functions to be used during surface compilation
	*/
	namespace Clip
	{
		inline int NoClip(index_t)	{ return NoClipHeight; }
	}




	static const float BottomPlanetRadius = ToRadius(CellStack::BottomHeight);
	static const float TopPlanetRadius = ToRadius(CellStack::TopHeight);







	//#pragma pack(push)
	//#pragma pack(1)
	struct TCellResourceCoverage
	{
		typedef TCellResourceCoverage Self;

		UINT32		cellIndex;
		BYTE		coverage;

		friend void		Deserialize(IReadStream&s, Self&v)
		{
			using Serialization::Deserialize;
			s.ReadSize(v.cellIndex);
			Deserialize(s,v.coverage);
		}
		friend void		Serialize(IWriteStream&s, const Self&v)
		{
			using Serialization::Serialize;
			s.WriteSize(v.cellIndex);
			Serialize(s,v.coverage);
		}
		bool operator!=(const Self&other) const {return !operator==(other);}
		bool operator==(const Self&other) const
		{
			return cellIndex == other.cellIndex && coverage == other.coverage;
		}
	};
	//#pragma pack(pop)

	void			FlushStackFamily(Ctr::Array<CellStack>&cellStack,  BYTE family);

	#ifdef VOXEL_SCANNER
		void		RemapToLowStacks(BYTE family);
	#endif



	#ifdef CLIENT
		#ifndef NO_SATELLITES
			extern OrbitGrid	visibleOrbitGrid;
		#endif

	#endif
}

