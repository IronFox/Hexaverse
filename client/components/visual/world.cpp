#ifdef HAS_WINDOW
#include <global_root.h>
#include "world.h"
#include <grid/worldData.h>
#include "../network.h"
#include "tutorial.h"
#include "lineRenderer.h"
#include <objects/structure.h>
#include "terrain.h"
#include "objectCommands.h"
#include "objectChoices.h"

namespace World
{
	bool allowRecompilation = true;


	void		Render(const Engine::Frustum<>&frustum)
	{
		VisualTerrain::RenderOpaqueTerrain(frustum);
	}

	void		RenderWater(const Engine::Frustum<>&frustum)
	{
		VisualTerrain::RenderWater(frustum);
	}

	void		RenderOverlay(const Engine::Frustum<>&frustum)
	{
		VisualTerrain::RenderOverlay(frustum);
	}

	void		RenderResources(const Engine::Frustum<>&frustum)
	{
		VisualTerrain::RenderResources(frustum);
	}


	void		RenderBuildBlocks()
	{
		if ( Objects::CommonCmd::IsModeActive(Objects::CursorMode::BuildBlockMode) )
		{
			display.SetDepthTest(false, false);
			LineRenderer::SetColor(float4(1,1, 1.f, 0.25f));

			foreach(Objects::buildCandidates, candidate)
			{
				const auto&n = Grid::high.grid.GetCell(candidate->stackID).neighborhood;
				const auto&c = Grid::high.cells[candidate->stackID];
				float	r0 = Grid::ToRadius(int(candidate->layer) - Grid::CellStack::HalfRange),
						r1 = Grid::ToRadius(int(candidate->layer) - Grid::CellStack::HalfRange + 1);
				if (candidate->layer > 0 && World::actual.GetCell((*candidate)-1).IsSolid())
				{
					for (int j = 0; j < n.numNeighbors; j++)
					{
						LineRenderer::Rasterize(c.cornerVertex[j].xyz * r0,
												c.cornerVertex[(j + 1) % n.numNeighbors].xyz * r0);
					}
				}
				for (int j = 0; j < n.numNeighbors; j++)
				{
					UINT32 j1 = (j + 1) % n.numNeighbors;
					if (World::actual.GetStack(n.neighborID[j1]).volumeCell[candidate->layer].IsSolid())
					{
						//
						LineRenderer::Rasterize(c.cornerVertex[j].xyz * r0,
												c.cornerVertex[j].xyz * r1);
						LineRenderer::Rasterize(c.cornerVertex[j1].xyz * r0,
												c.cornerVertex[j1].xyz * r1);
						LineRenderer::Rasterize(c.cornerVertex[j].xyz * r1,
												c.cornerVertex[j1].xyz * r1);
						LineRenderer::Rasterize(c.cornerVertex[j].xyz * r0,
												c.cornerVertex[j1].xyz * r0);
					}
				}

			}

			LineRenderer::SetColor(float4(1));
			foreach(Objects::cellSelection, cell)
			{
				const auto&n = Grid::high.grid.GetCell(cell->stackID).neighborhood;
				const auto&c = Grid::high.cells[cell->stackID];
				const float	r0 = Grid::ToRadius(cell->height),
							r1 = Grid::ToRadius(cell->height + 1);
				for (int j = 0; j < n.numNeighbors; j++)
				{
					LineRenderer::Rasterize(c.cornerVertex[j].xyz * r0,
						c.cornerVertex[j].xyz * r1);
					LineRenderer::Rasterize(c.cornerVertex[j].xyz * r0,
						c.cornerVertex[(j + 1) % n.numNeighbors].xyz * r0);
					LineRenderer::Rasterize(c.cornerVertex[j].xyz * r1,
						c.cornerVertex[(j + 1) % n.numNeighbors].xyz * r1);

				}


			}
		}

		if ( Objects::CommonCmd::IsModeActive(Objects::CursorMode::RemoveBlockMode) )
		{
			display.SetDepthTest(false, false);
			LineRenderer::SetColor(float4(1,1, 1.f, 0.25f));

			foreach(Objects::removeCandidates, candidate)
			{
				const auto&n = Grid::high.grid.GetCell(candidate->stackID).neighborhood;
				const auto&c = Grid::high.cells[candidate->stackID];
				const float	r0 = Grid::ToRadius(int(candidate->layer) - Grid::CellStack::HalfRange),
							r1 = Grid::ToRadius(int(candidate->layer) - Grid::CellStack::HalfRange + 1);
				if (candidate->layer + 1 < Grid::CellStack::Range && !World::actual.GetCell((*candidate)+1).IsSolid())
				{
					for (int j = 0; j < n.numNeighbors; j++)
					{
						LineRenderer::Rasterize(c.cornerVertex[j].xyz * r1,
												c.cornerVertex[(j + 1) % n.numNeighbors].xyz * r1);
					}
				}
				for (int j = 0; j < n.numNeighbors; j++)
				{
					UINT32 j1 = (j + 1) % n.numNeighbors;
					if (!World::actual.GetStack(n.neighborID[j1]).volumeCell[candidate->layer].IsSolid())
					{
						//
						LineRenderer::Rasterize(c.cornerVertex[j].xyz * r0,
												c.cornerVertex[j].xyz * r1);
						LineRenderer::Rasterize(c.cornerVertex[j1].xyz * r0,
												c.cornerVertex[j1].xyz * r1);
						LineRenderer::Rasterize(c.cornerVertex[j].xyz * r1,
												c.cornerVertex[j1].xyz * r1);
						LineRenderer::Rasterize(c.cornerVertex[j].xyz * r0,
												c.cornerVertex[j1].xyz * r0);
					}
				}

			}

			LineRenderer::SetColor(float4(1));
			foreach(Objects::cellSelection, cell)
			{
				const auto&n = Grid::high.grid.GetCell(cell->stackID).neighborhood;
				const auto&c = Grid::high.cells[cell->stackID];
				const float	r0 = Grid::ToRadius(cell->height),
							r1 = Grid::ToRadius(cell->height + 1);
				for (int j = 0; j < n.numNeighbors; j++)
				{
					LineRenderer::Rasterize(c.cornerVertex[j].xyz * r0,
											c.cornerVertex[j].xyz * r1);
					LineRenderer::Rasterize(c.cornerVertex[j].xyz * r0,
											c.cornerVertex[(j + 1) % n.numNeighbors].xyz * r0);
					LineRenderer::Rasterize(c.cornerVertex[j].xyz * r1,
											c.cornerVertex[(j + 1) % n.numNeighbors].xyz * r1);

				}


			}
		}

		LineRenderer::Flush();

	}



	void		RecompileIfNecessary()
	{

		if (allowRecompilation)
		{
			if (World::actual.UpdateLighting())
				VisualTerrain::Invalidate();

			bool anyOutdated = false;
//			if (World::actual.FlushAllChanges())
			{
				for (BaseGrid::family_t k = 0; k < BaseGrid::NumFamilies; k++)
					if (World::actual.FamilyIsOutdated(k))
						VisualTerrain::InvalidateFamily(k);

				Objects::structures.Foreach([&anyOutdated](const Objects::PStructure&s)
				{
					if (VisualTerrain::FamilyIsOutdated(Grid::low.grid.GetCell(s->location.stackID).family))
					{
						anyOutdated = true;
						s->changed = true;
					}
				});
				World::actual.FlushAllChanges();
			}

			if (anyOutdated && Network::client.IsConnected())
			{
				Tutorial::ShowMessage("Areas");
				Tutorial::ShowMessage("Controls");
			}

			VisualTerrain::RebuildOutdatedFamilies();
		}
	}


}

#endif
