#ifdef HAS_WINDOW
#include <global_root.h>
#include "world.h"
#include "../../shared/sphereGrid.h"
#include "network.h"
#include "tutorial.h"
#include "lineRenderer.h"
#include <objects/structure.h>


namespace World
{
	bool allowRecompilation = true;


	void		Render(const Engine::Frustum<>&frustum)
	{
		for (BYTE k = 0; k < BaseGrid::NumFamilies; k++)
		{
			Grid::TFamily::TData&dat = Grid::high.family[k].normal;
			if (frustum.IsVisible(dat.boundingSphere))
				display.Render(dat.terrain.instruction[Grid::Instruction::displayMode],true);
		}
	}

	void		RenderWater(const Engine::Frustum<>&frustum)
	{
		for (BYTE k = 0; k < BaseGrid::NumFamilies; k++)
		{
			Grid::TFamily::TData&dat = Grid::high.family[k].normal;
			if (frustum.IsVisible(dat.boundingSphere))
				display.Render(dat.water.instruction[Grid::Instruction::displayMode],true);
		}
	}

	void		RenderOverlay(const Engine::Frustum<>&frustum)
	{
		for (BYTE k = 0; k < BaseGrid::NumFamilies; k++)
		{
			Grid::TFamily::TData&dat = Grid::high.family[k].normal;
			if (frustum.IsVisible(dat.boundingSphere))
				display.Render(dat.overlay.instruction);
		}
	}

	void		RenderResources(const Engine::Frustum<>&frustum)
	{
		display.SetBlendMode(Engine::BlendMode::AlphaBlend);
	//	display.SetBlendMode(Engine::BlendMode::FlareBlend);
		for (BYTE k = 0; k < BaseGrid::NumFamilies; k++)
		{
			Grid::TFamily::TData&dat = Grid::low.family[k].normal;
			if (frustum.IsVisible(dat.boundingSphere))
				display.Render(dat.resources.instruction);
		}

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
				if (candidate->layer > 0 && World::actual.cellStack[candidate->stackID].volumeCell[candidate->layer - 1].IsSolid())
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
					if (World::actual.cellStack[n.neighborID[j1]].volumeCell[candidate->layer].IsSolid())
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
				if (candidate->layer + 1 < Grid::CellStack::Range && !World::actual.cellStack[candidate->stackID].volumeCell[candidate->layer + 1].IsSolid())
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
					if (!World::actual.cellStack[n.neighborID[j1]].volumeCell[candidate->layer].IsSolid())
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
			if (World::actual.lightingOutdated)
			{
				World::actual.UpdateLighting();
				if (!World::actual.lightingOutdated)
					Grid::SetAllFamiliesOutdated();
			}
			if (World::actual.structureChanged)
			{
				World::actual.structureChanged = false;
				Objects::structures.Foreach([](const Objects::PStructure&s)
				{
					if (Grid::high.family[Grid::low.grid.GetCell(s->location.stackID).family].normal.outdated)
					{
						s->changed = true;
						Objects::Structure::anyChanged = true;
					}
				});
			}

			BYTE familiesRebuilt = 0;
			Timer::Time t0 = timer.Now();

			for (BYTE k = 0; k < BaseGrid::NumFamilies; k++)
			{
				{
					Grid::TFamily::TData&dat = Grid::high.family[k].normal;
					if (dat.outdated)
					{
						if (Network::client.IsConnected())
						{
							Tutorial::ShowMessage("Areas");
							Tutorial::ShowMessage("Controls");
						}

						dat.outdated = false;

						#ifdef VOXEL_SCANNER
							Grid::RemapToLowStacks(k);
						#endif

						familiesRebuilt++;
						if (Grid::SeeThrough::IsEnabled())
						{
							Grid::high.BakeBlockInstruction(World::actual.cellStack, k, Grid::SeeThrough::ClipHeight);
						}
						else
						{
							Grid::high.BakeBlockInstruction(World::actual.cellStack, k, Grid::Clip::NoClip);
						}
					}
				}
				{
					Grid::TFamily::TData&dat = Grid::low.family[k].normal;
					if (dat.resourceOutdated)
					{
						dat.resourceOutdated = false;
						if (Grid::SeeThrough::IsEnabled())
						{
							Grid::low.BakeResourceInstruction(k, Grid::SeeThrough::ClipHeight,Grid::Resource::all.pointer(),Grid::Resource::all.Count());
						}
						else
						{
							Grid::low.BakeResourceInstruction(k, Grid::Clip::NoClip,Grid::Resource::all.pointer(),Grid::Resource::all.Count());
						}
					}
				}
			}
			if (familiesRebuilt > 0)
			{
				float seconds = timer.GetSecondsSince(t0);
				LogEvent(EventSignificance::PerformanceMeasurement, nullptr, "Rebuilt "+String(familiesRebuilt)+" families in "+String(seconds));
			}
		}
	}


}

#endif
