#include <global_root.h>
#include "planet.h"

namespace Grid
{
	TPlanetConfig	currentPlanetConfig;




	void			ExtendedPlanetConfig::InitUnknown(CellStack&stack, float2 heights) const
	{
		//float topHeight = heights.y;
		//based on this, h could go up to 40, though chances for that are excessively rare
		int up0 = (int)M::Round(heights.y * mountains.heightScale / RawLayerHeight / UnitScale);
		if (abs(up0) <= mountains.dampening)
			up0 = 0;
		else
			up0 -= mountains.dampening * M::Sign(up0);
		int up1 = (int)M::Round(heights.x * extendedCore.heightScale / RawLayerHeight / UnitScale);
		if (abs(up1) <= extendedCore.dampening)
			up1 = 0;
		else
			up1 -= extendedCore.dampening * M::Sign(up1);
		const int h = M::clamp(mountains.baseHeight + up0, CellStack::BottomHeight, CellStack::TopHeight - 1);
		const int coreH = M::clamp(extendedCore.baseHeight + up1, CellStack::BottomHeight, CellStack::TopHeight - 1);
		
//			int coreH = clamp(Round(fabs(heights.x) * config.coreScale / RawLayerHeight)-HalfRange, -HalfRange, HalfRange - 1);

		UINT uh = CellStack::ToLayer(h);
		UINT coreLayer = CellStack::ToLayer(coreH);

		#ifdef HAS_WINDOW
			stack.lowestOpenCell = (BYTE)uh + 1;
			stack.highestOpaqueCell = (BYTE)uh;
		#endif
		if (type != Content::Clear)
			for (UINT i = 0; i <= uh; i++)
				stack.volumeCell[i].content = CellStack::Cell::IsLiquid(type) ? Content::Water : Content::Unknown;
		else
			for (UINT i = 0; i <= uh; i++)
				stack.volumeCell[i].content = Content::Clear;


		for (UINT l = uh+1; l < CellStack::Range; l++)
			stack.volumeCell[l].content = Content::Clear;

		//if (Cell::IsLiquid(config.extendedCoreType ) || config.extendedCoreType == Content::Clear)
		//	for (UINT i = 0; i < coreLayer; i++)
		//		volumeCell[i].content = Content::Clear;
		//else
		if (extendedCoreType != Content::Clear)
			for (UINT i = 0; i < coreLayer; i++)
				stack.volumeCell[i].content = CellStack::Cell::IsLiquid(type) ? Content::Lava : Content::Granite;
		else
			for (UINT i = 0; i < coreLayer; i++)
				stack.volumeCell[i].content = Content::Clear;
	}

	void			ExtendedPlanetConfig::FillTypes(CellStack&stack) const
	{
		UINT top = 0;
		int h = CellStack::BottomHeight;
		for (UINT layer = CellStack::Range-1; layer < CellStack::Range; layer--)
		{
			BYTE&content = stack.volumeCell[layer].content;
			if (content == Content::Clear)
			{
				continue;
			}
			if (!top)
			{
				bool isSolid = true;
				//isSolid = Cell::IsSolid(content);

				if (content == Content::Unknown)
				{
					top = layer;
					h = CellStack::ToHeight(top);

					if (h > snowHeight)
						content = Content::Snow;
					else
						if (h < liquidHeight && CellStack::Cell::IsLiquid(liquidCoverage) && CellStack::Cell::IsSolid(liquidOutline))
							content = liquidOutline;
						else  
							if (type == Content::Unknown)
								content = h < 3 ? Content::Grass : Content::Rock;
							else
								content = type;
					continue;
				}
			}
			if (content == Content::Unknown || content == Content::Water)	//mountain layer
			{
				if (type == Content::Unknown)
				{
					//bool hasEarth = Cell::typeStrength[config.topType] <= Cell::typeStrength[Content::Earth];
					UINT rockHeight = h > 3 ? top : top - (3 - h);
					//if (!hasEarth)
					//	rockHeight = Range;
					
					content = layer <= rockHeight ? Content::Rock : Content::Earth;
				}
				else
					content = type;
			}
			else if (content == Content::Granite || content == Content::Lava)
				content = extendedCoreType;
		}


		if (liquidCoverage != Content::Clear)
		{
			UINT top = std::min(CellStack::ToLayer(liquidHeight),CellStack::Range-1);
			for (UINT i = 0; i <= top; i++)
				if (stack.volumeCell[i].content == Content::Clear)
				{
					stack.volumeCell[i].content = liquidCoverage;
				}
		}
	}


	float2	RandomHeight(const TVec3<>&center0, const ExtendedPlanetConfig&config);

	bool	ExtendedPlanetConfig::ApplySeed()
	{
		//LogEvent(EventSignificance::Uncommon,"seed: "+String(lastSeed));
		Random random(lastSeed);
		extendedCore.Randomize(random);
		mountains.Randomize(random);
		RandomizeSun(random);
		return true;
	}

	void	ExtendedPlanetConfig::SetSeed(UINT32 seed)
	{
		lastSeed = seed;
		//LogEvent(EventSignificance::Uncommon,"seed set "+String(lastSeed));
		ApplySeed();
	}

	void	ExtendedPlanetConfig::RandomizeSeeds(TwisterRandom&twisterRandom)
	{
		SetSeed(twisterRandom.Next(std::numeric_limits<UINT32>::max()));
	}

	

	float	RandomHeight(const TVec3<>&center0, const ExtendedPlanetConfig::NoiseSet&config)
	{
		//center *= 3.f;
		TVec3<> center = center0;
		float rs = 0;

		float strength = 1.f;
		//noiseLevels = std::min(noiseLevels,5);
		for (int i = 0; i < 5; i++)
		{
			center *= 2.f;
			strength /= 2.f;
			//const float3&seed = config.seed[i];
			rs += Get(config.fractal[i],center)*strength;
		}
		rs *= Get(config.product,center0*2.f)*0.5f + 0.5f;
		//noise.Get(center0.x*2.f + config.productSeed.x, center0.y*2.f + config.productSeed.y, center0.z*2.f + config.productSeed.z) * 0.5 + 0.5f;

		float rf = 1.5f;
		float depress = M::smoothstep(0.1f, 0.2f, 
						0.5f * Get(config.depression0, center0*6.f*rf)
							+ Get(config.depression1, center0*3.f*rf));
		float fc = 0.25f * Get(config.depression2, center0*4.f*rf) 
				+ 0.75f * Get(config.depression3, -center0*2.f*rf);
		//noise.Get(center0.x*2.f + 10.f, center0.y*2.f, center0.z*2.f);
		depress *= fc;// *fabs(fc);

		rs -= depress*0.3f;

		return rs;
		//return (float)Sign(rs)*pow(fabs(rs),1.3f) ;
	}

	float2	RandomHeight(const TVec3<>&center0, const ExtendedPlanetConfig&config)
	{
		return float2(
			RandomHeight(center0*config.extendedCore.frequency,config.extendedCore),
			RandomHeight(center0*config.mountains.frequency,config.mountains)
			);

	}



	void ExtendedPlanetConfig::BuildWorld(World::Data&world) const
	{
		world.ResetStructureCounters(true);
		CellStack::bottom.content = (BYTE)innerCoreType;

		World::Data::ChangeSequence seq = world.BeginChangeSequence(0);

		Concurrency::parallel_for(UINT32(0),HighSetup::GridType::CellCount,[&world,this,&seq](UINT32 i)
		{
			InitUnknown(seq.AccessStack(i).GetStack(),RandomHeight(high.cells[i].centerVertex.xyz,*this));
		});

		for (UINT32 i = 0; i < LowSetup::GridType::CellCount; i++)
		{
			const UINT32 fine = Grid::low.grid.GetCell(i).higherEquivalent;
			auto stackAccess = seq.AccessStack(fine);
			auto&stack = stackAccess.GetStack();
			const auto&n = Grid::high.grid.GetCell(fine).neighborhood;
			for (UINT l = 0; l < CellStack::Range; l++)
			{
				if (stack.volumeCell[l].IsClearOrLiquidIgnoreStructure())
				{
					int cnt = 0;
					BYTE staticDeltaContainer;
					for (int ni = 0; ni < n.numNeighbors; ni++)
					{
						const auto&other = world.GetCell(Grid::HCellIdU(n.neighborID[ni],l));
						if (!other.IsClearOrLiquidIgnoreStructure())
						{
							cnt ++;
							staticDeltaContainer = other.content;
						}
					}
					if (cnt > 1)
					{
						stack.volumeCell[l].content = staticDeltaContainer;
	
					}
					else if (cnt > 0)
					{
						for (int ni = 0; ni < n.numNeighbors; ni++)
						{
							const Grid::HCellIdU	otherAt(n.neighborID[ni],l);
							const auto&other = world.GetCell(otherAt);
							//auto&otherStack = world.cellStack[n.neighborID[ni]];
							//auto&other = otherStack.volumeCell[l];
							if (!other.IsClearOrLiquidIgnoreStructure())
							{
								staticDeltaContainer = stack.volumeCell[l].content;

								cnt ++;
								seq.Change(otherAt, stack.volumeCell[l].content);
							}
						}
					}
				}
			}
		}

		Concurrency::parallel_for(UINT32(0),LowSetup::GridType::CellCount,[this,&world,&seq](UINT32 i)
		{
			UINT32 fine = Grid::low.grid.GetCell(i).higherEquivalent;
			auto&centerStack = seq.AccessStack(fine);
			const auto&n = Grid::high.grid.GetCell(fine).neighborhood;
			const auto&n0 = Grid::low.grid.GetCell(i).neighborhood;
			for (int ni = 0; ni < n.numNeighbors; ni++)
			{
				if (n0.neighborID[ni] < i)
					continue;

				const auto&n2 = Grid::high.grid.GetCell(n.neighborID[ni]).neighborhood;

				auto nStackAccess = seq.AccessStack(n.neighborID[ni]);
				auto&nStack = nStackAccess.GetStack();
				for (UINT l = 0; l < CellStack::Range; l++)
				{
					if (!nStack.volumeCell[l].IsClearOrLiquidIgnoreStructure())
						continue;
					count_t cnt = 0;
					BYTE staticDeltaContainer = 0;
					for (int nj = 0; nj < n2.numNeighbors; nj++)
					{
						const auto&n2c = world.GetCell(Grid::HCellIdU(n2.neighborID[nj],l));
						if (!n2c.IsClearOrLiquidIgnoreStructure())
						{
							cnt ++;
							staticDeltaContainer = n2c.content;
						}
					}
					if (cnt > 3)
					{
						nStack.volumeCell[l].content = staticDeltaContainer;
					}
				}

			}
		});


		Concurrency::parallel_for(UINT32(0),HighSetup::GridType::CellCount,[&world,this,&seq](UINT32 i)
		{
			FillTypes(seq.AccessStack(i).GetStack());
		});


		const bool solidProx = CellStack::Cell::IsSolid(liquidOutline) && CellStack::Cell::IsLiquid(liquidCoverage);
		if (solidProx)
			for (UINT32 i = 0; i < HighSetup::GridType::CellCount; i++)
			{
				auto stackAccess = seq.AccessStack(i);
				auto&stack = stackAccess.GetStack();
				for (UINT l = 0; l < CellStack::Range; l++)
				{
					const BYTE staticDeltaContainer = stack.volumeCell[l].content;
					if (solidProx && staticDeltaContainer == liquidCoverage)
					{
						const auto&n = high.grid.GetCell(i).neighborhood;
						for (BYTE k = 0; k < n.numNeighbors; k++)
							seq.ChangeConditionally(Grid::HCellIdU(n.neighborID[k],l),liquidOutline,World::Condition::IsSolid);
					}

				}
			}
	}

	

	

}
