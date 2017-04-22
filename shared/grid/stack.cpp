#include <global_root.h>
#include "stack.h"
#include "worldData.h"
#ifdef HAS_WINDOW
	#include <engine/display.h>
#endif

#include "../objects.h"
#include "../logEvent.h"
//#include "../sphereGridHelper.h"
#include "../luaGameAPI.h"
#include "../globalEvents.h"


namespace Grid
{
	
	UINT16				empZonesLocked = 0,
						empZonesUnlocked = 0xFFFF;

	//static Resource::coverage_t	GetResourceCoverage(const TVec3<>&seed, const TVec3<>&center0,float rarity,Resource::coverage_t maxYield, float probabilityModifier);








	#ifdef CLIENT
		#ifndef NO_SATELLITES
			OrbitGrid	visibleOrbitGrid;
		#endif
	#else

	#endif


	static_assert(CellStack::Cell::TypeCount == Content::NumberOfPossibilities, "Adjust type count");

	/*static*/ float3	CellStack::Cell::typeColor[TypeCount] =
	{
		float3(1.f),	//Unknown
		float3(0.25f),	//Granite
		float3(94.f / 255.f, 69.f / 255.f, 28.f / 255.f)*2.f,	//earth
		float3(0.5f, 1.f, 0.5f),	//grass
		float3(0.8f),	//rock
		float3(1.f,1.f,0.6f)*1.2f,	//sand
		float3(1.2f),	//Snow,
		float3(0.6f),	//foundation
		float3(0.3f, 0.3f, 0.3f), //road
		float3(1.2f,0.6f,0.5f)*-2.5f,	//Lava
		-float3(0.6f,0.5f,1.2f),	//technite
		float3(0.5f, 0.6f, 1.f),	//Water
		float3(1)	//clear
	};
	

	
	/*static*/ int	CellStack::Cell::typeStrength[TypeCount] =
	{
		INT_MAX, //Unknown
		9,	//granite
		4,	//earth
		4,	//grass
		5,	//rock
		3,	//sand
		3,	//Snow,
		7,	//foundation
		6,	//road
		1,	//lava
		7,	//technite
		2,	//water
		0	//clear
	};

	/*static*/ bool	CellStack::Cell::typeColorMixes[TypeCount] =
	{
		true,	//Unkown
		true,	//granite
		true,	//earth
		true,	//grass
		true,	//rock
		true,	//sand
		true,	//Snow,
		false,	//foundation
		false,	//road
		false,	//lava
		false,	//technite
		false,	//water
		false	//clear
	};



	/*static*/ float	CellStack::Cell::typeStructureFactor[TypeCount] =
	{
		1,	//unknown
		1,	//granite
		1,	//earth
		1,	//grass
		1,	//rock
		1,	//sand
		1,	//Snow,
		0,	//foundation
		0,	//road
		1,	//lava
		0,	//technite
		1,	//water
		1	//clear
	};
			

	void			CellStack::ChangeStructureCounterI(int height, int by)
	{
		if (height >= BottomHeight && height <= TopHeight)
		{
			UINT layer = ToLayer(height);
			volumeCell[layer].structureCounter += by;
		}
	}

	int				CellStack::FindTopClearOverNonClear(bool failOnStructureCount)	const
	{
		UINT32 h;
		for (h = Range-1; h < Range; h--)
		{
			const Cell&c = volumeCell[h];
			if (c.content != Grid::Content::Clear)
				break;
		}
		h++;
		if (h >= Range-1)
			return BadHeight;
		if (failOnStructureCount)
		{
			const Cell&c = volumeCell[h];
			if (c.structureCounter > 1)
				return BadHeight;
		}

		return ToHeight(h);
	}


	HighSetup			high;
	LowSetup			low;
	
	#ifdef VOXEL_SCANNER
		Array<LowCellStack>	lowCellStack;
	#endif



	/*static*/ CellStack::Cell	CellStack::bottom(CellStack::Cell::Content::Granite, 0), CellStack::top(CellStack::Cell::Content::Clear, 255);	//extremes below the bottom most, and above the top most


	const CellStack::Cell&		CellStack::GetCellU(unsigned layer)	const
	{
		if (layer >= Range)
			return top;
		return volumeCell[layer];
	}

	const CellStack::Cell&		CellStack::GetCellI(int height)	const
	{
		if (height < -HalfRange)
			return bottom;
		if (height >= HalfRange)
			return top;
		return volumeCell[height + HalfRange];
	}

	CellStack::Cell&		CellStack::GetMutableCellI(int height)
	{
		if (height < -HalfRange)
			FATAL__("index out of bounds");
		if (height >= HalfRange)
			FATAL__("index out of bounds");
		return volumeCell[height + HalfRange];
	}

	int				CellStack::FindTopOccupied() const
	{
		for (UINT32 j = Range - 1; j < Range; j--)
			if (volumeCell[j].IsOccupied())
				return int(j) - HalfRange;
		return -HalfRange-1;
	}

	bool				CellStack::FindTopOccupied(int&outHeight) const
	{
		bool rs = false;
		if (bottom.content != Content::Clear && bottom.content != Content::Lava)
		{
			outHeight = BottomHeight;
			rs = true;
		}
		for (UINT32 j = Range - 1; j < Range; j--)
			if (volumeCell[j].IsOccupied())
			{
				outHeight = int(j) - HalfRange;
				return true;
			}
		return rs;
	}



	///*static*/ float		CellStack::HeightFactor(unsigned cellIndex)
	//{
	//	if (cellIndex >= Range)
	//		return 1.f;
	//	unsigned down = Range - 1 - cellIndex;
	//	float fdown = float(down) / (Range - 1);
	//	return 0.5f + 0.5f * (1.f - fdown * fdown * fdown);
	//}
	#ifdef HAS_WINDOW
		float	CellStack::Cell::GetLightLevel() const
		{
			if (IsOpaque())
				return 0.f;
			float rs = float(intLightLevel ) / float(0xFF);//*0.75f + 0.25f;
				//rs *= 0.1f;
			//rs *= 1.5f - 0.5f * float(visible > 0);
			return rs;
		}


		void	CellStack::Cell::SetLightLevel(float v)
		{
			BYTE val = BYTE(clamped(v * 0xFF,0,0xFF));
			if (IsOpaque())
				val = 0;
			else
				val = Degrade(val,64 * structureCounter);

			intLightLevel = val;
		}


	
		void	CellStack::Cell::SetIntLightLevel(int v)
		{
			BYTE val = BYTE(clamped(v,0,0xFF));
			if (IsOpaque())
				val = 0;
			else
				val = Degrade(val,64 * structureCounter);

			intLightLevel = val;
		}

		void	CellStack::Cell::SetSubmergence(UINT sub)
		{
			submergence = (BYTE)std::min(sub,255U);
		}
		/**
		Calculates submergence levels that are propagated down from this cell
		Returns 0 if the local cell is solid, and an incremented local submergence value if water
		*/
		UINT		CellStack::Cell::GetIncreasedSubmergence()	const
		{
			if (content == Content::Lava)
				return 0;
			if (IsOpaque())
				return submergence;
			UINT rs = submergence;
			if (content == Content::Water && rs < 0xFF)
				rs ++;
			return rs;
		}

	#endif



	void			CellStack::ZeroInit(bool resetVisibility)
	{
		#ifdef HAS_WINDOW
			lowestOpenCell = (BYTE)Range;
			highestOpaqueCell = (BYTE)Range-1;
			waterRange = IntRange<BYTE>(0,0);
		#endif
		for (UINT i = 0; i < Range; i++)
		{
			volumeCell[i].content = Cell::Content::Unknown;
			volumeCell[i].structureCounter = 0;
			#ifdef HAS_WINDOW
				volumeCell[i].intLightLevel = 0xFF;
				volumeCell[i].submergence = 0;
			#endif
			if (resetVisibility)
			{
				volumeCell[i].visibility = 0;
				volumeCell[i].temporarilyVisible = false;
			}

		}
		#ifdef HAS_WINDOW
			lightingChangedTo = Range;
		#endif

			
	}

	bool			CellStack::ResetStructureCounters(bool resetVisibility)
	{
		bool rs = false;
		for (UINT i = 0; i < Range; i++)
		{
			auto&cell = volumeCell[i];
			if (cell.structureCounter != 0)
			{
				rs = true;
				cell.structureCounter = 0;
			}
			if (resetVisibility && cell.visibility != 0)
			{
				cell.visibility = 0;
				rs = true;
			}
		}
		return rs;
	}

	int CellStack::FindMostApplicableGround(int h, bool canStandOnWater) const
	{
		UINT uh = ToLayer(h);
		//unsigned(h + HalfRange);
		if (uh < Range)
		{
			if (volumeCell[uh].IsSolid() || (volumeCell[uh].IsWater() && canStandOnWater))
			{
				for (UINT i = uh + 1; i < Range; i++)
					if (/*!volumeCell[i].IsOccupied() && */!volumeCell[i].IsSolid() && (!canStandOnWater || !volumeCell[i].IsWater()))
						return int(i) - HalfRange;
				return 0x1FFFFFFF;
			}
			for (UINT i = uh - 1; i < uh; --i)
			{
				if (volumeCell[i].IsSolid() || (volumeCell[i].IsWater() && canStandOnWater))
					return (int(i + 1) - HalfRange);
				//if (volumeCell[i].IsOccupied())
					//return 0x1FFFFFFF;
			}
			return -HalfRange;
		}
		return h;
	}



	static void 		SetW(BaseGrid::TNetCell&cell, float w)
	{
		cell.centerVertex.w = w;
		for (int i = 0; i < cell.numCorners; i++)
			cell.cornerVertex[i].w = w;
	}



	
	#ifdef CLIENT
		#ifdef HAS_WINDOW
			
	

		#endif
	#endif

	//static bool ReplaceTop(BYTE content)
	//{
	//	return content == Content::Earth || content == Content::Rock || content == Content::Sand;
	//}



	//static float CDF(float x)
	//{
	//	float sum = x;
	//	float value = x;
	//	for (int i = 0; i < 100; i++)
	//	{
	//		value = (value*x*x/(2*i+1));
	//		sum = sum+value;
	//	}
	//	static const float div = 0.39894228040143267793994605993438f;
	//	 //1.f/sqrt(2.f*M_PIF);
	//	return 0.5f+(sum*div)*exp(-(x*x)/2);
	//}



	void				CreateHighAndLow()
	{
		high.Create();
		low.Create();

		foreach(high.cells, cell)
			SetW(*cell, -1.f);

		bool tagged[HighSetup::GridType::CellCount];
		memset(tagged, false, sizeof(tagged));

			for (index_t i = 0; i < low.cells.count(); i++)
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



		GlobalEvents::terrainFlush << []()
		{
			empZonesLocked = 0;
			empZonesUnlocked = ~empZonesLocked;
		};

	}

	namespace Clip
	{
		bool NoClip(index_t, int)
		{
			return false;
		}

	}

	#ifdef CLIENT
		#ifdef HAS_WINDOW


			//void	RenderNeighborhood(index_t stackID, float radius)
			//{
			//	UpdatePlanetRenderingParameters(radius);
			//	//display.ClearDepth();
			//	low.RenderCell(stackID);
			//	stackID = low.grid.GetCell(stackID).higherEquivalent;
			//	const BaseGrid::TCell&cell = high.grid.GetCell(stackID);
			//	high.RenderCell(stackID);
			//	for (int i = 0; i < cell.neighborhood.numNeighbors; i++)
			//		high.RenderCell(cell.neighborhood.neighborID[i]);

			//}
		#endif
	#endif







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
		TVec3<> wa = low.cells[a.stackID].centerVertex.xyz * ToRadius(a.height);
		TVec3<> wb = low.cells[b.stackID].centerVertex.xyz * ToRadius(b.height);
		return Vec::quadraticDistance(wa,wb);
	}



	#ifdef HAS_WINDOW

		#ifdef DEBUG_SENSORY
			struct TSensor
			{
				LCellIdI	center;
				float radius;

				bool		operator==(const TSensor&other) const
				{
					return center == other.center && radius == other.radius;
				}
		
			};

			static Buffer0<TSensor> sensors;


		#endif



		//void	SetSphereDisplay(float3 center, float radius, const float4&color)
		//{
		//	Instruction::useSphereDisplay = true;
		//	float4 def(center,sqr(radius));
		//	if (def != Instruction::lastDef)
		//	{
		//		//LogEvent(EventSignificance::Debug,nullptr,"change1 detected");
		//		Instruction::lastDef = def;
		//		Instruction::sphereDef.Update(def);
		//	}
		//	if (Instruction::lastColor != color)
		//	{
		//		//LogEvent(EventSignificance::Debug,nullptr,"change2 detected");
		//		Instruction::lastColor = color;
		//		Instruction::sphereColor.Update(color);
		//	}
		//}
		//void	UnsetSphereDisplay()
		//{
		//	Instruction::useSphereDisplay = false;
		//}

	#endif






	void	FlushAllStackData()
	{
		empZonesLocked = 0;
		empZonesUnlocked = 0xFFFF;
	}


	#ifdef VOXEL_SCANNER
		void	RemapToLowStacks(BYTE family)
		{
			index_t offset = low.grid.GetFamilyOffset(family);
			count_t length = low.grid.CountFamilyMembers(family);
			Concurrency::parallel_for(offset,offset+length,[offset](index_t i)
			{
				LowCellStack&target = lowCellStack[i];
				const UINT32 high = Grid::low.grid.GetCell(i).higherEquivalent;
				const auto&n = Grid::high.grid.GetCell(high).neighborhood;

				for (unsigned layer = 0; layer < CellStack::Range; layer++)
				{
					int cnt = cellStack[high].volumeCell[layer].IsOpaque() ? 1 : 0;
					cellStack[high].volumeCell[layer].visible = target.volumeCell[layer].visibility;
					for (BYTE k = 0; k < n.numNeighbors; k++)
					{
						cellStack[n.neighborID[k]].volumeCell[layer].visible = target.volumeCell[layer].visibility;	//race-condition - dont care
						if (cellStack[n.neighborID[k]].volumeCell[layer].IsOpaque())
							cnt ++;
					}
					target.volumeCell[layer].IsEmpty = cnt == 0;
					target.volumeCell[layer].isFull = cnt == 1 + n.numNeighbors;
				}
			});
	
		}
	#endif



	#ifdef HYBRID_GRID
		TVec3<> ToWorld(const HCellIdU&id)
		{
			return high.cells[id.stackID].centerVertex.xyz * Grid::ToRadius(CellStack::ToHeight(id.layer));
		}
		TVec3<> ToWorld(const HCellIdI&id)
		{
			return high.cells[id.stackID].centerVertex.xyz * Grid::ToRadius(id.height);
		}
		TVec3<> ToWorld(const HCellIdU&id, float fLayerOffset)
		{
			return high.cells[id.stackID].centerVertex.xyz * Grid::ToRadiusf(fLayerOffset + CellStack::ToHeight(id.layer));
		}
		TVec3<> ToWorld(const HCellIdI&id, float fLayerOffset)
		{
			return high.cells[id.stackID].centerVertex.xyz * Grid::ToRadiusf(fLayerOffset + id.height);
		}
		TVec3<> ToWorldR(const HCellIdU&id, float offset)
		{
			return high.cells[id.stackID].centerVertex.xyz * (offset + Grid::ToRadius(CellStack::ToHeight(id.layer)));
		}
		TVec3<> ToWorldR(const HCellIdI&id, float offset)
		{
			return high.cells[id.stackID].centerVertex.xyz * (offset + Grid::ToRadius(id.height));
		}
	#endif
	TVec3<> ToWorld(const LCellIdU&id)
	{
		return low.cells[id.stackID].centerVertex.xyz * Grid::ToRadius(CellStack::ToHeight(id.layer));
	}
	TVec3<> ToWorld(const LCellIdI&id)
	{
		return low.cells[id.stackID].centerVertex.xyz * Grid::ToRadius(id.height);
	}
	TVec3<> ToWorld(const LCellIdU&id, float fLayerOffset)
	{
		return low.cells[id.stackID].centerVertex.xyz * Grid::ToRadiusf(fLayerOffset + CellStack::ToHeight(id.layer));
	}
	TVec3<> ToWorld(const LCellIdI&id, float fLayerOffset)
	{
		return low.cells[id.stackID].centerVertex.xyz * Grid::ToRadiusf(fLayerOffset + id.height);
	}
	TVec3<> ToWorldR(const LCellIdU&id, float offset)
	{
		return low.cells[id.stackID].centerVertex.xyz * (offset + Grid::ToRadius(CellStack::ToHeight(id.layer)));
	}
	TVec3<> ToWorldR(const LCellIdI&id, float offset)
	{
		return low.cells[id.stackID].centerVertex.xyz * (offset + Grid::ToRadius(id.height));
	}


	bool	AffectedByEMP(const HCellIdI&id)
	{
		return !(Grid::high.cells[id.stackID].cornerFlags & empZonesUnlocked);
	}
	bool	AffectedByEMP(const LCellIdI&id)
	{
		return !(Grid::low.cells[id.stackID].cornerFlags & empZonesUnlocked);
	}






	#ifdef THOROUGLY_DEBUG_SENSORY
		void	InsertDbgSensor(const LCellIdI&center, float radius)
		{
			Timer::Time t0 = timer.Now();

			Math::Box<> test;
			const TVec3<> worldCenter = ToWorld(center);
			test.SetCenter(worldCenter,radius);
			for (BYTE family = 0; family < BaseGrid::NumFamilies; family++)
			{
				if (!high.planetFamilyBoundingBox[family].Intersects(test))
					continue;

				const index_t offset = high.grid.GetFamilyOffset(family);
				const count_t range = high.grid.CountFamilyMembers(family);
				Concurrency::parallel_for(offset,offset+range,[worldCenter,radius](index_t i)
				{
					//bool anyChanged = false;
					bool anyVisible = false;
					Grid::HCellIdU cell((UINT32)i,0U);
					{
						Grid::HCellIdU aboveTop(cell.stackID, Grid::CellStack::Range);
						TVec3<> bottom = Grid::ToWorld(cell),
								top = Grid::ToWorld(aboveTop);

						anyVisible = Obj::DetectSphereEdgeIntersection(worldCenter,radius,bottom,top);
					}
					if (!anyVisible)
						return;

					bool wasIn = false;
					for (unsigned j = 0; j < Grid::CellStack::Range; j++)
					{
						cell.layer = j;
						if (Vec::quadraticDistance(Grid::ToWorld(cell),worldCenter) > sqr(radius))
						{
							if (wasIn)
								break;
							continue;
						}
						wasIn = true;
						cellStack[i].volumeCell[j].dbgVisibility ++;
					}
					Grid::high.family[Grid::high.grid.GetCell(i).family].SetOutdated(0);
				});
			}

			LogEvent(EventSignificance::Debug,nullptr,"Sensory change processed in "+String(timer.GetSecondsSince(t0)));
			//ShowMessage(timer.GetSecondsSince(t0));
		}
	#endif



	//void		Volume::InsertSensor(const LCellIdI&cell, float radius)
	//{
	//	ChangeSensory(cell,radius,1);
	//	#ifdef DEBUG_SENSORY
	//		TSensor s;
	//		s.center = cell;
	//		s.radius = radius;
	//		//ASSERT__(!sensors.Contains(s));	//this can fire falsely, if a vehicle is moved to the previous location of another vehicle (which hasn't updated yet)
	//		sensors.Append(s);
	//	#endif
	//}
	//void		Volume::RemoveSensor(const LCellIdI&cell, float radius)
	//{
	//	#ifdef DEBUG_SENSORY
	//		TSensor s;
	//		s.center = cell;
	//		s.radius = radius;
	//		ASSERT__(sensors.FindAndErase(s));	//this must always work, however
	//	#endif
	//	ChangeSensory(cell,radius,-1);
	//}




}
