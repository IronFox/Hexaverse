#pragma once

#ifdef HAS_WINDOW

#include <math/vclasses.h>
#include <grid/stack.h>
#include "../shadedRenderInstruction.h"


namespace VisualTerrain
{
	static const constexpr count_t BatchSize = 16;	//the bigger, the less paralism, but the less the overhead during locking (since more work per locking)



	struct TWeightedColor;

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
		/**/	TLitColor(const TBaseColor&base, float lightLevel, float emp, const Grid::TVisibility&vis):Super(base),lightLevel(lightLevel),emp(emp),visibility(vis.value)	{}
		/**/	TLitColor(const TWeightedColor&base, float lightLevel, float emp=0.f):Super(base),lightLevel(lightLevel),emp(emp)	{}
		/**/	TLitColor(float lightLevel, float emp=0.f):lightLevel(lightLevel),emp(emp)	{}
		/**/	TLitColor(const Grid::TVisibility&v):visibility(v.value)	{}
		void	CompileFinalColor(TVec4<>&)	const;

	private:
		static void ApplyEMP(float intensity,TVec3<>&rs);
		void	CapColor(TVec3<>&rs)	const;
	};


	TBaseColor		GetBaseColor(const Grid::CellStack::Cell&cell);
	TLitColor		GetColorU(const Grid::CellStack&stack,  unsigned layer, Grid::Content oppositeContent, bool&mixes, Grid::Content&outType);
	TLitColor		GetDownColorU(const Grid::CellStack&stack, unsigned layer, Grid::Content oppositeContent, bool&mixes, Grid::Content&outType);
	TWeightedColor	GetMixColorU(const Grid::CellStack&stack, unsigned layer, Grid::Content oppositeContent, float&sumBrightnessTo, int&sumVisibilityTo, Grid::Content&outContent);
	TWeightedColor	GetDownMixColorU(const Grid::CellStack&stack, unsigned layer, Grid::Content oppositeContent, float&sumBrightnessTo, int&sumVisibilityTo, Grid::Content&outContent);



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
