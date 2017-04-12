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
			volumeCell[height + HalfRange].structureCounter += by;
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

	#ifdef 0


		TLitColor			CellStack::GetColorU(unsigned layer, Cell::Content oppositeContent, bool&mixes, Cell::Content&outType) const
		{
			if (layer >= Range)
			{
				mixes = false;
				outType = Cell::Content::Clear;
				return TLitColor(TVisibility(1.f));
			}
			const Cell&cell = layer > 0 ? this->volumeCell[layer - 1] : bottom;
			const TVisibility&vis = layer > 0 ? this->volumeCell[layer - 1].GetVisibility() : this->volumeCell[0].GetVisibility();
			mixes = cell.content == oppositeContent || Cell::typeColorMixes[cell.content];
			outType = Cell::Content::Reinterpret(cell.content);
			return TLitColor(cell.GetBaseColor(), this->volumeCell[layer].GetLightLevel(),0.f,vis);
		}
	
		TLitColor			CellStack::GetDownColorU(unsigned layer, Cell::Content oppositeContent, bool&mixes, Cell::Content&outType) const
		{
			if (layer+1 >= Range)
			{
				mixes = false;
				outType = Cell::Content::Clear;
				return TLitColor();
			}
			const Cell&cell = this->volumeCell[layer + 1];
			mixes = oppositeContent==cell.content || Cell::typeColorMixes[cell.content];
			outType = Cell::Content::Reinterpret(cell.content);
			return TLitColor(cell.GetBaseColor(), this->volumeCell[layer].GetLightLevel(),0.f,cell.GetVisibility());
		}


		TWeightedColor			CellStack::GetMixColorU(unsigned layer, Cell::Content oppositeContent, float&sumBrightnessTo, int&sumVisibilityTo, Content&outContent) const
		{
			if (layer >= Range)
			{
				//sumBrightnessTo += 1.f;
				outContent = Content::Clear;
				sumVisibilityTo ++;
				return TWeightedColor();
			}
			const Cell&upper = this->volumeCell[layer];

			if (!layer)
			{
				sumBrightnessTo += upper.GetLightLevel();
				sumVisibilityTo += upper.visible > 0;
				outContent = Content::Clear;
				return TWeightedColor();
			}
			const Cell&cell = this->volumeCell[layer - 1];
			float light = upper.GetLightLevel();
			sumVisibilityTo += (cell.visible > 0);
			if (!cell.IsOpaque() || upper.IsOpaque())
			{
				sumBrightnessTo += light * 1.3f;
				outContent = Content::Clear;
				return TWeightedColor();
					//float4(0, 0, 0, -0.1f);
					//float4(0.2f) * light;	//slight light increase
			}
			else
			{
				sumBrightnessTo += light;
				if (oppositeContent == cell.content || Cell::typeColorMixes[cell.content])
				{
					outContent = Content::Reinterpret( cell.content );
					return TWeightedColor(cell.GetBaseColor(), 1.f);
				}
				outContent = Content::Clear;
				return TWeightedColor();
			}

		}


	
		TWeightedColor			CellStack::GetDownMixColorU(unsigned layer, Cell::Content oppositeContent, float&sumBrightnessTo, int&sumVisibilityTo, Content&outContent) const
		{
			if (layer >= Range)
			{
				sumBrightnessTo += 1.f;
				outContent = Content::Clear;
				return TWeightedColor();
			}
			if (!layer)
			{
				sumBrightnessTo += this->volumeCell[layer].GetLightLevel();
				outContent = Content::Clear;
				return TWeightedColor();
			}
			const Cell&cell = this->volumeCell[layer - 1],
				&upper = this->volumeCell[layer];
			float light = cell.GetLightLevel();
			sumVisibilityTo += cell.visible > 0;
			if (cell.IsOpaque() || !upper.IsOpaque())
			{
				sumBrightnessTo += light * 1.3f;
				outContent = Content::Clear;
				return TWeightedColor();
					//float4(0, 0, 0, -0.1f);
					//float4(0.2f) * light;	//slight light increase
			}
			else
			{
				sumBrightnessTo += light;
				if (oppositeContent == upper.content || Cell::typeColorMixes[upper.content])
				{
					outContent = Content::Reinterpret( upper.content );
					return TWeightedColor(upper.GetBaseColor(), 1.f);
				}
				outContent = Content::Clear;

				return TWeightedColor();
			}

		}


	
	#endif

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



	void			CellStack::ZeroInit()
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

		}
		#ifdef HAS_WINDOW
			lightingChangedTo = Range;
		#endif
	}

	void			CellStack::ResetStructureCounters()
	{
		for (UINT i = 0; i < Range; i++)
		{
			volumeCell[i].structureCounter = 0;
		}
	}


	bool			CellStack::MakeLocalHole(int h, World::Data&owner)
	{
		UINT uh = ToLayer(h);
		if (uh < Range)
		{
			if (volumeCell[uh].content != Content::Clear)
			{
				volumeCell[uh].content = Cell::Content::Clear;

				#ifdef HAS_WINDOW
					lowestOpenCell = std::min(lowestOpenCell, (BYTE)uh);
					if (highestOpaqueCell == (BYTE)uh)
					{
						while (!volumeCell[highestOpaqueCell].IsOpaque() && highestOpaqueCell > 0)
							highestOpaqueCell--;
					}
					owner.structureChanged = true;
					owner.lightingOutdated = true;
					DebugCheckRange();
				#endif
				return true;
			}
		}
		return false;
	}


	bool			CellStack::LocalOverwrite(int h, Cell::Content type, World::Data&owner)
	{
		UINT uh = ToLayer(h);
		if (uh < Range)
		{
			if (volumeCell[uh].content != type)
			{
				#if 0
					//this would be nice to capture some errors...
					//sadly, foundations also register as structures although they are submerged most of the time.
					//even if type was limited to foundation, mines would instantly trigger an asseration violation.
					//damn
					#if defined _DEBUG && !defined CLIENT
					if (type != Content::Clear && volumeCell[uh].content == Content::Clear)
					{
						ASSERT_EQUAL1__(volumeCell[uh].structureCounter,0, uh);
					}
					#endif
				#endif /*0*/
				bool wasSolid = volumeCell[uh].IsOpaque();
				volumeCell[uh].content = type;

			
				#ifdef HAS_WINDOW
					if (!wasSolid && volumeCell[uh].IsOpaque())
					{
						if (lowestOpenCell == (BYTE)uh)
							while (volumeCell[lowestOpenCell].IsOpaque() && lowestOpenCell < (BYTE)Range)
								lowestOpenCell++;

						highestOpaqueCell = std::max(highestOpaqueCell, (BYTE)uh);
					}
					else
						if (wasSolid && !volumeCell[uh].IsOpaque())
						{
							lowestOpenCell = std::min(lowestOpenCell, (BYTE)uh);
							if (highestOpaqueCell == (BYTE)uh)
							{
								while (!volumeCell[highestOpaqueCell].IsOpaque() && highestOpaqueCell > 0)
									highestOpaqueCell--;
							}
						}
					owner.structureChanged = true;
					owner.lightingOutdated = true;
					DebugCheckRange();
				#endif
				return true;
			}
		}
		return false;
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



/*
	void	ApplyFamilyDelta(Array<CellStack>&stacks, BYTE family, const Array<CellStack::TTypeDeltaBlock>&data0, const Array<CellStack::TStructCountDeltaBlock>&data1, bool initialization)
	{
		const index_t offset = high.grid.GetFamilyOffset(family);
		const count_t range = high.grid.CountFamilyMembers(family);

		index_t at = 0;
		foreach (data0,block)
		{
			for (index_t i = 0; i < block->repitition; i++)
			{
				index_t stackIndex = (at % range) + offset;
				index_t layer = at / range;

				CellStack&stack = stacks[stackIndex];

				bool rs = stack.volumeCell[layer].ApplyDelta(block->value);
				#ifdef _DEBUG
					if (initialization)
						ASSERT__(rs || !block->value.IsSet());
				#endif
				
				at++;

			}
			
		}
		at = 0;
		foreach (data1,block)
		{
			for (index_t i = 0; i < block->repitition; i++)
			{
				index_t stackIndex = (at % range) + offset;
				index_t layer = at / range;

				CellStack&stack = stacks[stackIndex];

				stack.volumeCell[layer].ApplyDelta(block->value);
				
				at++;
			}
			
		}
	}
*/

	void	Volume::ApplyFamilyDelta(BYTE family, const Array<CellStack::TTypeDelta>&data0, bool initialization)
	{
		const index_t offset = high.grid.GetFamilyOffset(family);
		const count_t range = high.grid.CountFamilyMembers(family);
		ASSERT_EQUAL__(data0.Count(), range * CellStack::Range);
		//ASSERT_EQUAL__(data1.Count(), range * CellStack::Range);

		#ifdef HAS_WINDOW
			Queue<index_t>	changed;
		#endif
		{
			const CellStack::TTypeDelta*delta = data0.pointer();
			for (index_t i = 0; i < range; i++)
			{
				CellStack&stack = cellStack[i+offset];
				bool anyChanged = false;
				for (index_t j = 0; j < CellStack::Range; j++)
				{
					if (stack.volumeCell[j].ApplyDelta(*delta))
					{
						anyChanged = true;
						#ifdef HAS_WINDOW
							stack.lightingChangedTo = std::max<BYTE>(stack.lightingChangedTo,(BYTE)j+1);
						#endif
					}
					#ifdef _DEBUG
					else
						if (initialization)
							FATAL__("Failed to initialize");
					#endif
					delta ++;
				}
				if (anyChanged)
				{
					#ifdef HAS_WINDOW
						stack.UpdateRange();
						changed << i + offset;
					#endif
				}
			}
			ASSERT_CONCLUSION(data0,delta);
		}
		high.family[family].SetOutdated(0);
		low.family[family].SetOutdated(0);



		#ifdef HAS_WINDOW
			index_t current;
			while (changed >> current)
			{
				BYTE h = cellStack[current].lightingChangedTo;
				//ASSERT_LESS__(h,CellStack::Range);
				//if (h+1 >= cellStack[current].lowestOpenCell)
				{
					high.family[high.grid.GetCell(current).family].SetOutdated(0);
					low.family[high.grid.GetCell(current).family].SetOutdated(0);
					lightingOutdated = true;
					structureChanged = true;
				}
				if (!h)
					continue;
				h--;
				//smallest = std::min(smallest,h);
				//counter++;

		
				const auto&n = high.grid.GetCell(current).neighborhood;
				for (BYTE k = 0; k < n.numNeighbors; k++)
				{
					UINT32 nid = n.neighborID[k];
					if (cellStack[nid].lightingChangedTo < h)
					{
						cellStack[nid].lightingChangedTo = h;
						changed << nid;
					}
				}
			}

		#endif
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


		void		SetSphereDisplay(const TVec3<>&center, float radius, const float4&color)
		{
			Instruction::displayMode = Instruction::SphereColored;
			Instruction::SphereDef.Update(float4(center,radius));
			Instruction::SphereColor.Update(color);
		}

		void		SetParaboloidDisplay(const TVec3<>&direction, float height, float flatRadius, const float4&_color)
		{
			Instruction::displayMode = Instruction::ParaboloidColored;
			Instruction::ParaboloidDirection_Height.Update(float4(direction,height));
			float4 color_r = float4(_color.rgb, flatRadius);

			if (Instruction::lastColor != color_r)
			{
				//LogEvent(EventSignificance::Debug,nullptr,"change2 detected");
				Instruction::lastColor = color_r;
				Instruction::ParaboloidColor_FlatRadius.Update(color_r);
			}
		}

		void		UnsetParaboloidAndSphereDisplay()
		{
			Instruction::displayMode = Instruction::NormalRendering;
		}


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






	void	FlushAllVolumeData()
	{
		empZonesLocked = 0;
		empZonesUnlocked = 0xFFFF;

		actualWorld.FlushAllData();
		#ifndef CLIENT
			initialWorld.FlushAllData();
			for (int i = 0; i < BaseGrid::NumFamilies; i++)
			{
				high.family[i].SetOutdated(0);
				low.family[i].SetOutdated(0);
			}
		#endif

		#ifdef HAS_WINDOW
			ResetSunRotation();
		#endif
			
		#ifdef CLIENT
			#ifndef NO_SATELLITES
				visibleOrbitGrid.Clear();
			#endif
			#ifdef HAS_WINDOW
				for (BYTE k = 0; k < BaseGrid::NumFamilies; k++)
				{
					high.family[k].SetOutdated(0);
					low.family[k].SetOutdated(0);
				}

				UnsetParaboloidAndSphereDisplay();
			#endif
			foreach (Resource::all,res)
				memset(res->known,false,sizeof(res->known));
		#endif

		#ifdef DEBUG_SENSORY
			sensors.Clear();
		#endif
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

	void	SetAllFamiliesOutdated(float t)
	{
		for (BYTE k = 0; k < BaseGrid::NumFamilies; k++)
		{
			high.family[k].SetOutdated(t);
			low.family[k].SetOutdated(t);
		}
	}


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



	bool	AffectedByEMP(const HCellIdI&id)
	{
		return !(Grid::high.cells[id.stackID].cornerFlags & empZonesUnlocked);
	}
	bool	AffectedByEMP(const LCellIdI&id)
	{
		return !(Grid::low.cells[id.stackID].cornerFlags & empZonesUnlocked);
	}








	#ifdef CLIENT
		void		UpdateResourceMap(ConstName resourceName,const ArrayData<TCellResourceCoverage>&data)
		{
			foreach (Resource::all, res)
				if (res->GetCName() == resourceName)
				{
					foreach (data,el)
					{
						ASSERT_LESS__(el->cellIndex,ARRAYSIZE(res->coverage));
						res->coverage[el->cellIndex] = el->coverage;
						res->known[el->cellIndex] = true;
						low.family[low.grid.GetCell(el->cellIndex).family].SetOutdated(0);
					}
					return;
				}
			FATAL__("Unable to locate resource type #"+String((int)resourceName));
		}
	#endif

	#ifdef HAS_WINDOW




		//void	OrbitGrid::Color(const LCellIdI&center, int fuelRadius)
		//{
		//	TColorArea newArea = {center,fuelRadius,true};
		//	if (newArea == colorArea)
		//		return;
		//}
		//void	OrbitGrid::StripColor()
		//{
		//	if (!colorArea.active)
		//		return;
		//	colorArea.active = false;
		//}


	void	OrbitGrid::Render(const Engine::Frustum<>&frustum)
	{
		if (!valid)
			ForceRecompileNow();
		display.StoreRendererState();
			display.SetBlendMode(Engine::BlendMode::AlphaBlend);
			display.SetDepthTest(true,false);
			display.SetRasterizer(true,true);
			display.Render(instruction,true);
		display.RestoreRendererState();
	}


	void	OrbitGrid::ForceRecompileNow()
	{
		valid = true;

		const count_t numVertices = low.cellVertices.Count();
		vertices.SetSize(numVertices);
		Concurrency::parallel_for(index_t(0),numVertices,[this](index_t i)
		{
			Vertex&out = vertices[i];
			const BaseGrid::TCellVertex&in = low.cellVertices[i];
			
			float opacity = cells[in.owner[0]].visible ? 1.f : 0.f;
			float emp = !(low.grid.GetCell(in.owner[0]).cornerFlags & empZonesUnlocked) ? 1.f : 0.f;
			for (BYTE k = 1; k < in.ownerCount; k++)
			{
				opacity += cells[in.owner[k]].visible ? 1.f : 0.f;
				emp += !(low.grid.GetCell(in.owner[k]).cornerFlags & empZonesUnlocked) ? 1.f : 0.f;
			}

			opacity /= in.ownerCount;
			emp /= in.ownerCount;
			
			out.Set(in.centerDirection,in.ownerCount>1 ? 1.f : 0.f,orbit,opacity,emp);
		});

		((Engine::DX11::BaseInstructionBlock&)instruction) = Instruction::orbitGridBase;
		instruction.ibo = low.gridInstruction.ibo;
		instruction.vbo.CreateFromStructs(vertices);
	}



	#endif


	#ifdef CLIENT


	
	void		OrbitGrid::ChangeSensory(const LCellIdI&center, float radius, signed char change)
	{
		Timer::Time t0 = timer.Now();
		LogEvent(EventSignificance::Debug,nullptr,"Processing orbital sensor change around center "+center.ToString()+", radius="+String(radius)+", change="+String(change));

		Math::Box<> test;
		const TVec3<> worldCenter = ToWorld(center);
		test.SetCenter(worldCenter,radius);
		float r2 = radius*radius;
		Concurrency::parallel_for(index_t(0),LowSetup::GridType::CellCount,[this,worldCenter,r2,change](index_t i)
		{
			//bool anyChanged = false;
			bool anyVisible = false;
			UINT32 cell((UINT32)i);

			bool wasIn = false;
			TVec3<> localCenter = low.cells[i].centerVertex.xyz * orbit;
			float dist2 = Vec::quadraticDistance(localCenter,worldCenter);
			if (dist2 <= r2)
			{
				cells[i].visible += change;
				DBG_ASSERT_GREATER_OR_EQUAL__(cells[i].visible,0);

				//DBG_ASSERT_LESS_OR_EQUAL__(cells[i].visible,1);	//enable to debug in scenarios where sensors do not overlap
				valid = false;
			}
		});

		LogEvent(EventSignificance::PerformanceMeasurement,nullptr,"Orbital sensory change processed in "+String(timer.GetSecondsSince(t0))+". Used orbit was "+String(orbit));
		//ShowMessage(timer.GetSecondsSince(t0));
	}

	bool		Volume::GridCanSeeAny(const HCellIdU&base, UINT height) const
	{
		const auto&stack = cellStack[base.stackID];
		UINT end = std::min(base.layer + height,CellStack::Range);
		for (UINT layer = base.layer; layer < end; layer++)
			if (stack.volumeCell[layer].visible > 0)
				return true;
		return false;
	}

	bool		Volume::GridCanSee(const LCellIdI&center, int height, bool broad) const
	{
		UINT32 fine = low.grid.GetCell(center.stackID).higherEquivalent;
		UINT32 layer0 = CellStack::ToLayer(center.height);
		if (GridCanSeeAny(HCellIdU(fine,layer0),(UINT)height))
			return true;
		if (!broad)
			return false;
		const auto&n = high.grid.GetCell(fine).neighborhood;
		for (int i = 0; i < n.numNeighbors; i++)
			if (GridCanSeeAny(HCellIdU(n.neighborID[i],layer0),(UINT)height))
				return true;
		return false;
	}
	

	void		Volume::ChangeSensorCoverage(const Sphere<>&state, signed char change)
	{
		bool familyChanged[BaseGrid::NumFamilies] = {false};
		TemplateHelper::ChangeSensory(state.center,state.radius,change,cellStack.pointer(),familyChanged);
		for (index_t i = 0; i < BaseGrid::NumFamilies; i++)
			if (familyChanged[i])
				Grid::high.family[i].SetOutdated(0);
	}

	void		Volume::ChangeSensorCoverage(const HighSetup::TVolumeDescriptor&state, signed char change)
	{
		bool familyChanged[BaseGrid::NumFamilies] = {false};
		TemplateHelper::ChangeSensory(state,change,cellStack.pointer(),familyChanged);
		for (index_t i = 0; i < BaseGrid::NumFamilies; i++)
			if (familyChanged[i])
				Grid::high.family[i].SetOutdated(0);
	}

	void		Volume::ChangeSensorCoverage(const SkySensorState&state, signed char change)
	{
		bool familyChanged[BaseGrid::NumFamilies] = {false};
		TemplateHelper::ChangeSensorCoverage(state,change,cellStack.pointer(),familyChanged);
		for (index_t i = 0; i < BaseGrid::NumFamilies; i++)
			if (familyChanged[i])
				Grid::high.family[i].SetOutdated(0);
	}

	void		Volume::ChangeSensory(const LCellIdI&center, float radius, signed char change)
	{
		bool familyChanged[BaseGrid::NumFamilies] = {false};
		TemplateHelper::ChangeSensory(Grid::ToWorld(center),radius,change,cellStack.pointer(),familyChanged);
		for (index_t i = 0; i < BaseGrid::NumFamilies; i++)
			if (familyChanged[i])
				Grid::high.family[i].SetOutdated(0);
		
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



	bool		Volume::HasSensor(const LCellIdI&cell, float radius) const
	{
		#ifdef DEBUG_SENSORY
			TSensor s;
			s.center = cell;
			s.radius = radius;
			return sensors.Contains(s);	
		#else
			return true;
		#endif
	}

	void				Volume::InsertSensor(const Sphere<>&desc)
	{
		ChangeSensorCoverage(desc,1);
	}

	void				Volume::RemoveSensor(const Sphere<>&desc)
	{
		ChangeSensorCoverage(desc,-1);
	}

	void				Volume::InsertSensor(const HighSetup::TVolumeDescriptor&desc)
	{
		ChangeSensorCoverage(desc,1);
	}

	void				Volume::RemoveSensor(const HighSetup::TVolumeDescriptor&desc)
	{
		ChangeSensorCoverage(desc,-1);
	}

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

	#endif

	void TSharedPlanetConfig::Apply() const
	{
		Grid::Content coreContent = Grid::Content::Reinterpret(innerCoreType);
		Grid::CellStack::bottom.content = coreContent;
		#ifdef HAS_WINDOW
			Grid::UpdateAtmosphere(atmosphereColor);
			Grid::UpdateSun(sunPlaneNormal_Speed,sunColor);
		#endif
	}

	/**/					Volume::Volume():cellStack(HighSetup::GridType::CellCount)
	{}

}
