#pragma once

#ifdef HAS_WINDOW

#include <math/vclasses.h>
#include <grid/stack.h>
#include "../shadedRenderInstruction.h"


namespace VisualTerrain
{
	static const constexpr count_t BatchSize = 16;	//the bigger, the less paralism, but the less the overhead during locking (since more work per locking)

	struct TBounds
	{
		M::Box<>					boundingBox;
		M::Sphere<>					boundingSphere;
	};


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
		
		static void ApplyEMP(float intensity,M::TVec3<>&rs);
		static void ColorInvisible(M::TVec3<>&rs);

	private:
		void	CapColor(M::TVec3<>&rs)	const;
	};


	TBaseColor		GetBaseColor(const Grid::CellStack::Cell&cell);
	TLitColor		GetColorU(const Grid::CellStack&stack,  unsigned layer, Grid::Content oppositeContent, bool&mixes, Grid::Content&outType);
	TLitColor		GetDownColorU(const Grid::CellStack&stack, unsigned layer, Grid::Content oppositeContent, bool&mixes, Grid::Content&outType);
	TWeightedColor	GetMixColorU(const Grid::CellStack&stack, unsigned layer, Grid::Content oppositeContent, float&sumBrightnessTo, int&sumVisibilityTo, Grid::Content&outContent);
	TWeightedColor	GetDownMixColorU(const Grid::CellStack&stack, unsigned layer, Grid::Content oppositeContent, float&sumBrightnessTo, int&sumVisibilityTo, Grid::Content&outContent);



	

	

	template <typename Idx>
		concept IsIndex =	std::integral<Idx>
							&&
							requires(Idx idx) {{UINT32(idx)};};

	template<typename Vtx>
		concept IsValidVertex = requires(const Vtx&v) {{ v.GetPosition()} -> std::convertible_to<M::TVec3<> >;};





	template <typename Vtx>
		class PlainPointSet
		{
		public:
			typedef PlainPointSet<Vtx> Self;


			friend void swap(Self&a, Self&b)
			{
				using std::swap;
				swap(a.vertices,b.vertices);
			}

			void			Clear()
			{
				vertices.Clear();
			}


			void		AppendTo(Self&set) const
			{
				set.vertices.AppendAll(vertices);
			}

			void		Append(const ArrayRef<const Vtx>&ref)
			{
				index_t at = vertices.Count();
				vertices.Append(ref.begin(),ref.Count());
			}

			Self&		operator<<(const Vtx&v)
			{
				vertices << v;
				return *this;
			}



			ArrayRef<Vtx>	GetVertices() const
			{
				return vertices.ToRef();
			}

			const Vtx&	operator[](index_t idx) const
			{
				return vertices[idx];
			}

			bool		HasVertices() const
			{
				return vertices.IsNotEmpty();
			}

			count_t		CountVertices() const
			{
				return vertices.Count();
			}

		private:
			Buffer0<Vtx>	vertices;
		};

	template <typename Vtx> 
		requires IsValidVertex<Vtx>
		class PointSet : public PlainPointSet<Vtx>
		{
		public:
			typedef PointSet<Vtx> Self;
			typedef PlainPointSet<Vtx> Super;

			friend void swap(Self&a, Self&b)
			{
				using std::swap;
				swap((Super&)a,(Super&)b);
				swap(a.box,b.box);
			}

			void			Clear()
			{
				Super::Clear();
				box = M::Box<>::Invalid;
			}


			void		AppendTo(Self&set) const
			{
				Super::AppendTo((Super&)set);
				set.box.Include(box);
			}

			void		Append(const ArrayRef<const Vtx>&ref)
			{
				index_t at = Super::CountVertices();
				Super::Append(ref);
				for (index_t i = at; i < Super::CountVertices(); i++)
					box.Include((*this)[i].GetPosition());
			}

			Self&		operator<<(const Vtx&v)
			{
				Super::operator<<(v);
				box.Include(v.GetPosition());
				return *this;
			}


			const M::Box<>&	GetBounds() const
			{
				return box;
			}

			void			IncludeSquareIn(M::Sphere<>&sphere) const
			{
				for (const auto&v : Super::GetVertices())
					sphere.IncludeSquare(v.GetPosition());
			}

			M::Sphere<>		GetBoundingSphere() const
			{
				M::Sphere<> rs;
				box.GetCenter(rs.center);

				rs.radius = 0;

				IncludeSquareIn(rs);

				rs.radius = sqrt(rs.radius);
				return rs;
			}


		private:
			M::Box<>		box = M::Box<>::Invalid;
		};

	template <typename Vtx>
		requires IsValidVertex<Vtx>
		class Hull : public PointSet<Vtx>
		{
		public:
			typedef PointSet<Vtx>	Super;
			typedef Hull<Vtx> Self;


			friend void swap(Self&a, Self&b)
			{
				using std::swap;
				swap((Super&)a,(Super&)b);
				swap(a.indices,b.indices);
			}

			void			Clear()
			{
				Super::Clear();
				indices.Clear();
			}

			void		AppendIndex(UINT32 idx)
			{
				indices << idx;
				DBG_ASSERT_LESS__(idx, Super::CountVertices());
			}

			void		AppendTo(Self&hull) const
			{
				UINT32 offset = (UINT32)hull.CountVertices();
				Super::AppendTo(hull);
				hull.AppendOffsetIndices(indices.ToRef().Const(),offset);
			}


			template <typename Idx>
				requires IsIndex<Idx>
				void		AppendOffsetIndices(const ArrayRef<const Idx>&indices, UINT32 offset)
				{
					auto iFrom = indices.begin();
					UINT32*iTo = this->indices.AppendRow(indices.size());
					while (iFrom != indices.end())
					{
						(*iTo) = UINT32(*iFrom) + offset;
						DBG_ASSERT_LESS__((*iTo),Super::CountVertices());
						iFrom++;
						iTo++;
					}
					DBG_ASSERT__(iTo == this->indices.end());
				}

			ArrayRef<UINT32>	GetIndices() const
			{
				return indices.ToRef();
			}

			count_t				CountIndices() const
			{
				return indices.Count();
			}

		private:
			Buffer<UINT32, 0>	indices;
		};


	
	template <typename T>
		requires IsValidVertex<T>
		static void LoadHull(Engine::DX11::InstructionBlock32&inst, const Hull<T>&hull)
		{
			inst.vbo.CreateFromStructs(hull.GetVertices());
			inst.ibo.Create(hull.GetIndices());
		}

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

			T&			Last()
			{
				return *(current-1);
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

			ArrayRef<T const>	ToRef() const {return ArrayRef<const T>(data,Count());} 

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
							return *this
								<< i0 << i1 << i2
								<< i0 << i2 << i3
							;
						}

			StaticMesh&	InvQuad(int i0, int i1, int i2, int i3)
						{
							return Quad(i3,i2,i1,i0);
						}
			StaticMesh&	operator<<(index_t index)
						{
							index_t idx = index + offset;
							//DBG_ASSERT_LESS2__(idx,vertexData.Count(),index,offset);	//isn't always appropriate
							DBG_ASSERT_LESS__(idx,0x10000);
							indexData << UINT16(idx);
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

			void		AppendTo(Hull<Vtx>&hull) const
						{
							UINT32 offset = (UINT32)hull.CountVertices();
							hull.Append(vertexData.ToRef());
							ASSERT_EQUAL2__(hull.CountVertices(),offset + vertexData.Count(),offset,vertexData.Count());
							hull.AppendOffsetIndices(indexData.ToRef(),offset);
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

			void		AppendTo(Hull<Vtx>&hull) const
						{
							UINT32 offset = (UINT32)hull.vertices.Count();
							//foreach (vertices,v)
								//hull.vertices.push_back(*v);
							hull.Append(vertices.ToRef());
							//const UINT32*iFrom = indices.pointer();
							hull.AppendOffsetIndices(indices.ToRef(),offset);
						}
		};

}
#endif
