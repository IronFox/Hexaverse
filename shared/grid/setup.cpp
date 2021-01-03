#include "setup.h"


namespace Grid
{
		
	UINT16				empZonesLocked = 0,
						empZonesUnlocked = 0xFFFF;


	HighSetup			high;
	LowSetup			low;


	#ifdef HYBRID_GRID
		void ToWorld(const HCellIdU& id, M::TVec3<>& rs)
		{
			Vec::mult(high.cells[id.stackID].centerVertex.xyz, Grid::ToRadius(CellStack::ToHeight(id.layer)), rs);
		}
		void ToWorld(const HCellIdI& id, M::TVec3<>& rs)
		{
			Vec::mult(high.cells[id.stackID].centerVertex.xyz, Grid::ToRadius(id.height), rs);
		}

		M::TVec3<> ToWorld(const HCellIdU& id)
		{
			return high.cells[id.stackID].centerVertex.xyz * Grid::ToRadius(CellStack::ToHeight(id.layer));
		}
		M::TVec3<> ToWorld(const HCellIdI& id)
		{
			return high.cells[id.stackID].centerVertex.xyz * Grid::ToRadius(id.height);
		}
		M::TVec3<> ToWorld(const HCellIdU& id, float fLayerOffset)
		{
			return high.cells[id.stackID].centerVertex.xyz * Grid::ToRadiusf(fLayerOffset + CellStack::ToHeight(id.layer));
		}
		M::TVec3<> ToWorld(const HCellIdI& id, float fLayerOffset)
		{
			return high.cells[id.stackID].centerVertex.xyz * Grid::ToRadiusf(fLayerOffset + id.height);
		}
		M::TVec3<> ToWorldR(const HCellIdU& id, float offset)
		{
			return high.cells[id.stackID].centerVertex.xyz * (offset + Grid::ToRadius(CellStack::ToHeight(id.layer)));
		}
		M::TVec3<> ToWorldR(const HCellIdI& id, float offset)
		{
			return high.cells[id.stackID].centerVertex.xyz * (offset + Grid::ToRadius(id.height));
		}
	#endif

	void ToWorld(const LCellIdU& id, M::TVec3<>& rs)
	{
		Vec::mult(low.cells[id.stackID].centerVertex.xyz, Grid::ToRadius(CellStack::ToHeight(id.layer)), rs);
	}
	void ToWorld(const LCellIdI& id, M::TVec3<>& rs)
	{
		Vec::mult(low.cells[id.stackID].centerVertex.xyz, Grid::ToRadius(id.height), rs);
	}

	M::TVec3<>  ToWorld(const LCellIdU& id)
	{
		return low.cells[id.stackID].centerVertex.xyz * Grid::ToRadius(CellStack::ToHeight(id.layer));
	}
	M::TVec3<>  ToWorld(const LCellIdI& id)
	{
		return low.cells[id.stackID].centerVertex.xyz * Grid::ToRadius(id.height);
	}


	M::TVec3<> ToWorld(const LCellIdU& id, float fLayerOffset)
	{
		return low.cells[id.stackID].centerVertex.xyz * Grid::ToRadiusf(fLayerOffset + CellStack::ToHeight(id.layer));
	}
	M::TVec3<> ToWorld(const LCellIdI& id, float fLayerOffset)
	{
		return low.cells[id.stackID].centerVertex.xyz * Grid::ToRadiusf(fLayerOffset + id.height);
	}
	M::TVec3<> ToWorldR(const LCellIdU& id, float offset)
	{
		return low.cells[id.stackID].centerVertex.xyz * (offset + Grid::ToRadius(CellStack::ToHeight(id.layer)));
	}
	M::TVec3<> ToWorldR(const LCellIdI& id, float offset)
	{
		return low.cells[id.stackID].centerVertex.xyz * (offset + Grid::ToRadius(id.height));
	}



	bool	AffectedByEMP(const HCellIdU& id, UINT16 empZonesUnlocked)
	{
		return !(Grid::high.cells[id.stackID].cornerFlags & empZonesUnlocked);
	}
	bool	AffectedByEMP(const LCellIdU& id, UINT16 empZonesUnlocked)
	{
		return !(Grid::low.cells[id.stackID].cornerFlags & empZonesUnlocked);
	}

	bool	AffectedByEMP(const HCellIdI& id, UINT16 empZonesUnlocked)
	{
		return !(Grid::high.cells[id.stackID].cornerFlags & empZonesUnlocked);
	}
	bool	AffectedByEMP(const LCellIdI& id, UINT16 empZonesUnlocked)
	{
		return !(Grid::low.cells[id.stackID].cornerFlags & empZonesUnlocked);
	}

	bool	AffectedByEMP(const HCellIdI& id)
	{
		return AffectedByEMP(id, empZonesUnlocked);
	}
	bool	AffectedByEMP(const LCellIdI& id)
	{
		return AffectedByEMP(id, empZonesUnlocked);
	}

	bool	AffectedByEMP(const HCellIdU& id)
	{
		return AffectedByEMP(id, empZonesUnlocked);
	}
	bool	AffectedByEMP(const LCellIdU& id)
	{
		return AffectedByEMP(id, empZonesUnlocked);
	}

	void			AssertIsHorizontallyValid(const Grid::LCellIdI& location, const TCodeLocation& loc)
	{
		if (location.stackID >= LowSetup::GridType::CellCount)
			throw Except::Program::UnsupportedRequest(loc, "StackID " + String(location.stackID) + " exceeds valid range [0," + String(LowSetup::GridType::CellCount) + ")");
	}
	void			AssertIsVerticallyValid(const Grid::LCellIdI& location, const TCodeLocation& loc)
	{
		if (location.height < CellStack::BottomHeight || location.height > CellStack::TopHeight)
			throw Except::Program::UnsupportedRequest(loc, "Location height " + String(location.height) + " falls outside valid range [" + String(CellStack::BottomHeight) + "," + String(CellStack::TopHeight) + "]");
	}


	void			AssertIsValid(const Grid::LCellIdI& location, const TCodeLocation& loc)
	{
		AssertIsHorizontallyValid(location, loc);
		AssertIsVerticallyValid(location, loc);
	}


	

	void TagCluster(bool*rebuild, index_t stackID)
	{
		const auto&n2 = Grid::high.grid.GetCell(stackID).neighborhood;
		rebuild[Grid::high.grid.GetCell(stackID).family] = true;
		for (int k = 0; k < n2.numNeighbors; k++)
			rebuild[Grid::high.grid.GetCell(n2.neighborID[k]).family] = true;
	}

	UINT32 TagLowCluster(bool *rebuild, const LCellIdI&cell)
	{
		#ifdef HYBRID_GRID
			UINT32 finer = low.grid.GetCell(cell.stackID).higherEquivalent;
		#else
			UINT32 finer = cell.stackID;
		#endif
		TagCluster(rebuild, finer);
		return finer;
	}


	static void 		SetW(BaseGrid::TNetCell& cell, float w)
	{
		cell.centerVertex.w = w;
		for (int i = 0; i < cell.numCorners; i++)
			cell.cornerVertex[i].w = w;
	}

	void				CreateHighAndLow()
	{
		high.Create();
		low.Create();

		foreach(high.cells, cell)
			SetW(*cell, -1.f);

		bool tagged[HighSetup::GridType::CellCount];
		memset(tagged, false, sizeof(tagged));

			for (index_t i = 0; i < low.cells.Count(); i++)
			{
				#ifdef HYBRID_GRID
				index_t high = low.grid.GetCell(i).higherEquivalent;
				const auto&n = Grid::high.grid.GetCell(high).neighborhood;

				for (int j = 0; j < n.numNeighbors; j++)
				{
					const UINT32 c = n.neighborID[j];
					if (tagged[c])
						continue;
					tagged[c] = true;
					const auto&n2 = Grid::high.grid.GetCell(c).neighborhood;
					int found = -1;
					for (int k = 0; k < n2.numNeighbors; k++)
						if (n2.neighborID[k] == high)
						{
							found = k;
							break;
						}
					ASSERT__(found != -1);
					auto&cell = Grid::high.cells[c];


					float3 v0 = cell.cornerVertex[(found + 1)%cell.numCorners].xyz;
					float3 v1 = cell.cornerVertex[(found + cell.numCorners-2)%cell.numCorners].xyz;
					float3 normal = (v0 | v1).Normalized();
				
					auto&vtx = cell.centerVertex;

					vtx.w = Vec::dot(vtx.xyz, normal) * 100.f;
					for (int k = 0; k < cell.numCorners; k++)
					{
						auto&vtx = cell.cornerVertex[k];
						vtx.w = Vec::dot(vtx.xyz, normal) * 100.f;
					}

				}
				#else
					auto&cell = Grid::high.cells[i];
					for (int i = 0; i < cell.numCorners; i++)
						cell.cornerVertex[i].w = 0.f;
					cell.centerVertex.w = 2.f;
				#endif
			}

		#ifdef VOXEL_SCANNER
			lowCellStack.SetSize(low.cells.Count());
		#endif

		//CellStack::bottom.content = config.innerCoreType;



		//GlobalEvents::terrainFlush << []()
		//{
		//	empZonesLocked = 0;
		//	empZonesUnlocked = ~empZonesLocked;
		//};

	}


	


	bool	IsHighPartOfLow(UINT32 highCell, UINT32 lowCell)
	{
		#ifdef HYBRID_GRID
			if (highCell >= HighSetup::GridType::CellCount || lowCell >= LowSetup::GridType::CellCount)
				return false;
			UINT32 lowEq = low.grid.GetCell(lowCell).higherEquivalent;
			if (lowEq == highCell)
				return true;
			const auto&n = high.grid.GetCell(lowEq).neighborhood;
			for (int i = 0; i < n.numNeighbors; i++)
				if (n.neighborID[i] == highCell)
					return true;
			return false;
		#else
			return highCell == lowCell;
		#endif
	}
	
	float	GetGeometricDistance(const LCellIdI&a, const LCellIdI&b)
	{
		return sqrt(GetQuadraticGeometricDistance(a,b));
	}
	float	GetQuadraticGeometricDistance(const LCellIdI&a, const LCellIdI&b)
	{
		#ifndef _DEBUG
			ASSERT_LESS__(a.stackID,low.cells.Count());	//just to be sure ... for now
			ASSERT_LESS__(b.stackID,low.cells.Count());
		#endif
		M::TVec3<> wa = low.cells[a.stackID].centerVertex.xyz * ToRadius(a.height);
		M::TVec3<> wb = low.cells[b.stackID].centerVertex.xyz * ToRadius(b.height);
		return Vec::quadraticDistance(wa,wb);
	}



	void	FlushAllStackData()
	{
		empZonesLocked = 0;
		empZonesUnlocked = 0xFFFF;
	}

}
