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
		void	CompileFinalColor(M::TVec4<>&)	const;

	private:
		static void ApplyEMP(float intensity,M::TVec3<>&rs);
		void	CapColor(M::TVec3<>&rs)	const;
	};


	TBaseColor		GetBaseColor(const Grid::CellStack::Cell&cell);
	TLitColor		GetColorU(const Grid::CellStack&stack,  unsigned layer, Grid::Content oppositeContent, bool&mixes, Grid::Content&outType);
	TLitColor		GetDownColorU(const Grid::CellStack&stack, unsigned layer, Grid::Content oppositeContent, bool&mixes, Grid::Content&outType);
	TWeightedColor	GetMixColorU(const Grid::CellStack&stack, unsigned layer, Grid::Content oppositeContent, float&sumBrightnessTo, int&sumVisibilityTo, Grid::Content&outContent);
	TWeightedColor	GetDownMixColorU(const Grid::CellStack&stack, unsigned layer, Grid::Content oppositeContent, float&sumBrightnessTo, int&sumVisibilityTo, Grid::Content&outContent);

	template <typename Vtx>
		struct TPointSet
		{
			Buffer<Vtx, 0>	vertices;


			void			Clear()
			{
				vertices.Clear();
			}


			void		AppendTo(TPointSet<Vtx>&set) const
			{
				set.vertices.AppendAll(vertices);
			}

		};

	template <typename Vtx>
		struct THull : public TPointSet<Vtx>
		{
			typedef TPointSet<Vtx>	Super;

			Buffer<UINT32, 0>	indices;


			void			Clear()
			{
				Super::Clear();
				indices.Clear();
			}

			void		AppendTo(THull<Vtx>&hull) const
			{
				UINT32 offset = (UINT32)hull.vertices.Count();
				Super::AppendTo(hull);
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


	template <typename T, count_t maxElements>
		class StaticList
		{
		private:
			static const constexpr count_t MaxElements = maxElements;

			T			data[MaxElements];	//should be POD to work
			T			*current;
		public:
			count_t		Count() const
			{
				return current - data;
			}
			count_t		size() const
			{
				return current - data;
			}

			T*			begin()
			{
				return data;
			}
			const T*	begin() const
			{
				return data;
			}
			T*			end()
			{
				return current;
			}
			const T*	end() const
			{
				return current;
			}
				
			void		Reset()
			{
				current = data;
			}

			T&			Append()
			{
				T*rs = current;
				++current;
				return *rs;
			}

			void		operator<<(const T&el)
			{
				//DBG_ASSERT_EQUAL__((void*)data ,(void*)castedFirst);
				DBG_ASSERT__(current - data < MaxElements);
				(*current) = el;
				++current;
			}

			bool		IsEmpty() const {return current == data;}
			bool		IsNotEmpty() const {return current != data;}

		};

	/**
	Stack-stored mesh container with constant capacity
	*/
	template <typename Vtx, count_t maxVertices, count_t maxIndices>
		class StaticMesh
		{
		public:
			static const constexpr count_t MaxVertices = maxVertices;
			static const constexpr count_t MaxIndices = maxIndices;

			StaticList<Vtx,MaxVertices>		vertexData;
			StaticList<UINT16,MaxIndices>	indexData;

			UINT16		offset;

			///**/		StaticMesh() :vertexRange((Vtx*)vertexData),vat((Vtx*)vertexData), iat(indexRange), offset(0)	{}
			///**/		StaticMesh(const StaticMesh<Vtx,maxVertices,maxIndices>&other)	{Reset();}	//nothing for now

			//void		operator=(const StaticMesh<Vtx,maxVertices,maxIndices>&other)	{Reset();}	//nothing for now


			void		Reset()
			{
				vertexData.Reset();
				indexData.Reset();
				offset = (0);
			}

			bool		IsEmpty() const {return indexData.IsEmpty();}
			bool		IsNotEmpty() const {return indexData.IsNotEmpty();}



			void		SetOffset()
						{
							offset = (UINT16)(vertexData.Count());
						}

			StaticMesh&	Quad(int i0, int i1, int i2, int i3)
						{
							indexData << UINT16(i0) + offset;
							indexData << UINT16(i1) + offset;
							indexData << UINT16(i2) + offset;
							indexData << UINT16(i0) + offset;
							indexData << UINT16(i2) + offset;
							indexData << UINT16(i3) + offset;
							return *this;
						}

			StaticMesh&	InvQuad(int i0, int i1, int i2, int i3)
						{
							indexData << UINT16(i0) + offset;
							indexData << UINT16(i2) + offset;
							indexData << UINT16(i1) + offset;
							indexData << UINT16(i0) + offset;
							indexData << UINT16(i3) + offset;
							indexData << UINT16(i2) + offset;
							return *this;
						}
			StaticMesh&	operator<<(UINT16 index)
						{
							indexData << index + offset;
							return *this;
						}
			StaticMesh&	operator<<(const Vtx&vtx)
						{
							vertexData << vtx;
							return *this;
						}

			Vtx&		NewVertex()
						{
							return vertexData.Append();

						}

			void		AppendTo(THull<Vtx>&hull) const
						{
							UINT32 offset = (UINT32)hull.vertices.Count();
							hull.vertices.Append(vertexData.begin(), vertexData.Count());
							const UINT16*iFrom = indexData.begin();
							const UINT16*const iat = indexData.end();
							UINT32*iTo = hull.indices.AppendRow(indexData.Count());
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
