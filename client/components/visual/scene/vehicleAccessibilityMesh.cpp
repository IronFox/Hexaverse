#include "vehicleAccessibilityMesh.h"
#include "../objectSelection.h"
#include <grid/worldData.h>
#include <objects/pathfinding.h>
#include <objects/vehicle.h>
#include <container/hashtable.h>
#include <geometry/cgs.h>
#include "../display.h"
#include "../../clientEvents.h"


namespace VehicleAccessibilityMesh
{
	namespace Details
	{
		Objects::Pathfinding::ReachabilityMapper	mapper(false);

		Engine::DX11::InstructionBlock32	mesh;


		struct TNode
		{
			int					movementCostsFromRoot = 0;
			int					movementCostsFromPredecessor=0,speedCost=0;
			index_t				stepsToReach = 0;
			TNode*				edgeNeighbor[6] = {nullptr};
			TNode				*predecessor = nullptr;
			Grid::LCellIdI		loc;

			static HashTable<Grid::LCellIdI,TNode>	nodeMap;

			/**/		TNode()
			{}

			/**/		TNode(const Objects::Pathfinding::ReachabilityMapper::MapNode&source)
			{
				movementCostsFromRoot = source.movementCostsFromRoot;
				loc = source.cell;
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

		void MakeFlatVertex(CGS::Constructor<>::Object&obj, const M::TVec3<>&cornerCoords, int height, float t0, float t1)
		{
			obj.MakeVertex(cornerCoords * Grid::ToRadiusf(0.02f + height),cornerCoords,float2(t0,t1));
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

		if (!e)
			return;
		const int speed = e->GetMovementStepsPerRound();

		int offset = 0;
		count_t offsetSteps = 0;


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

			mapper.Map(e.get(),e->GetLastPointOfOrigin(),nullptr,speed * 5);
		}
		else
			mapper.Map(e.get(),e->location,nullptr,speed * 5);


		TNode::nodeMap.Clear();

		foreach (mapper.result,n)
			TNode::nodeMap.Set(n->cell,*n);

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
		TNode::nodeMap.FilterEntries([offsetSteps](const Grid::LCellIdI&, const TNode&n)
		{
			return n.stepsToReach <= offsetSteps+2;
		});

		foreach (mapper.result,n)
		{
			auto*n2 = TNode::nodeMap.QueryPointer(n->cell);
			if (!n2)
				continue;
			if (n->predecessor != InvalidIndex)
				n2->predecessor= &TNode::nodeMap.Require(mapper.result[n->predecessor].cell);
			else
				n2->predecessor = nullptr;
			n2->CheckNeighborhood(e->GetMaxFallingHeight(),e->GetMaxClimbingHeight());
		}

				


		CGS::Constructor<>	cnst(1,CGS::HasNormalFlag);
		auto&obj = cnst.AppendObject();


		TNode::nodeMap.VisitAllValues([&obj](const TNode&n)
		{
			obj.SetVertexOffsetToCurrent();
			const auto&cell = Grid::low.cells[n.loc.stackID];
			MakeFlatVertex(obj,cell.cornerCenter,n.loc.height,0,0);
			for (int k = 0; k < cell.numCorners; k++)
			{
				const bool hardEdge0 = !n.edgeNeighbor[k] || n.edgeNeighbor[k]->stepsToReach > n.stepsToReach;
				const int kPrev = (k+cell.numCorners-1)%cell.numCorners;
				const int kNext = (k+1)%cell.numCorners;
				const int kNextNext = (kNext+1)%cell.numCorners;
				const bool hardEdge1 = !n.edgeNeighbor[kPrev] || n.edgeNeighbor[kPrev]->stepsToReach > n.stepsToReach;

				MakeFlatVertex(obj,cell.cornerVertex[kPrev].xyz,n.loc.height,1,hardEdge0 || hardEdge1);
				MakeFlatVertex(obj,(cell.cornerVertex[kPrev].xyz + cell.cornerVertex[k].xyz)/2,n.loc.height,1,0.5f + 0.5f * hardEdge0);

				obj.MakeTriangle(0,1+k*2,1+k*2+1);
				obj.MakeTriangle(0,1+k*2+1,1+kNext*2);
			}
		});
		mesh.ibo.Create(obj.GetTriangleIndices(0),obj.CountTriangleIndices(0));
		mesh.vbo.Create(obj.GetVertices(0),obj.CountVertices(0),obj.GetVertexSize()*sizeof(float));

	}

	void		Render(const Engine::Frustum<>&)
	{
		display.StoreRendererState();
			display.SetDepthTest(false,false);
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
			bnd.texcoords.SetSize(1);
			bnd.texcoords.First().Set(6,2);
			bnd.floats_per_vertex = 8;
			mesh.binding = bnd;
		}

		mesh.SetTriangles();
		mesh.shader.LoadFromFile("shaders/vehicleAccessibilityMesh.hlsl","vsMain","psMain");
		mesh.UpdateLayout();

		GlobalEvents::applicationTerminate << [](){mesh.Clear();};

	}

}
