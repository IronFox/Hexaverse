#include "plants.h"

namespace VisualTerrain
{
	using namespace Grid;

	namespace Instruction
	{
		Engine::DX11::BaseInstructionBlock	leafBlobShadowBase,treeTrunkShadowBase;
		ShadedInstruction					leafBlobBase, treeTrunkBase;
	}
	

	class PlantCompiler
	{
	public:

		static const constexpr float TreeHeight = SingleLayerHeight*0.5f;
		static const constexpr float TreeRadius = SingleLayerHeight*0.25f;
		static const constexpr float TrunkRadius = TreeRadius/4.f;

		const CellStack * stacks;
		const BaseGrid::TCell*  gridCells;
		const BaseGrid::TNetCell*  netCells;
		FClipHeight	clipHeight;
		bool	wasEmpty;
		TLitColor	cornerColor[6];
		StaticList<TLeafBlob,BatchSize * CellStack::Range *3>	leafBlobs;
		StaticList<TTreeTrunk,BatchSize * CellStack::Range *3>	treeTrunks;
		//more types added later
			
		void	PutTree(const TMatrix3<>&sys, const TVec3<>&dir, float z)
		{
			leafBlobs.Append().Set(sys,dir,z+TreeHeight,TreeRadius,1.f);
			treeTrunks.Append().Set(sys,dir,z,TrunkRadius,TreeHeight);
		}

		void	Setup(const CellStack *stacks,const BaseGrid::TCell*cells, const BaseGrid::TNetCell*netCells,FClipHeight clipHeight)
		{
			leafBlobs.Reset();
			treeTrunks.Reset();
			this->stacks = stacks;
			this->netCells = netCells;
			this->gridCells = cells;
			this->clipHeight = clipHeight;
			wasEmpty = true;
		}
		void	Compile(const index_t offset, const index_t end,TPlantHull<TLeafBlob>&leafBlobs,TPlantHull<TTreeTrunk>&treeTrunks,SpinLock&lock);
	};


	void	PlantCompiler::Compile(const index_t offset, const index_t end,TPlantHull<TLeafBlob>&leafBlobs,TPlantHull<TTreeTrunk>&treeTrunks,SpinLock&lock)
	{
		for (index_t i = offset; i < offset+BatchSize && i < end; i++ )
		{
			const ::Grid::CellStack&stack = stacks[i];
			const auto&cell = netCells[i];
			const bool empThis = !(cell.cornerFlags & empZonesUnlocked);
			const TVec3<>&up = cell.cornerCenter;
			TMatrix3<> sys;
			Mat::makeAxisSystem(up,2,sys);
				
			float orientation = float((i*45959)%10000)/10000.f * M_PIF*2.f;

			static const constexpr count_t NumDirs = 6;


			TVec3<> plantDirection[NumDirs];//for now only three plants in equally spaced directions
			for (index_t k = 0; k < NumDirs; k++)
			{
				float a = float(k)/float(NumDirs)*2.f*M_PIF+orientation;
				float x = cos(a);
				float y = sin(a);
				plantDirection[k] = sys.x * x + sys.y * y;
			}


			const float r = cell.minCornerDistance;
			const UINT32 to = vmin(CellStack::Range, CellStack::ToLayer( clipHeight(i) ));
			
			for (UINT ih = 0; ih < to; ih++)
			{
				if (ih+1 == CellStack::Range || stack.volumeCell[ih+1].content == Content::Clear)
					switch (stack.volumeCell[ih].content)
					{
						case Content::Trees:
						case Content::Forest:
						{
							float h = ToRadius(CellStack::ToHeight(ih+1));
							const float r2 = r * h * 0.7f;
							for (index_t k = 0; k < NumDirs; k++)
								PutTree(sys,plantDirection[k]*r2,h);
							PutTree(sys,Vector<>::zero,h);
						}
						break;
						case Content::Shrubs:
							//later
						break;
					}
			}
		}

		if (this->leafBlobs.IsNotEmpty() || this->treeTrunks.IsNotEmpty())
		{
			lock.lock();

			leafBlobs.vertices.Append(this->leafBlobs.begin(),this->leafBlobs.Count());
			treeTrunks.vertices.Append(this->treeTrunks.begin(),this->treeTrunks.Count());

			lock.unlock();
		}
	}

	void		CompilePlants(const Array<Grid::CellStack>&stacks, TPlantHull<TLeafBlob>&leafBlobs,TPlantHull<TTreeTrunk>&treeTrunks, index_t offset, count_t count, Grid::FClipHeight clipHeight)
	{
		const index_t end = offset + count;
		SpinLock lock;
		Concurrency::parallel_for(offset, end, BatchSize,[end,&lock, &leafBlobs,&treeTrunks, &stacks, clipHeight](index_t i)	//looks good - at least in release
		{
			BYTE data[sizeof(PlantCompiler)];	//prevent implicit memset in release mode. i'm serious. visual studio has some funny ideas there...
			PlantCompiler&compiler = *(PlantCompiler*)data;
			compiler.Setup(stacks.pointer(),Grid::high.grid.GetCells(),Grid::high.cells.pointer(),clipHeight);
			compiler.Compile(i,end,leafBlobs,treeTrunks,lock);
		});
	}	
}
