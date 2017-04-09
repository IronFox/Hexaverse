#include <global_root.h>
#include "sensorCoverage.h"

namespace SensorCoverage
{

	void				Volume::ChangeSensory(const Grid::HCellIdU&center, float t, short changeBy)
	{
		bool familyChanged[BaseGrid::NumFamilies] = { false };

		Grid::TemplateHelper::ChangeSensory(center, changeBy, cellStack.pointer(), familyChanged);

		for (index_t i = 0; i < BaseGrid::NumFamilies; i++)
			if (familyChanged[i])
				families[i].changed = true;
	}

	//void				Volume::ChangeSensory(const Grid::HighSetup::TVolumeDescriptor&desc, float t, short changeBy)
	//{
	//	bool familyChanged[BaseGrid::NumFamilies] = { false };
	//	Grid::TemplateHelper::ChangeSensory(desc, changeBy, cellStack.pointer(), familyChanged);
	//	for (index_t i = 0; i < BaseGrid::NumFamilies; i++)
	//		if (familyChanged[i])
	//		{
	//			families[i].changed = true;
	//			//families[i].lastChange
	//		}
	//}

	//void				VisibilityState::ChangeSensory(const LCellIdI&center, float radius, float t, short changeBy)
	//{
	//	bool familyChanged[BaseGrid::NumFamilies] = { false };
	//	Grid::TemplateHelper::ChangeSensory(Grid::ToWorld(center), radius, changeBy, cellStack.pointer(), familyChanged);
	//	for (index_t i = 0; i < BaseGrid::NumFamilies; i++)
	//		if (familyChanged[i])
	//			families[i].changed = true;
	//}
	/*void				Volume::ChangeSensory(const SkySensorState&state, float t, short changeBy)
	{
		bool familyChanged[BaseGrid::NumFamilies] = { false };
		Grid::TemplateHelper::ChangeSensorCoverage(state, changeBy, cellStack.pointer(), familyChanged);
		for (index_t i = 0; i < BaseGrid::NumFamilies; i++)
			if (familyChanged[i])
				families[i].changed = true;
	}*/

	void				Volume::RegisterExplosionCrater(const Grid::HCellIdI&center, float radius, float t)
	{
		Sensor(center,0.5f,radius,Sensor::SolidSphere).ApplyTo(*this,0);

		//bool familyChanged[BaseGrid::NumFamilies] = { false };
		//Grid::TemplateHelper::ChangeSensory(c3, radius, 0, cellStack.pointer(), familyChanged);
		//for (index_t i = 0; i < BaseGrid::NumFamilies; i++)
		//	if (familyChanged[i])
		//		families[i].changed = true;
	}



	void Volume::FlushAllData()
	{
		for (index_t i = 0; i < ARRAYSIZE(families); i++)
			families[i].Reset();
		foreach (cellStack,stack)
			stack->Reset();

	}


	void Resources::FlushAllData()
	{
		resourcesDiscovered.Fill(false);
	}


	void		Resources::Update(const Grid::LCellIdI&center,float range, BasicBuffer<UINT32>&outNewDiscoveries)
	{
		static IndexSet covered;
		static Buffer0<UINT32>	coverage;
		static Queue<UINT32>	stacks;
		covered.Clear();
		coverage.Clear();
		stacks.Clear();
		outNewDiscoveries.Clear();

		stacks << center.stackID;

		const TVec3<> worldCenter = Grid::ToWorld(center);

		UINT32 stackID;
		while (stacks >> stackID)
		{
			const TVec3<> world = Grid::ToWorld(Grid::LCellIdI(stackID,center.height));
			if (Vec::quadraticDistance(world,worldCenter) > sqr(range))
				continue;
			if (!covered.IsSet(stackID))
			{
				covered.Set(stackID);
				coverage << stackID;
			}
			else
				continue;

			const auto&n = Grid::low.grid.GetCell(stackID).neighborhood;
			for (BYTE k = 0; k < n.numNeighbors; k++)
				stacks << n.neighborID[k];
		}
			
		foreach (coverage,cov)
		{
			if (resourcesDiscovered[*cov])
				continue;
			resourcesDiscovered[*cov] = true;
			outNewDiscoveries << *cov;
		}
	}


	void		Volume::CheckChanges(float t)
	{
		for (BYTE f = 0; f < BaseGrid::NumFamilies; f++)
		{
			if (families[f].changed)
			{
				families[f].changed = false;
				const index_t offset = Grid::high.grid.GetFamilyOffset(f);
				const count_t range = Grid::high.grid.CountFamilyMembers(f);
				bool anyChanged = false;
				Concurrency::parallel_for(offset, offset + range, [this, &anyChanged](index_t i)
				{
					for (unsigned j = 0; j < Grid::CellStack::Range; j++)
					{
						CellStack::Cell&cell1 = cellStack[i].volumeCell[j];
						if (cell1.Changed(true))
							anyChanged = true;
					}
				});
				if (anyChanged)
				{
					families[f].coverageChanged = true;
					families[f].lastChange = t;
				}
			}
		}

	}

	void			Sensor::ApplyShaftTo(Volume&v, short change, bool*familyChanged, const std::function<UINT32(UINT32 stackID)>&traceFunction)	const
	{
		volume.Clear();

		const UINT32 highCenter = this->gridCenter.stackID;
		op.stacksLeft << highCenter;

		const float r = worldCenter.Length();

		const float hr2 = sqr(radius/r);
		const TVec3<>& center = Grid::high.cells[highCenter].centerVertex.xyz;
		UINT32 current;
		while (op.stacksLeft >> current)
		{
			if (op.stackVisited[current])
				continue;
			op.stackVisited[current] = true;

			float d2 = Vec::quadraticDistance(Grid::high.cells[current].centerVertex.xyz,center);
			if (d2 > hr2)
				continue;

			auto&cell = volume.coverage.Append();
			cell.stackID = current;
			cell.range.end = gridCenter.layer+1;
			cell.range.start = traceFunction(current);
			const auto&n = Grid::high.grid.GetCell(current).neighborhood;
			for (int k = 0; k < n.numNeighbors; k++)
				op.stacksLeft << n.neighborID[k];
		}

		op.ResetVisited(volume);
		ApplyVolume(v,change,familyChanged);
	}


	/*static*/ Grid::HighSetup::TVolumeDescriptor::Operator Sensor::op;
	/*static*/ Grid::HighSetup::TVolumeDescriptor	Sensor::volume;

	void		Sensor::ApplySolidSphereTo(Volume&v, short changeBy, bool*familyChanged)	const
	{
		volume.Clear();
		UINT32 centerStack = this->gridCenter.stackID;
		op.stacksLeft << centerStack;

		const float br = Grid::ToRadius(Grid::CellStack::BottomHeight);
		const float tr = Grid::ToRadius(Grid::CellStack::TopHeight);
		UINT32 next;
		while (op.stacksLeft >> next)
		{
			if (op.stackVisited[next])
				continue;
			op.stackVisited[next] = true;
			const TVec3<>&dir = Grid::high.cells[next].cornerCenter;
			TVec2<> distances;
			TVec3<> bottom,top;
			Vec::mult(dir,br,bottom);
			Vec::mult(dir,tr,top);
			Grid::HighSetup::TVolumeDescriptor::TStack&st = volume.coverage.Append();
			st.stackID = next;
			if (!Obj::DetectSphereEdgeIntersection(worldCenter,radius,bottom,top,distances))
			{
				st.range.SetEmpty(0);
				continue;
			}
			
			st.range.start = (UINT32)floor(std::max(distances.x,0.f) * (Grid::CellStack::Range-1));
			st.range.end = 1 + (UINT32)ceil(std::min(distances.y,1.f) * (Grid::CellStack::Range-1));
			const auto&n = Grid::high.grid.GetCell(next).neighborhood;
			for (int i = 0; i < n.numNeighbors; i++)
				op.stacksLeft << n.neighborID[i];
		}
		op.ResetVisited(volume);


		ApplyVolume(v, changeBy,familyChanged);
	}


	/*static*/ void	Sensor::ApplyVolume(Volume&v, short changeBy, bool*familyChanged)
	{

		#ifdef _DEBUG
		{
			UINT32 crc = CRC32::getChecksum(volume.coverage.pointer(),volume.coverage.GetContentSize());
			LogEvent(EventSignificance::Debug,"Applying sensory change ("+String(changeBy)+"), using descriptor with "+String(volume.coverage.Count())+" stack(s), crc="+IntToHex((int)crc,8));	
		}

		#endif


		foreach (volume.coverage,c)
		{
			#ifdef _DEBUG
				ASSERT_LESS__(c->stackID,Grid::HighSetup::GridType::CellCount);
				ASSERT_LESS_OR_EQUAL__(c->range.start,c->range.end);
				ASSERT_LESS__(c->range.start,Grid::CellStack::Range);
			#endif
			if (c->range.IsEmpty())
				continue;
			auto&stack = v.cellStack[c->stackID];
			familyChanged[Grid::high.grid.GetCell(c->stackID).family] = true;
			for (UINT32 layer = c->range.start; layer < c->range.end; layer++)
			{
				stack.volumeCell[layer].AlterVisibility(changeBy);
			}
		}
	}


	void		Sensor::ApplyShadedSphereTo(Volume&v, short change, bool*familyChanged)	const
	{
		enum
		{
			Visited = 0x1,
			Visible = 0x2,
			Translucent = 0x4
		};

		const Grid::HCellIdU center(Grid::high.Locate(worldCenter),Grid::CellStack::ToLayer( Grid::HeightFromRadius(Vec::length( worldCenter )) ));
		Buffer0<Grid::HCellIdU>	marked;
		{
			auto&cell = v.cellStack[center.stackID].volumeCell[center.layer];
			cell.user = Visited | Visible | Translucent;	//is visible
			marked << center;
		}
		Queue<Grid::HCellIdU>	work;
		Grid::high.VisitNeighborsOf(center,[&work](const Grid::HCellIdU&c)
		{
			work << c;
		});
		Grid::HCellIdU current;
		while (work >> current)
		{
			auto&c = v.cellStack[current.stackID].volumeCell[current.layer];
			if (c.user&Visited)
				continue;
			const TVec3<>c3 = Grid::ToWorld(current);
			if (Vec::quadraticDistance(c3,worldCenter) > sqr(radius))
				continue;

			float3 d03 = c3-worldCenter;
			const TVec3<>&up = Grid::high.grid.GetCell(current.stackID).center;
			d03 -= up * Vec::dot(d03,up);//bend around sphere
			d03 += up * Grid::SingleLayerHeight* float((int)current.layer - (int)center.layer);
			d03.Normalize0();
				
			c.user = Visited;
			marked << current;

			bool translucent = Grid::actualWorld.cellStack[current.stackID].volumeCell[current.layer].IsClearOrWaterIgnoreStructure();

			float visibleWeight=0, totalWeight=0;
			Grid::high.VisitNeighborsOf(current,[&v,&visibleWeight,&totalWeight, &work,current,d03,c3,translucent](const Grid::HCellIdU&next)
			{
				auto&n = v.cellStack[next.stackID].volumeCell[next.layer];
				const TVec3<> n3 = Grid::ToWorld(next);
				const float3	d3 = (n3-c3).Normalized();
				const float dot = Vec::dot(d3,d03);
				if (dot > 0)
				{
					//going outwards
					work << next;
				}
				else
				{
					//going inwards
					if (translucent)
					{
						float w = pow(fabs(dot),64);
						totalWeight += w;
						if ((n.user & Visible))
							visibleWeight += w;
					}
				}
			});
			if (translucent)
			{
				c.user |= Translucent;
				if (visibleWeight  > 0.5f * totalWeight)
				{
					c.user |= Visible;
				}
			}
		}

		foreach (marked,m)
		{
			auto&c = v.cellStack[m->stackID].volumeCell[m->layer];
			bool makeVisible = (c.user & Visible) != 0;

			if (!makeVisible)
			{
				Grid::high.VisitNeighborsOf(*m,[&v,&makeVisible](const Grid::HCellIdU&next)
				{
					const auto&n = v.cellStack[next.stackID].volumeCell[next.layer];
					if (n.user & Visible)
						makeVisible = true;
				});
			}

			if (makeVisible)
			{
				c.AlterVisibility(change);
				familyChanged[Grid::high.grid.GetCell(m->stackID).family] = true;
			}
		}

		foreach (marked,m)
			v.cellStack[m->stackID].volumeCell[m->layer].user = 0;
	}




	void		Sensor::ApplyTo(Volume&v, short change)	const
	{
		bool familyChanged[BaseGrid::NumFamilies] = { false };

		const Timer::Time t0 = timer.Now();
		switch (type)
		{
			case SolidSphere:
				ApplySolidSphereTo(v,change,familyChanged);
			break;
			case ShadedSphere:
				ApplyShadedSphereTo(v,change,familyChanged);
			break;
			case SolidCylinder:
				ApplyShaftTo(v,change,familyChanged,[](UINT32){return 0;});
			break;
			case ShadedCylinder:
				ApplyShaftTo(v,change,familyChanged,
					[this](UINT32 stackID)
					{
						return Grid::actualWorld.TraceSkySensorLayer(stackID,gridCenter.layer,this->terrainPenetration);
					}
				);
			break;
		};

		LogEvent(EventSignificance::PerformanceMeasurement,nullptr,"Sensory change processed in "+String(timer.GetSecondsSince(t0)));

		for (index_t i = 0; i < BaseGrid::NumFamilies; i++)
			if (familyChanged[i])
				v.families[i].changed = true;
	}

}
