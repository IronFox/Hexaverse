#pragma once


#include <math/matrix.h>
#include <engine\aspect.h>
#include "base.h"
#include "../stringTable.h"
#include "../logEvent.h"
#include <general/random.h>
#include <math/SimplexNoise.h>
#include "types.h"


static const constexpr float WorldScale = 40.f;

namespace Grid
{
	

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


	extern UINT16				empZonesLocked,
								empZonesUnlocked;


	static const constexpr float RawLayerHeight = 0.025f;
	#ifdef LOWER_RESOLUTION_PLANET
		static const constexpr float UnitScale = 2.f;
	#else
		static const constexpr float UnitScale = 1.f;
	#endif
	static const constexpr float VerticalScale = 0.7f * UnitScale * WorldScale;
	static const constexpr float VerticalBase = WorldScale;

#ifdef GAME_BLOB_SYSTEM
	Buffer<Volume::Blob>	blobs;
#endif

	static const float SingleLayerHeight = RawLayerHeight * VerticalScale;

	inline float ToRadiusf(float height)
	{
		//ASSERT__(layer < 1000.f);
		return height * SingleLayerHeight + VerticalBase;
	}
	inline float HeightFromRadiusf(float radius)
	{
		return (radius - VerticalBase) / SingleLayerHeight;

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
		static const int	TopHeight = HalfRange-1,
							BottomHeight = -HalfRange;


		struct TTypeDelta
		{
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
		};

		static_assert(sizeof(TTypeDelta) == 1, "Struct-alignment out of whack");

		#pragma pack(push)
		#pragma pack(1)
		struct TTypeDeltaBlock
		{
			UINT32				repitition;
			TTypeDelta			value;
			bool		operator!=(const TTypeDeltaBlock&other) const
			{
				return repitition != other.repitition || value != other.value;
			}
		};
		#pragma pack(pop)

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


			#ifdef HAS_WINDOW
			/**/	Cell() :CompactCell(Content::Clear), intLightLevel(255), submergence(0) {}
			/**/	Cell(Content t, BYTE lightLevel) :CompactCell(t), intLightLevel(lightLevel), submergence(0)	{}
			#else
			/**/	Cell() :CompactCell(Content::Clear) {}
			/**/	Cell(Content t, BYTE /*lightLevel*/) :CompactCell(t)	{}
			#endif
			void	SetToIfSolid(BYTE cnt)	{if (IsSolid()) content = cnt;}
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
			BYTE			lowestOpenCell,
							highestOpaqueCell,
							lightingChangedTo;
			TIntRange<BYTE>	waterRange;
		#endif


		static UINT		ToLayer(int height)	{return (UINT)(height < -HalfRange ? 0 : height + HalfRange);}
		static int		ToHeight(UINT layer) {return int(layer) - CellStack::HalfRange;}

		/**
		Finds the top-most clear block that is located above a non-clear cell.
		@param failOnStructureCount If set will cause the method to fail if the located clear cell has a >1 struct-count
		@return Height level that is within the valid range, clear, and above a non-clear block. Returns BadHeight on failure
		*/
		int				FindTopClearOverNonClear(bool failOnStructureCount)	const;

		#ifdef HAS_WINDOW
		bool			HasWater() const { return waterRange.IsNotEmpty(); }


		void			DebugCheckRange()
		{
			//for (BYTE i = 0; i < lowestOpenCell; i++)
			//	ASSERT__(volumeCell[i].IsSolid());
			//for (BYTE i = highestOpaqueCell+1; i < Range; i++)
			//	ASSERT__(!volumeCell[i].IsSolid());
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


		void			ZeroInit();
		void			ResetStructureCounters();
		#ifdef HAS_WINDOW

			#ifdef THOROUGLY_DEBUG_SENSORY
				void	ClearDbgSensory()	{for (index_t i = 0; i < ARRAYSIZE(cell); i++) volumeCell[i].dbgVisibility = 0;}
				void	CheckDbgSensory()	{for (index_t i = 0; i < ARRAYSIZE(cell); i++) ASSERT_EQUAL1__(volumeCell[i].dbgVisibility,volumeCell[i].visible,i);}
			#endif
		#endif

		bool			MakeLocalHole(int h, World::Data&owner);

		bool			LocalOverwrite(int h, Cell::Content type, World::Data&owner);

		/**
		Attempts to find the most applicable ground relative to the given location.
		A ground location is a cell that is not occupied but above an occupied cell.
		If the specified cell is occupied, any applicable ground must be ab
		*/
		int				FindMostApplicableGround(int h, bool canStandOnWater) const;
	};

	typedef CellStack::Cell::Content Content;


	class Resource;

	struct TDistanceRange : public TFloatRange<>
	{
		typedef TFloatRange<>	Super;

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

	template <unsigned Exponent>
	class Setup
	{
	public:
		typedef GridSetup<Exponent>	GridType;
		GridType					grid;
		static Array<BaseGrid::TNetCell>	cells;
		Array<BaseGrid::TCellVertex>	cellVertices;
		TDistanceRange				neighborCellDistances;


		struct TDistanceField
		{
			static const count_t	NumEntries = GridType::CellCount;
			UINT32					distance[GridType::CellCount];

			/**/					TDistanceField()
			{
				Reset();
			}

			void					Reset()
			{
				memset(distance, 0xFF, sizeof(distance));
			}
		};

		struct TVolumeDescriptor
		{
			struct TStack
			{
				UINT32				stackID;	//high-resolution cell IDs
				TIntRange<UINT32>	range;		//range.end is one higher than actual limit
			};

			Buffer0<TStack>			coverage;

			void					Clear()
			{
				coverage.Clear();
			}

			count_t					GetVolume() const
			{
				count_t rs = 0;
				foreach (coverage,c)
					rs += c->range.GetExtent();
				return rs;
			}


			struct Operator
			{
				bool	stackVisited[GridType::CellCount];
				Queue<UINT32>	stacksLeft;

				/**/		Operator()
				{
					memset(stackVisited,false,sizeof(stackVisited));
				}

				void		ResetVisited(const TVolumeDescriptor&res)
				{
					foreach (res.coverage,c)
						stackVisited[c->stackID] = false;

				}
			};
		};



		/**/						Setup()
		{

		}

		/**
		Determines the extended neighborhood around the given center cell.
		Should be used for small neighborhoods only since it uses linear search on all already found neighbors
		*/
		void						GetExtendedNeighborhood(index_t centerCell, UINT32 maxDistance, BasicBuffer<UINT32>&outNeighbors) const
		{
			Queue<std::pair<index_t, UINT32> >	queue;
			queue << std::make_pair(centerCell, 0);
			std::pair<index_t, UINT32> current;
			outNeighbors.Clear();
			while (queue >> current)
			{
				if (outNeighbors.Contains(current.first))
					continue;
				outNeighbors << (UINT32)current.first;
				if (current.second == maxDistance)
					continue;
				const auto&n = grid.GetCell(current.first).neighborhood;
				for (int i = 0; i < n.numNeighbors; i++)
					queue << std::make_pair(n.neighborID[i], current.second + 1);
			}
		}

		void						UpdatePlanarDistanceField(index_t centerCell, UINT32 maxDistance, TDistanceField&field) const
		{
			field.Reset();

			field.distance[centerCell] = 0;
			Queue<index_t>	queue;
			queue << centerCell;
			index_t stackID;
			while (queue >> stackID)
			{
				UINT32 d = field.distance[stackID] + 1;
				if (d > maxDistance)
					continue;
				const auto &nh = grid.GetCell(stackID).neighborhood;
				for (int i = 0; i < nh.numNeighbors; i++)
				{
					UINT32 n = nh.neighborID[i];
					if (field.distance[n] > d)
					{
						field.distance[n] = d;
						queue << n;
					}
				}
			}
		}

		void			VisitNeighborsOf(const typename GridType::CellIdU&center, const std::function<void(const typename GridType::CellIdU&)>&visitor) const
		{
			const auto&n = grid.GetCell(center.stackID).neighborhood;
			if (center.layer > 0)
				for (BYTE k = 0; k < n.numNeighbors; k++)
					visitor(typename GridType::CellIdU(n.neighborID[k],center.layer-1));

			if (center.layer+1 < CellStack::Range)
				for (BYTE k = 0; k < n.numNeighbors; k++)
					visitor(typename GridType::CellIdU(n.neighborID[k],center.layer+1));
			for (BYTE k = 0; k < n.numNeighbors; k++)
				visitor(typename GridType::CellIdU(n.neighborID[k],center.layer));
		}

		/**
		Attempts to find the closest stack match of the specified directional vector
		@param v Vector to match. May be any length
		@param startID index of the stack to start from
		@return Closest matching stack index
		*/
		UINT32			Locate(const TVec3<>&v, UINT32 startID = 0)	const
		{
			UINT32 stackID = startID;
			for (;;)
			{
				float d0 = Vec::dot(v, grid.GetCell(stackID).center);
				float d = d0;
				UINT32 closest = 0xFFFFFFFF;
				const auto &nh = grid.GetCell(stackID).neighborhood;
				for (int i = 0; i < nh.numNeighbors; i++)
				{
					UINT32 n = nh.neighborID[i];
					float d1 = Vec::dot(v, grid.GetCell(n).center);
					if (d1 > d)
					{
						d = d1;
						closest = n;
					}
				}
				if (closest == 0xFFFFFFFF)
					return stackID;
				stackID = closest;
			}
		}


		void		Create()
		{
			grid.Create();
			grid.Construct(cellVertices, cells);

			neighborCellDistances = TDistanceRange();
			foreach (cells,c)
			{
				neighborCellDistances.Include(c->minCornerDistance * 2.f);	//not the most precise, but should be somewhat acceptable on average
			}

		}

		static float FindStackMaxHeight(const TIntRange<>&range, const CellStack&stack)
		{
			foreach(stack.sections, section)
				if (section->range.Intersects(range))
					//if (section->range.start < range.start && section->range.end > range.start)
					return section->GetMax();

			return -1.f;
		}
		static float FindStackMaxHeightAbove(const TIntRange<>&range, const CellStack&stack)
		{
			foreach(stack.sections, section)
				if (range.Contains(section->range.start) && range.end < section->range.end)
					return section->GetMax();

			return -1.f;
		}

	


		//void	GenerateVolume(const Sphere<>&sphere,TVolumeDescriptor&volume) const
		//{
		//	static TVolumeDescriptor::Operator op;
		//	volume.Clear();
		//	UINT32 centerStack = Locate(sphere.center);
		//	op.stacksLeft << centerStack;

		//	const float br = ToRadius(CellStack::BottomHeight);
		//	const float tr = ToRadius(CellStack::TopHeight);
		//	UINT32 next;
		//	while (op.stacksLeft >> next)
		//	{
		//		if (op.stackVisited[next])
		//			continue;
		//		op.stackVisited[next] = true;
		//		const TVec3<>&dir = cells[next].cornerCenter;
		//		TVec2<> distances;
		//		TVec3<> bottom,top;
		//		Vec::mult(dir,br,bottom);
		//		Vec::mult(dir,tr,top);
		//		TVolumeDescriptor::TStack&st = volume.coverage.Append();
		//		st.stackID = next;
		//		if (!Obj::DetectSphereEdgeIntersection(sphere.center,sphere.radius,bottom,top,distances))
		//		{
		//			st.range.SetEmpty(0);
		//			continue;
		//		}
		//	
		//		st.range.start = (UINT32)floor(std::max(distances.x,0.f) * (CellStack::Range-1));
		//		st.range.end = 1 + (UINT32)ceil(std::min(distances.y,1.f) * (CellStack::Range-1));
		//		const auto&n = grid.GetCell(next).neighborhood;
		//		for (int i = 0; i < n.numNeighbors; i++)
		//			op.stacksLeft << n.neighborID[i];
		//	}
		//	op.ResetVisited(volume);
		//}

		#ifdef HAS_WINDOW
			
		#endif
	};

	template <UINT Exponent>
		Array<BaseGrid::TNetCell>	Setup<Exponent>::cells;

	#ifdef LOWER_RESOLUTION_PLANET
		static const constexpr UINT HighExponent=5;
	#else
		static const constexpr UINT HighExponent=6;
	#endif

	static const constexpr UINT LowExponent = HighExponent-1;


	typedef Setup<LowExponent>	LowSetup;

	#ifdef HYBRID_GRID
		typedef Setup<HighExponent>	HighSetup;
	#else
		typedef LowSetup	HighSetup;
	#endif
	typedef HighSetup::TDistanceField	THighDistanceField;


	typedef HighSetup::GridType::CellIdI	HCellIdI;
	typedef HighSetup::GridType::CellIdU	HCellIdU;
	typedef LowSetup::GridType::CellIdI		LCellIdI;
	typedef LowSetup::GridType::CellIdU		LCellIdU;


	extern HighSetup		high;
	extern LowSetup			low;

	//extern DX11::ObjectBuffer<float4>	terrainColor;


	
	static const UINT32			CutoffRadius = 14;

	
	
	inline float Get(const SimplexNoise&n, const TVec3<>&p)
	{
		return (float)n.Get(p.x,p.y,p.z);
	}
	inline float GetContinuous(const SimplexNoise&n, const TVec3<>&p)
	{
		return (float)n.GetUniform(p.x,p.y,p.z);
	}

	bool IsRoad(BYTE /*CellStack::Cell::Content::type_t*/ t, bool clearIsRoad);



	///**
	//Visualizes the outlines of the spcified low-resolution stackID and all high-resolution cells that intersect it.
	//*/
	//void	RenderNeighborhood(index_t cellID, float radius);

	///**
	//Calculates a fractal random height for the given center direction
	//*/
	//float	RandomHeight(const TVec3<>&center0);


	/**
	Initializes the grid and all static gpu resources. Call ONCE before creating objects (and their shadows)
	*/
	void				Create0();
	/**
	Concludes grid initialization. Call ONCE after Create0(), and creating objects (and their shadows)
	*/
	void				Create1();
	/**
	Destroys all gpu-resouces allocated by local structures. Call ONCE when shutting down
	*/
	void				Destroy();

	/**
	Clipping functions to be used during surface compilation
	*/
	namespace Clip
	{
		inline int NoClip(index_t)	{ return NoClipHeight; }
	}




	/**
	@brief Sets all family entries in \a rebuild to true that the specified high-res cell and its neighbors belong to.
	
	Does not alter entries of other families. Does not set values to false.
	*/
	void TagCluster(bool*rebuild, index_t cellID);

	UINT32 TagLowCluster(bool *rebuild, const LCellIdI&cell);







	/**
	Tests wether the high resolution cell @a highCell is a part (center or edge) of the low resolution of cell @a lowCell
	*/
	bool	IsHighPartOfLow(UINT32 highCell, UINT32 lowCell);


	inline bool	ValidCell(const LCellIdI&cell)	{return cell.stackID < LowSetup::GridType::CellCount 
													&& 
													(
														(cell.height >= CellStack::BottomHeight && cell.height <= CellStack::TopHeight)
													)
													;}
	#ifdef HYBRID_GRID
		inline bool	ValidCell(const HCellIdI&cell)	{return cell.stackID < HighSetup::GridType::CellCount && cell.height >= -CellStack::HalfRange && cell.height < CellStack::HalfRange;}
	#endif

	float	GetGeometricDistance(const LCellIdI&a, const LCellIdI&b);
	float	GetQuadraticGeometricDistance(const LCellIdI&a, const LCellIdI&b);


	bool	AffectedByEMP(const HCellIdI&);
	bool	AffectedByEMP(const LCellIdI&);

	/**
	Sets both high resolution and low resolution families outdated, and to the specified time
	*/
	void	SetAllFamiliesOutdated(float t=0);

	TVec3<> ToWorld(const HCellIdU&);
	TVec3<> ToWorld(const HCellIdI&);
	TVec3<> ToWorld(const LCellIdU&);
	TVec3<> ToWorld(const LCellIdI&);
	TVec3<> ToWorld(const HCellIdU&, float fLayerOffset);
	TVec3<> ToWorld(const HCellIdI&, float fLayerOffset);
	TVec3<> ToWorld(const LCellIdU&, float fLayerOffset);
	TVec3<> ToWorld(const LCellIdI&, float fLayerOffset);

	inline HCellIdI	ToHigh(const LCellIdI&cellID)	{return HCellIdI(low.grid.GetCell(cellID.stackID).higherEquivalent,cellID.height); }
	inline HCellIdU	ToHigh(const LCellIdU&cellID)	{return HCellIdU(low.grid.GetCell(cellID.stackID).higherEquivalent,cellID.layer); }
	inline HCellIdU	ToLayered(const HCellIdI&cellID)	{return HCellIdU(cellID.stackID,CellStack::ToLayer(cellID.height));}
	inline LCellIdU	ToLayered(const LCellIdI&cellID)	{return LCellIdU(cellID.stackID,CellStack::ToLayer(cellID.height));}

	#pragma pack(push)
	#pragma pack(1)
	struct TCellResourceCoverage
	{
		UINT32		cellIndex;
		BYTE		coverage;
	};
	#pragma pack(pop)

	void			FlushStackFamily(Array<CellStack>&cellStack,  BYTE family);
	void			FlushAllVolumeData();

	void			UpdateResourceMap(ConstName resourceName,const ArrayData<TCellResourceCoverage>&);
	#ifdef HAS_WINDOW
		void		SetParaboloidDisplay(const TVec3<>&direction, float height, float flatRadius, const float4&color);
		void		SetSphereDisplay(const TVec3<>&center, float radius, const float4&color);
		void		UnsetParaboloidAndSphereDisplay();



		#ifdef THOROUGLY_DEBUG_SENSORY
			void	InsertDbgSensor(const LCellIdI&, float radius);
		#endif

	#endif
	#ifdef VOXEL_SCANNER
		void		RemapToLowStacks(BYTE family);
	#endif





	class OrbitGrid
	{
	public:
		struct TCell
		{
			short visible = 0;


		};


		TCell	cells[LowSetup::GridType::CellCount];
		float	orbit = 1.f;

		#if 0
		#ifdef HAS_WINDOW
			struct Vertex
			{
				float4		vertex3_edge;
				float4		normal3_opacity;
				float		emp;

				/**/		Vertex()	{};
				/**/		Vertex(const float3&n,float edge, float orbit, float opacity, float emp) : vertex3_edge(n*orbit,edge), normal3_opacity(n,opacity),emp(emp){}
				void		Set(const float3&n,float edge, float orbit, float opacity, float emp) {vertex3_edge = float4(n*orbit,edge); normal3_opacity = float4(n,opacity); this->emp = emp;}
			};
	
			Array<Vertex>	vertices;
			DX11::InstructionBlock32	instruction;
			void		ForceRecompileNow();
			void		Render(const Engine::Frustum<>&);
		#endif
		#endif /*0*/

		bool			valid = false;
		void			Invalidate() {valid = false;}

			
		/**/			OrbitGrid()
		{
			Clear();
		}

		void			ChangeSensory(const LCellIdI&center, float radius, signed char change);

		void			Clear()
		{
			TCell clear;
			for (index_t i = 0; i < LowSetup::GridType::CellCount; i++)
				cells[i] = clear;
			Invalidate();
			//colorArea.active = false;
		}


		//void	Color(const LCellIdI&center, int fuelRadius);
		//void	StripColor();

	};


	void		CreateHighAndLow();

	#ifdef CLIENT
		#ifndef NO_SATELLITES
			extern OrbitGrid	visibleOrbitGrid;
		#endif

	#endif
}

