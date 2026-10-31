#pragma once

#include <container/queue.h>
#include <container/bit_set.h>
#include "base.h"
#include "stack.h"


namespace Grid
{
	namespace Ctr = DeltaWorks::Container;


	extern UINT16				empZonesLocked,
								empZonesUnlocked;

	struct TNeighborCellID
	{
		UINT32						stackID;
		UINT32						distanceFromOrigin;
	};


	template <unsigned Exponent>
	class Setup
	{
	public:
		typedef GridSetup<Exponent>	GridType;
		GridType					grid;
		static Ctr::Array<BaseGrid::TNetCell>	cells;
		Ctr::Array<BaseGrid::TCellVertex>	cellVertices;
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
				M::TIntRange<UINT32>range;		//range.end is one higher than actual limit
			};

			Ctr::Vector<TStack>		coverage;

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
				Ctr::Queue<UINT32>	stacksLeft;

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
		Will utilize internal bit set beyond max distance 4
		*/
		void						GetExtendedNeighborhood(UINT32 centerCell, UINT32 maxDistance, Ctr::BasicVector<UINT32>&outNeighbors) const
		{
			if (maxDistance > 4)
			{
				Ctr::BitSet visited;
				GetExtendedNeighborhood(centerCell,maxDistance,outNeighbors,visited);
				return;
			}
			Ctr::Queue<std::pair<UINT32, UINT32> >	queue;
			queue << std::make_pair(centerCell, 0);
			std::pair<UINT32, UINT32> current;
			outNeighbors.Clear();
			while (queue >> current)
			{
				if (outNeighbors.Contains(current.first))
					continue;
				outNeighbors << current.first;
				if (current.second == maxDistance)
					continue;
				const auto&n = grid.GetCell(current.first).neighborhood;
				for (int i = 0; i < n.numNeighbors; i++)
					queue << std::make_pair(n.neighborID[i], current.second + 1);
			}
		}

		/**
		Determines the extended neighborhood around the given center cell
		*/
		void						GetExtendedNeighborhood(UINT32 centerCell, UINT32 maxDistance, Ctr::BasicVector<UINT32>&outNeighbors, Ctr::BitSet&inOutVisitedCells) const
		{
			Ctr::Queue<std::pair<UINT32, UINT32> >	queue;
			queue << std::make_pair(centerCell, 0);
			std::pair<UINT32, UINT32> current;
			outNeighbors.Clear();
			inOutVisitedCells.ResizeZero(GridType::CellCount);
			while (queue >> current)
			{
				if (inOutVisitedCells.IsSet(current.first))
					continue;
				outNeighbors << (UINT32)current.first;
				inOutVisitedCells.Set(current.first);
				if (current.second == maxDistance)
					continue;
				const auto&n = grid.GetCell(current.first).neighborhood;
				for (int i = 0; i < n.numNeighbors; i++)
					queue << std::make_pair(n.neighborID[i], current.second + 1);
			}
		}

		/**
		Adds the extended neighborhood around the given center cell to a hash set of stack ids. The bit set is not cleared before
		adding new cells
		*/
		void						AddExtendedNeighborhood(UINT32 centerCell, UINT32 maxDistance, Ctr::BitSet&inOutVisitedCells) const
		{
			Ctr::Queue<std::pair<UINT32, UINT32> >	queue;
			queue << std::make_pair(centerCell, 0);
			std::pair<UINT32, UINT32> current;
			while (queue >> current)
			{
				if (inOutVisitedCells.IsSet(current.first))
					continue;
				inOutVisitedCells.Set(current.first);
				if (current.second == maxDistance)
					continue;
				const auto&n = grid.GetCell(current.first).neighborhood;
				for (int i = 0; i < n.numNeighbors; i++)
					queue << std::make_pair(n.neighborID[i], current.second + 1);
			}
		}

		/**
		Determines the extended neighborhood around the given center cell, including distances
		*/
		void						GetExtendedNeighborhood(UINT32 centerCell, UINT32 maxDistance, Ctr::BasicVector<TNeighborCellID>&outNeighbors) const
		{
			Ctr::Queue<std::pair<UINT32, UINT32> >	queue;
			queue << std::make_pair(centerCell, 0);
			std::pair<UINT32, UINT32> current;
			outNeighbors.Clear();
			Ctr::Array<UINT32> distances;

			const constexpr auto Unset = std::numeric_limits<UINT32>::max();
			distances.SetSize(GridType::CellCount);
			distances.Fill(Unset);
			Vector<UINT32> set;
			while (queue >> current)
			{
				auto oldDist = distances[current.first];
				if (oldDist <= current.second)
					continue;
				if (oldDist == Unset)
					set << current.first;
				distances[current.first] = current.second;
				if (current.second == maxDistance)
					continue;
				const auto&n = grid.GetCell(current.first).neighborhood;
				for (int i = 0; i < n.numNeighbors; i++)
					queue << std::make_pair(n.neighborID[i], current.second + 1);
			}

			foreach (set,idx)
			{
				auto&cell = outNeighbors.Append();
				cell.stackID = *idx;
				cell.distanceFromOrigin = distances[*idx];
			}
		}

		void						UpdatePlanarDistanceField(index_t centerCell, UINT32 maxDistance, TDistanceField&field) const
		{
			field.Reset();

			field.distance[centerCell] = 0;
			Ctr::Queue<index_t>	queue;
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
		UINT32			Locate(const M::TVec3<>&v, UINT32 startID = 0)	const
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
		/*
		static float FindStackMaxHeight(const M::TIntRange<>&range, const CellStack&stack)
		{
			foreach(stack.sections, section)
				if (section->range.Intersects(range))
					//if (section->range.start < range.start && section->range.end > range.start)
					return section->GetMax();

			return -1.f;
		}
		static float FindStackMaxHeightAbove(const M::TIntRange<>&range, const CellStack&stack)
		{
			foreach(stack.sections, section)
				if (range.Contains(section->range.start) && range.end < section->range.end)
					return section->GetMax();

			return -1.f;
		}
		*/
		void	GenerateVolume(const M::Sphere<>&sphere,typename TVolumeDescriptor::Operator&op,TVolumeDescriptor&volume) const
		{
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
				const M::TVec3<>&dir = cells[next].cornerCenter;
				M::TVec2<> distances;
				M::TVec3<> bottom,top;
				Vec::mult(dir,br,bottom);
				Vec::mult(dir,tr,top);
				typename TVolumeDescriptor::TStack&st = volume.coverage.Append();
				st.stackID = next;
				if (!M::Obj::DetectSphereEdgeIntersection(sphere.center,sphere.radius,bottom,top,distances))
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
	};

	template <UINT Exponent>
		Ctr::Array<BaseGrid::TNetCell>	Setup<Exponent>::cells;

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


	/**
	Checks whether or not a given location is affected by residual emp fields, given the specified zone flag set
	*/
	bool	AffectedByEMP(const HCellIdI&, UINT16 empZonesUnlocked);
	bool	AffectedByEMP(const HCellIdU&, UINT16 empZonesUnlocked);
	/**
	Checks whether or not a given location is affected by residual emp fields, given the specified zone flag set
	*/
	bool	AffectedByEMP(const LCellIdI&, UINT16 empZonesUnlocked);
	bool	AffectedByEMP(const LCellIdU&, UINT16 empZonesUnlocked);
	/**
	Checks whether or not a given location is affected by residual emp fields, using the global zone flag set
	*/
	bool	AffectedByEMP(const HCellIdI&);
	bool	AffectedByEMP(const HCellIdU&);
	/**
	Checks whether or not a given location is affected by residual emp fields, using the global zone flag set
	*/
	bool	AffectedByEMP(const LCellIdI&);
	bool	AffectedByEMP(const LCellIdU&);


	void	ToWorld(const HCellIdU&,M::TVec3<>&rs);
	void	ToWorld(const HCellIdI&,M::TVec3<>&rs);
	void	ToWorld(const LCellIdU&,M::TVec3<>&rs);
	void	ToWorld(const LCellIdI&,M::TVec3<>&rs);
	M::TVec3<> ToWorld(const HCellIdU&);
	M::TVec3<> ToWorld(const HCellIdI&);
	M::TVec3<> ToWorld(const LCellIdU&);
	M::TVec3<> ToWorld(const LCellIdI&);
	M::TVec3<> ToWorld(const HCellIdU&, float fLayerOffset);
	M::TVec3<> ToWorld(const HCellIdI&, float fLayerOffset);
	M::TVec3<> ToWorld(const LCellIdU&, float fLayerOffset);
	M::TVec3<> ToWorld(const LCellIdI&, float fLayerOffset);
	M::TVec3<> ToWorldR(const HCellIdU&, float radiusOffset);
	M::TVec3<> ToWorldR(const HCellIdI&, float radiusOffset);
	M::TVec3<> ToWorldR(const LCellIdU&, float radiusOffset);
	M::TVec3<> ToWorldR(const LCellIdI&, float radiusOffset);

	inline HCellIdI	ToHigh(const LCellIdI&cellID)	{return HCellIdI(low.grid.GetCell(cellID.stackID).higherEquivalent,cellID.height); }
	inline HCellIdU	ToHigh(const LCellIdU&cellID)	{return HCellIdU(low.grid.GetCell(cellID.stackID).higherEquivalent,cellID.layer); }
	inline HCellIdU	ToLayered(const HCellIdI&cellID)	{return HCellIdU(cellID.stackID,CellStack::ToLayer(cellID.height));}
	inline LCellIdU	ToLayered(const LCellIdI&cellID)	{return LCellIdU(cellID.stackID,CellStack::ToLayer(cellID.height));}



	/**
	Asserts that the specified location falls within the horizontal cell range.
	Throws Except::Program::UnsupportedRequest if not
	@param location Location to check
	@param loc Caller
	*/
	void			AssertIsHorizontallyValid(const Grid::LCellIdI& location, const TCodeLocation& loc);
	/**
	Asserts that the specified location falls within the vertical stack range.
	Throws Except::Program::UnsupportedRequest if not.
	Extra height levels such as orbital or sub orbital are not respected and lead to failure.
	@param location Location to check
	@param loc Caller
	*/
	void			AssertIsVerticallyValid(const Grid::LCellIdI& location, const TCodeLocation& loc);
	/**
	Asserts that the specified location falls both in the horizontal and vertical valid coordinate range.
	Throws Except::Program::UnsupportedRequest if not.
	Extra height levels such as orbital or sub orbital are not respected and lead to failure.
	@param location Location to check
	@param loc Caller
	*/
	void			AssertIsValid(const Grid::LCellIdI& location, const TCodeLocation& loc);



	void			CreateHighAndLow();
	void			FlushAllStackData();

	/**
	Tests wether the high resolution cell @a highCell is a part (center or edge) of the low resolution of cell @a lowCell
	*/
	bool			IsHighPartOfLow(UINT32 highCell, UINT32 lowCell);

}

