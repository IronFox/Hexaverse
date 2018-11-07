#include "solidTerrain.h"

#include <grid/worldData.h>

namespace VisualTerrain
{
	
	class TerrainCompiler
	{
	public:
		typedef Grid::CellStack	CellStack;
		typedef Grid::FClipHeight FClipHeight;
		typedef Grid::Content Content;


		StaticMesh<TerrainVertex,BatchSize * CellStack::Range * (7 + 7 + 4 * 6),BatchSize * CellStack::Range * 3 * (7 * 2 + 2 * 7)>	solidMesh;
		StaticMesh<OverlayVertex,BatchSize * 7,BatchSize * 3*7>	coverMesh;
		

		const CellStack * stacks;
		const BaseGrid::TCell*  gridCells;
		const BaseGrid::TNetCell*  netCells;
		FClipHeight	clipHeight;
		bool	wasEmpty;
		TLitColor	cornerColor[6];


		void	Setup(const CellStack *stacks, const BaseGrid::TCell*cells, const BaseGrid::TNetCell*netCells,FClipHeight clipHeight)
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
//			static void ApplyEMP(UINT16 cornerFlags,M::TVec4<>&rs);
		static bool FlagsAffectedByEMP(UINT16 cornerFlags) {return !(cornerFlags & Grid::empZonesUnlocked);}
		static bool AffectedByEMP(index_t stackIndex);


		void	Compile(const index_t offset, const index_t end);
		void	CompileLayer(	const UINT ih, const CellStack&, const int clipH,
								const BaseGrid::TNetCell&cell,const BaseGrid::TCellNeighborhood&neighborhood);
		void	CompileBottomLayer(	const CellStack&stack, const int clipH,
									const BaseGrid::TNetCell&cell,const BaseGrid::TCellNeighborhood&neighborhood);
		void	CompileAboveTerrainOutline(const UINT ih, const int ir,const BaseGrid::TNetCell&cell,const BaseGrid::TCellNeighborhood&neighbors);
		void	CompileCap(	const UINT ih,const BaseGrid::TNetCell&cell, const bool capped, const CellStack::Cell&thisCell, 
							const M::TFloatRange<>&,const BaseGrid::TCellNeighborhood&, const CellStack&);
		void	CompileDownCap(	const UINT ih,const BaseGrid::TNetCell&cell, const CellStack::Cell&thisCell, 
								const int ir,const BaseGrid::TCellNeighborhood&, const CellStack&);
		void	CompileFaceToNeighbor(const int j, const UINT ih, const int ir,const BaseGrid::TNetCell&cell,const BaseGrid::TCellNeighborhood&neighbors,TLitColor newCornerColor[6], 
										const CellStack::Cell&thisCell, const CellStack::Cell&lastCell, const M::TFloatRange<>&range);
		void	DeliverResults(THull<TerrainVertex>&solidHull, THull<OverlayVertex>&coverHull);
	};


	void	TerrainCompiler::CompileAboveTerrainOutline(const UINT ih, const int ir,const BaseGrid::TNetCell&cell,const BaseGrid::TCellNeighborhood&neighbors)
	{
		float h = Grid::ToRadius(ir + 1);
		coverMesh.SetOffset();
		coverMesh << OverlayVertex(cell.centerVertex, h, 0);
		for (int j = 0; j < cell.numCorners; j++)
		{
			const UINT32 n0 = neighbors.neighborID[j];
			const UINT32 n1 = neighbors.neighborID[(j + 1) % cell.numCorners];
			const UINT32 n2 = neighbors.neighborID[(j + 2) % cell.numCorners];
			bool anyOpen = !stacks[n0].GetCellU(ih + 1).IsOpaque() || !stacks[n1].GetCellU(ih + 1).IsOpaque();

			coverMesh << OverlayVertex(cell.cornerVertex[j], h, anyOpen ? 0.45f : 0.f);

			coverMesh << 0;
			coverMesh << 1 + (UINT16)j;
			coverMesh << 1 + (UINT16)(j + 1) % cell.numCorners;
		}	
	}

	TWeightedColor	TerrainCompiler::GetMixColorU(index_t stackIndex,  UINT ih, Content oppositeContent, float&brightness, int&sumVisibilityTo, Content&outContent) const
	{
		return VisualTerrain::GetMixColorU(stacks[stackIndex],ih,oppositeContent, brightness, sumVisibilityTo, outContent);
		//return rs;
	}

	TWeightedColor	TerrainCompiler::GetDownMixColorU(index_t stackIndex,  UINT ih, Content oppositeContent, float&brightness, int&sumVisibilityTo, Content&outContent) const
	{
		return VisualTerrain::GetDownMixColorU(stacks[stackIndex],ih, oppositeContent, brightness, sumVisibilityTo, outContent);
		//return rs;
	}



	/*static*/ bool TerrainCompiler::AffectedByEMP(index_t stackIndex)
	{
		return FlagsAffectedByEMP(Grid::high.cells[stackIndex].cornerFlags);
	}


	
	void	TerrainCompiler::CompileCap(const UINT ih,const BaseGrid::TNetCell&cell, const bool capped, const CellStack::Cell&thisCell, const M::TFloatRange<>&range,const BaseGrid::TCellNeighborhood&neighbors, const CellStack&stack)
	{

		bool mixes0;
		Content centerType;
		TLitColor color0 = GetColorU(stack,ih, Content::Clear, mixes0,centerType);
		color0.capped = capped;
		color0.emp = FlagsAffectedByEMP(cell.cornerFlags);
			
		solidMesh << TerrainVertex(cell.centerVertex, range.max, cell.centerVertex.xyz, color0, thisCell.GetStructureFactor());
		for (int j = 0; j < cell.numCorners; j++)
		{
			{
				float brightness = color0.lightLevel;
				TWeightedColor color;

				float emp = color0.emp;
				int visibility = color0.visibility > 0.5f;
				//if (capped)
				//{
				//	//brighness = 0.f;
				//	color = float4(0,0,0,1);
				//	brightness = 0;
				//}
				//else
				{
				//	color = TLitColor();
					Content c0,c1;
					index_t n1 = neighbors.neighborID[(j + 1) % cell.numCorners];
					index_t n2 = neighbors.neighborID[j];
					color += GetMixColorU(n1,ih,centerType,brightness,visibility,c0);
					color += GetMixColorU(n2, ih, centerType,brightness,visibility,c1);
					emp += AffectedByEMP(n1);
					emp += AffectedByEMP(n2);
					emp /= 3;

					TWeightedColor add(color0, 1.f);
					if (!mixes0)
					{
							//mild bleeding...
						if (c0 != centerType)
							add *= 2.f;
						if (c1 != centerType)
							add *= 2.f;
					}
					color += add;
					brightness /= 3;
					//color.rgb *= brightness / color.a;
					//brightness /= color.a;
				}
				TLitColor col(color,brightness,emp);
				col.visibility = float(visibility) / 3;
				col.capped = capped;

				solidMesh << TerrainVertex(cell.cornerVertex[j], range.max, cell.cornerVertex[j].xyz, col, 1.f);
			}
			solidMesh << 0 << 1 + (UINT16)j << 1 + (UINT16)(j + 1) % cell.numCorners;
		}	
	}

	
	void	TerrainCompiler::CompileDownCap(const UINT ih,const BaseGrid::TNetCell&cell, const CellStack::Cell&thisCell, const int ir,const BaseGrid::TCellNeighborhood&neighbors, const CellStack&stack)
	{
		float h = Grid::ToRadius(ir + 1);
		bool mixes0;
		solidMesh.SetOffset();
		Content centerType;
		TLitColor color0 = GetDownColorU(stack, ih, Content::Clear, mixes0,centerType);
		color0.emp = FlagsAffectedByEMP(cell.cornerFlags) ? 1.f : 0.f;

		solidMesh << TerrainVertex(cell.centerVertex, h, -cell.centerVertex.xyz, color0, thisCell.GetStructureFactor());

		for (int j = 0; j < cell.numCorners; j++)
		{
			{
				float brightness = color0.lightLevel;
				TWeightedColor color(color0.color, 1.f);
				int visibility = color0.visibility > 0.5f;
				float emp = color0.emp;
				{
					if (!mixes0)
						color *= 4.f; //mild bleeding
					Content c0,c1;
					index_t n1 = neighbors.neighborID[(j + 1) % cell.numCorners];
					index_t n2 = neighbors.neighborID[j];
					color += GetDownMixColorU(n1,ih + 1, centerType,brightness,visibility,c0);
					color += GetDownMixColorU(n2,ih + 1, centerType,brightness,visibility,c1);
					emp += AffectedByEMP(n1);
					emp += AffectedByEMP(n2);
					brightness /= 3;
					emp /= 3;
					//color.rgb *= brightness / color.a;
					//brightness /= color.a;
				}
				TLitColor col(color,brightness,emp);
				col.visibility = float(visibility) / 3;
				solidMesh << TerrainVertex(cell.cornerVertex[j], h, -cell.cornerVertex[j].xyz, col, 1.f);
			}
			solidMesh << 0 << 1 + (UINT16)(j + 1) % cell.numCorners << 1 + (UINT16)j;
		}	
	}

	void	TerrainCompiler::CompileFaceToNeighbor(const int j, const UINT ih, const int ir,const BaseGrid::TNetCell&cell,
											const BaseGrid::TCellNeighborhood&neighbors,TLitColor newCornerColor[6], const CellStack::Cell&thisCell, const CellStack::Cell&lastCell,
											const M::TFloatRange<>&range )
	{
		const UINT32 n0 = neighbors.neighborID[j];
		const UINT32 n1 = neighbors.neighborID[(j + 1) % cell.numCorners];
		const UINT32 n2 = neighbors.neighborID[(j + 2) % cell.numCorners];
		const CellStack&sn = stacks[n1];
		const bool filled = sn.GetCellU(ih).IsOpaque();
		//bool wasEmpty = false;
		const bool ncutoff = Grid::IsClipped(clipHeight, n1, ir);
		{
			TLitColor	color1 = newCornerColor[j],
					color2 = newCornerColor[(j + 1) % cell.numCorners],
					lowerColor1 = wasEmpty ? newCornerColor[j] : cornerColor[j],
					lowerColor2 = wasEmpty ? newCornerColor[(j + 1) % cell.numCorners] : cornerColor[(j + 1) % cell.numCorners];


			if (ncutoff && filled)
			{
				color1.capped = true;
				color2.capped = true;
				lowerColor1.capped = true;
				lowerColor2.capped = true;
			}
			if (!thisCell.Mixes() || !lastCell.Mixes())
			{
				lowerColor1.color = color1.color;// * lowerColor1.w;
				lowerColor2.color = color2.color;// * lowerColor2.w;
			}
			else
			{
				//lowerColor1.xyz = lowerColor1.xyz * lowerColor1.w;
				//lowerColor2.xyz = lowerColor2.xyz * lowerColor2.w;
			}
			//color1.xyz = color1.xyz * color1.w;
			//color2.xyz = color2.xyz * color2.w;

			if (!filled || ncutoff)
			{
				M::TVec3<> normal;
				const auto&v1 = cell.cornerVertex[(j + 1) % cell.numCorners];
				const auto&v0 = cell.cornerVertex[j];
				Vec::cross(v0.xyz, v0.xyz - v1.xyz, normal);
				Vec::normalize(normal);
				solidMesh.SetOffset();
				solidMesh <<  TerrainVertex(v0, range.max, normal, color1, 1.f);
				solidMesh <<  TerrainVertex(v0, range.min, normal, lowerColor1, 1.f);
				solidMesh <<  TerrainVertex(v1, range.min, normal, lowerColor2, 1.f);
				solidMesh <<  TerrainVertex(v1, range.max, normal, color2, 1.f);
				solidMesh << 0 << 1 << 2 << 0 << 2 << 3;
			}


		}
	
	}




	void	TerrainCompiler::CompileLayer(const UINT ih, const CellStack&stack, const int clipH,const BaseGrid::TNetCell&cell,const BaseGrid::TCellNeighborhood&neighbors)
	{
		const int ir = CellStack::ToHeight(ih);

		//if (IsClipped(clipHeight, i, ir))
			//break;

		const CellStack::Cell&lastCell = stack.GetCellI(ir - 1);
		const CellStack::Cell&thisCell = stack.volumeCell[ih];
		const CellStack::Cell&nextCell = stack.GetCellU(ih + 1);
			

		if (!thisCell.IsOpaque())
		{
			wasEmpty = true;
			//for (int j = 0; j < cell.numCorners; j++)
				//cornerColor[j] = float4(1);
			const bool capped = nextCell.IsOpaque();
			if (capped
				
				)
			{
				if (ir+1 >= clipH)
					CompileAboveTerrainOutline(ih,ir,cell,neighbors);
				else
				{
					CompileDownCap(ih,cell,thisCell,ir,neighbors,stack);
				}
			}
			return;
		}

		const M::TFloatRange<> range = M::FloatRange(
			Grid::ToRadius(ir),
			Grid::ToRadius(ir + 1));
		{
			//float b = M::SmoothStep(0.9, 1, range.max / WorldScale)*0.9f + 0.1f;
			static const float delta = 0.1f;

			solidMesh.SetOffset();

			TLitColor baseColor = TLitColor(GetBaseColor(thisCell),0.f,FlagsAffectedByEMP( cell.cornerFlags),thisCell.GetVisibility());
			TLitColor nextColor = TLitColor(nextCell.IsOpaque() && nextCell.Mixes() ? TLitColor(GetBaseColor(nextCell),0.f, baseColor.emp,thisCell.GetVisibility()) : baseColor);

			const bool capped = nextCell.IsOpaque();
			if (!capped
				|| ir+1 >= clipH
				)
			{
				CompileCap(ih+1,cell,capped,thisCell,range,neighbors,stack);
			}

			TLitColor newCornerColor[6];

			{
				TLitColor color = thisCell.Mixes() ? nextColor : baseColor;
				for (int j = 0; j < cell.numCorners; j++)
				{
					const UINT32 n0 = neighbors.neighborID[j];
					const UINT32 n1 = neighbors.neighborID[(j + 1) % cell.numCorners];
					const CellStack&sn = stacks[n1];
					{
						float c1 = (stacks[n1].GetCellU(ih).GetLightLevel() + stacks[n1].GetCellU(ih + 1).GetLightLevel()) / 2.f;
						float light = (c1 * 2.f
							+
							((stacks[n0].GetCellU(ih).GetLightLevel()) + (stacks[n0].GetCellU(ih + 1).GetLightLevel())) / 2.f);	//original 'mild' levels

						light /= 3.f;

						newCornerColor[j] = color;
						newCornerColor[j].lightLevel = light;
					}
				}
			}
			for (int j = 0; j < cell.numCorners; j++)
			{
				CompileFaceToNeighbor(j,ih,ir,cell,neighbors,newCornerColor,thisCell,lastCell,range);
			}
			for (int j = 0; j < cell.numCorners; j++)
				cornerColor[j] = newCornerColor[j];
			wasEmpty = false;
		}
		
	}
	

	void	TerrainCompiler::CompileBottomLayer(const CellStack&stack, const int clipH,const BaseGrid::TNetCell&cell,const BaseGrid::TCellNeighborhood&neighbors)
	{
		//static const int ir = -CellStack::HalfRange;
		//static const UINT ih = 0;

		//if (IsClipped(clipHeight, i, ir))
			//break;

		const CellStack::Cell&thisCell = CellStack::bottom;
		const CellStack::Cell&nextCell = stack.GetCellU(0);

			
		if (thisCell.content == Content::Clear)
		{
			if (nextCell.content != Content::Clear)
			{
				float h = Grid::ToRadius(-CellStack::HalfRange);
				bool mixes0;
				solidMesh.SetOffset();
				Content centerContent;
				TLitColor color0 = GetDownColorU(stack, 0, Content::Clear,mixes0,centerContent);
				solidMesh << TerrainVertex(cell.centerVertex, h, -cell.centerVertex.xyz, color0, thisCell.GetStructureFactor());
				for (int j = 0; j < cell.numCorners; j++)
				{
					{
						float brightness = color0.lightLevel;
						TWeightedColor color(color0.color, 1.f);
						int visibility = color0.visibility > 0.5f;
						{
							Content c0,c1;

							if (!mixes0)
								color *= 4.f; //mild bleeding
							const UINT32 n0 = neighbors.neighborID[(j + 1) % cell.numCorners];
							const UINT32 n1 = neighbors.neighborID[j];
							color += GetDownMixColorU(n0,0, centerContent,brightness,visibility,c0);
							color += GetDownMixColorU(n1,0, centerContent,brightness,visibility,c1);

							brightness /= 3;
							//color.rgb *= brightness / color.a;
							//brightness /= color.a;
						}
						TLitColor col(color,brightness);
						col.visibility = float(visibility)/3;
						solidMesh << TerrainVertex(cell.cornerVertex[j], h, -cell.cornerVertex[j].xyz, col, 1.f);
					}
					solidMesh << 0 << 1 + (UINT16)(j + 1) % cell.numCorners << 1 + (UINT16)j;
				}	

			}
			return;
		}


		const M::TFloatRange<> range = M::FloatRange(
			Grid::ToRadius(-1-CellStack::HalfRange),
			Grid::ToRadius(-CellStack::HalfRange));
		{
			//float b = M::SmoothStep(0.9, 1, range.max / WorldScale)*0.9f + 0.1f;
			static const float delta = 0.1f;

			solidMesh.SetOffset();

			//const TBaseColor baseColor = thisCell.GetBaseColor();
			//const TBaseColor nextColor = nextCell.IsOpaque() && nextCell.Mixes() ? nextCell.GetBaseColor() : baseColor;
			const bool capped = nextCell.IsOpaque();
			if (!capped
				)
			{
				CompileCap(0,cell,capped,thisCell,range,neighbors,stack);
			}


		}
		
	}


	void	TerrainCompiler::Compile(const index_t offset, const index_t end)
	{
		for (index_t i = offset; i < offset+BatchSize && i < end; i++ )
		{
			const ::Grid::CellStack&stack = stacks[i];
			const auto&neighbors = gridCells[i].neighborhood;
			const auto&cell = netCells[i];



			UINT32 begin = stack.lowestOpenCell ? (UINT32)stack.lowestOpenCell - 1 : 0;
			begin = std::min(begin, CellStack::ToLayer(clipHeight(i)-1));
			//while (begin > 0 && clipped(i, begin))
				//begin--;
			UINT32 end = (UINT32)stack.highestOpaqueCell + 1;

			//M::TVec4<> cornerColor[6];

			for (int j = 0; j < cell.numCorners; j++)
			{
				UINT32 n = neighbors.neighborID[j];
				const ::Grid::CellStack&stack = stacks[n];
				UINT32 _begin = stack.lowestOpenCell ? (UINT32)stack.lowestOpenCell - 1 : 0;
				_begin = std::min(_begin, CellStack::ToLayer(clipHeight(n)-1));
				//while (_begin > 0 && clipped(n, _begin))
				//	_begin--;
				UINT32 _end = (UINT32)stack.highestOpaqueCell + 1;
				begin = std::min(begin, _begin);
				end = std::max(end, _end);

			}
			wasEmpty = true;

			const int clipH = clipHeight(i);
			end = std::min(end, CellStack::ToLayer(clipH));
			if (begin == 0 || CellStack::bottom.content == Content::Clear)
				CompileBottomLayer(stack,clipH,cell,neighbors);
			if (end >= CellStack::Range)
				begin = std::min(begin,end - 1);
			for (UINT ih = begin; ih < end; ih++)
			{
				CompileLayer(ih,stack,clipH,cell,neighbors);
			}
		}
	}

	void TerrainCompiler::DeliverResults(THull<TerrainVertex>&solidHull, THull<OverlayVertex>&coverHull)
	{
		if (solidMesh.IsNotEmpty() || coverMesh.IsNotEmpty())
		{
			//lock.lock();

			if (solidMesh.IsNotEmpty())
			{
				solidMesh.AppendTo(solidHull);
			}

			if (coverMesh.IsNotEmpty())
			{
				coverMesh.AppendTo(coverHull);
			}

			//lock.unlock();
		}
		
	}




	void		CompileSolidTerrain(const Array<Grid::CellStack>&stacks, THull<TerrainVertex>&solid, THull<OverlayVertex>&cover, index_t offset, count_t count, Grid::FClipHeight clipHeight)
	{
		const index_t end = offset + count;
		//SpinLock lock;
		const count_t numBatches = M::CeilDiv(count,BatchSize);
		static Array<THull<TerrainVertex> >	hulls;
		static Array<THull<OverlayVertex> >	covers;
		count_t numWorkers = System::ParallelLoop::GetGlobalWorkerCount();
		if (hulls.Count() < numWorkers)
		{
			hulls.SetSize(numWorkers);
			covers.SetSize(numWorkers);
		}
				
				


		System::ParallelFor(numBatches,8,[end,offset, &stacks, clipHeight](index_t i, index_t workerIndex)
		{
			BYTE data[sizeof(TerrainCompiler)];	//prevent implicit memset in release mode. i'm serious. visual studio has some funny ideas there...
			TerrainCompiler&compiler = *(TerrainCompiler*)data;
			compiler.Setup(stacks.pointer(),Grid::high.grid.GetCells(),Grid::high.cells.pointer(),clipHeight);
			compiler.Compile(offset + i*BatchSize,end);
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


}
