#include "water.h"

namespace VisualTerrain
{

	using namespace DeltaWorks;
	using namespace DeltaWorks::Math;

	class WaterCompiler
	{
	public:
		typedef Grid::CellStack CellStack;
		typedef Grid::FClipHeight FClipHeight;
		typedef Grid::Content Content;

		static const float4 waterColor;


		StaticMesh<WaterVertex, BatchSize * CellStack::Range * (7 + 7 + 4 * 6),BatchSize * CellStack::Range * 3 * (7 * 2 + 2 * 7)>	mesh;
		

		const CellStack * stacks;
		const BaseGrid::TCell*  gridCells;
		const BaseGrid::TNetCell*  netCells;
		float	cornerBrightness[6];
		FClipHeight	clipHeight;
		bool	wasEmpty;

		void	Setup(const CellStack *stacks,const BaseGrid::TCell*cells, const BaseGrid::TNetCell*netCells,FClipHeight clipHeight)
		{
			mesh.Reset();
			this->stacks = stacks;
			this->netCells = netCells;
			this->gridCells = cells;
			this->clipHeight = clipHeight;
		}


		void	Compile(const index_t offset, const index_t end, THull<WaterVertex>&, Sys::SpinLock&lock);
		void	CompileLayer(	const index_t i, const UINT ih, const CellStack&stack,
								const int clipH,const BaseGrid::TNetCell&cell,
								const BaseGrid::TCellNeighborhood&neighborhood, bool localEMP);
		void	CompileFaceToNeighbor(const int j, const UINT ih, const int ir,const BaseGrid::TNetCell&cell,const BaseGrid::TCellNeighborhood&neighbors,float newCornerColor[6], 
									const CellStack::Cell&thisCell, const TFloatRange<>&range, bool localEMP);
		void	CompileBottomLayer(const CellStack&stack, const int clipH,const BaseGrid::TNetCell&cell,
									const BaseGrid::TCellNeighborhood&neighbors, bool empThis);
		void	CompileCap(	const UINT ih,const BaseGrid::TNetCell&cell, const bool capped, const CellStack::Cell&thisCell, 
							const TFloatRange<>&range, const BaseGrid::TCellNeighborhood&neighbors, 
							const CellStack&stack, bool empThis);

		static float	GetWaterLightLevel(const CellStack::Cell&cell);
		float			GetWaterLightLevel(UINT32 stackID, UINT32 layer)	const;
	};

	/*static*/ const float4 WaterCompiler::waterColor = float4(0.5f, 0.6f, 1.f,0.6f);

	

	void	WaterCompiler::CompileBottomLayer(const CellStack&stack, const int clipH,const BaseGrid::TNetCell&cell,const BaseGrid::TCellNeighborhood&neighbors, bool empThis)
	{
		//static const int ir = -CellStack::HalfRange;
		//static const UINT ih = 0;

		//if (IsClipped(clipHeight, i, ir))
			//break;

		const CellStack::Cell&thisCell = CellStack::bottom;
		const CellStack::Cell&nextCell = stack.GetCellU(0);

			
		if (!thisCell.IsWater() || nextCell.IsWater())
		{
			return;
		}


		const TFloatRange<> range = FloatRange(
			Grid::ToRadius(-1-CellStack::HalfRange),
			Grid::ToRadius(-CellStack::HalfRange));
		{
			//float b = smoothstep(0.9, 1, range.max / WorldScale)*0.9f + 0.1f;
			static const float delta = 0.1f;

			//mesh.SetOffset();

			//const TVec3<> baseColor = thisCell.GetBaseColor();
			//const TVec3<> nextColor = nextCell.IsOpaque() && nextCell.Mixes() ? nextCell.GetBaseColor() : baseColor;
			//const bool capped = nextCell.IsOpaque();
			//if (!capped
				//)
			{
				//CompileCap(ih+1,cell,capped,thisCell,range,neighbors,stack);
				CompileCap(0,cell,false,thisCell,range,neighbors,stack,empThis);
			}


		}
		
	}


	void	WaterCompiler::CompileFaceToNeighbor(const int j, const UINT ih, const int ir,const BaseGrid::TNetCell&cell,
											const BaseGrid::TCellNeighborhood&neighbors,float newCornerLight[6], const CellStack::Cell&thisCell, 
											const TFloatRange<>&range,
											bool localEMP )
	{
			//const UINT32 n1 = neighbors.neighborID[(j + 1) % cell.numCorners];
			//const CellStack&sn = stacks[n1];
			//const bool filled = sn.GetCellU(ih).content == CellStack::Cell::Content::Water;
			//{


			//	if (!filled )
			//	{
			//		//const UINT32 n0 = neighbors.neighborID[j];
			//		//const UINT32 n2 = neighbors.neighborID[(j + 2) % cell.numCorners];
			//		float	light1 = newCornerLight[j],
			//				light2 = newCornerLight[(j + 1) % cell.numCorners],
			//				lowerLight1 = wasEmpty ? newCornerLight[j] : cornerBrightness[j],
			//				lowerLight2 = wasEmpty ? newCornerLight[(j + 1) % cell.numCorners] : cornerBrightness[(j + 1) % cell.numCorners];
			//		
			//		TVec3<> normal;
			//		const auto&v1 = cell.cornerVertex[(j + 1) % cell.numCorners];
			//		const auto&v0 = cell.cornerVertex[j];
			//		Vec::cross(v0.xyz, v0.xyz - v1.xyz, normal);
			//		Vec::normalize(normal);

			//		mesh.SetOffset();
			//		mesh <<  WaterVertex(v0, range.max, normal, light1);
			//		mesh <<  WaterVertex(v0, range.min, normal, lowerLight1);
			//		mesh <<  WaterVertex(v1, range.min, normal, lowerLight2);
			//		mesh <<  WaterVertex(v1, range.max, normal, light2);
			//		mesh << 0 << 1 << 2 << 0 << 2 << 3;
			//	}
			//}


		const UINT32 n0 = neighbors.neighborID[j];
		const UINT32 n1 = neighbors.neighborID[(j + 1) % cell.numCorners];
//		const UINT32 n2 = neighbors.neighborID[(j + 2) % cell.numCorners];
		const CellStack&sn = stacks[n1];
		const bool filled = sn.GetCellU(ih).content == CellStack::Cell::Content::Water;
		//const bool filled = sn.GetCellU(ih).IsOpaque();
		//bool wasEmpty = false;
		const bool ncutoff = Grid::IsClipped(clipHeight, n1, ir);
		{
			//TVec4<>	color1 = newCornerColor[j],
			//		color2 = newCornerColor[(j + 1) % cell.numCorners],
			//		lowerColor1 = wasEmpty ? newCornerColor[j] : cornerColor[j],
			//		lowerColor2 = wasEmpty ? newCornerColor[(j + 1) % cell.numCorners] : cornerColor[(j + 1) % cell.numCorners];

			//if (ncutoff && filled)
			//{
			//	color1 = float4();
			//	color2 = float4();
			//	lowerColor1 = float4();
			//	lowerColor2 = float4();
			//}
			//if (!thisCell.Mixes() || !lastCell.Mixes())
			//{
			//	lowerColor1.xyz = color1.xyz;// * lowerColor1.w;
			//	lowerColor2.xyz = color2.xyz;// * lowerColor2.w;
			//}
			//else
			//{
			//	//lowerColor1.xyz = lowerColor1.xyz * lowerColor1.w;
			//	//lowerColor2.xyz = lowerColor2.xyz * lowerColor2.w;
			//}
			////color1.xyz = color1.xyz * color1.w;
			////color2.xyz = color2.xyz * color2.w;

			if (!filled || ncutoff)
			{
				float	light1 = newCornerLight[j],
						light2 = newCornerLight[(j + 1) % cell.numCorners],
						lowerLight1 = wasEmpty ? newCornerLight[j] : cornerBrightness[j],
						lowerLight2 = wasEmpty ? newCornerLight[(j + 1) % cell.numCorners] : cornerBrightness[(j + 1) % cell.numCorners];
						
				TVec3<> normal;
				const auto&v1 = cell.cornerVertex[(j + 1) % cell.numCorners];
				const auto&v0 = cell.cornerVertex[j];
				Vec::cross(v0.xyz, v0.xyz - v1.xyz, normal);
				Vec::normalize(normal);


				bool empThat0 = !(this->gridCells[n0].cornerFlags & Grid::empZonesUnlocked);
				bool empThat1 = !(this->gridCells[n1].cornerFlags & Grid::empZonesUnlocked);
				int empMix = (int(localEMP) + int(empThat0) + int(empThat1));

				mesh.SetOffset();
				mesh <<  WaterVertex(v0, range.max, normal, light1, empMix);
				mesh <<  WaterVertex(v0, range.min, normal, lowerLight1, empMix);
				mesh <<  WaterVertex(v1, range.min, normal, lowerLight2, empMix);
				mesh <<  WaterVertex(v1, range.max, normal, light2, empMix);
				mesh << 0 << 1 << 2 << 0 << 2 << 3;

/*

				TVec3<> normal;
				const auto&v1 = cell.cornerVertex[(j + 1) % cell.numCorners];
				const auto&v0 = cell.cornerVertex[j];
				Vec::cross(v0.xyz, v0.xyz - v1.xyz, normal);
				Vec::normalize(normal);
				solidMesh.SetOffset();
				solidMesh <<  TerrainVertex(v0, range.max, normal, color1, 1.f);
				solidMesh <<  TerrainVertex(v0, range.min, normal, lowerColor1, 1.f);
				solidMesh <<  TerrainVertex(v1, range.min, normal, lowerColor2, 1.f);
				solidMesh <<  TerrainVertex(v1, range.max, normal, color2, 1.f);
				solidMesh << 0 << 1 << 2 << 0 << 2 << 3;*/
			}


		}
	
	}

	/*static*/ float	WaterCompiler::GetWaterLightLevel(const CellStack::Cell&cell)
	{
		#ifdef DEBUG_HIDE_SENSORY
			return 1.f;
		#else
			return float(cell.IsCurrentlyVisible());
		#endif
	}

	float	WaterCompiler::GetWaterLightLevel(UINT32 stackID, UINT32 layer) const
	{
		return GetWaterLightLevel(stacks[stackID].volumeCell[vmin(layer,CellStack::Range-1)]);
	}



	void	WaterCompiler::CompileCap(const UINT ih,const BaseGrid::TNetCell&cell, const bool capped, const CellStack::Cell&thisCell, const TFloatRange<>&range,const BaseGrid::TCellNeighborhood&neighbors, const CellStack&stack, bool empThis)
	{
		const UINT layer = vmin(ih, CellStack::Range-1);
		float light0 = GetWaterLightLevel(stack.volumeCell[layer]);
		
		mesh.SetOffset();
		mesh <<  WaterVertex(cell.centerVertex, range.max, cell.centerVertex.xyz, light0,empThis?3:0);
		for (int j = 0; j < cell.numCorners; j++)
		{
			{
				float brightness = light0;
				UINT32 n1 = neighbors.neighborID[(j + 1) % cell.numCorners];
				UINT32 n0 = neighbors.neighborID[j];
				{
					const auto& sn1 = stacks[n1];
					const auto&	sn2 = stacks[n0];
					brightness += GetWaterLightLevel(sn1.volumeCell[layer]);
					brightness += GetWaterLightLevel(sn2.volumeCell[layer]);

					brightness /= 3;
				}
				bool empThat0 = !(this->gridCells[n0].cornerFlags & Grid::empZonesUnlocked);
				bool empThat1 = !(this->gridCells[n1].cornerFlags & Grid::empZonesUnlocked);

				int empMix = (int(empThis) + int(empThat0) + int(empThat1));

				mesh << WaterVertex(cell.cornerVertex[j], range.max, cell.cornerVertex[j].xyz, brightness,empMix);
			}
					
			mesh << 0 <<  1 + (UINT16)j << 1 + (UINT16)(j + 1) % cell.numCorners;
		}
	}


	void	WaterCompiler::CompileLayer(const index_t i, const UINT ih, const CellStack&stack, const int clipH, const BaseGrid::TNetCell&cell,const BaseGrid::TCellNeighborhood&neighbors, bool empThis)
	{
		const int ir = int(ih) - CellStack::HalfRange;
		//if (clipped(i, ir))
			//break;

		const CellStack::Cell&thisCell = stack.volumeCell[ih];
		const CellStack::Cell&nextCell = stack.GetCellU(ih + 1);


		if (thisCell.content != CellStack::Cell::Content::Water)
		{
			wasEmpty = true;
			return;
		}

		const TFloatRange<> range = FloatRange(
			Grid::ToRadius(ir),
			Grid::ToRadius(ir + 1));
		{
			static const float delta = 0.1f;
	
			const bool capped = nextCell.content == CellStack::Cell::Content::Water;
			if (!capped
				|| ir+1 >= clipH
				)
			{
							
				//color0.rgb = waterColor;
				//CompileCap(const UINT ih,const BaseGrid::TNetCell&cell, const bool capped, const CellStack::Cell&thisCell, const TFloatRange<>&range,const BaseGrid::TCellNeighborhood&neighbors, const CellStack&stack)
				CompileCap(ih+1,cell,capped,thisCell,range,neighbors,stack, empThis);
			
			}

			float newCornerLight[6];

			for (int j = 0; j < cell.numCorners; j++)
			{
				const UINT32 n0 = neighbors.neighborID[j];
				const UINT32 n1 = neighbors.neighborID[(j + 1) % cell.numCorners];
				const CellStack&sn = stacks[n1];
				{
					float c1 = (GetWaterLightLevel(n1,ih) + GetWaterLightLevel(n1,ih+1))/2.f;
					float color1 = (c1 * 2.f
						+
						((GetWaterLightLevel(n0,ih)) + (GetWaterLightLevel(n0,ih+1))) / 2.f);	//originally 'mild' levels

					color1 /= 3.f;
					newCornerLight[j] = color1;
				}
			}
			for (int j = 0; j < cell.numCorners; j++)
			{
				CompileFaceToNeighbor(j,ih,ir,cell,neighbors,newCornerLight,thisCell,range,empThis);
					
			}
			for (int j = 0; j < cell.numCorners; j++)
				cornerBrightness[j] = newCornerLight[j];
			wasEmpty = false;
		}
	}


	void	WaterCompiler::Compile(const index_t offset, const index_t end, THull<WaterVertex>&coverHull, Sys::SpinLock&lock)
	{

		//for (index_t i = offset; i < offset+BatchSize && i < end; i++ )
		//{
		//	const ::Grid::CellStack&stack = stacks[i];
		//	const auto&neighbors = gridCells[i].neighborhood;
		//	const auto&cell = netCells[i];



		//	UINT32 begin = stack.lowestOpenCell ? (UINT32)stack.lowestOpenCell - 1 : 0;
		//	begin = std::min(begin, CellStack::ToLayer(clipHeight(i)-1));
		//	//while (begin > 0 && clipped(i, begin))
		//		//begin--;
		//	UINT32 end = (UINT32)stack.highestOpaqueCell + 1;

		//	//TVec4<> cornerColor[6];

		//	for (int j = 0; j < cell.numCorners; j++)
		//	{
		//		UINT32 n = neighbors.neighborID[j];
		//		const ::Grid::CellStack&stack = stacks[n];
		//		UINT32 _begin = stack.lowestOpenCell ? (UINT32)stack.lowestOpenCell - 1 : 0;
		//		_begin = std::min(_begin, CellStack::ToLayer(clipHeight(n)-1));
		//		//while (_begin > 0 && clipped(n, _begin))
		//		//	_begin--;
		//		UINT32 _end = (UINT32)stack.highestOpaqueCell + 1;
		//		begin = std::min(begin, _begin);
		//		end = std::max(end, _end);

		//	}
		//	wasEmpty = true;

		//	const int clipH = clipHeight(i);
		//	end = std::min(end, CellStack::ToLayer(clipH));
		//	if (begin == 0)
		//		CompileBottomLayer(stack,clipH,cell,neighbors);
		//	for (UINT ih = begin; ih < end; ih++)
		//	{
		//		CompileLayer(ih,stack,clipH,cell,neighbors);
		//	}
		//}

		for (index_t i = offset; i < offset+BatchSize && i < end; i++ )
		{
			const ::Grid::CellStack&stack = stacks[i];
			const auto&cell = netCells[i];
			const bool empThis = !(cell.cornerFlags & Grid::empZonesUnlocked);
			if (!stack.HasWater())
			{
				if (CellStack::bottom.IsWater())
					CompileBottomLayer(stack, 100,netCells[i],gridCells[i].neighborhood,empThis);

				continue;
			}

			const auto&neighbors = gridCells[i].neighborhood;


			
			TIntRange<BYTE> waterRange = stack.waterRange;

			//waterRange.end = (BYTE)std::min<UINT>(waterRange.end, CellStack::ToLayer(clipHeight(i)));
			waterRange.start = (BYTE)std::min<UINT>(waterRange.start, CellStack::ToLayer(clipHeight(i)-1));

			for (int j = 0; j < cell.numCorners; j++)
			{
				UINT32 n = neighbors.neighborID[j];
				const ::Grid::CellStack&stack = stacks[n];
				TIntRange<BYTE> otherRange = stack.waterRange;
				//otherRange.end = (BYTE)std::min<UINT>(otherRange.end, CellStack::ToLayer(clipHeight(n)));
				otherRange.start = (BYTE)std::min<UINT>(otherRange.start, CellStack::ToLayer(clipHeight(n)-1));

				waterRange.Include(otherRange);
			}
			wasEmpty = true;
			//static const float waterOpacity = 0.6f;

			const int clipH = clipHeight(i);
			waterRange.end = (BYTE)std::min<UINT>(waterRange.end, CellStack::ToLayer(clipH));
			if (CellStack::bottom.IsWater())
				CompileBottomLayer(stack, clipH,cell,neighbors,empThis);
			for (UINT ih = waterRange.start; ih < waterRange.end; ih++)
			{
				CompileLayer(i,ih,stack, clipH,cell,neighbors,empThis);
			}
		}

		if (mesh.IsNotEmpty())
		{
			lock.lock();

			mesh.AppendTo(coverHull);

			lock.unlock();
		}
	}



	void		CompileWater(const Array<Grid::CellStack>&stacks, THull<WaterVertex>&cover, index_t offset, count_t count, Grid::FClipHeight clipHeight)
	{
		const index_t end = offset + count;
		Sys::SpinLock lock;
		Concurrency::parallel_for(offset, end, BatchSize,[end,&lock, &cover, &stacks, clipHeight](index_t i)	//looks good - at least in release
		{
			BYTE data[sizeof(WaterCompiler)];	//prevent implicit memset in release mode. i'm serious. visual studio has some funny ideas there...
			WaterCompiler&compiler = *(WaterCompiler*)data;
			compiler.Setup(stacks.pointer(),Grid::high.grid.GetCells(),Grid::high.cells.pointer(),clipHeight);
			compiler.Compile(i,end,cover,lock);
		});
	}


}
