#include "resourceVisualization.h"
#include <grid/resources.h>
#include <grid/worldData.h>
#include <objects/gridAndChoices.h>
#include "../objectRendering.h"
#include "../../../cameraControl.h"

namespace VisualTerrain
{
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
	static float3 GetColor(float level, const byte3&flavor)
	{
		return GetColor(level,float3(flavor)/255.f);
	}

	class MeshBuilder
	{
	public:
		typedef MeshBuilder		Self;

		/**/					MeshBuilder(Hull<ResourceVertex>&hull):hull(hull)
		{}

		void					SetOffset() {vOffset = (UINT32)hull.CountVertices();}

		Self&					operator<<(const ResourceVertex&v)	{hull << v; return *this;}
		Self&					operator<<(UINT32 idx)
		{
			hull.AppendIndex(idx + vOffset);
			return *this;
		}

		Self&					Quad(UINT32 a, UINT32 b, UINT32 c, UINT32 d)
		{
			return *this 
				<< a << b << c    
				<< a << c << d;
		}

		Self&					InvQuad(UINT32 a, UINT32 b, UINT32 c, UINT32 d)
		{
			return Quad(d,c,b,a);
		}


		void MakeDisk(const int resolution, const float r, const float z, const float level, const M::TVec3<>&color)
		{
			auto&mesh = *this;
			mesh.SetOffset();
			mesh << ResourceVertex(float3(0,0,z),float3(0,0,1),float4(color,level+0.8f),1.f);
			for (int j = 0; j < resolution; j ++)
			{
				float a = float(j) / float(resolution) * 2.f * M_PIF;
				
				float2 d(cos(a),sin(a));
				mesh << ResourceVertex(float3(d*r,z),float3(0,0,1),float4(color,level),1.f);
			}
			for (int j = 0; j < resolution; j ++)
				mesh << 0 << 1 + j << 1 + (j+1)%resolution;
		}

		void MakeRing(const int resolution, const float r, const float z0, const float z1, const float topLevel, const M::TVec3<>&color, bool inv)
		{
			SetOffset();
			for (int j = 0; j < resolution; j ++)
			{
				float a = float(j) / float(resolution) * 2.f * M_PIF;
				float2 d(cos(a),sin(a));
				float2 p = d*r;
				float3 n(d,0);
				*this << ResourceVertex(float3(p,z0),n,float4(color,0.f),1.f);
				*this << ResourceVertex(float3(p,z1),n,float4(color,topLevel),1.f);

			}
			for (int j = 0; j < resolution; j ++)
				if (inv)
					InvQuad(j * 2 + 1, j * 2, ((j+1)%resolution)*2, ((j+1)%resolution)*2+1);
				else
					Quad(j * 2 + 1, j * 2, ((j+1)%resolution)*2, ((j+1)%resolution)*2+1);
		}

		void MakeCoat(const int resolution, const float r, const unsigned total)
		{
			float level = float(total);

			float3 color (-0.1f);

			float h2 = (level*0.25f + 0.05f);

			MakeDisk(resolution,r,0, level, color);
			MakeRing(resolution,r,0,h2, level, color, true);
		}

	private:
		Hull<ResourceVertex>	&hull;
		UINT32					vOffset = 0;

	};





	void	ResourceStack::Compile(const Objects::Resource::CoreVector&vec)
	{
		for (index_t lod = 0; lod < details.Count(); lod++)
		{
			auto&detail = details[lod];

			const auto resolution = 8 * (1<<lod);


			float add = 0.1f;
			//if (fineStack.volumeCell[layer].structureCounter > 0)
				//add += 0.5f;

			
			unsigned total = vec.GetSum();
		
			auto mesh = MeshBuilder(detail.hull);
		
			if (total)
			{
				//dark shaded coat:
				{
					const float r = Grid::low.neighborCellDistances.min / 4.f * 0.8f;	//dark coat
					mesh.MakeCoat(resolution, r, total);
				}

				{
					const float r = Grid::low.neighborCellDistances.min / 4.f * 0.6f;

					Objects::Resource::CoreVector::T offset = 0;

					for (index_t ri = 0; ri < vec.Count(); ri++)
					{
						auto cov = vec[ri];
						if (!cov)
							continue;
						float level = float(cov);

						
						float3 color = Grid::Resource::all[ri].GetColor();
							//GetColor(level/20.f,vec.GetResource(ri).ToType().color);
						float h1 = (offset*0.25f);
						float h2 = h1 + (level*0.25f);
						offset += cov;

						mesh.MakeDisk(resolution,r, h2, level, color);
						mesh.MakeRing(resolution,r, h1, h2,level,color, false);
					}
				}
			}
			else
			{
				//no resources here: dark disk
				const float r = Grid::low.neighborCellDistances.min / 4.f * 0.6f;
				float level = 0.1f;

				float3 color (0.2f);

				float h2 = (level*0.25f);

				mesh.MakeDisk(resolution,r, h2, level, color);
				mesh.MakeRing(resolution,r, 0, h2,level,color, false);
			}
		}
	}




	class ResourceCompiler
	{
	public:
		static const count_t Resolution = 10;

		FixedArray<TStackInstance,BatchSize>	regularInstances, occupiedInstances;
		count_t									regularInstanceCount, occupiedInstanceCount;
		ResourceStackStore						*store;

		const Grid::CellStack * stacks;
		const BaseGrid::TCell*  lowGridCells, *highGridCells;
		const BaseGrid::TNetCell*  lowNetCells;
		const Grid::Resource*resources;
		const Grid::ResourceDiscovery*visibility;
		count_t			numResources;

		Grid::FClipHeight	clipHeight;

		static float GetExtend(float level) {return level/3.f;}
	
		void	Setup(	const Grid::CellStack *stacks,const Grid::ResourceDiscovery*visibility,
						const BaseGrid::TCell*lowGridCells, const BaseGrid::TCell*highGridCells,  
						const BaseGrid::TNetCell*lowNetCells,Grid::FClipHeight clipHeight,
						const ArrayRef<Grid::Resource>&resources,ResourceStackStore&store
					)
		{
			regularInstanceCount = 0;
			occupiedInstanceCount = 0;

			this->store = &store;
			this->stacks = stacks;
			this->lowNetCells = lowNetCells;
			this->highGridCells = highGridCells;
			this->lowGridCells = lowGridCells;
			this->clipHeight = clipHeight;
			this->resources = resources.GetPointer();
			this->numResources = resources.Count();
			this->visibility = visibility;
		}

		void	Compile(const index_t offset, const index_t end, TResourceCover&, Sys::SpinLock&lock);
	};


	void	ResourceCompiler::Compile(const index_t offset, const index_t end, TResourceCover&coverHull, Sys::SpinLock&lock)
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
			{
				if (Grid::CellStack::bottom.content == Grid::Content::Clear || Grid::CellStack::bottom.content == Grid::Content::Lava)
					continue;
				while (layer+1 < Grid::CellStack::Range && fineStack.volumeCell[layer].structureCounter > 0)
					layer++;
			}
		/*	else
				if (CellStack::bottom.content == Content::Clear)
					continue;*/

			float add = 0.1f;
			//if (fineStack.volumeCell[layer].structureCounter > 0)
				//add += 0.5f;

			float h;
			const auto&cell = lowNetCells[i];
			M::TMatrix4<>	system;

			const auto baseHeight = Grid::CellStack::ToHeight(layer);
			const auto layerHeight = Grid::GetLayerHeight(baseHeight);
			h =  Grid::ToRadiusf( float(baseHeight)+add);
			const M::TVec3<>&center = structureCenter? cell.cornerCenter : cell.centerVertex.xyz;
			Mat::makeAxisSystem(center * h,center,2,system);
			system.w.xyz += system.z.xyz * (layerHeight * 0.1f);

			system.z.xyz *= layerHeight;
			system.x.xyz *= h;
			system.y.xyz *= h;


			bool occupied = false;

			Objects::Resource::CoreVector vec;
			vec.SampleWorld(i,false);

			if (vec.IsNotEmpty())
				for (int h = Grid::CellStack::BottomHeight; h <= Grid::CellStack::TopHeight; h++)
				{
					Objects::PStructure s = Objects::GetStructure(Grid::LCellIdI(i,h));
					if (s && s->IsWorkingStructure() && To<Objects::WorkingStructure>::Cast(s)->IsMine())
					{
						occupied = true;
						break;
					}
				}

			TStackInstance&inst = occupied ? occupiedInstances[occupiedInstanceCount++]  : regularInstances[regularInstanceCount++];
			StackKey key(vec,occupied);

			inst = store->Instantiate(key,system,lock);
		}

		if (regularInstanceCount > 0 || occupiedInstanceCount > 0)
		{
			lock.lock();

			coverHull.regularStacks.Append(regularInstances.data(),regularInstanceCount);
			coverHull.occupiedStacks.Append(occupiedInstances.data(),occupiedInstanceCount);

			lock.unlock();
		}

	}


	void CompileResources(TResourceCover&cover, Grid::ResourceDiscovery&visibility, index_t offset, count_t count, Grid::FClipHeight clipHeight, const ArrayRef<Grid::Resource>&resources, ResourceStackStore&store)
	{
		const index_t end = offset + count;
		Sys::SpinLock lock;
		Concurrency::parallel_for(offset, end, BatchSize,[end,&lock,&visibility, &cover, clipHeight,resources,&store](index_t i)	//looks good - at least in release
		{
			BYTE data[sizeof(ResourceCompiler)];	//prevent implicit memset in release mode. i'm serious. visual studio has some funny ideas there...
			ResourceCompiler&compiler = *(ResourceCompiler*)data;
			compiler.Setup(World::actual.GetStacks().pointer(),&visibility,Grid::low.grid.GetCells(),Grid::high.grid.GetCells(), Grid::low.cells.pointer(),clipHeight,resources, store);
			compiler.Compile(i,end,cover,lock);
		});
	}


	void	ResourceStackStore::Render(const TStackInstance&inst) const
	{
		float distance = Vec::distance(Camera::scene.GetAbsoluteLocation(),inst.bounds.boundingSphere.center);
		auto lod = Objects::DetermineLODInstruction(distance,2);
//			inst.bounds.boundingSphere.radius);

		const auto&det = stacks[inst.stackIndex].details[lod];
		display.SetObjectMatrix(inst.system);
		display.Render(det.instruction,true);
	}


	void	ResourceStackStore::PrepareRendering(const DX11::BaseInstructionBlock&base)
	{
		for (index_t i = newFrom; i < stacks.Count(); i++)
		{
			ResourceStack&s=  stacks[i];


			foreach (s.details,detail)
			{
				(DX11::BaseInstructionBlock&)detail->instruction = base;

				LoadHull(detail->instruction,detail->hull);
			}
		}

		newFrom = stacks.Count();
	}


	
}
