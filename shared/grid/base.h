#pragma once

#include <math/vclasses.h>
#include <fractal/segment_map.h>
#include <fractal/interface.h>

#include "config.h"

/**
Define to reduce the world grid resolution exponent by one (reduce to around 1/4 of the cells)
*/
#define LOWER_RESOLUTION_PLANET


class BaseGrid
{
protected:
	static index_t		_ToIndex(unsigned col, unsigned row)
	{
		return row*(row + 1) / 2 + col;
	}
public:
	typedef BYTE	family_t;

	enum cell_type_t
	{
		NoCell,

		CornerCell,
		EdgeCell,
		FaceCell,

	};


	struct TCellNeighborhood
	{
		UINT32		neighborID[6];	//valid through numNeighbors-1
		BYTE		numNeighbors;	//0 for invalid cells, 5 for corner cells, 6 for all other cells
	};

	struct TCellGeometry
	{
		float3		center,
					corners[6];	//valid through numCorners-1
		BYTE		numCorners;	//0 for invalid cells, 5 for corner cells, 6 for all other cells
	};

	struct TNetCell
	{
		float4		centerVertex,
					cornerVertex[6];
		UINT32		centerVertexId,
					cornerVertexId[6];	//valid through numCorners-1
		BYTE		numCorners;	//0 for invalid cells, 5 for corner cells, 6 for all other cells
		UINT32		voffset;	//custom usage, not filled by grid base implementation
		float3		cornerCenter;		//normalized arithmetic average of all corner vertex locations
		float		minCornerDistance;	//minimum distance between centerVertex and cornerVertex
		USHORT		cornerFlags;	//one bit set for each original corner affecting the local cell
	};

	struct TCellVertex
	{
		float3		centerDirection;
		BYTE		ownerCount = 0;	//!< Number of cells this vertex borders (either 1 or 3)
		UINT32		owner[3];

		float4		ToFloat4() const {return float4(centerDirection,ownerCount == 1 ? 0.f : 1.f);}
	};

	static constexpr const BYTE		NumFamilies = 1 + 20 *2;
	static constexpr const count_t	IsolatedCornerCount = 3;	//max 3 isolated corners on an icosahedron
	static UINT32			IsolatedCornerIndex[IsolatedCornerCount];
	static bool				CornerIsIsolated[12];


	/**
	Keeps track of which families have changed as a result of some operation.
	The set is initially defined as empty, indicating no change at all.
	Use iterator access begin()/end() to iterate over all changed families
	*/
	struct FamiliesChanged
	{
		struct		const_iterator
		{
			constexpr		const_iterator()	{}
			bool			operator==(const const_iterator&other) const {return current == other.current;}
			bool			operator!=(const const_iterator&other) const {return current != other.current;}
			const_iterator	operator++(int) {auto rs = *this; current = FindNext(current+1); return rs;}
			const_iterator&	operator++() {current = FindNext(current+1); return *this;};
			family_t		operator*() const {return current;}
		private:
			friend struct FamiliesChanged;
			const FamiliesChanged*parent=nullptr;
			BYTE			current=NumFamilies;

			/**/			const_iterator(const FamiliesChanged*parent):parent(parent)	{current=FindNext(0);}
			constexpr		const_iterator(const FamiliesChanged*parent, BYTE current):parent(parent),current(current)	{}

			BYTE			FindNext(BYTE fromInclusive) const
			{
				if (parent)
					for (;fromInclusive < NumFamilies; fromInclusive++)
						if (parent->changed[fromInclusive])
							return fromInclusive;
				return NumFamilies;
			}
		};

		const_iterator		begin() const {return anyChanged? const_iterator(this) : end();}
		constexpr const_iterator	end() const {return const_iterator(nullptr,NumFamilies);}

		void		Set(family_t f)	{changed[f] = true; anyChanged = true;}
		bool		AnyChanged() const {return anyChanged;}

	private:
		bool		changed[NumFamilies] = {false};
		bool		anyChanged = false;
	};

	struct TCell
	{
		#ifdef HYBRID_GRID
			UINT32						higherEquivalent;	//index of this cell in the next higher resolution grid
		#endif
		float3							center;	//normalized
		TCellNeighborhood				neighborhood;
		//cell_type_t						type;
		family_t						family;	//[0, NumFamilies)
		USHORT							cornerFlags;	//one bit set for each original corner affecting the local cell
	};


	/**
	Checks if the specified neighborhood contains the specified stackID

	*/
	static bool Contains(const TCellNeighborhood&, UINT32 cellID);
	
	/**
	Attempts to find a cell shared by both neighborhoods
	@return Cell index of the first found shared cell, or 0xFFFFFFFF
	*/
	static UINT32 FindIntersection(const TCellNeighborhood&, const TCellNeighborhood&);
	
	/**
	Identical to FindIntersection() but causes a fatal if no intersection is found
	*/
	static UINT32 RequireIntersection(const TCellNeighborhood&, const TCellNeighborhood&);

	static bool IsIsolatedCorner(index_t stackID)	{DBG_ASSERT_LESS__(stackID,12); return CornerIsIsolated[stackID];}

protected:
	static void UpdateRadii(Array<TNetCell>&cells);
	static void	RadialInterpolation(const float3&v0, const float3&v1, float at, float3&out);
	static void	RadialInterpolation(const float3&v0, const float3&v1, const float3&v2, float xat, float yat, float3&out);

};

template<unsigned Exponent>
class GridSetup : public BaseGrid
{
	//the second value seems to always be 1 for the higher resolution map, and 2 for the next lower...
	#ifdef LOWER_RESOLUTION_PLANET
		typedef Fractal::BaseTemplateMap<Exponent,6-Exponent>	BaseMap;
	#else
		typedef Fractal::BaseTemplateMap<Exponent,7-Exponent>	BaseMap;
	#endif

	BaseMap								baseMap;
	Fractal::VertexMap					fullMap;
	




	struct TEdgeSide
	{
		index_t							triangleIndex;
		BYTE							sideIndex;	//0 to 2
		bool							forward;

		/**/							TEdgeSide()	{}
		/**/							TEdgeSide(index_t triangleIndex, BYTE sideIndex, bool forward) :triangleIndex(triangleIndex), sideIndex(sideIndex), forward(forward){}

	};

	struct TEdgeLink
	{
		index_t							edgeIndex;
		BYTE							sideIndex;	//0 to 1
		bool							forward;

		/**/							TEdgeLink()	{}
		/**/							TEdgeLink(index_t edgeIndex, BYTE sideIndex, bool forward) :edgeIndex(edgeIndex), sideIndex(sideIndex), forward(forward){}
	};

	index_t								cornerToFirstEdgeCell[12][5];
	TEdgeSide							edgeToSides[30][2];
	TEdgeSide							triangleToNeighbor[20][3];
	TEdgeLink							triangleToEdge[20][3];
	
	
	
	//Map		map;



	TCell								invalid;
	//BaseMap::range+1;

	Fractal::Geometry					geometry;




	inline index_t _EdgeAddress(index_t edgeIndex)	const
	{
		return 12 + edgeIndex * CellsPerEdge;
	}

	inline index_t _FaceAddress(index_t faceIndex)	const
	{
		return 12 + 30 * CellsPerEdge + faceIndex * BaseMap::vertices;
	}

	void			_DescribeCell(index_t id, TCellNeighborhood& out);
	float3			_LocateCell(index_t id, bool normalize);
	USHORT			_CornerFlags(index_t id);
	cell_type_t		_DescribeCell(index_t id);
	index_t			_GetGigherEquivalent(index_t id);




public:
	struct	CellIdI
	{
		UINT32	stackID;
		int		height;

		/**/	CellIdI()	{}
		/**/	CellIdI(index_t cellID, int height) :stackID((UINT32)cellID), height(height){}
		bool	operator==(const CellIdI&other)const { return stackID == other.stackID && height == other.height; }
		bool	operator!=(const CellIdI&other)const { return stackID != other.stackID || height != other.height; }
		CellIdI	operator+(int heightDelta) const { return CellIdI(stackID, height + heightDelta); }
		CellIdI	operator-(int heightDelta) const { return CellIdI(stackID, height - heightDelta); }
		void	operator++(int)	{height ++ ;}
		void	operator--(int) {height --;}

		String	ToString() const {return String(stackID)+ "[h="+String(height)+"]";}

		static const CellIdI	Invalid;

		static hash_t	hash(const CellIdI&cell)
		{
			return (hash_t)(cell.stackID*31+cell.height);
		}
	private:
		/**/	CellIdI(index_t stackID, UINT layer) {}
	};

	struct CellIdU
	{
		UINT32	stackID,
				layer;
		/**/	CellIdU()	{}
		/**/	CellIdU(index_t cellID, UINT32 layer) :stackID((UINT32)cellID), layer(layer){}
		bool	operator==(const CellIdU&other)const { return stackID == other.stackID && layer == other.layer; }
		bool	operator!=(const CellIdU&other)const { return stackID != other.stackID || layer != other.layer; }

		CellIdU	operator+(int heightDelta) const { return CellIdU(stackID, LayerSum(layer,heightDelta)); }
		CellIdU	operator-(int heightDelta) const { return CellIdU(stackID, LayerSum(layer,-heightDelta)); }
		void	operator++(int)	{layer ++ ;}
		void	operator--(int) {ASSERT_GREATER__(layer,0); layer --;}

		static UINT32 LayerSum(UINT32 base, int delta)	{if (delta >= 0) return base + delta; ASSERT_GREATER_OR_EQUAL__(base,(UINT32)-delta); return base + delta;}

		String	ToString() const {return String(stackID)+ "["+String(layer)+"]";}

		static hash_t	hash(const CellIdU&cell)
		{
			return (hash_t)(cell.stackID*31+cell.layer);
		}

		static const CellIdU	Invalid;

	private:
		/**/	CellIdU(index_t stackID, int height){}
	};


	static const count_t CellsPerEdge = BaseMap::range +1;



	static const count_t CellCount = 12 //corner cells
							+
							CellsPerEdge * 30	//edge-only cells
							+
							BaseMap::vertices * 20;	//side-only cells;


private:
	TCell				cellField[CellCount];
public:


	void		Create();


	const TCell*GetCells() const {return cellField;}
	const TCell&GetCell(index_t)const;
	const TCell&GetCell(family_t family, index_t memberIndex)const;
	count_t		CountFamilyMembers(family_t family) const;
	index_t		GetFamilyOffset(family_t family) const;

	void		Construct(Array<TCellVertex>&vertices, Array<TNetCell>&cells);	//fourth component contains 0 for center vertices and 1 for outline vertices

};
template<unsigned Exponent>
const typename GridSetup<Exponent>::CellIdU GridSetup<Exponent>::CellIdU::Invalid(UINT_MAX,UINT_MAX);
template<unsigned Exponent>
const typename GridSetup<Exponent>::CellIdI GridSetup<Exponent>::CellIdI::Invalid(UINT_MAX,INT_MAX);


//void Relax(BaseGrid::TCell*cells, count_t numCells);


template <unsigned Exponent>
	void		GridSetup<Exponent>::Create()
	{
		std::cout<<"Creating grid map for exponent "<<Exponent<<" ..."<<std::endl;

		baseMap.Create(fullMap);

		std::cout<<"Creating "<<fullMap.vertex_count<<"/"<<CellCount<<" cells ..."<<std::endl;

		Fractal::makeSphereGeometry(geometry);

		index_t at[12] = { 0, 0, 0, 0, 0 };

		memset(BaseGrid::CornerIsIsolated,true,sizeof(BaseGrid::CornerIsIsolated));


		for (UINT32 i = 0; i < 12; i++)
		{
			if (BaseGrid::CornerIsIsolated[i])
			{
				for (index_t e = 0; e < geometry.edgeField.Count(); e += 2)
				{
					UINT32	a = geometry.edgeField[e],
							b = geometry.edgeField[e+1];
					DBG_ASSERT_LESS__(a,12);
					DBG_ASSERT_LESS__(b,12);
					if (a == i)
						BaseGrid::CornerIsIsolated[b] = false;
					elif (b == i)
						BaseGrid::CornerIsIsolated[a] = false;
				}
			
			}
		}
		{
			count_t at = 0;
			for (UINT32 i = 0; i < 12; i++)
				if (BaseGrid::CornerIsIsolated[i])
				{
					BaseGrid::IsolatedCornerIndex[at++] = i;
					ASSERT_LESS_OR_EQUAL__(at,BaseGrid::IsolatedCornerCount);
				}
			ASSERT_EQUAL__(at,BaseGrid::IsolatedCornerCount);
		}





		for (index_t i = 0; i < 30; i++)
		{
			const UINT32*edge = geometry.edgeField + i * 2;
			ASSERT_LESS__(edge[0], 12);
			ASSERT_LESS__(edge[1], 12);
			ASSERT_LESS__(at[edge[0]], 5);
			ASSERT_LESS__(at[edge[1]], 5);
			cornerToFirstEdgeCell[edge[0]][at[edge[0]]] = 12 + i * CellsPerEdge;
			cornerToFirstEdgeCell[edge[1]][at[edge[1]]] = 12 + i * CellsPerEdge + CellsPerEdge - 1;
			at[edge[0]]++;
			at[edge[1]]++;
		}
		for (int i = 0; i < 12; i++)
		{
			ASSERT_EQUAL1__(at[i], 5, i);

			float3 axis = geometry.vertexField[i];
			TMatrix3<> matrix, transpose;
			Mat::makeAxisSystem(axis, 2, matrix);
			Mat::transpose(matrix, transpose);

			struct TSort
			{
				index_t element;
				float	angle;


				bool	operator<(const TSort&other) const
				{
					return angle < other.angle;
				}
			};

			TSort field[5];

			for (int j = 0; j < 5; j++)
			{
				float3 at = _LocateCell(cornerToFirstEdgeCell[i][j], false);
				Mat::Mult(transpose, at);
				field[j].element = cornerToFirstEdgeCell[i][j];
				field[j].angle = -atan2(at.y, at.x);
			}
			std::sort(field, field + 5);
			for (int j = 0; j < 5; j++)
				cornerToFirstEdgeCell[i][j] = field[j].element;

		}

		index_t at2[30] = { 0 };

		for (index_t i = 0; i < 30; i++)
		{
			const UINT32*edge = geometry.edgeField + i * 2;
			index_t found = InvalidIndex;
			unsigned root = 0;
			bool forward = false;
			for (index_t j = 0; j < geometry.triangleField.Count(); j += 3)
			{
				for (unsigned k = 0; k < 3; k++)
					if (geometry.triangleField[j + k] == edge[0] && geometry.triangleField[j + (k + 1) % 3] == edge[1])
					{
						found = j / 3;
						forward = true;
						root = k;

						ASSERT_LESS__(at2[i], 2);
						edgeToSides[i][at2[i]] = TEdgeSide(found, (BYTE)root, forward);
						at2[i]++;
						}
						else
							if (geometry.triangleField[j + k] == edge[1] && geometry.triangleField[j + (k + 1) % 3] == edge[0])
							{
						found = j / 3;
						forward = false;
						root = k;

						ASSERT_LESS__(at2[i], 2);
						edgeToSides[i][at2[i]] = TEdgeSide(found, (BYTE)root, forward);
						at2[i]++;
					}
			}
			ASSERT__(found != InvalidIndex);



			triangleToNeighbor[edgeToSides[i][0].triangleIndex][edgeToSides[i][0].sideIndex] = edgeToSides[i][1];
			triangleToNeighbor[edgeToSides[i][1].triangleIndex][edgeToSides[i][1].sideIndex] = edgeToSides[i][0];
			triangleToEdge[edgeToSides[i][0].triangleIndex][edgeToSides[i][0].sideIndex] = TEdgeLink(i, 0, edgeToSides[i][0].forward);
			triangleToEdge[edgeToSides[i][1].triangleIndex][edgeToSides[i][1].sideIndex] = TEdgeLink(i, 1, edgeToSides[i][1].forward);
		}










		for (index_t i = 0; i < CellCount; i++)
		{
			cellField[i].center = _LocateCell(i, true);
			_DescribeCell(i, cellField[i].neighborhood);
			cellField[i].cornerFlags = _CornerFlags(i);
	//		cellField[i].type = _DescribeCell(i);
			#ifdef HYBRID_GRID
				cellField[i].higherEquivalent = (UINT32)_GetGigherEquivalent(i);
			#endif
			if (i < 12 + 30 * CellsPerEdge)
				cellField[i].family = 0;
			else
			{
				index_t at = (i - 12 - 30 * CellsPerEdge);
				index_t side = at / BaseMap::vertices;
				index_t onSide = at % BaseMap::vertices;
				index_t sideSet = side * 2 + (onSide >= (BaseMap::vertices >> 1) ? 1 : 0);
				cellField[i].family = (BYTE)(1 + sideSet);
			}
			DBG_ASSERT__(cellField[i].family <= 40);

		}

//		Relax(cellField, CellCount);
		



//		invalid.type = NoCell;
		invalid.neighborhood.numNeighbors = 0;
		invalid.center = float3(1, 0, 0);

		std::cout<<"Grid constructed"<<std::endl;

	}



template <unsigned Exponent>
	void		GridSetup<Exponent>::_DescribeCell(index_t id, TCellNeighborhood& out)
	{
		if (id < 12)
		{
			//corner cell
			out.numNeighbors = 5;
			for (int i = 0; i < 5; i++)
				out.neighborID[i] = (UINT32)cornerToFirstEdgeCell[id][i];
			return;
		}
		id -= 12;
		if (id < CellsPerEdge * 30)
		{
			//edge-only cell
			out.numNeighbors = 6;
			const index_t edgeIndex = id / CellsPerEdge;
			const index_t edgeOffset = id % CellsPerEdge;
			out.neighborID[3] = (UINT32)(edgeOffset > 0 ? 12 + id - 1 : geometry.edgeField[edgeIndex * 2]);	//corner cell
			out.neighborID[0] = (UINT32)(edgeOffset + 1 < CellsPerEdge ? 12 + id + 1 : geometry.edgeField[edgeIndex * 2 + 1]);	//corner cell

			for (int i = 0; i < 2; i++)
			{
				const TEdgeSide& side = edgeToSides[edgeIndex][0].forward == (i == 0) ? edgeToSides[edgeIndex][0] : edgeToSides[edgeIndex][1];

				int i0 = 2 - i,
					i1 = 1 + i;

				index_t at0 = edgeOffset - 1,
					at1 = edgeOffset;
				if (!side.forward)
				{
					at0 = CellsPerEdge - at0 - 2;
					at1 = CellsPerEdge - at1 - 2;
				}
				if (edgeOffset > 0)
					out.neighborID[i * 3 + i0] = (UINT32)(12 //corner cells
					+
					CellsPerEdge * 30
					+
					BaseMap::vertices*(side.triangleIndex)
					+
					fullMap.border_index[side.sideIndex][at0]);
				else
				{
					auto otherEdge = triangleToEdge[side.triangleIndex][(side.sideIndex + (side.forward ? 2 : 1)) % 3];
					bool forward = geometry.edgeField[otherEdge.edgeIndex * 2] == geometry.edgeField[edgeIndex * 2];
					out.neighborID[i * 3 + i0] = (UINT32)(12 + otherEdge.edgeIndex * CellsPerEdge + (forward ? edgeOffset : CellsPerEdge - edgeOffset - 1));
				}
				if (edgeOffset + 1 < CellsPerEdge)
					out.neighborID[i * 3 + i1] = (UINT32)(12 //corner cells
					+
					CellsPerEdge * 30
					+
					BaseMap::vertices*(side.triangleIndex)
					+
					fullMap.border_index[side.sideIndex][at1]);
				else
				{
					auto otherEdge = triangleToEdge[side.triangleIndex][(side.sideIndex + (side.forward ? 1 : 2)) % 3];
					bool forward = geometry.edgeField[otherEdge.edgeIndex * 2] == geometry.edgeField[edgeIndex * 2 + 1];
					out.neighborID[i * 3 + i1] = (UINT32)(12 + otherEdge.edgeIndex * CellsPerEdge + (!forward ? edgeOffset : CellsPerEdge - edgeOffset - 1));
				}
			}
			return;
		}
		id -= CellsPerEdge * 30;
		if (id < BaseMap::vertices * 20)
		{
			out.numNeighbors = 6;
			const index_t faceIndex = id / BaseMap::vertices;
			const index_t vertexIndex = id % BaseMap::vertices;
			static const index_t outOffset =
				12 //corner cells
				+
				CellsPerEdge * 30;
			const index_t myOffset = outOffset + faceIndex * BaseMap::vertices;

			const auto&desc = fullMap.vertex_descriptor[vertexIndex];
			//unsigned x = desc.x;
			if (desc.borderDirection == -1)
			{
				out.neighborID[0] = (UINT32)(myOffset + BaseMap::getIndex(desc.x - 1, desc.y));
				out.neighborID[5] = (UINT32)(myOffset + BaseMap::getIndex(desc.x - 1, desc.y - 1));
				out.neighborID[4] = (UINT32)(myOffset + BaseMap::getIndex(desc.x, desc.y - 1));
				out.neighborID[3] = (UINT32)(myOffset + BaseMap::getIndex(desc.x + 1, desc.y));
				out.neighborID[2] = (UINT32)(myOffset + BaseMap::getIndex(desc.x + 1, desc.y + 1));
				out.neighborID[1] = (UINT32)(myOffset + BaseMap::getIndex(desc.x, desc.y + 1));
			}
			else
			{
				const TEdgeSide&opposite = triangleToNeighbor[faceIndex][desc.borderDirection];
				if (desc.indexAlongBorder > 0)
					out.neighborID[0] = (UINT32)(myOffset + fullMap.border_index[desc.borderDirection][desc.indexAlongBorder - 1]);
				else
				{
					const auto&edge = triangleToEdge[faceIndex][(desc.borderDirection + 2) % 3];
					out.neighborID[0] = (UINT32)_EdgeAddress(edge.edgeIndex);
					if (edge.forward)
						out.neighborID[0] += CellsPerEdge - 1;
				}

				if (desc.indexAlongBorder + 1 < BaseMap::range)
					out.neighborID[3] = (UINT32)(myOffset + fullMap.border_index[desc.borderDirection][desc.indexAlongBorder + 1]);
				else
				{
					const auto&edge = triangleToEdge[faceIndex][(desc.borderDirection + 1) % 3];
					out.neighborID[3] = (UINT32)(_EdgeAddress(edge.edgeIndex));
					if (!edge.forward)
						out.neighborID[3] += CellsPerEdge - 1;
				}

				{
					const auto&edge = triangleToEdge[faceIndex][desc.borderDirection];
					if (edge.forward)
					{
						out.neighborID[1] = (UINT32)_EdgeAddress(edge.edgeIndex) + desc.indexAlongBorder;
						out.neighborID[2] = (UINT32)_EdgeAddress(edge.edgeIndex) + desc.indexAlongBorder + 1;
					}
					else
					{
						out.neighborID[1] = (UINT32)_EdgeAddress(edge.edgeIndex) + CellsPerEdge - 1 - desc.indexAlongBorder;
						out.neighborID[2] = (UINT32)_EdgeAddress(edge.edgeIndex) + CellsPerEdge - 1 - desc.indexAlongBorder - 1;
					}
				}

				if (desc.indexAlongBorder > 0)
					out.neighborID[5] = (UINT32)(myOffset + fullMap.inner_border_index[desc.borderDirection][desc.indexAlongBorder - 1]);
				else
				{
					const auto&edge = triangleToEdge[faceIndex][(desc.borderDirection + 2) % 3];
					out.neighborID[5] = (UINT32)(_EdgeAddress(edge.edgeIndex) + 1);
					if (edge.forward)
						out.neighborID[5] += (UINT32)(CellsPerEdge - 3);
				}
				if (desc.indexAlongBorder + 1 < BaseMap::range)
					out.neighborID[4] = (UINT32)(myOffset + fullMap.inner_border_index[desc.borderDirection][desc.indexAlongBorder]);
				else
				{
					const auto&edge = triangleToEdge[faceIndex][(desc.borderDirection + 1) % 3];
					out.neighborID[4] = (UINT32)(_EdgeAddress(edge.edgeIndex) + 1);
					if (!edge.forward)
						out.neighborID[4] += (UINT32)(CellsPerEdge - 3);
				}



			}
			return;
		}
		FATAL__("bad call");


	}

	//void		DescribeCell(index_t id, TCellGeometry&out)
	//{
	//	out.center = LocateCell(id, true);
	//}


template <unsigned Exponent>
	USHORT		GridSetup<Exponent>::_CornerFlags(index_t id)
	{
		if (id < 12)
		{
			return USHORT(1 << id);
		}
		id -= 12;
		if (id < CellsPerEdge * 30)
		{
			const index_t edgeIndex = id / CellsPerEdge;
			const index_t edgeOffset = id % CellsPerEdge;
			float at = float(edgeOffset + 1) / float(CellsPerEdge + 1);

			USHORT rs = 0;
			if (at < 0.75f)
				rs |= USHORT(1 << geometry.edgeField[edgeIndex * 2]);
			if (at > 0.25f)
				rs |= USHORT(1 << geometry.edgeField[edgeIndex * 2 + 1]);
			return rs;
			//float at = float(edgeOffset + 1) / float(CellsPerEdge + 1);
			//float3 out;

			//RadialInterpolation(geometry.vertexField[geometry.edgeField[edgeIndex * 2]], geometry.vertexField[geometry.edgeField[edgeIndex * 2 + 1]], at, out);


			////Vec::interpolate(geometry.vertexField[geometry.edgeField[edgeIndex * 2]], geometry.vertexField[geometry.edgeField[edgeIndex * 2 + 1]], at, out);
			////if (normalize)
			////	Vec::normalize(out);
			//return out;

		}
		id -= CellsPerEdge * 30;
		if (id < BaseMap::vertices * 20)
		{
			const index_t faceIndex = id / BaseMap::vertices;
			const index_t vertexIndex = id % BaseMap::vertices;
			const auto&desc = fullMap.vertex_descriptor[vertexIndex];

			float fx = float(desc.x + 1) / float(BaseMap::range + 2);
			float fy = 1.f - float(desc.y + 2) / float(BaseMap::range + 2);

			USHORT rs = 0;
			if (fx + fy < 0.75f)
				rs |= USHORT(1<<geometry.triangleField[faceIndex * 3 + 1]);
			if (fx > 0.25f)
				rs |= USHORT(1<<geometry.triangleField[faceIndex * 3 + 2]);
			if (fy > 0.25f)
				rs |= USHORT(1<<geometry.triangleField[faceIndex * 3 + 0]);
			return rs;
		}
		FATAL__("bad call");
		return 0;
	}

template <unsigned Exponent>
	float3		GridSetup<Exponent>::_LocateCell(index_t id, bool normalize)
	{
		if (id < 12)
		{
			return geometry.vertexField[id];
		}
		id -= 12;
		if (id < CellsPerEdge * 30)
		{
			const index_t edgeIndex = id / CellsPerEdge;
			const index_t edgeOffset = id % CellsPerEdge;
			float at = float(edgeOffset + 1) / float(CellsPerEdge + 1);
			float3 out;

			RadialInterpolation(geometry.vertexField[geometry.edgeField[edgeIndex * 2]], geometry.vertexField[geometry.edgeField[edgeIndex * 2 + 1]], at, out);


			//Vec::interpolate(geometry.vertexField[geometry.edgeField[edgeIndex * 2]], geometry.vertexField[geometry.edgeField[edgeIndex * 2 + 1]], at, out);
			//if (normalize)
			//	Vec::normalize(out);
			return out;

		}
		id -= CellsPerEdge * 30;
		if (id < BaseMap::vertices * 20)
		{
			const index_t faceIndex = id / BaseMap::vertices;
			const index_t vertexIndex = id % BaseMap::vertices;
			const auto&desc = fullMap.vertex_descriptor[vertexIndex];

			float fx = float(desc.x + 1) / float(BaseMap::range + 2);
			float fy = 1.f - float(desc.y + 2) / float(BaseMap::range + 2);

			//float2 f(fx, fy);
			//float2 c = float2(1.f / 3.f);
			//float r = Vec::distance(f, c);
			//float r2 = powf(r, 1.0125f);
			////r2 = r * r + r2 * (1 - r);
			//f = (f - c) * r2 / r + c;
			//fx = f.x;
			//fy = f.y;

			float3 out;

			RadialInterpolation(geometry.vertexField[geometry.triangleField[faceIndex * 3 + 1]], geometry.vertexField[geometry.triangleField[faceIndex * 3 + 2]], geometry.vertexField[geometry.triangleField[faceIndex * 3 + 0]], fx, fy, out);


	/*		float3 dx = geometry.vertexField[geometry.triangleField[faceIndex * 3 + 2]] - geometry.vertexField[geometry.triangleField[faceIndex * 3 + 1]];
			float3 dy = geometry.vertexField[geometry.triangleField[faceIndex * 3 + 0]] - geometry.vertexField[geometry.triangleField[faceIndex * 3 + 1]];
			out = geometry.vertexField[geometry.triangleField[faceIndex * 3 + 1]] + dx * fx + dy * fy;
			if (normalize)
				Vec::normalize(out);*/
			return out;
		}
		FATAL__("bad call");
		return float3();
	}

template <unsigned Exponent>
	BaseGrid::cell_type_t	GridSetup<Exponent>::_DescribeCell(index_t id)
	{
		if (id < 12)
			return CornerCell;
		id -= 12;
		if (id < CellsPerEdge * 30)
			return EdgeCell;
		id -= CellsPerEdge * 30;
		if (id < BaseMap::vertices * 20)
			return FaceCell;
		return NoCell;
	}

template <unsigned Exponent>
	const BaseGrid::TCell&	GridSetup<Exponent>::GetCell(index_t id) const
	{
		if (id < CellCount)
			return cellField[id];
		FATAL__("Invalid cell index " + String(id));
		return invalid;
	}

template <unsigned Exponent>
	const BaseGrid::TCell&	GridSetup<Exponent>::GetCell(family_t family, index_t memberIndex) const
	{
		return GetCell(GetFamilyOffset(family) + memberIndex);
	}

template <unsigned Exponent>
	index_t		GridSetup<Exponent>::GetFamilyOffset(family_t family) const
	{
		if (family == 0)
		{
			return 0;
		}
		family_t at = family - 1;
		return BaseMap::vertices * (at / 2) + (BaseMap::vertices >> 1) * (at % 2) + 12 + 30 * CellsPerEdge;
	}


template <unsigned Exponent>
	count_t		GridSetup<Exponent>::CountFamilyMembers(family_t family) const
	{
		if (family == 0)
			return 12 + 30 * CellsPerEdge;
		return BaseMap::vertices>>1;
	}


struct TriangleKey
{
	UINT32	stackID[3];


	/**/				TriangleKey()	{}
	/**/				TriangleKey(index_t i0, index_t i1, index_t i2)
	{
		stackID[0] = (UINT32)i0;
		stackID[1] = (UINT32)i1;
		stackID[2] = (UINT32)i2;
		if (stackID[0] > stackID[1])
			std::swap(stackID[0], stackID[1]);
		if (stackID[0] > stackID[2])
			std::swap(stackID[0], stackID[2]);
		if (stackID[1] > stackID[2])
			std::swap(stackID[1], stackID[2]);

	}
	bool			operator==(const TriangleKey&k) const
	{
		return stackID[0] == k.stackID[0] && stackID[1] == k.stackID[1] && stackID[2] == k.stackID[2];
	}
	bool			operator!=(const TriangleKey&k) const { return !operator==(k); }

	static hash_t		hash(const TriangleKey&k)
	{
		return HashValue(k.stackID[0]).add(k.stackID[1]).add(k.stackID[2]);
	}

};



template <unsigned Exponent>
	void		GridSetup<Exponent>::Construct(Array<TCellVertex>&vertices, Array<TNetCell>&cells)
	{
		cells.SetSize(CellCount);
		Buffer<TCellVertex>	vtx;
		GenericHashTable<TriangleKey, UINT32, TriangleKey>	vtxMap;
		for (index_t i = 0; i < CellCount; i++)
		{
			TCellVertex&v = vtx.Append();
			v.centerDirection = cellField[i].center;
			v.ownerCount = 1;
			v.owner[0] = (UINT32)i;
		}


		UINT32 id;
		for (index_t i = 0; i < CellCount; i++)
		{
			auto&c = cells[i];
			c.cornerFlags = cellField[i].cornerFlags;
			c.centerVertexId = (UINT32)i;
			c.centerVertex = float4(vtx[i].centerDirection,0.f);
			const auto&n = cellField[i].neighborhood;
			c.numCorners = n.numNeighbors;
			for (BYTE j = 0; j < n.numNeighbors; j++)
			{
				TriangleKey key(i, n.neighborID[j], n.neighborID[(j + 1) % n.numNeighbors]);
				if (vtxMap.query(key, id))
				{
					TCellVertex&v = vtx[id];
					DBG_ASSERT_LESS__(v.ownerCount,3);
					v.owner[v.ownerCount++] = (UINT32)i;
				}
				else
				{
					TVec3<> center;
					Vec::center(cellField[key.stackID[0]].center, cellField[key.stackID[1]].center, cellField[key.stackID[2]].center, center);
					id = (UINT32)vtx.Count();
					vtxMap.set(key, id);
					TCellVertex&v = vtx.Append();
					v.centerDirection = center;
					v.ownerCount = 1;
					v.owner[0] = (UINT32)i;
				}
				c.cornerVertexId[j] = id;
				c.cornerVertex[j] = float4(vtx[id].centerDirection,1.f);
			}
		}
		vtx.CopyToArray(vertices);
		UpdateRadii(cells);
	}

template <unsigned Exponent>
	index_t			GridSetup<Exponent>::_GetGigherEquivalent(index_t id)
	{
		if (id < 12)
			return id;
		id -= 12;
		if (id < CellsPerEdge * 30)
		{
			const index_t edgeIndex = id / CellsPerEdge;
			const index_t edgeOffset = id % CellsPerEdge;
			const count_t nextCellsPerEdge = ((CellsPerEdge + 1) * 2 - 1);
			return (12 + nextCellsPerEdge *edgeIndex + edgeOffset * 2 + 1);
		}
		id -= CellsPerEdge * 30;
		if (id < BaseMap::vertices * 20)
		{
			const index_t faceIndex = id / BaseMap::vertices;
			const index_t vertexIndex = id % BaseMap::vertices;
			const count_t nextCellsPerEdge = ((CellsPerEdge + 1) * 2 - 1);
			const count_t nextCellsPerSide = NUM_VERTICES2(nextCellsPerEdge-2);

			const auto&desc = fullMap.vertex_descriptor[vertexIndex];

			return 12 + nextCellsPerEdge * 30 + faceIndex * nextCellsPerSide + _ToIndex(desc.x*2+1, desc.y*2+2);
		}
		FATAL__("bad call");
		return 0;
	}






