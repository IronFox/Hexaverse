#include "vehicleAccessibilityMesh.h"
#include "../objectSelection.h"
#include <grid/worldData.h>
#include <objects/pathfinding.h>
#include <objects/vehicle.h>
#include <container/hashtable.h>
#include <geometry/cgs.h>
#include "../display.h"
#include "../../clientEvents.h"
#include "lighting.h"

namespace DX11 = Engine::DX11;

namespace VehicleAccessibilityMesh
{
	bool	visible = false;

	namespace Details
	{
		Objects::Pathfinding::ReachabilityMapper	mapper(false);

		Engine::DX11::InstructionBlock32	mesh;


		struct TNode
		{
			int					movementCostsFromRoot = 0;
			int					movementCostsFromPredecessor=0,speedCost=0;
			count_t				progression = 0;
			index_t				stepsToReach = 0;
			TNode*				edgeNeighbor[6] = {nullptr};
			TNode				*predecessor = nullptr;
			Grid::LCellIdI		loc;
			bool				invisible=false;

			static HashTable<Grid::LCellIdI,TNode>	nodeMap;

			/**/		TNode()
			{}

			/**/		TNode(const Objects::Pathfinding::ReachabilityMapper::MapNode&source)
			{
				movementCostsFromRoot = source.movementCostsFromRoot;
				loc = source.cell;
			}

			bool		HasNeighbor(const TNode*n) const
			{
				for (index_t i = 0; i < ARRAYSIZE(edgeNeighbor); i++)
					if (edgeNeighbor[i] == n)
						return true;
				return false;
			}

			void		UpdateStepCount(const int speed)
			{
				if (!predecessor)
					return;
				if (stepsToReach || speedCost)
					return;
				predecessor->UpdateStepCount(speed);
				speedCost = predecessor->speedCost + movementCostsFromPredecessor;
				stepsToReach = predecessor->stepsToReach;


				if (speedCost >= speed)
				{
					stepsToReach++;
					speedCost = movementCostsFromPredecessor;
				}

				if (stepsToReach == predecessor->stepsToReach)
					progression = predecessor->progression + 1;
				else
					progression = 0;
			}

			void		CheckNeighborhood(const int maxFallingDistance, const int maxClimbingDistance)
			{
				const auto&n = Grid::low.grid.GetCell(loc.stackID).neighborhood;
				for (int j = -maxFallingDistance; j <= maxClimbingDistance; j++)
				{
					const int h = j + loc.height;
					if (h < Grid::CellStack::BottomHeight || h > Grid::CellStack::TopHeight)
						continue;
					for (int k = 0; k < n.numNeighbors; k++)
					{
						const Grid::LCellIdI candidate(n.neighborID[k],h);
						auto*cell = nodeMap.QueryPointer(candidate);
						if (cell)
							edgeNeighbor[k] = cell;
					}
				}

			}
		};

		HashTable<Grid::LCellIdI,TNode>	TNode::nodeMap;

		void MakeVertex( CGS::Constructor<>::Object&obj, const M::TVec3<>&cornerCoords, int height, float t0, float t1, const TNode&n)
		{
			float neg = 0.f;//1.f - 1.f / (1.f + progression);
			float4 color = float4(0.6,0.7,1.0,1.1f);
			color.rgb *= 1.f/(1.f + n.stepsToReach);
			color.a = 0.5f /(1.f + 0.5f*n.stepsToReach);
			color.a -= 0.1f;
			if (n.stepsToReach > 1)
				color.a = 0;

			obj.MakeVertex(cornerCoords * Grid::ToRadiusf(0.05f + height),color,float2(t0,t1));
		}


		
	}


	/**
	Flushes the previous mesh and creates a new mesh on the current terrain for the specified entity in its current state
	@param e Entity to build the mesh for. None if e is empty
	*/
	void		RebuildFor(const Objects::PVehicle&e)
	{
		using namespace Details;

		mesh.ibo.Clear();

		if (!e || e->IsDeadOrGhost())
			return;
		const int speed = e->GetMovementStepsPerRound();

		int offset = 0;
		count_t offsetSteps = 0;

		Grid::LCellIdI root = e->location;

		if (Objects::ShouldQueueCommands())
		{
			if (e->myTasks.IsNotEmpty() && e->myTasks.GetNewest().type == Objects::MobileEntity::Task::MoveTo)
			{
				foreach (e->myTasks.GetNewest().path,n)
				{
					offset += n->movementCostsFromPrevNode;
					if (offset >= speed)
					{
						offsetSteps++;
						offset = n->movementCostsFromPrevNode;
					}
				}
			}

			root = e->GetLastPointOfOrigin();
		}
		if (Objects::WorldEntity::IsAboveGrid(root))
			return;

		mapper.Map(e.get(),root,nullptr,0xFFFFFFFF,false,true,true);

		const count_t rootAt = mapper.result.Count();
		mapper.result.Append().cell = root;

		TNode::nodeMap.Clear();

		foreach (mapper.result,n)
		{
			const bool isLast =  n+1 == mapper.result.end();
			if (n->predecessor == InvalidIndex && !isLast)
				n->predecessor = rootAt;
			auto&n2 = TNode::nodeMap.Set(n->cell,*n);
		}

		foreach (mapper.result,n)
		{
			auto&n2 = TNode::nodeMap.Require(n->cell);
			if (n->predecessor != InvalidIndex)
			{
				n2.predecessor= &TNode::nodeMap.Require(mapper.result[n->predecessor].cell);
				n2.movementCostsFromPredecessor = n2.movementCostsFromRoot - n2.predecessor->movementCostsFromRoot;
			}
			else
			{
				n2.movementCostsFromPredecessor = n2.movementCostsFromRoot;
				n2.stepsToReach = offsetSteps;
				n2.speedCost = offset + n2.movementCostsFromPredecessor;
				if (n2.speedCost >= speed)
				{
					n2.stepsToReach++;
					n2.speedCost = n2.movementCostsFromPredecessor;
				}
			}
		}

		foreach (mapper.result,n)
		{
			auto&n2 = TNode::nodeMap.Require(n->cell);
			n2.UpdateStepCount(speed);
		}
		TNode::nodeMap.FilterEntries([offsetSteps](const Grid::LCellIdI&, TNode&n)
		{
			if (n.stepsToReach > offsetSteps+5)
			{
				n.invisible = true;
				return true;
			}
			n.stepsToReach -= offsetSteps;
			//n.stepsToReach = n.stepsToReach > offsetSteps+1 ? offsetSteps+1 : n.stepsToReach;

			return true;
		});

		const int climb = e->GetMaxClimbingHeight();
		foreach (mapper.result,n)
		{
			auto*n2 = TNode::nodeMap.QueryPointer(n->cell);
			if (!n2)
				continue;
			if (n->predecessor != InvalidIndex)
				n2->predecessor= &TNode::nodeMap.Require(mapper.result[n->predecessor].cell);
			else
				n2->predecessor = nullptr;
			n2->CheckNeighborhood(e->GetMaxFallingHeight(),climb);
		}

				


		CGS::Constructor<>	cnst(1,CGS::HasNormalFlag| CGS::HasColorFlag);
		auto&obj = cnst.AppendObject();


		TNode::nodeMap.VisitAllValues([&obj,climb](const TNode&n)
		{
			if (n.invisible)
				return;
			const count_t prog = n.progression;
			obj.SetVertexOffsetToCurrent();
			const auto&cell = Grid::low.cells[n.loc.stackID];
			MakeVertex(obj,cell.cornerCenter,n.loc.height,0,0,n);
			bool hardEdges[6] = {false};
			float3 center[6];
			for (int k = 0; k < cell.numCorners; k++)
			{
				const bool hardEdge0 = !n.edgeNeighbor[k]/* || n.edgeNeighbor[k]->stepsToReach > n.stepsToReach*/;// || (n.edgeNeighbor[k]->loc.height < n.loc.height - climb);
				const int kPrev = (k+cell.numCorners-1)%cell.numCorners;
				const int kNext = (k+1)%cell.numCorners;
				const int kNextNext = (kNext+1)%cell.numCorners;
				const bool hardEdge1 = !n.edgeNeighbor[kPrev]/* || n.edgeNeighbor[kPrev]->stepsToReach > n.stepsToReach*/;// || (n.edgeNeighbor[kPrev]->loc.height < n.loc.height - climb);
				hardEdges[k] = hardEdge0;

				center[k] = (cell.cornerVertex[kPrev].xyz + cell.cornerVertex[k].xyz)/2;

				MakeVertex(obj,cell.cornerVertex[kPrev].xyz,n.loc.height,1,hardEdge0 || hardEdge1,n);
				MakeVertex(obj,center[k],n.loc.height,1,0.5f + 0.5f * hardEdge0,n);

				obj.MakeTriangle(0,1+k*2,1+k*2+1);
				obj.MakeTriangle(0,1+k*2+1,1+kNext*2);
			}

			for (int k = 0; k < cell.numCorners; k++)
			{
				if (n.edgeNeighbor[k] && n.edgeNeighbor[k]->loc.height < n.loc.height)

				//if (n.edgeNeighbor[k] > &n || 
					//(n.edgeNeighbor[k] && !n.edgeNeighbor[k]->HasNeighbor(&n)))
				{
					const int nHeight = n.edgeNeighbor[k]->loc.height;
					const int kPrev = (k+cell.numCorners-1)%cell.numCorners;
					const int kNext = (k+1)%cell.numCorners;
					const bool hardEdge = hardEdges[k];
					const bool hardEdgePrev = hardEdges[kPrev];
					const bool hardEdgeNext = hardEdges[kNext];

					obj.SetVertexOffsetToCurrent();

					const bool e0 = (hardEdge || hardEdgePrev);
					const bool e1 = (hardEdge || hardEdgeNext);
					const bool different = e0 != hardEdge || e1 != hardEdge;
					MakeVertex(obj,cell.cornerVertex[kPrev].xyz,n.loc.height,1,e0 && different,n);
					MakeVertex(obj,cell.cornerVertex[kPrev].xyz,nHeight,1,e0 && different,n);
					MakeVertex(obj,center[k],n.loc.height,1,0.5f + 0.5f * (hardEdge && different),n);
					MakeVertex(obj,center[k],nHeight,1,0.5f + 0.5f * (hardEdge && different),n);
					MakeVertex(obj,cell.cornerVertex[k].xyz,n.loc.height,1,e1 && different,n);
					MakeVertex(obj,cell.cornerVertex[k].xyz,nHeight,1,e1 && different,n);
					

					obj.MakeTriangle(0,1,2);
					obj.MakeTriangle(1,3,2);

					obj.MakeTriangle(2,3,4);
					obj.MakeTriangle(3,5,4);
				}

			}


		});
		obj.ComputeNormals();
		mesh.ibo.Create(obj.GetTriangleIndices(0),obj.CountTriangleIndices(0));
		mesh.vbo.Create(obj.GetVertices(0),obj.CountVertices(0),obj.GetVertexSize()*sizeof(float));

	}

	void		Render( const Engine::Frustum<>& )
	{
		if (!visible)
			return;
		display.StoreRendererState();
			display.SetDepthTest(true,false);
			display.SetRasterizer(true,false);
			display.Render(Details::mesh);
		display.RestoreRendererState();
	}

	void		Refresh()
	{
		RebuildFor(Objects::selectedVehicle);
	}


	void		Create()
	{
		GlobalEvents::invalidateObjectCache << Refresh;
		World::actual.onTerrainChange << [](const World::Data&, const BaseGrid::FamiliesChanged&,float) {Refresh();};

		using namespace Details;
		{
			Engine::VertexBinding bnd;
			bnd.vertex.Set(0,3);
			bnd.normal.Set(3,3);
			bnd.color.Set(6,4);
			bnd.texcoords.SetSize(1);
			bnd.texcoords.First().Set(10,2);
			bnd.floats_per_vertex = 12;
			mesh.binding = bnd;
		}

		Buffer0<DX11::Shader::Binding> bindings;

		//bindings;
		
		bindings <<	DX11::Shader::Binding(Lighting::GPUResources::sunDirection,"Sun");
		bindings << DX11::Shader::Binding(Lighting::GPUResources::sunColor,"SunColor");

		mesh.SetTriangles();
		mesh.shader.LoadFromFile("shaders/vehicleAccessibilityMesh.hlsl","vsMain","psMain",bindings.GetPointer(),bindings.Count());
		mesh.UpdateLayout();

		GlobalEvents::applicationTerminate << [](){mesh.Clear();};

	}

}
