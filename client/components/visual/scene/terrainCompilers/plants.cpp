#include "plants.h"

namespace VisualTerrain
{
	using namespace Grid;

	namespace Instruction
	{
		ShadedInstructionBlock32I			plantBase;
		Engine::DX11::InstructionBlock32I	plantShadowBase;
		Engine::DX11::InstructionBlock32I::Prototype	tree,dendrite;
		//Engine::DX11::BaseInstructionBlock	leafBlobShadowBase,treeTrunkShadowBase;
		//ShadedInstruction					leafBlobBase, treeTrunkBase;
	}
	typedef M::TVec3<>	V3;



	class PlantConstructor
	{
	public:
		struct TVertex
		{
			float3		position,
						normal;
			float4		colorRGB_windSensitivity;
		};
		static_assert(sizeof(TVertex) == 10 * sizeof(float),"Alignment error");


		Buffer0<TVertex>	vertices;
		Buffer0<UINT32>		indices;

		float3				currentColor = float3(1);
		index_t				indexOffset = 0;

		void			Vertex(const float3&position, float windSensitivity)
		{
			TVertex&vtx = vertices.Append();
			vtx.position = position;
			vtx.colorRGB_windSensitivity = float4(currentColor,windSensitivity);
		}
		
		void			SetIndexOffset()
		{
			indexOffset = vertices.Count();
		}


		void			Triangle(index_t v0, index_t v1, index_t v2)
		{
			Index(v0);
			Index(v1);
			Index(v2);
		}

		void			TriangleInv(index_t v0, index_t v1, index_t v2)
		{
			Index(v0);
			Index(v2);
			Index(v1);
		}

		void			Quad(index_t v0, index_t v1, index_t v2, index_t v3)
		{
			Triangle(v0,v1,v2);
			Triangle(v0,v2,v3);
		}

		void			QuadInv(index_t v0, index_t v1, index_t v2, index_t v3)
		{
			TriangleInv(v0,v1,v2);
			TriangleInv(v0,v2,v3);
		}




		void			Finish(DX11::InstructionBlock32I::Prototype&target)
		{
			for (index_t i = 0; i < indices.Count(); i+=3)
			{
				M::TVec3<> n;
				auto&v0 = vertices[indices[i]];
				auto&v1 = vertices[indices[i+1]];
				auto&v2 = vertices[indices[i+2]];
				M::Obj::triangleNormal(v0.position,v1.position,v2.position,n);
				v0.normal += n;
				v1.normal += n;
				v2.normal += n;
			}
			foreach (vertices,vtx)
				Vec::normalize0(vtx->normal);

			target.ibo.Create(indices);
			target.vbo.CreateFromStructs(vertices);
		}

	private:

		void			Index(index_t i)
		{
			indices << UINT32(i + indexOffset);
		}


	};



	static void CreateTree(DX11::InstructionBlock32I::Prototype& target)
	{

		PlantConstructor c;

		//trunk:
		c.currentColor = float3(5,0.35,0.2);
		const float tr = 0.2f;	//trunk radius
		const float lr = 0.2f;	//leaf radius
		const float th = 1.f - lr;	//trunk height = leaf center
		const float wt = 1.f;	//wind top
		c.Vertex(float3(tr,0,0),0);
		c.Vertex(float3(0,tr,0),0);
		c.Vertex(float3(-tr,0,0),0);
		c.Vertex(float3(0,-tr,0),0);

		c.Vertex(float3(tr,0,th),wt);
		c.Vertex(float3(0,tr,th),wt);
		c.Vertex(float3(-tr,0,th),wt);
		c.Vertex(float3(0,-tr,th),wt);

		c.Quad(0,1,5,4);
		c.Quad(1,2,6,5);
		c.Quad(2,3,7,6);
		c.Quad(3,0,4,7);

		c.SetIndexOffset();

		c.currentColor = float3(0.4,1,0.3);
		
		c.Vertex(float3(0,0,th-lr),wt);
		c.Vertex(float3(lr,0,th),wt);
		c.Vertex(float3(0,lr,th),wt);
		c.Vertex(float3(-lr,0,th),wt);
		c.Vertex(float3(0,-lr,th),wt);
		c.Vertex(float3(0,0,th+lr),wt);

		c.Triangle(0,1,2);
		c.Triangle(0,2,3);
		c.Triangle(0,3,4);
		c.Triangle(0,4,1);

		c.Triangle(1,2,5);
		c.Triangle(2,3,5);
		c.Triangle(3,4,5);
		c.Triangle(4,1,5);

		c.Finish(target);








	}

	void	CreatePlants()
	{
		//LoadPrototype(Instruction::dendrite,"objects/dendrite.obj");
		//LoadPrototype(Instruction::tree,"objects/tree.obj");

		CreateTree(Instruction::tree);
		Instruction::tree.vDesc = Instruction::plantBase.prototype.vDesc;



	}


	class PlantCompiler
	{
	public:

		//static const constexpr float TreeHeight = SingleLayerHeight*0.5f;
		//static const constexpr float TreeRadius = SingleLayerHeight*0.25f;
		//static const constexpr float TrunkRadius = TreeRadius/4.f;

		const CellStack * stacks;
		const BaseGrid::TCell*  gridCells;
		const BaseGrid::TNetCell*  netCells;
		FClipHeight	clipHeight;
		bool	wasEmpty;
		TLitColor	cornerColor[6];
		//StaticList<TLeafBlob,BatchSize * CellStack::Range *3>	leafBlobs;
		//StaticList<TTreeTrunk,BatchSize * CellStack::Range *3>	treeTrunks;
		StaticList<TPlantInstance,BatchSize * CellStack::Range *3>	instances;
		
			
		void	PutTree(const M::TVec3<>&position, const M::TVec3<>&up,const M::TVec3<>&dir, float scale)
		{
			//const float singleLayerHeight = Grid::GetLayerHeight(locationHeight);
			//const float treeHeight = singleLayerHeight * 0.5f;
			//const float treeRadius = singleLayerHeight * 0.25f;
			//const float trunkRadius = treeRadius / 4.f;
			//leafBlobs.Append().Set(sys,dir,z+treeHeight,treeRadius,1.f);
			//treeTrunks.Append().Set(sys,dir,z,trunkRadius,treeHeight);
			instances.Append().Set(position,up,dir,scale);
		}

		void	Setup(const CellStack *stacks,const BaseGrid::TCell*cells, const BaseGrid::TNetCell*netCells,FClipHeight clipHeight)
		{
			instances.Reset();
			//leafBlobs.Reset();
			//treeTrunks.Reset();
			this->stacks = stacks;
			this->netCells = netCells;
			this->gridCells = cells;
			this->clipHeight = clipHeight;
			wasEmpty = true;
		}
		void	Compile(const index_t offset, const index_t end,TPlantHull<TPlantInstance>&treeTrunks,Sys::SpinLock&lock);
		//void	Compile(const index_t offset, const index_t end,TPlantHull<TLeafBlob>&leafBlobs,TPlantHull<TTreeTrunk>&treeTrunks,Sys::SpinLock&lock);
	};


	//void	PlantCompiler::Compile(const index_t offset, const index_t end,TPlantHull<TLeafBlob>&leafBlobs,TPlantHull<TTreeTrunk>&treeTrunks,Sys::SpinLock&lock)
	//{
	//	for (index_t i = offset; i < offset+BatchSize && i < end; i++ )
	//	{
	//		const ::Grid::CellStack&stack = stacks[i];
	//		const auto&cell = netCells[i];
	//		const bool empThis = !(cell.cornerFlags & empZonesUnlocked);
	//		const M::TVec3<>&up = cell.cornerCenter;
	//		M::TMatrix3<> sys;
	//		Mat::makeAxisSystem(up,2,sys);
	//			
	//		float orientation = float((i*45959)%10000)/10000.f * M_PIF*2.f;

	//		static const constexpr count_t NumDirs = 6;


	//		M::TVec3<> plantDirection[NumDirs];//for now only three plants in equally spaced directions
	//		for (index_t k = 0; k < NumDirs; k++)
	//		{
	//			float a = float(k)/float(NumDirs)*2.f*M_PIF+orientation;
	//			float x = cos(a);
	//			float y = sin(a);
	//			plantDirection[k] = sys.x * x + sys.y * y;
	//		}


	//		const float r = cell.minCornerDistance;
	//		const UINT32 to = M::Min(CellStack::Range, CellStack::ToLayer(clipHeight(i)));
	//		
	//		for (UINT ih = 0; ih < to; ih++)
	//		{
	//			if (ih+1 == CellStack::Range || stack.volumeCell[ih+1].content == Content::Clear)
	//				switch (stack.volumeCell[ih].content)
	//				{
	//					case Content::Trees:
	//					case Content::Forest:
	//					{
	//						const int hl = Grid::CellStack::ToHeight(ih);
	//						float h = ToRadius(CellStack::ToHeight(ih+1));
	//						const float r2 = r * h * 0.7f;
	//						for (index_t k = 0; k < NumDirs; k++)
	//							PutTree(sys,plantDirection[k]*r2,h,hl);
	//						PutTree(sys,M::Vector3<>::zero,h,hl);
	//					}
	//					break;
	//					case Content::Shrubs:
	//						//later
	//					break;
	//				}
	//		}
	//	}

	//	if (this->leafBlobs.IsNotEmpty() || this->treeTrunks.IsNotEmpty())
	//	{
	//		lock.lock();

	//		leafBlobs.vertices.Append(this->leafBlobs.begin(),this->leafBlobs.Count());
	//		treeTrunks.vertices.Append(this->treeTrunks.begin(),this->treeTrunks.Count());

	//		lock.unlock();
	//	}
	//}

	
	void	PlantCompiler::Compile(const index_t offset, const index_t end,TPlantHull<TPlantInstance>&instances,Sys::SpinLock&lock)
	{
		for (index_t i = offset; i < offset+BatchSize && i < end; i++ )
		{
			const ::Grid::CellStack&stack = stacks[i];
			const auto&cell = netCells[i];
			const bool empThis = !(cell.cornerFlags & empZonesUnlocked);
			const M::TVec3<>&up = cell.cornerCenter;
			M::TMatrix3<> sys;
			Mat::makeAxisSystem(up,2,sys);
				
			float orientation = float((i*45959)%10000)/10000.f * M_PIF*2.f;

			static const constexpr count_t NumDirs = 6;


			M::TVec3<> plantDirection[NumDirs];//for now only three plants in equally spaced directions
			for (index_t k = 0; k < NumDirs; k++)
			{
				float a = float(k)/float(NumDirs)*2.f*M_PIF+orientation;
				float x = cos(a);
				float y = sin(a);
				plantDirection[k] = sys.x * x + sys.y * y;
			}


			const float cellRadius = cell.minCornerDistance;
			const UINT32 to = M::Min(CellStack::Range, CellStack::ToLayer(clipHeight(i)));
			
			for (UINT ih = 0; ih < to; ih++)
			{
				if (ih+1 == CellStack::Range || stack.volumeCell[ih+1].content == Content::Clear)
					switch (stack.volumeCell[ih].content)
					{
						case Content::Trees:
						{
							const int height = CellStack::ToHeight(ih+1);
							const float heightRadius = ToRadius(height);
							const float r2 = cellRadius * heightRadius * 0.7f;
							const float scale = Grid::GetLayerHeight(height) * 0.5f;
							for (index_t k = 0; k < NumDirs; k+=2)
								PutTree(up*heightRadius+plantDirection[k]*r2,up,sys.x,scale);
						}
						break;
						case Content::Forest:
						{
							const int height = CellStack::ToHeight(ih+1);
							const float heightRadius = ToRadius(height);
							const float r2 = cellRadius * heightRadius * 0.7f;
							const float scale = Grid::GetLayerHeight(height) * 0.5f;
							for (index_t k = 0; k < NumDirs; k++)
								PutTree(up*heightRadius+plantDirection[k]*r2,up,sys.x,scale);
							PutTree(up*heightRadius,up,sys.x,scale);
						}
						break;
						case Content::Shrubs:
							//later
						break;
					}
			}
		}

		if (this->instances.IsNotEmpty())
		{
			lock.lock();

			instances.vertices.Append(this->instances.begin(),this->instances.Count());

			lock.unlock();
		}
	}


	void		CompilePlants(const Array<Grid::CellStack>&stacks, TPlantHull<TPlantInstance>&instances, index_t offset, count_t count, Grid::FClipHeight clipHeight)
	{
		const index_t end = offset + count;
		Sys::SpinLock lock;
		Concurrency::parallel_for(offset, end, BatchSize,[end,&lock, &instances, &stacks, clipHeight](index_t i)	//looks good - at least in release
		{
			BYTE data[sizeof(PlantCompiler)];	//prevent implicit memset in release mode. i'm serious. visual studio has some funny ideas there...
			PlantCompiler&compiler = *(PlantCompiler*)data;
			compiler.Setup(stacks.pointer(),Grid::high.grid.GetCells(),Grid::high.cells.pointer(),clipHeight);
			compiler.Compile(i,end,instances,lock);
		});
	}

	//void		CompilePlants(const Array<Grid::CellStack>&stacks, TPlantHull<TLeafBlob>&leafBlobs,TPlantHull<TTreeTrunk>&treeTrunks, index_t offset, count_t count, Grid::FClipHeight clipHeight)
	//{
	//	const index_t end = offset + count;
	//	Sys::SpinLock lock;
	//	Concurrency::parallel_for(offset, end, BatchSize,[end,&lock, &leafBlobs,&treeTrunks, &stacks, clipHeight](index_t i)	//looks good - at least in release
	//	{
	//		BYTE data[sizeof(PlantCompiler)];	//prevent implicit memset in release mode. i'm serious. visual studio has some funny ideas there...
	//		PlantCompiler&compiler = *(PlantCompiler*)data;
	//		compiler.Setup(stacks.pointer(),Grid::high.grid.GetCells(),Grid::high.cells.pointer(),clipHeight);
	//		compiler.Compile(i,end,leafBlobs,treeTrunks,lock);
	//	});
	//}	
}
