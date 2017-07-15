#include "resourceVisualization.h"
#include <grid/resources.h>
#include <grid/worldData.h>

namespace VisualTerrain
{


	class ResourceCompiler
	{
	public:
		static const count_t Resolution = 10;
		StaticMesh<ResourceVertex,BatchSize * 2 * 2 * (1 + Resolution + Resolution*2 ),BatchSize * 3 * 2* 2*(Resolution + 2*Resolution)>	mesh;
		
		const Grid::CellStack * stacks;
		const BaseGrid::TCell*  lowGridCells, *highGridCells;
		const BaseGrid::TNetCell*  lowNetCells;
		const Grid::Resource*resources;
		const Grid::ResourceDiscovery*visibility;
		count_t			numResources;
		Grid::FClipHeight	clipHeight;

		static float3 GetColor(float level)	{return float3(std::min(1.f,level*8.f),std::min(1.f,level*3.f), std::min(1.f,level)) * 0.7f + 0.3f;}
		static float3 GetColor(float level, const float3&flavor)
		{
			return flavor;//*0.75f + 0.5f*(std::min(1.f,level));
			//return flavor*0.5f + (1.2f * std::min(1.f,level) + 0.3f);
			// & (GetColor(level)*0.5f + 0.5f);
			//return flavor * 0.75 + GetColor(level) * 0.25f;
			//return 0.3f + 0.7f * std::min(1.f,level*8.f);
			//return float3(std::min(1.f,level*8.f),std::min(1.f,level*3.f), std::min(1.f,level)) * 0.7f + 0.3f;
		}
		static float GetExtend(float level) {return level/3.f;}
	
		void	Setup(	const Grid::CellStack *stacks,const Grid::ResourceDiscovery*visibility,
						const BaseGrid::TCell*lowGridCells, const BaseGrid::TCell*highGridCells,  
						const BaseGrid::TNetCell*lowNetCells,Grid::FClipHeight clipHeight,
						const Grid::Resource*resources, count_t numResources)
		{
			mesh.Reset();
			this->stacks = stacks;
			this->lowNetCells = lowNetCells;
			this->highGridCells = highGridCells;
			this->lowGridCells = lowGridCells;
			this->clipHeight = clipHeight;
			this->resources = resources;
			this->numResources = numResources;
			this->visibility = visibility;
		}

		void	Compile(const index_t offset, const index_t end, THull<ResourceVertex>&, Sys::SpinLock&lock);
	};


	void	ResourceCompiler::Compile(const index_t offset, const index_t end, THull<ResourceVertex>&coverHull, Sys::SpinLock&lock)
	{
		for (index_t i = offset; i < offset+BatchSize && i < end; i++ )
		{
			//const ::Grid::CellStack&stack = stacks[i];
			if (!this->visibility->resourcesDiscovered[i])	//
				continue;	//nothing known

			#ifndef _DEBUG
				if (!(lowGridCells[i].cornerFlags & Grid::empZonesUnlocked))
					continue;
			#endif

			#ifdef HYBRID_GRID
				const UINT32 fine = lowGridCells[i].higherEquivalent;
			#else
				const UINT32 fine = (UINT32)i;
			#endif
			//const auto&n = highGridCells[fine].neighborhood;
			const auto&fineStack = stacks[fine];
			UINT layer = fineStack.highestOpaqueCell;
			if (fineStack.waterRange.IsNotEmpty())
				layer = std::max<UINT>(fineStack.waterRange.end-1, layer);
			const int clipH = clipHeight(fine);
			const UINT clipL = Grid::CellStack::ToLayer(clipH);
			layer = std::min(layer, clipL-1);
			while (layer > 0 && fineStack.volumeCell[layer].content == Grid::Content::Clear)
				layer--;
			bool structureCenter = false;
			while (layer+1 < Grid::CellStack::Range && (fineStack.volumeCell[layer+1].structureCounter > 0 /*|| fineStack.volumeCell[layer].structureCounter > 0*/))
			{
				layer++;
				structureCenter = true;
			}

			if (layer != 0)
				layer++;
			else
				if (Grid::CellStack::bottom.content == Grid::Content::Clear || Grid::CellStack::bottom.content == Grid::Content::Lava)
					continue;
		/*	else
				if (CellStack::bottom.content == Content::Clear)
					continue;*/

			float add = 0.1f;
			//if (fineStack.volumeCell[layer].structureCounter > 0)
				//add += 0.5f;

			float h;
			const auto&cell = lowNetCells[i];
			TMatrix4<>	system;


			h =  Grid::ToRadiusf( float(Grid::CellStack::ToHeight(layer))+add);
			const TVec3<>&center = structureCenter? cell.cornerCenter : cell.centerVertex.xyz;
			Mat::makeAxisSystem(center * h,center,2,system);
			float r = cell.minCornerDistance * h / 4.f * 1.5f;	//dark coat

			int total = 0;
			for (index_t ri = 0; ri < this->numResources; ri++)
				total += this->resources[ri].coverage[i];
			if (total)
			{
				{
					float level = float(total);

					float3 color (-0.1f);

					float h2 = h + Grid::SingleLayerHeight * (level*0.25f + 0.05f);

					mesh.SetOffset();
					mesh << ResourceVertex(center,h,center,float4(color,level+0.8f),1.f);
					for (int j = 0; j < Resolution; j ++)
					{
						float a = float(j) / float(Resolution) * 2.f * M_PIF;
				
						TVec3<> p;
						Mat::Transform(system,float3(cos(a)*r,sin(a)*r,0.f),p);
						//Vec::center(cell.cornerVertex[j].xyz,cell.centerVertex.xyz,p);
						Vec::normalize(p);
						mesh << ResourceVertex(float4(p,1.f),h,p,float4(color,level),1.f);
						mesh << 0 << 1 + j << 1 + (j+1)%Resolution;
					}

					if (h2 > h)
					{
						mesh.SetOffset();
						for (int j = 0; j < Resolution; j ++)
						{
							float a = float(j) / float(Resolution) * 2.f * M_PIF;
							TVec3<> p;
							Mat::Transform(system,float3(cos(a)*r,sin(a)*r,0.f),p);
							//Vec::center(cell.cornerVertex[j].xyz,cell.centerVertex.xyz,p);
							Vec::normalize(p);
							float3 n = (p - cell.centerVertex.xyz).Normalized();
							mesh << ResourceVertex(float4(p,1.f),h,n,float4(color,0.f),1.f);
							mesh << ResourceVertex(float4(p,1.f),h2,n,float4(color,level),1.f);


							mesh.InvQuad(j * 2 + 1, j * 2, ((j+1)%Resolution)*2, ((j+1)%Resolution)*2+1);
						}
					}
				}

				r = cell.minCornerDistance * h / 4.f; 
				h =  Grid::ToRadiusf( float(Grid::CellStack::ToHeight(layer))+add);
				for (index_t ri = 0; ri < this->numResources; ri++)
				{
					const Grid::Resource&res = this->resources[ri];
					if (!res.coverage[i])
						continue;
					float level = float(res.coverage[i]);

					float3 color = GetColor(level/20.f,res.GetColor());

					float h2 = h + Grid::SingleLayerHeight * (level*0.25f);

					mesh.SetOffset();
					mesh << ResourceVertex(center,h2,center,float4(color,level+0.8f),1.f);
					for (int j = 0; j < Resolution; j ++)
					{
						float a = float(j) / float(Resolution) * 2.f * M_PIF;
				
						TVec3<> p;
						Mat::Transform(system,float3(cos(a)*r,sin(a)*r,0.f),p);
						//Vec::center(cell.cornerVertex[j].xyz,cell.centerVertex.xyz,p);
						Vec::normalize(p);
						mesh << ResourceVertex(float4(p,1.f),h2,p,float4(color,level),1.f);
						mesh << 0 << 1 + j << 1 + (j+1)%Resolution;
					}

					if (h2 > h)
					{
						mesh.SetOffset();
						for (int j = 0; j < Resolution; j ++)
						{
							float a = float(j) / float(Resolution) * 2.f * M_PIF;
							TVec3<> p;
							Mat::Transform(system,float3(cos(a)*r,sin(a)*r,0.f),p);
							//Vec::center(cell.cornerVertex[j].xyz,cell.centerVertex.xyz,p);
							Vec::normalize(p);
							float3 n = (p - cell.centerVertex.xyz).Normalized();
							mesh << ResourceVertex(float4(p,1.f),h,n,float4(color,0.f),1.f);
							mesh << ResourceVertex(float4(p,1.f),h2,n,float4(color,level),1.f);


							mesh.Quad(j * 2 + 1, j * 2, ((j+1)%Resolution)*2, ((j+1)%Resolution)*2+1);
						}
					}

					h = h2;
				}
			}
			else
			//if (!anyThere)
			{
				r = cell.minCornerDistance * h / 4.f;
				float level = 0.1f;

				float3 color (0.2f);

				float h2 = h + Grid::SingleLayerHeight * (level*0.25f);

				mesh.SetOffset();
				mesh << ResourceVertex(center,h2,center,float4(color,level+0.8f),1.f);
				for (int j = 0; j < Resolution; j ++)
				{
					float a = float(j) / float(Resolution) * 2.f * M_PIF;
				
					TVec3<> p;
					Mat::Transform(system,float3(cos(a)*r,sin(a)*r,0.f),p);
					//Vec::center(cell.cornerVertex[j].xyz,cell.centerVertex.xyz,p);
					Vec::normalize(p);
					mesh << ResourceVertex(float4(p,1.f),h2,p,float4(color,level),1.f);
					mesh << 0 << 1 + j << 1 + (j+1)%Resolution;
				}

				//if (h2 > h)
				{
					mesh.SetOffset();
					for (int j = 0; j < Resolution; j ++)
					{
						float a = float(j) / float(Resolution) * 2.f * M_PIF;
						TVec3<> p;
						Mat::Transform(system,float3(cos(a)*r,sin(a)*r,0.f),p);
						//Vec::center(cell.cornerVertex[j].xyz,cell.centerVertex.xyz,p);
						Vec::normalize(p);
						float3 n = (p - cell.centerVertex.xyz).Normalized();
						mesh << ResourceVertex(float4(p,1.f),h,n,float4(color,0.f),1.f);
						mesh << ResourceVertex(float4(p,1.f),h2,n,float4(color,level),1.f);


						mesh.Quad(j * 2 + 1, j * 2, ((j+1)%Resolution)*2, ((j+1)%Resolution)*2+1);
					}
				}

			}

		}

		if (mesh.IsNotEmpty())
		{
			lock.lock();

			mesh.AppendTo(coverHull);

			lock.unlock();
		}

	}


	void CompileResources(THull<ResourceVertex>&cover, Grid::ResourceDiscovery&visibility, index_t offset, count_t count, Grid::FClipHeight clipHeight, const Grid::Resource*resources, count_t numResources)
	{
		const index_t end = offset + count;
		Sys::SpinLock lock;
		Concurrency::parallel_for(offset, end, BatchSize,[end,&lock,&visibility, &cover, clipHeight,resources, numResources](index_t i)	//looks good - at least in release
		{
			BYTE data[sizeof(ResourceCompiler)];	//prevent implicit memset in release mode. i'm serious. visual studio has some funny ideas there...
			ResourceCompiler&compiler = *(ResourceCompiler*)data;
			compiler.Setup(World::actual.GetStacks().pointer(),&visibility,Grid::low.grid.GetCells(),Grid::high.grid.GetCells(), Grid::low.cells.pointer(),clipHeight,resources, numResources);
			compiler.Compile(i,end,cover,lock);
		});
	}



	
}
