#pragma once
#ifdef HAS_WINDOW

#include <grid/worldData.h>
#include <grid/sensorCoverage.h>
#include <math/matrix.h>

namespace Grid
{

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

		static const size_t sTHeightMapOrientation = sizeof(THeightMapOrientation);
		static_assert(sTHeightMapOrientation==sizeof(float)*16*3,"Sruct alignment out of wack");
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
}

#endif
