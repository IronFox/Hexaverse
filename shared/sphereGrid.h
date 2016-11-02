#ifndef sphereGridH
#define sphereGridH

#include <math/matrix.h>
#include <engine\aspect.h>
#include "grid.h"
#include "stringTable.h"
#include "logEvent.h"
#include <general/random.h>
#include <math/SimplexNoise.h>

#ifdef HAS_WINDOW
	#include "../client/ExtRenderer/DirectX11.h"
#endif



static const constexpr float WorldScale = 40.f;

namespace Grid
{
	


	CONSTRUCT_ENUMERATION4(WorldDiscovery,
		None,
		Water,
		Liquids,
		FullTerrain
	)

	static const int BadHeight = 0x1FFFFFFF;	//keep equal to gridAndChoices.h Objects::badHeight

	class ExtendedPlanetConfig;

	#ifdef HAS_WINDOW
		namespace DX11 = Engine::DX11;
	#endif

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
	Buffer<WorldData::Blob>	blobs;
#endif

	static const float SingleLayerHeight = RawLayerHeight * VerticalScale;

	inline float ToRadiusf(float height)
	{
		//ASSERT__(layer < 1000.f);
		return height * SingleLayerHeight + VerticalBase;
	}
	inline float FromRadiusf(float radius)
	{
		return (radius - VerticalBase) / SingleLayerHeight;

	}

	inline float ToRadius(int height)
	{
		return ToRadiusf(height);
	}

	inline int FromRadius(float radius)
	{
		return (int)M::Round(FromRadiusf(radius));
	}


	inline int FloorFromRadius(float radius)
	{
		return (int)floor(FromRadiusf(radius));
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

	struct TSharedPlanetConfig;
	#ifndef CLIENT
		class ExtendedPlanetConfig;
		typedef ExtendedPlanetConfig	TPlanetConfig;
	#else
		typedef TSharedPlanetConfig	TPlanetConfig;
	#endif

	extern TPlanetConfig	currentPlanetConfig;
	


	#ifdef HAS_WINDOW

		struct TWeightedColor;

		struct TVisibility
		{
			float value = 0.f;

			/**/	TVisibility(float value):value(value)	{}
		};


		struct TBaseColor
		{
			float3	color;

			/**/	TBaseColor() = default;
			/**/	TBaseColor(const float3&c):color(c)	{}
			/**/	TBaseColor(const TWeightedColor&c);

			void	operator+=(const TBaseColor&c)
			{
				color += c.color;
			}
			void	operator*=(float fc)
			{
				color *= fc;
			}
		};

		struct TWeightedColor : TBaseColor
		{
			typedef TBaseColor	Super;

			float	weight=0.f;

			/**/	TWeightedColor() = default;
			/**/	TWeightedColor(const TBaseColor&c, float weight):Super(c),weight(weight)	{}

			void	operator+=(const TWeightedColor&c)
			{
				Super::operator+=(c);
				weight += c.weight;
			}
			void	operator*=(float fc)
			{
				Super::operator*=(fc);
				weight *= fc;
			}
		};


	
		struct TLitColor : public TBaseColor
		{
			typedef TBaseColor	Super;

			float	lightLevel=0.f;
			bool	capped = false;
			float	emp = 0.f;
			float	visibility=0.f;

			/**/	TLitColor() = default;
			/**/	TLitColor(const TBaseColor&base, float lightLevel, float emp=0.f):Super(base),lightLevel(lightLevel),emp(emp)	{}
			/**/	TLitColor(const TBaseColor&base, float lightLevel, float emp, const TVisibility&vis):Super(base),lightLevel(lightLevel),emp(emp),visibility(vis.value)	{}
			/**/	TLitColor(const TWeightedColor&base, float lightLevel, float emp=0.f):Super(base),lightLevel(lightLevel),emp(emp)	{}
			/**/	TLitColor(float lightLevel, float emp=0.f):lightLevel(lightLevel),emp(emp)	{}
			/**/	TLitColor(const TVisibility&v):visibility(v.value)	{}
			void	CompileFinalColor(TVec4<>&)	const;

		private:
			static void ApplyEMP(float intensity,TVec3<>&rs);
			void	CapColor(TVec3<>&rs)	const;
		};


	#endif


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
			#ifdef CLIENT
				short	visible=0;	//indicates levels of visibility. should not fall below 0. might make it unsigned when i am convinced it works. values >= 1 indicate this cell is visible

				#ifdef HAS_WINDOW
					TVisibility	GetVisibility() const {return TVisibility(visible>0);}
				#endif

			#endif

			/**/	CompactCell() :content(Content::Clear), structureCounter(0) {}
			/**/	CompactCell(Content t, short visibility) :content(t), structureCounter(0)
			{
			#ifdef CLIENT
				visible = visibility;
			#endif
			}


			#ifdef CLIENT
				void	AlterVisibility(short change)
				{
					visible += change;
					DBG_ASSERT__(visible >= 0);
				}
			#endif


			void	GetDeltaFrom(const CompactCell&other, TTypeDelta&tout)	const
			{
				tout.newType = content != other.content ? content+1 : 0;
			}
			void	GetDeltaFromAndUpdate(CompactCell&other, TTypeDelta&tout) const
			{
				GetDeltaFrom(other,tout);
				other.content = this->content;
			}


			//bool	ApplyDelta(const TTypeDelta&tdelta, const TStructCountDelta&sdelta)
			//{
			//	if (tdelta.newType == 0 && sdelta.newStructureCounter == 0)
			//		return false;
			//	if (tdelta.newType)
			//	{
			//		content = tdelta.newType;
			//		DBG_ASSERT_LESS__(content,Content::NumberOfPossibilities);
			//	}
			//	if (sdelta.newStructureCounter)
			//	{
			//		structureCounter = sdelta.newStructureCounter-1;
			//		DBG_ASSERT_LESS__(structureCounter,3);
			//	}
			//	return true;
			//}
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
			//bool	ApplyDelta(const TStructCountDelta&sdelta)
			//{
			//	if (sdelta.newStructureCounter)
			//	{
			//		structureCounter = sdelta.newStructureCounter-1;
			//		DBG_ASSERT_LESS__(structureCounter,3);
			//		return true;
			//	}
			//	return false;
			//}
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
			/**/	Cell() :CompactCell(Content::Clear,0), intLightLevel(255), submergence(0) {}
			/**/	Cell(Content t, BYTE lightLevel, short visibility) :CompactCell(t,visibility), intLightLevel(lightLevel), submergence(0)	{}
			#else
			/**/	Cell() :CompactCell(Content::Clear,0) {}
			/**/	Cell(Content t, BYTE /*lightLevel*/, short visibility) :CompactCell(t,visibility)	{}
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
				float	GetWaterLightLevel() const;
				float	GetRawLightLevel() const	{return float(intLightLevel) / 255.f;}
				void	SetLightLevel(float v);
				void	SetIntLightLevel(int v);
				void	SetSubmergence(UINT sub);
				UINT	GetIncreasedSubmergence() const;
				float	GetStructureFactor() const	{ return typeStructureFactor[content]; }
				TBaseColor	GetBaseColor() const;
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
		#ifdef HAS_WINDOW

			TLitColor	GetDownColorU(unsigned layer, Cell::Content oppositeContent, bool&mixes, Cell::Content&outType) const;
			TLitColor	GetColorU(unsigned layer, Cell::Content oppositeContent, bool&mixes, Cell::Content&outType) const;
			//float3			GetSelectiveColorU(unsigned layer, const float3&colorIfNonSolid, bool&mixes) const;
			TWeightedColor	GetMixColorU(unsigned layer, Cell::Content oppositeContent, float&sumBrightnessTo, int&sumVisibilityTo, Cell::Content&outContent) const;
			TWeightedColor	GetDownMixColorU(unsigned layer, Cell::Content oppositeContent, float&sumBrightnessTo, int&sumVisibilityTo, Cell::Content&outContent) const;
		#endif
		void			ChangeStructureCounterI(int height, int by);


		//bool			PutLightStructureI(int height);
		//bool			ClearLightStructureI(int height);

		int				FindTopOccupied() const;
		bool			FindTopOccupied(int&outHeight) const;


		void			InitUnknown(float2 heights, const ExtendedPlanetConfig&);
		void			FillTypes(const ExtendedPlanetConfig&);

		void			ZeroInit();
		void			ResetStructureCounters(bool resetVisibility);
		#ifdef HAS_WINDOW

			#ifdef THOROUGLY_DEBUG_SENSORY
				void	ClearDbgSensory()	{for (index_t i = 0; i < ARRAYSIZE(cell); i++) volumeCell[i].dbgVisibility = 0;}
				void	CheckDbgSensory()	{for (index_t i = 0; i < ARRAYSIZE(cell); i++) ASSERT_EQUAL1__(volumeCell[i].dbgVisibility,volumeCell[i].visible,i);}
			#endif
		#endif

		bool			MakeLocalHole(int h);

		bool			LocalOverwrite(int h, Cell::Content type);

		/**
		Attempts to find the most applicable ground relative to the given location.
		A ground location is a cell that is not occupied but above an occupied cell.
		If the specified cell is occupied, any applicable ground must be ab
		*/
		int				FindMostApplicableGround(int h, bool canStandOnWater) const;
	};

	typedef CellStack::Cell::Content Content;


	#ifdef VOXEL_SCANNER

	class LowCellStack
	{
	public:
		class Cell
		{
		public:
			bool	IsEmpty,	//indicates that there are no solid blocks intersecting this cell
					isFull;		//indicates that no clear blocks intersect this cell
			enum sensor_visibility_t
			{
				Unchecked,
				Visible,
				Invisible
			}		visibility;

		
		
			/**/	Cell():IsEmpty(true),isFull(false),visibility(Unchecked)	{}
		};
		Cell		volumeCell[CellStack::Range];		//All contained cells

			
	};

	extern Array<LowCellStack>	lowCellStack;

	#endif

	#ifdef HAS_WINDOW
		namespace Instruction
		{	
			enum displayMode_t
			{
				NormalRendering,
				ParaboloidColored,
				SphereColored
			};

			extern displayMode_t				displayMode;
			extern DX11::ObjectBuffer<float4>	ParaboloidDirection_Height,SphereDef,SphereColor;
			extern DX11::ObjectBuffer<float4>	ParaboloidColor_FlatRadius;
			extern DX11::ObjectBuffer<VecN<float,8> > atmosphereConfig;
			extern DX11::ObjectBuffer<float4> sunDirection,sunColor;


			struct THeightMapOrientation
			{
				TMatrix4<>	orientation0,
							orientation1,
							orientation2;
			};

			static_assert(sizeof(THeightMapOrientation)==sizeof(float)*16*3,"Sruct alignment out of wack");
			extern DX11::ObjectBuffer<THeightMapOrientation>	heightMapOrientation;	//used for lava
			extern DX11::Texture								cubeHeightMap;	//used for lava

			
			extern DX11::BaseInstructionBlock	terrainBase, terrainSphereBase, terrainParaboloidBase, 
												terrainShadowBase, 
												waterBase, waterSphereBase, waterParaboloidBase,
												overlayBase, overlayShadowBase, resourceBase;// , terrainEdgeBase;
		}
		struct TerrainVertex
		{
			float4		vertex;
			sbyte4		normal_structure;
			float4		color;

			/**/		TerrainVertex()	{};
			/**/		TerrainVertex(const float4&vtx, float h, const float3&n, const TLitColor&color, float structure) : vertex(vtx.xyz*h, vtx.w), normal_structure(n*127.f,structure*127.f) {color.CompileFinalColor(this->color);}
			/**/		TerrainVertex(const float4&vtx, float h, const float3&n, const float3&color, float structure) : vertex(vtx.xyz*h, vtx.w), normal_structure(n*127.f, structure*127.f), color(color,1.f){}
		};
	
		struct ResourceVertex //! Temporarily the same layout as TerrainVertex
		{
			float4		vertex;
			float4		normal;
			float4		color;

			/**/		ResourceVertex()	{};
			/**/		ResourceVertex(const float4&vtx, float h, const float3&n, const float4&color, float structure) : vertex(vtx.xyz*h, vtx.w), normal(n,structure), color(color){}
			/**/		ResourceVertex(const float4&vtx, float h, const float3&n, const float3&color, float structure) : vertex(vtx.xyz*h, vtx.w), normal(n, structure), color(color,1.f){}
			/**/		ResourceVertex(const float3&vtx, float h, const float3&n, const float4&color, float structure) : vertex(vtx*h, 1.f), normal(n,structure), color(color){}
			/**/		ResourceVertex(const float3&vtx, float h, const float3&n, const float3&color, float structure) : vertex(vtx*h, 1.f), normal(n, structure), color(color,1.f){}
		};

		struct OverlayVertex
		{
			float3		vertex;
			float		opacity;	//overlay is always black

			/**/		OverlayVertex()	{};
			/**/		OverlayVertex(const float4&vtx, float h, float opacity) : vertex(vtx.xyz*h), opacity(opacity){}
		};

		struct WaterVertex
		{
			float4		vertex;
			float3		normal;
			float		light;	//water derives color from shader variable

			/**/		WaterVertex()	{};
			/**/		WaterVertex(const float4&vtx, float h, const float3&n,float light, int emp) : vertex(vtx.xyz*h, vtx.w), normal(n),light((float(emp) + 0.05f + light*0.9f) * 0.25f ){}
		};
	

		template <typename Vtx>
			struct THull
			{
				Buffer<Vtx, 0>	vertices;
				Buffer<UINT32, 0>	indices;


				void			Clear()
				{
					vertices.Clear();
					indices.Clear();
				}

				void		AppendTo(THull<Vtx>&hull) const
				{
					UINT32 offset = (UINT32)hull.vertices.Count();
					hull.vertices.Append(vertices);
					const UINT32*iFrom = indices.pointer();
					UINT32*iTo = hull.indices.AppendRow(this->indices.Count());
					while (iFrom != this->indices.end())
					{
						(*iTo) = (*iFrom) + offset;
						iFrom++;
						iTo++;
					}
					DBG_ASSERT__(iTo == hull.indices.end());
				}
			};

		struct TTerrainHull : public THull<TerrainVertex>
		{
			DX11::InstructionBlock32	instruction[3],shadowInstruction;

			void						Destroy()
			{
				THull<TerrainVertex>::Clear();
				instruction[0] = instruction[1] = instruction[2]
				 = shadowInstruction = DX11::InstructionBlock32();
			}
		};
		struct TResourceHull : public THull<ResourceVertex>
		{
			DX11::InstructionBlock32	instruction,shadowInstruction;	// might cast shadow some day

			void						Destroy()
			{
				THull<ResourceVertex>::Clear();
				shadowInstruction = instruction = DX11::InstructionBlock32();
			}
		};
		struct TWaterHull : public THull<WaterVertex>
		{
			DX11::InstructionBlock32	instruction[3];

			void						Destroy()
			{
				THull<WaterVertex>::Clear();
				instruction[0] = instruction[1] = instruction[2]
					= DX11::InstructionBlock32();
			}
		};

		struct TOverlayHull : public THull<OverlayVertex>
		{
			DX11::InstructionBlock32	instruction,shadowInstruction;

			void						Destroy()
			{
				THull<OverlayVertex>::Clear();
				shadowInstruction = instruction = DX11::InstructionBlock32();
			}
		};
		/**
		Stack-stored mesh container with constant capacity
		*/
		template <typename Vtx, count_t maxVertices, count_t maxIndices>
			class StaticMesh
			{
			public:
				static const count_t MaxVertices = maxVertices;
				static const count_t MaxIndices = maxIndices;

				BYTE		vertexData[maxVertices * sizeof(Vtx)];	//prevent constructor execution
				Vtx			*vertexRange,
							*vat;
				UINT16		indexRange[maxIndices],
							*iat,
							offset;

				/**/		StaticMesh() :vertexRange((Vtx*)vertexData),vat((Vtx*)vertexData), iat(indexRange), offset(0)	{}
				/**/		StaticMesh(const StaticMesh<Vtx,maxVertices,maxIndices>&other)	{Reset();}	//nothing for now

				void		operator=(const StaticMesh<Vtx,maxVertices,maxIndices>&other)	{Reset();}	//nothing for now


				void		Reset()
				{
					vertexRange = ((Vtx*)vertexData);
					vat = ((Vtx*)vertexData);
					iat = (indexRange);
					offset = (0);
				}

				bool		IsEmpty() const {return indexRange == iat;}
				bool		IsNotEmpty() const {return indexRange != iat;}



				void		SetOffset()
							{
								offset = (UINT16)(vat - vertexRange);
							}

				StaticMesh&	Quad(int i0, int i1, int i2, int i3)
							{
								DBG_ASSERT__(iat - indexRange+6 <= maxIndices);
								(*iat) = UINT16(i0) + offset;iat++;
								(*iat) = UINT16(i1) + offset;iat++;
								(*iat) = UINT16(i2) + offset;iat++;
								(*iat) = UINT16(i0) + offset;iat++;
								(*iat) = UINT16(i2) + offset;iat++;
								(*iat) = UINT16(i3) + offset;iat++;
								return *this;
							}

				StaticMesh&	InvQuad(int i0, int i1, int i2, int i3)
							{
								DBG_ASSERT__(iat - indexRange+6 <= maxIndices);
								(*iat) = UINT16(i0) + offset;iat++;
								(*iat) = UINT16(i2) + offset;iat++;
								(*iat) = UINT16(i1) + offset;iat++;
								(*iat) = UINT16(i0) + offset;iat++;
								(*iat) = UINT16(i3) + offset;iat++;
								(*iat) = UINT16(i2) + offset;iat++;
								return *this;
							}
				StaticMesh&	operator<<(UINT16 index)
							{
								DBG_ASSERT__(iat - indexRange < maxIndices);
								(*iat) = index + offset;
								iat++;
								return *this;
							}
				StaticMesh&	operator<<(const Vtx&vtx)
							{
								DBG_ASSERT_EQUAL__((void*)vertexRange ,(void*)vertexData);
								DBG_ASSERT__(vat - vertexRange < maxVertices);
								(*vat) = vtx;
								vat++;
								return *this;
							}

				void		AppendTo(THull<Vtx>&hull) const
							{
								UINT32 offset = (UINT32)hull.vertices.Count();
								hull.vertices.Append(vertexRange, vat - vertexRange);
								const UINT16*iFrom = indexRange;
								UINT32*iTo = hull.indices.AppendRow(iat - indexRange);
								while (iFrom != iat)
								{
									(*iTo) = UINT32(*iFrom) + offset;
									//DBG_ASSERT__(*iTo < _vertices.Count());
									iFrom++;
									iTo++;
								}
								DBG_ASSERT__(iTo == hull.indices.end());
							}
			};


		template <typename Vtx>
			class DynamicMesh
			{
			public:
				Buffer<Vtx>	vertices;
				Buffer<UINT32>	indices;
				//std::vector<Vtx>	vertices;
				//std::vector<UINT32>	indices;
				UINT32		offset;

				/**/		DynamicMesh() :offset(0)	{}

				void		Clear()
				{
					vertices.clear();
					indices.clear();
					offset = 0;
				}

				bool		IsEmpty() const {return indices.IsEmpty();}
				bool		IsNotEmpty() const {return indices.IsNotEmpty();}



				void		SetOffset()
							{
								offset = (UINT32)vertices.size();
							}

				DynamicMesh&	Quad(int i0, int i1, int i2, int i3)
							{
								//indices.push_back(i0);
								//indices.push_back(i1);
								//indices.push_back(i2);
								//indices.push_back(i0);
								//indices.push_back(i2);
								//indices.push_back(i3);
								UINT32*iat = indices.AppendRow(6);
								(*iat) = UINT32(i0) + offset;iat++;
								(*iat) = UINT32(i1) + offset;iat++;
								(*iat) = UINT32(i2) + offset;iat++;
								(*iat) = UINT32(i0) + offset;iat++;
								(*iat) = UINT32(i2) + offset;iat++;
								(*iat) = UINT32(i3) + offset;iat++;
								return *this;
							}

				DynamicMesh&	operator<<(UINT32 index)
							{
								//indices << index;
								indices.push_back(offset + index);
								return *this;
							}
				DynamicMesh&	operator<<(const Vtx&vtx)
							{
								//vertices << vtx;
								vertices.push_back(vtx);
								return *this;
							}

				void		AppendTo(THull<Vtx>&hull) const
							{
								UINT32 offset = (UINT32)hull.vertices.Count();
								//foreach (vertices,v)
									//hull.vertices.push_back(*v);
								hull.vertices.Append(vertices);
								//const UINT32*iFrom = indices.pointer();
								auto iFrom = indices.begin();
								UINT32*iTo = hull.indices.AppendRow(indices.size());
								while (iFrom != indices.end())
								{
									(*iTo) = UINT32(*iFrom) + offset;
									//DBG_ASSERT__(*iTo < _vertices.Count());
									iFrom++;
									iTo++;
								}
								DBG_ASSERT__(iTo == hull.indices.end());
							}
			};
	#endif
	/**
	Homogenous geometrical section of the planet surface, to be rendered and updated as one chunk
	*/
	struct TFamily
	{
		struct TData
		{
			#ifdef HAS_WINDOW

				/**
				Various hulls used during rendering.
				*/
				TTerrainHull			terrain;	//!< high-resolution solid terrain, no transparency, shaded
				TOverlayHull			overlay;	//!< high-resolution overhanging terrain overlay, transparent
				TWaterHull				water;		//!< high-resolution overhanging terrain water, transparent, shaded
				TResourceHull			resources;	//!< low-resolution resource visualization. Similar to @a overlay, but uses different shader
			#endif 
			Box<>						boundingBox;
			Sphere<>					boundingSphere;
			bool						outdated,resourceOutdated;

			/**/						TData() :outdated(true),resourceOutdated(true)
			{}

			void						Destroy()
			{
				#ifdef HAS_WINDOW
					terrain.Destroy();
					overlay.Destroy();
					water.Destroy();
					resources.Destroy();
				#endif
			}
		};

		TData							normal;// , lower;
		float							outdatedAt;

		void							Destroy()
		{
			//	lower.Destroy();
			normal.Destroy();
		}


		void							SetOutdated(float atTime)
		{
			normal.outdated = normal.resourceOutdated = true;
			outdatedAt = atTime;
		}


	};



	#ifdef HAS_WINDOW

		class TerrainCompiler
		{
		public:
			static const count_t BatchSize = 16;	//the bigger, the less paralism, but the less the overhead during locking (since more work per locking)

			StaticMesh<TerrainVertex,BatchSize * CellStack::Range * (7 + 7 + 4 * 6),BatchSize * CellStack::Range * 3 * (7 * 2 + 2 * 7)>	solidMesh;
			StaticMesh<OverlayVertex,BatchSize * 7,BatchSize * 3*7>	coverMesh;
		

			const CellStack * stacks;
			const BaseGrid::TCell*  gridCells;
			const BaseGrid::TNetCell*  netCells;
			FClipHeight	clipHeight;
			bool	wasEmpty;
			TLitColor	cornerColor[6];


			void	Setup(const CellStack *stacks,const BaseGrid::TCell*cells, const BaseGrid::TNetCell*netCells,FClipHeight clipHeight)
			{
				solidMesh.Reset();
				coverMesh.Reset();
				this->stacks = stacks;
				this->netCells = netCells;
				this->gridCells = cells;
				this->clipHeight = clipHeight;
				wasEmpty = true;
			}


			TWeightedColor	GetMixColorU(index_t stackIndex,  UINT ih, Content oppositeContent, float&brightness, int&sumVisibilityTo, Content&outContent) const;
			TWeightedColor	GetDownMixColorU(index_t stackIndex,  UINT ih, Content oppositeContent, float&brightness, int&sumVisibilityTo, Content&outContent) const;
//			static void ApplyEMP(UINT16 cornerFlags,TVec4<>&rs);
			static bool FlagsAffectedByEMP(UINT16 cornerFlags) {return !(cornerFlags & empZonesUnlocked);}
			static bool AffectedByEMP(index_t stackIndex);


			void	Compile(const index_t offset, const index_t end);
			void	CompileLayer(const UINT ih, const CellStack&stack, const int clipH,const BaseGrid::TNetCell&cell,const BaseGrid::TCellNeighborhood&neighborhood);
			void	CompileBottomLayer(const CellStack&stack, const int clipH,const BaseGrid::TNetCell&cell,const BaseGrid::TCellNeighborhood&neighborhood);
			void	CompileAboveTerrainOutline(const UINT ih, const int ir,const BaseGrid::TNetCell&cell,const BaseGrid::TCellNeighborhood&neighbors);
			void	CompileCap(const UINT ih,const BaseGrid::TNetCell&cell, const bool capped, const CellStack::Cell&thisCell, const TFloatRange<>&range,const BaseGrid::TCellNeighborhood&neighbors, const CellStack&stack);
			void	CompileDownCap(const UINT ih,const BaseGrid::TNetCell&cell, const CellStack::Cell&thisCell, const int ir,const BaseGrid::TCellNeighborhood&neighbors, const CellStack&stack);
			void	CompileFaceToNeighbor(const int j, const UINT ih, const int ir,const BaseGrid::TNetCell&cell,const BaseGrid::TCellNeighborhood&neighbors,TLitColor newCornerColor[6], 
											const CellStack::Cell&thisCell, const CellStack::Cell&lastCell, const TFloatRange<>&range);
			void	DeliverResults(THull<TerrainVertex>&solidHull, THull<OverlayVertex>&coverHull);
		};


	
		class WaterCompiler
		{
		public:
			static const count_t BatchSize = TerrainCompiler::BatchSize;
			static const float4 waterColor;


			StaticMesh<WaterVertex, BatchSize * CellStack::Range * (7 + 7 + 4 * 6),BatchSize * CellStack::Range * 3 * (7 * 2 + 2 * 7)>	mesh;
		

			const CellStack * stacks;
			const BaseGrid::TCell*  gridCells;
			const BaseGrid::TNetCell*  netCells;
			float	cornerBrightness[6];
			FClipHeight	clipHeight;
			bool	wasEmpty;

			void	Setup(const CellStack *stacks,const BaseGrid::TCell*cells, const BaseGrid::TNetCell*netCells,FClipHeight clipHeight)
			{
				mesh.Reset();
				this->stacks = stacks;
				this->netCells = netCells;
				this->gridCells = cells;
				this->clipHeight = clipHeight;
			}


			void	Compile(const index_t offset, const index_t end, THull<WaterVertex>&, SpinLock&lock);
			void	CompileLayer(const index_t i, const UINT ih, const CellStack&stack, const int clipH,const BaseGrid::TNetCell&cell,
								const BaseGrid::TCellNeighborhood&neighborhood, bool localEMP);
			void	CompileFaceToNeighbor(const int j, const UINT ih, const int ir,const BaseGrid::TNetCell&cell,const BaseGrid::TCellNeighborhood&neighbors,float newCornerColor[6], 
										const CellStack::Cell&thisCell, const TFloatRange<>&range, bool localEMP);
			void	CompileBottomLayer(const CellStack&stack, const int clipH,const BaseGrid::TNetCell&cell,
										const BaseGrid::TCellNeighborhood&neighbors, bool empThis);
			void	CompileCap(const UINT ih,const BaseGrid::TNetCell&cell, const bool capped, const CellStack::Cell&thisCell, const TFloatRange<>&range,
										const BaseGrid::TCellNeighborhood&neighbors, const CellStack&stack, bool empThis);
		};

	#endif

	class Resource;


	template <unsigned Exponent>
	class Setup
	{
	public:
		typedef GridSetup<Exponent>	GridType;
		TFamily						family[BaseGrid::NumFamilies];
		GridType					grid;
		static Array<BaseGrid::TNetCell>	cells;
		Array<BaseGrid::TCellVertex>	cellVertices;


		#ifdef HAS_WINDOW
			DX11::InstructionBlock32	gridInstruction,
										transparentGridInstruction;
		#endif

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

		void		Destroy()
		{
			for (BYTE k = 0; k < BaseGrid::NumFamilies; k++)
				family[k].Destroy();
			#ifdef HAS_WINDOW
				gridInstruction.Clear();
				transparentGridInstruction.Clear();
			#endif
		}


		Box<>		unitSphereFamilyBoundingBox[BaseGrid::NumFamilies];
		Box<>		planetFamilyBoundingBox[BaseGrid::NumFamilies];

		void		Create(bool keepVerticesAndLoadGrid)
		{
			grid.Create();
			grid.Construct(cellVertices, cells);

			for (index_t i = 0; i < ARRAYSIZE(unitSphereFamilyBoundingBox); i++)
				unitSphereFamilyBoundingBox[i] = planetFamilyBoundingBox[i] =  Box<>::Invalid();

			float r0 = ToRadius(-CellStack::HalfRange);
			float r1 = ToRadius(CellStack::HalfRange);
			foreach (cells,c)
			{
				Box<>&box = planetFamilyBoundingBox[grid.GetCell(c - cells.pointer()).family];
				box.Include(c->centerVertex.xyz * r0);
				box.Include(c->centerVertex.xyz * r1);
				for (int i = 0; i < c->numCorners; i++)
				{
					box.Include(c->cornerVertex[i].xyz*r0);
					box.Include(c->cornerVertex[i].xyz*r1);
				}
			}
			foreach (cells,c)
			{
				Box<>&box = unitSphereFamilyBoundingBox[grid.GetCell(c - cells.pointer()).family];
				box.Include(c->centerVertex.xyz);
				for (int i = 0; i < c->numCorners; i++)
				{
					box.Include(c->cornerVertex[i].xyz);
				}
			}

			#ifdef HAS_WINDOW
				if (keepVerticesAndLoadGrid)
				{
					Array<float4> verticesToLoad(cellVertices.Count());
					for (index_t i = 0; i < cellVertices.Count(); i++)
						verticesToLoad[i] = float4(cellVertices[i].centerDirection,float(cellVertices[i].ownerCount > 1));
					gridInstruction.vbo.CreateFromStructs(verticesToLoad);
					{
						Buffer<UINT32>	ibo;
						foreach(cells, cell)
						{
							cell->voffset = (UINT32)ibo.Count();
							for (int j = 0; j < cell->numCorners; j++)
								ibo << cell->centerVertexId << cell->cornerVertexId[j] << cell->cornerVertexId[(j + 1) % cell->numCorners];
						}
						gridInstruction.ibo.Create(ibo);
					}

					gridInstruction.shader = gridShader;
					{
						Engine::VertexBinding	binding;
						binding.vertex.set(0, 4);
						binding.normal.unset();
						binding.floats_per_vertex = 4;
						gridInstruction.binding = binding;
					}
					gridInstruction.UpdateLayout();
					gridInstruction.SetTriangles();


					transparentGridInstruction = gridInstruction;
					transparentGridInstruction.shader = transparentGridShader;
					transparentGridInstruction.UpdateLayout();
				}
			#endif

			if (!keepVerticesAndLoadGrid)
				cellVertices.Clear();
			//blockInstruction = gridInstruction;
			//blockInstruction.shader = blockShader;
			//blockInstruction.UpdateLayout();

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

	


		void	GenerateVolume(const Sphere<>&sphere,TVolumeDescriptor&volume) const
		{
			static TVolumeDescriptor::Operator op;
			volume.Clear();
			UINT32 centerStack = Locate(sphere.center);
			op.stacksLeft << centerStack;

			const float br = ToRadius(CellStack::BottomHeight);
			const float tr = ToRadius(CellStack::TopHeight);
			UINT32 next;
			while (op.stacksLeft >> next)
			{
				if (op.stackVisited[next])
					continue;
				op.stackVisited[next] = true;
				const TVec3<>&dir = cells[next].cornerCenter;
				TVec2<> distances;
				TVec3<> bottom,top;
				Vec::mult(dir,br,bottom);
				Vec::mult(dir,tr,top);
				TVolumeDescriptor::TStack&st = volume.coverage.Append();
				st.stackID = next;
				if (!Obj::DetectSphereEdgeIntersection(sphere.center,sphere.radius,bottom,top,distances))
				{
					st.range.SetEmpty(0);
					continue;
				}
			
				st.range.start = (UINT32)floor(std::max(distances.x,0.f) * (CellStack::Range-1));
				st.range.end = 1 + (UINT32)ceil(std::min(distances.y,1.f) * (CellStack::Range-1));
				const auto&n = grid.GetCell(next).neighborhood;
				for (int i = 0; i < n.numNeighbors; i++)
					op.stacksLeft << n.neighborID[i];
			}
			op.ResetVisited(volume);
		}

		#ifdef HAS_WINDOW
			void		Compile(const Array<CellStack>&stacks, THull<TerrainVertex>&solid, THull<OverlayVertex>&cover, index_t offset, count_t count, FClipHeight clipHeight)
			{
				const index_t end = offset + count;
				//SpinLock lock;
				const count_t numBatches = CeilDiv(count,TerrainCompiler::BatchSize);
				static Array<THull<TerrainVertex> >	hulls;
				static Array<THull<OverlayVertex> >	covers;
				count_t numWorkers = System::ParallelLoop::global_instance.getWorkerCount();
				if (hulls.Count() < numWorkers)
				{
					hulls.SetSize(numWorkers);
					covers.SetSize(numWorkers);
				}
				
				


				System::parallelFor(numBatches,8,[this, end,offset, &stacks, clipHeight](index_t i, index_t workerIndex)
				{
					BYTE data[sizeof(TerrainCompiler)];	//prevent implicit memset in release mode. i'm serious. visual studio has some funny ideas there...
					TerrainCompiler&compiler = *(TerrainCompiler*)data;
					compiler.Setup(stacks.pointer(),grid.GetCells(),this->cells.pointer(),clipHeight);
					compiler.Compile(offset + i*TerrainCompiler::BatchSize,end);
					compiler.DeliverResults(hulls[workerIndex],covers[workerIndex]);
				});
				for (index_t i = 0; i < numWorkers; i++)
				{
					hulls[i].AppendTo(solid);
					covers[i].AppendTo(cover);
					hulls[i].Clear();
					covers[i].Clear();
				}

			}



			void		CompileWater(const Array<CellStack>&stacks, THull<WaterVertex>&cover, index_t offset, count_t count, FClipHeight clipHeight)
			{
				const index_t end = offset + count;
				SpinLock lock;
				Concurrency::parallel_for(offset, end, WaterCompiler::BatchSize,[this, end,&lock, &cover, &stacks, clipHeight](index_t i)	//looks good - at least in release
				{
					BYTE data[sizeof(WaterCompiler)];	//prevent implicit memset in release mode. i'm serious. visual studio has some funny ideas there...
					WaterCompiler&compiler = *(WaterCompiler*)data;
					compiler.Setup(stacks.pointer(),grid.GetCells(),this->cells.pointer(),clipHeight);
					compiler.Compile(i,end,cover,lock);
				});
			}





			void		BakeBlockInstruction(const Array<CellStack>&stacks, BYTE family, FClipHeight clipHeight)
			{
				//if (!section)
				//{
				//	maxHeight = CellStack::HalfRange;
				//	maxDistance = 0xFFFFFFFF;
				//}
				TFamily &fam = this->family[family];
				TFamily::TData&dat = fam.normal;

				dat.terrain.Clear();
				dat.overlay.Clear();
				dat.water.Clear();
				Compile(stacks, dat.terrain, dat.overlay, grid.GetFamilyOffset(family), grid.CountFamilyMembers(family), clipHeight);
				CompileWater(stacks, dat.water, grid.GetFamilyOffset(family), grid.CountFamilyMembers(family), clipHeight);

				Box<> box = Box<>::Invalid();

				foreach(dat.terrain.vertices, vtx)
					box.Include(vtx->vertex.xyz);
				foreach(dat.water.vertices, vtx)
					box.Include(vtx->vertex.xyz);
				dat.boundingBox = box;
				dat.boundingSphere.center = box.GetCenter();
				dat.boundingSphere.radius = 0.f;
				foreach(dat.terrain.vertices, vtx)
					dat.boundingSphere.IncludeSquare(vtx->vertex.xyz);
				dat.boundingSphere.radius = sqrt(dat.boundingSphere.radius);


				if (dat.terrain.instruction[0].shader.IsEmpty())
				{
					(DX11::BaseInstructionBlock&)dat.terrain.shadowInstruction = Instruction::terrainShadowBase;
					(DX11::BaseInstructionBlock&)dat.terrain.instruction[0] = Instruction::terrainBase;
					(DX11::BaseInstructionBlock&)dat.terrain.instruction[1] = Instruction::terrainParaboloidBase;
					(DX11::BaseInstructionBlock&)dat.terrain.instruction[2] = Instruction::terrainSphereBase;

					(DX11::BaseInstructionBlock&)dat.overlay.instruction = Instruction::overlayBase;
					(DX11::BaseInstructionBlock&)dat.overlay.shadowInstruction = Instruction::overlayShadowBase;
					(DX11::BaseInstructionBlock&)dat.water.instruction[0] = Instruction::waterBase;
					(DX11::BaseInstructionBlock&)dat.water.instruction[1] = Instruction::waterParaboloidBase;
					(DX11::BaseInstructionBlock&)dat.water.instruction[2] = Instruction::waterSphereBase;
				}


				dat.terrain.instruction[0].vbo.CreateFromStructs(dat.terrain.vertices);
				dat.terrain.instruction[0].ibo.Create(dat.terrain.indices);
				dat.terrain.shadowInstruction.vbo = dat.terrain.instruction[0].vbo;
				dat.terrain.shadowInstruction.ibo = dat.terrain.instruction[0].ibo;
				dat.terrain.instruction[1].vbo = dat.terrain.instruction[0].vbo;
				dat.terrain.instruction[1].ibo = dat.terrain.instruction[0].ibo;
				dat.terrain.instruction[2].vbo = dat.terrain.instruction[0].vbo;
				dat.terrain.instruction[2].ibo = dat.terrain.instruction[0].ibo;
				dat.overlay.instruction.vbo.CreateFromStructs(dat.overlay.vertices);
				dat.overlay.instruction.ibo.Create(dat.overlay.indices);
				dat.overlay.shadowInstruction.vbo = dat.overlay.instruction.vbo;
				dat.overlay.shadowInstruction.ibo = dat.overlay.instruction.ibo;
				dat.water.instruction[0].vbo.CreateFromStructs(dat.water.vertices);
				dat.water.instruction[0].ibo.Create(dat.water.indices);
				dat.water.instruction[1].vbo = dat.water.instruction[0].vbo;
				dat.water.instruction[1].ibo = dat.water.instruction[0].ibo;
				dat.water.instruction[2].vbo = dat.water.instruction[0].vbo;
				dat.water.instruction[2].ibo = dat.water.instruction[0].ibo;
			}

			void		BakeResourceInstruction(BYTE family, FClipHeight clipHeight,const Resource*resources, count_t numResources)
			{
			//	return;
				TFamily &fam = this->family[family];
				TFamily::TData&dat = fam.normal;

				dat.resources.Clear();
				actualWorld.CompileResources(dat.resources, grid.GetFamilyOffset(family), grid.CountFamilyMembers(family), clipHeight, resources, numResources);

				Box<> box = Box<>::Invalid();

				foreach(dat.resources.vertices, vtx)
					box.Include(vtx->vertex.xyz);
				dat.boundingBox = box;
				dat.boundingSphere.center = box.GetCenter();
				dat.boundingSphere.radius = 0.f;
				foreach(dat.resources.vertices, vtx)
					dat.boundingSphere.IncludeSquare(vtx->vertex.xyz);
				dat.boundingSphere.radius = sqrt(dat.boundingSphere.radius);


				if (dat.resources.instruction.shader.IsEmpty())
				{
					(DX11::BaseInstructionBlock&)dat.resources.instruction = Instruction::resourceBase;
				}

				dat.resources.instruction.vbo.CreateFromStructs(dat.resources.vertices);
				dat.resources.instruction.ibo.Create(dat.resources.indices);
			}

			void		BakeLineInstruction(const Array<CellStack>&stacks, BYTE family, int minHeight)
			{
				if (!section)
					minHeight = -CellStack::HalfRange;
				TFamily &fam = this->family[family];
				TFamily::TData&dat = fam.GetData(section);

				dat.edgeVertices.Clear();
				dat.edgeIndices.Clear();
				CompileEdges(stacks, dat.edgeVertices, dat.edgeIndices, grid.GetFamilyOffset(family), grid.CountFamilyMembers(family), minHeight);

				if (dat.edgeInstruction.shader.IsEmpty())
				{
					(DX11::BaseInstructionBlock&)dat.edgeInstruction = terrainEdgeBase;
					dat.edgeInstruction.SetLines();
				}


				dat.edgeInstruction.vbo.CreateFromStructs(dat.edgeVertices);
				dat.edgeInstruction.ibo.Create(dat.edgeIndices);
			}


			void		RenderCell(index_t stackID)
			{
				display.Render(gridInstruction, cells[stackID].voffset, cells[stackID].numCorners * 3);
			}
			/**
			Second-style version of RenderCell().
			Tran

			*/
			void		RenderTransparentCell(index_t stackID)
			{
				display.Render(transparentGridInstruction, cells[stackID].voffset, cells[stackID].numCorners * 3);
			}

			void RenderBlock(index_t cellID, float h0, float h1, const float4&color)
			{
				TMatrix4<> config;
				memset(&config, 0, sizeof(config));
				config.x.x = h0;
				config.x.y = h1;
				config.z = color;
				Mat::transpose(config);
				blockConfig.Update(config);
				display.Render(blockInstruction, cells[stackID].voffset, cells[stackID].numCorners * 3);
			}

			static const count_t HullTestBatch = 16;
			bool	TestHullIntersection(const TVec3<>&b, const TVec3<>&d, float&distance, TVec3<>&outNormal, bool includeWater) const
			{
				bool rs = false;
				SpinLock lock;
				for (BYTE k = 0; k < BaseGrid::NumFamilies; k++)
				{
					const TFamily&fam = family[k];
					float dst = distance;
					const TFamily::TData&dat = fam.normal;
					if (_oDetectOpticalBoxIntersection(dat.boundingBox, b, d, dst))
					{
						if (includeWater)
						{
							const auto*indices = dat.water.indices.pointer();

							Concurrency::parallel_for(index_t(0), dat.water.indices.Count(), index_t(3) * HullTestBatch, [this, &lock, &distance, b, d, &rs, &dat, indices, &outNormal](index_t _i)
								//for (index_t i = 0; i < hullIndices.Count(); i += 3)
							{
								float dist = distance;
								bool any = false;
								TVec3<> normal;
								for (index_t i = _i; i < _i + HullTestBatch*3 && i < dat.water.indices.Count(); i+=3)
								{
									if (Vec::dot(dat.water.vertices[indices[i]].normal,d) > 0.f)
										continue;
									if (Obj::detectOpticalIntersection(
										dat.water.vertices[indices[i]].vertex.xyz,
										dat.water.vertices[indices[i + 1]].vertex.xyz,
										dat.water.vertices[indices[i + 2]].vertex.xyz,
										b, d, dist))
									{
										any = true;
										normal = dat.water.vertices[indices[i]].normal;	//correct enough
									}
								}
								if (any)
								{
									lock.lock();
									if (dist < distance)
									{
										rs = true;
										distance = dist;
										outNormal = normal;	//correct enough
									}
									lock.unlock();
								}

							});
						}

						{
							const auto*indices = dat.terrain.indices.pointer();

							Concurrency::parallel_for(index_t(0), dat.terrain.indices.Count(), index_t(3) * HullTestBatch, [this, &lock, &distance, b, d, &rs, &dat, indices, &outNormal](index_t _i)
								//for (index_t i = 0; i < hullIndices.Count(); i += 3)
							{
								float dist = distance;
								bool any = false;
								TVec3<> normal;
								for (index_t i = _i; i < _i + HullTestBatch*3 && i < dat.terrain.indices.Count(); i+=3)
								{
									if (Vec::dot(dat.terrain.vertices[indices[i]].normal_structure.xyz,d) > 0.f)
										continue;
									if (Obj::detectOpticalIntersection(
										dat.terrain.vertices[indices[i]].vertex.xyz,
										dat.terrain.vertices[indices[i + 1]].vertex.xyz,
										dat.terrain.vertices[indices[i + 2]].vertex.xyz,
										b, d, dist))
									{
										any = true;
										Vec::copy(dat.terrain.vertices[indices[i]].normal_structure.xyz,normal);	//correct enough
										Vec::div(normal,127.f);
									}
								}
								if (any)
								{
									lock.lock();
									if (dist < distance)
									{
										rs = true;
										distance = dist;
										outNormal = normal;	//correct enough
									}
									lock.unlock();
								}

							});
						}
					}
				}
				return rs;
			}

			index_t	TestGridIntersection(TVec3<> b, TVec3<> d, float radius, float&distance) const
			{
				Vec::div(b,radius);
				Vec::div(d,radius);

				index_t rs = InvalidIndex;

				for (BYTE f = 0; f < BaseGrid::NumFamilies; f++)
				{
					float dist = distance;
					if (unitSphereFamilyBoundingBox[f].Contains(b) || _oDetectOpticalBoxIntersection(unitSphereFamilyBoundingBox[f],b,d,dist))
					{

						const index_t begin = grid.GetFamilyOffset(f);
						const count_t cnt = grid.CountFamilyMembers(f),
										end = begin+cnt;
						for (index_t i = begin; i < end; i++)
						{
							const auto&c = cells[i];
							for (int j = 0; j < c.numCorners; j++)
							{
								if (Obj::detectOpticalIntersection(c.centerVertex.xyz, c.cornerVertex[j].xyz,
									c.cornerVertex[(j + 1) % c.numCorners].xyz, b, d, distance))
									rs = i;
							}
				
						}
					}			
				}
	/*
				for (index_t i = 0; i < grid.CellCount; i++)
				{
					const auto&c = cells[i];
					for (int j = 0; j < c.numCorners; j++)
					{
						if (Obj::detectOpticalIntersection(c.centerVertex.xyz, c.cornerVertex[j].xyz,
							c.cornerVertex[(j + 1) % c.numCorners].xyz, b, d, distance))
							rs = i;
					}

				}*/
				return rs;
			}

			void RenderGrid(float radius)
			{
				TMatrix4<> matrix = Matrix<>::eye4;
				matrix.x.xyz *= radius;
				matrix.y.xyz *= radius;
				matrix.z.xyz *= radius;

				display.SetObjectMatrix(matrix);
				display.Render(gridInstruction);
			}
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

	
	class Resource
	{
		String					name;
		ConstName			cname;
		float3					color;

	public:
		typedef BYTE			coverage_t;

		static const count_t	NumEntries = LowSetup::GridType::CellCount;

		coverage_t				coverage[NumEntries];
		#ifdef CLIENT
			bool				known[NumEntries];	//for now these must be in sync. resourceA.know[i] == resourceB.known[i] : always
		#endif

		/**
		Computes the coverage for a given location
		*/
		static coverage_t		GetRandomCoverage(const SimplexNoise&n, const float3&direction, float p, coverage_t max, float scale);

		static float			probability[],density[];
		static coverage_t		maximum[];

		static Buffer0<Resource>all;
		static void				Create(const String&name, ConstName cname, const float3&color, index_t);
		static Resource&		Require(const String&name);
		static Resource&		Require(ConstName name);
		static coverage_t		Sample(const String&resourceName, index_t cellID);
		static coverage_t		Sample(ConstName resourceName, index_t cellID);

		index_t					resourceIndex;

		/**/					Resource()
		{
			#ifdef CLIENT
				memset(known,/*false*/true,sizeof(known));
			#endif
		}
		void					Name(const String&name, ConstName cname, const float3&color)	{this->name = name; this->cname = cname; this->color = color;}
		
		void					Fill(Random&random, float p, coverage_t, float density);
		void					Fill(Random&random);

		static void				FillAll(Random&random);

		const String&			GetName() const {return name;}
		ConstName				GetCName() const {return cname;}
		const float3&			GetColor() const {return color;}
	};

	#ifdef HAS_WINDOW
		class ResourceCompiler
		{
		public:
			static const count_t Resolution = 10;
			static const count_t BatchSize = TerrainCompiler::BatchSize;
			StaticMesh<ResourceVertex,BatchSize * 2 * 2 * (1 + Resolution + Resolution*2 ),BatchSize * 3 * 2* 2*(Resolution + 2*Resolution)>	mesh;
		
			const CellStack * stacks;
			const BaseGrid::TCell*  lowGridCells, *highGridCells;
			const BaseGrid::TNetCell*  lowNetCells;
			const Resource*resources;
			count_t			numResources;
			FClipHeight	clipHeight;

			static float3 GetColor(float level)	{return float3(std::min(1.f,level*8.f),std::min(1.f,level*3.f), std::min(1.f,level)) * 0.7f + 0.3f;}
			static float3 GetColor(float level, const float3&flavor)
			{
				return flavor;//*0.75f + 0.5f*(std::min(1.f,level));
				//return flavor*0.5f + (1.2f * std::min(1.f,level) + 0.3f);
				// & (GetColor(level)*0.5f + 0.5f);
				//return flavor * 0.75 + GetColor(level) * 0.25f;
				//return 0.3f + 0.7f * std::min(1.f,level*8.f);
				//return float3(std::min(1.f,level*8.f),std::min(1.f,level*3.f), std::min(1.f,level)) * 0.7f + 0.3f;
			}
			static float GetExtend(float level) {return level/3.f;}
	
			void	Setup(const CellStack *stacks,const BaseGrid::TCell*lowGridCells, const BaseGrid::TCell*highGridCells,  const BaseGrid::TNetCell*lowNetCells,FClipHeight clipHeight,const Resource*resources, count_t numResources)
			{
				mesh.Reset();
				this->stacks = stacks;
				this->lowNetCells = lowNetCells;
				this->highGridCells = highGridCells;
				this->lowGridCells = lowGridCells;
				this->clipHeight = clipHeight;
				this->resources = resources;
				this->numResources = numResources;
			}

			void	Compile(const index_t offset, const index_t end, THull<ResourceVertex>&, SpinLock&lock);
		};
	#endif



	


	#ifdef HAS_WINDOW
	namespace SeeThrough
	{

		/**
		Updates see-through to the specified settings. Do not alter related values manually
		@param enable Enable see-through. Set false to disable see-through
		*/
		void	Update(bool enable, const Grid::LCellIdI&focusPoint);

		/**
		Queries whether see-through is currently enabled
		*/
		bool	IsEnabled();

		/**
		Queries the current focus point of the see-through clipping
		*/
		const Grid::LCellIdI&	GetFocusPoint();

		/**
		Retrieves the low-resolution cell id closest to the current camera focus point for use to update see-through.
		If the current camera location is within proximity of the current cut-off focus cell, then the focus cell is returned instead.
		The returned height is set to the current focus height and may be ignored if not needed.
		*/
		Grid::LCellIdI		LocateCamera();



		/**
		Clipping function for see-through. Returns the int height (not unsigned layer) at which (and above) clipping occurs
		*/
		int		ClipHeight(index_t cellID);


	}

	/**
	Updates plantary parameters to be used prior to rendering flat grid components (RenderNeighborhood(), low/high.RenderCell()). Does not affect other rendering operations
	*/
	void	UpdatePlanetRenderingParameters(float radius);
	#endif

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

	inline HCellIdI	ToHigh(const LCellIdI&cellID)	{return HCellIdI(low.grid.GetCell(cellID.stackID).higherEquivalent,cellID.height); }
	inline HCellIdU	ToHigh(const LCellIdU&cellID)	{return HCellIdU(low.grid.GetCell(cellID.stackID).higherEquivalent,cellID.layer); }


	#pragma pack(push)
	#pragma pack(1)
	struct TCellResourceCoverage
	{
		UINT32		cellIndex;
		BYTE		coverage;
	};
	#pragma pack(pop)

	void			FlushStackFamily(Array<CellStack>&cellStack,  BYTE family);
	void			FlushAllData();
	void			ResetStructureCounters(bool resetVisibility=true);

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





	class SkySensorState
	{
	public:
		struct TCell
		{
			UINT32				hStackID;	//high-resolution cell IDs
			int					downToHeight;
		};

		static const int Uninitialized = BadHeight;

		int						groundPenetration=0;

		Buffer0<TCell>			coverage;
		IndexTable<int>			coverageMap;	//maps hstackID to downToHeight

		void					Clear()
		{
			coverage.Clear();
			coverageMap.Clear();
			groundPenetration = 0;
		}
	};

	class VisibilityState
	{
	private:
		count_t		lockLevel = 0;
	public:
		struct CellStack
		{
			struct Cell
			{
				short	visibility;
				bool	oldVisible, temporarilyVisible;

				#ifdef HAS_WINDOW
					TVisibility	GetVisibility() const {return TVisibility(visibility>0);}
				#endif

				bool	Changed(bool reset)
				{
					bool visible = temporarilyVisible || visibility > 0;
					bool before = oldVisible;
					if (reset)
						oldVisible = visibility > 0;	//dont include temp here
					return visible != before;
				}

				void	AlterVisibility(short change)
				{
					visibility += change;
					if (change >= 0)
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
			};
			Cell	volumeCell[Grid::CellStack::Range];

			/**/	CellStack()
			{
				memset(volumeCell, 0, sizeof(volumeCell));
			}

			void	Reset()
			{
				memset(volumeCell, 0, sizeof(volumeCell));
			}
		};

		struct FamilyState
		{
			float			lastChange;
			bool			coverageChanged, changed;

			/**/			FamilyState() :lastChange(0), coverageChanged(false), changed(false)
			{}
			void			Reset()
			{
				lastChange = 0;
				coverageChanged = false;
				changed = false;
			}

		};
		FamilyState			families[BaseGrid::NumFamilies];

		Array<bool >		resourcesDiscovered;//discovers all resources (for now)


		Array<CellStack>	cellStack;


		/**/				VisibilityState():cellStack(Grid::HighSetup::GridType::CellCount)
		{
			resourcesDiscovered.SetSize(Grid::Resource::NumEntries);
			resourcesDiscovered.Fill(false);
		}
		/**
		Affects the cell and its immediate neighbors only
		*/
		void				ChangeSensory(const Grid::HCellIdU&center, float t, short changeBy);
//		void				ChangeSensory(const Grid::LCellIdI&center, float radius, float t, short changeBy);
		void				ChangeSensory(const Grid::HighSetup::TVolumeDescriptor&desc, float t, short changeBy);
		void				ChangeSensory(const Grid::SkySensorState&state, float t, short changeBy);
		void				RegisterExplosionCrater(const Grid::HCellIdI&center, float radius, float t);

		/**
		Completelye resets the internal state
		@param includeResources Also flush resource coverage knowledge. If set false, only terrain visibility is reset
		*/
		void				FlushAllData(bool includeResources=true);
		/**
		Prevents all calls to CheckChanges() until UnlockChangeChecking() is called.
		Supports nested calls up to depth 16
		*/
		void				LockChangeChecking()	{ASSERT_LESS__(lockLevel,16);lockLevel++; }
		/**
		Unlocks calls to CheckChanges(). Calls CheckChanges() automatically, if the lock level falls back to zero
		*/
		void				UnlockChangeChecking(float t)	{ASSERT_GREATER__(lockLevel,0); if (--lockLevel == 0) CheckChanges(t);}

		/**
		Processes all families for which FamilyState::changed is true, and detects if the visible terrain
		coverage actually changed. If so, the respective value FamilyState::coverageChange is set true,
		and FamilyState::lastChange is set to the specified parameter t.
		Finally, FamilyState::changed is set false for all families.
		*/
		void				CheckChanges(float t);


		/**
		Detects resource coverage changes within the specified sphere, and updates @a resourcesDiscovered where necessary.
		Uses some static fields. Do not call in parallel.
		@param center Terrain center to detect resources from
		@param range Sphere radius to detect resources within
		@param[inout] outNewDiscoveries Buffer to add newly discovered cell stacks to. Cleared automatically
		*/
		void				UpdateResourceCoverage(const Grid::LCellIdI&center,float range, BasicBuffer<UINT32>&outNewDiscoveries);


	};

	
	class WorldData
	{
	private:
		static short			ResourceDistance(const Sphere<>&resourceSphere, const TVec3<>&cellCenter);
	public:
		Array<CellStack>		cellStack;
		#ifdef HAS_WINDOW
			bool				lightingOutdated = true;
			bool				structureChanged = true;
		#endif

		/**/					WorldData();

		void					swap(WorldData&other)
		{
			cellStack.swap(other.cellStack);
			#ifdef HAS_WINDOW
				swp(lightingOutdated,other.lightingOutdated);
				swp(structureChanged,other.structureChanged);
			#endif
		}
		/**
		Checks if the specified low-resolution cell is not at all covered (not even partially)
		*/
		bool					LowCellIsClear(const LCellIdI&cell)const;
		String					LowCellReport(const LCellIdI&cell)const;
		bool					LowCellIsClearOrWater(const LCellIdI&cell)const;
		bool					LowCellHasStructure(const LCellIdI&cell)const;
		bool					LowCellIsClearIgnoreStructure(const LCellIdI&cell)const;
		/**
		Determines the weakest and strongest content types found in the specified low-resolution cell
		@param includeUnknown Consider unknown blocks for outWeakest and outStrongest. If set false, outStrongest and outWeakest may still end up being unknown, if all are unknown
		@return true, if any Content::Unknown blocks were encountered, false otherwise
		*/
		bool					GetStrengthRange(const LCellIdI&cell, Content&outWeakest, Content&outStrongest, bool includeUnknown) const;

		bool					IsRoad(const LCellIdI&, bool ignoreClearCellsOnEdge) const;

		void					FlushAllData()
		{
			for (index_t i = 0; i < cellStack.Count(); i++)
			{
				cellStack[i].ZeroInit();
			}
		}
		void					ResetStructureCounters(bool resetVisibility)
		{
			for (index_t i = 0; i < cellStack.Count(); i++)
			{
				cellStack[i].ResetStructureCounters(resetVisibility);
			}
		}
		void					ApplyFamilyDelta(BYTE family, const Array<CellStack::TTypeDelta>&data0, bool initialization);
		void					ApplyFamilyDelta(BYTE family, const Array<CellStack::TTypeDeltaBlock>&data0, bool initialization, bool actualWorld);
		//void					ApplyFamilyDelta(Array<CellStack>&stacks, BYTE family, const Array<CellStack::TTypeDeltaBlock>&data0, const Array<CellStack::TStructCountDeltaBlock>&data1, bool initialization);

		/**
			Signals that the fine geometry in and on the edge of the specified cell has changed.
			This and horizontally neighboring cells will be updated (TODO)
		*/
		void					SignalChange(const LCellIdI&)	{}
		#ifdef HYBRID_GRID
			void				SignalChange(const HCellIdI&)	{}
		#endif
		/**
			Executes SignalChange() on all low cells up to and including the specified height
		*/
		void					SignalChangeUpTo(const LCellIdI&c)
		{
			//for (int h = -CellStack::HalfRange; h <= c.height; h++)	//no point now
				//SignalChange(LCellIdI(c.cellID,h));
		}

		//these manipulations should only be called on Grid::actualWorld, since they alter the outdated state of global families
		/**
		@brief Clears all high resolution cells intersecting the specified low resolution cell
		@return true, if any matter was removed, false otherwise
		*/
		bool					Clear(const LCellIdI&ch, float atTime);
		/**
		Similar to Clear() but fails if lava is attempted to remove
		@return true, if no lava was hit, false otherwise
		*/
		bool					ClearNonLava(const LCellIdI&ch, float atTime);
		bool					Overwrite(const LCellIdI&ch, Content with,  float atTime);
		void					Explode(const HCellIdI&,float radius, float atTime);
		void					Explode(const HCellIdI&id,float radius, bool*familyChanged);

		/**
		Redefines the world volume, based on the specified world configuration and discovery setting
		MUST NOT be used on the actual world while a session with active factions/players
		*/
		void					BuildWorld(const ExtendedPlanetConfig&config);
		/**
		Copies the volumetric composition of the local world data to the specified target according to the specified
		world discovery value.
		@param[out] target Target container to hold the filtered world data
		@param discoveryLevel Level of terrain obfuscation to apply to the copy. If set to FullTerrain, then @a target will contain an exact copy of the local world data.
		*/
		void					ExportDiscovered(WorldData&target, WorldDiscovery discoveryLevel) const;

		int						TraceSkySensor(UINT32 highCellCenter, int penetration) const;

		/**
		Creates a blanket sensor state coverage. Actual height is not mapped, all @a downToHeight values will be set to SkySensorState::Uninitialized
		@param lowCellCenter Sensor center cell in low-resolution cell space
		@param radius Horizontal scanning radius (at height= @a refHeight ) that defines the corridor
		@param refHeight Reference height that the scanning radius is as defined
		@param groundPenetration Number of cells that the sensor can penetrate. 0 means only the top non-clear cell will be revealed. Water counts 1/2 (1 can scan 2+1 cells into the ocean)
		@param[out] outState Resulting state. @a state.coverage will be initialized with all @a downToHeight values set to SkySensorState::Uninitialized. @a state.coverageMap will be initialized analogously
		*/
		void					MapSensorState(UINT32 lowCellCenter, float radius, int refHeight, int groundPenetration, SkySensorState&outState) const;
		/**
		Updates all @a downToHeight values and @a state.coverageMap to reflect the current terrain situation.
		MapSensorState() must be called on the given state at least once before calling UpdateSensorState()
		@param state Sensor state to update. Only the current coverage of the given state will be checked.
		@return true, if at least one @a downToHeight value changed, false otherwise
		*/
		bool					UpdateSensorState(SkySensorState&state) const;


		#ifdef HAS_WINDOW
			/**
			Updates the visibility of all cells to the specified value.
			The previous value is discarded.
			*/
			void				OverrideVisibility(short v);
			/**
			Retrieves the average light-level of all high-resolution cells touched by the specified low-resolution cell
			*/
			float				GetAverageLightLevel(const LCellIdI&) const;

			/**
			Retrieves the highest light-level among all high-resolution cells touched by the specified low-resolution cell
			*/
			float				GetHighestLightLevel(const LCellIdI&id) const;


			void				UpdateLighting();	//updates local and neighbor stack lighting
			void				CompileResources(THull<ResourceVertex>&target, index_t cellOffset, count_t cellCount, FClipHeight clipHeight,const Resource*resources, count_t numResources) const;
			#ifdef HYBRID_GRID
				void			SignalLightingChange(const HCellIdI&cellID);
			#endif
			void				SignalLightingChange(const LCellIdI&cellID);


			#if 0
			void				ClearResourceDistanceField();
			/**
			Paints a sphere into the resource distance field, 2 units larger than the specified sphere.
			This method uses static instances of classes and must not be called in parallel
			*/
			void				PaintResourceDistanceField(const Sphere<>&resourceSphere);

			/**
			Detailed version of the above. This version may be called in parallel
			*/
			void				PaintResourceDistanceField(const HighSetup::TVolumeDescriptor&desc, const Sphere<>&resourceSphere);
			#endif /*0*/
		#endif
		void					InsertSensor(const HighSetup::TVolumeDescriptor&desc);
		void					RemoveSensor(const HighSetup::TVolumeDescriptor&desc);

		void					InsertSensorCoverage(const SkySensorState&);
		void					RemoveSensorCoverage(const SkySensorState&);

		bool					HasSensor(const LCellIdI&, float radius) const;

		bool					GridCanSee(const LCellIdI&, int height, bool broad) const;
		bool					GridCanSeeAny(const HCellIdU&, UINT height) const;

		private:
			void				ChangeSensory(const LCellIdI&center, float radius, signed char change);
			void				ChangeSensorCoverage(const SkySensorState&,signed char change);
			void				ChangeSensorCoverage(const HighSetup::TVolumeDescriptor&state, signed char change);
	};

	
	extern WorldData			actualWorld;
	#ifndef CLIENT
		extern WorldData		initialWorld;
	#endif



	struct TSharedPlanetConfig
	{
		UINT32			innerCoreType = Content::Granite;	//type used beneath the lowest cell
		float4			atmosphereColor = float4(0.7,0.8,1.0,0.5);	//alpha=1.0 is really dense
		float4			sunPlaneNormal_Speed = float4(0,0,1,0.5f);	//fourth parameter is rotation speed in radians per second
		float4			sunColor = float4(1,1,1,1);	//fourth parameter is ignored
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
			//float4(random.getFloat(0.8,1.05f),random.getFloat(0.8,1.05f),random.getFloat(0.8,1.05f),1);

			Vec::stretch(sunColor.rgb,atmosphereColor.rgb,atmosphereColor.rgb);
			//sunColor.rgb = atmosphereColor.rgb;


			sunPlaneNormal_Speed = float4(
					random.getFloat(-1,1),random.getFloat(-1,1),random.getFloat(-1,1),
					0.07f
					//random.getFloat(-0.2,0.2)
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
					seed[i] = random.get();
				depressionSeed0 = random.get();
				depressionSeed1 = random.get();
				depressionSeed2 = random.get();
				depressionSeed3 = random.get();
				productSeed = random.get();
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

	class OrbitGrid
	{
	public:
		struct TCell
		{
			short visible = 0;


		};


		TCell	cells[LowSetup::GridType::CellCount];
		float	orbit = 1.f;


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

	#ifdef CLIENT
		#ifndef NO_SATELLITES
			extern OrbitGrid	visibleOrbitGrid;
		#endif

	#endif
}


#endif
