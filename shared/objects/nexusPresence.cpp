#include "nexusPresence.h"
#include "../logEvent.h"
#include "../grid/base.h"
#include "../grid/worldData.h"
#include "gridAndChoices.h"
#include "faction.h"
#include "prototypes.h"
#include "structure/tower.h"
#include "structure/vehiclePlant.h"
#include "../grid/resources.h"
#include "../objects/pathfinding.h"

namespace NexusPresence
{

	void			Place(World::Data::ChangeSequence&seq, const Grid::LCellIdI&loc, BYTE content);

	template <typename T>
		static std::shared_ptr<T> Create(const Objects::PFaction&nexusFaction, World::Data&world, const Grid::LCellIdI&location, const Objects::PStructure&prototype)
		{
			std::shared_ptr<T> structure = To<T>::Cast(
				prototype->CreateNewInstance(nexusFaction,location,0));
			#ifdef CLIENT
				structure->id = TUniqueID::New();
			#endif
			structure->ClearOccupiedTerrain(world, 0);
			float t = 0;
			Objects::InsertConstructed(structure,t);

			return structure;
		}
	

	template <typename T>
		static std::shared_ptr<T> Create(const Objects::PFaction&nexusFaction, World::Data&world, const Grid::LCellIdI&location)
		{
			return Create<T>(nexusFaction,world,location,T::globalPrototype);
		}
	


	void			SpawnNexusPortAtPole(World::Data&world, const Objects::PFaction&nexusFaction, int pole)
	{
		LogEvent(EventSignificance::Common,"Spawning Nexus Port at pole "+String(pole));

		Grid::LCellIdI location((UINT32)pole,0);	//first 12 are corner cells
		int h = Objects::FindApplicableGround(world, location,true,true); //do not submerge into lava
		if (h == Objects::badHeight)
			h = 0;	//just spawn it near median
		static const int R = 5;
		h = M::clamp(h,Grid::CellStack::BottomHeight+1,Grid::CellStack::TopHeight -2);

		location.height = h;
		auto port = Create<Objects::NexusPort>(nexusFaction,world, location);
		#ifndef CLIENT
		{
			float t = 0;
			port->SetEnabled(true,t,false,Objects::WorkingStructure::ForceEnable);
			Objects::RefreshCompoundCoverageIfNeeded(t);
		}
		#endif

		Grid::Resource::Require(Objects::Resource::Fuel).coverage[pole] = 0;
		Grid::Resource::Require(Objects::Resource::Minerals).coverage[pole] = 0;
		Grid::Resource::Require(Objects::Resource::Uranium).coverage[pole] = 0;
	}



	
	class Presence;

	class Pathway
	{
	public:
		void		Init(const World::Data&world, const Presence&start, const Presence&end);
		void		RenderRoad(World::Data::ChangeSequence&seq)	const;
		void		RenderFoundation(World::Data::ChangeSequence&seq)	const;
		void		Clear(World::Data::ChangeSequence&seq)	const;

	private:
		Array<int>	heights;
		index_t		ownerPole, destinationPole;
		Buffer0<Objects::PathNode> path;
		const Presence*start,*end;

	};



	class RingSegment : public Array<UINT32> 
	{
	public:
		UINT32				root;
	};

	class Ring : public Array<RingSegment>
	{
	public:
		/**/				Ring(UINT32 pole, const Buffer0<Grid::TNeighborCellID>&domain, UINT32 radius)
		{
			SetSize(5);
			foreach (*this,seg)
				seg->SetSize(radius-1);

			BitSet contained(Grid::LowSetup::GridType::CellCount);

			const auto constexpr Invalid = std::numeric_limits<UINT32>::max();
			UINT32 root = Invalid;
			foreach (domain,cell)
				if (cell->distanceFromOrigin == radius)
				{
					contained.Set(cell->stackID);
					if (root == Invalid && Grid::low.grid.GetCell(cell->stackID).family == 0)
					{
						//edge
						root = cell->stackID;
					}
				}
			ASSERT__(root != Invalid);

			index_t atSegment = InvalidIndex;
			index_t atElement = 0;
			UINT32 current = root;
			UINT32 prev = Invalid;

			for (;;)
			{
				const auto&cell = Grid::low.grid.GetCell(current);
				if (cell.family == 0)
				{
					ASSERT__(atElement == 0 || atElement == at(0).Count());
					atElement = 0;
					atSegment++;
					if (atSegment == Count())
						return;
					at(atSegment).root = current;
				}
				else
				{
					at(atSegment)[atElement++] = current;
				}

				for (BYTE k = 0; k < cell.neighborhood.numNeighbors; k++)
				{
					const auto n = cell.neighborhood.neighborID[k];
					if (!contained.IsSet(n) || n == prev)
						continue;
					prev = current;
					current = n;
					break;
				}
			}

		}

	};


	class Presence
	{
	public:
		void				Init(const World::Data&world, UINT32 pole, bool isBase, bool hasPort)
		{
			this->isBase = isBase;
			this->hasPort = hasPort || isBase;
			this->pole = pole;
			Grid::low.GetExtendedNeighborhood(pole,Grid::low.grid.CellsPerEdge/2,lowDomain);
			Grid::high.GetExtendedNeighborhood(Grid::low.grid.GetCell(pole).higherEquivalent,Grid::high.grid.CellsPerEdge,highDomain);

			terrainRange = M::TIntRange<>::Invalid;
			heightSum = 0;
			heightCount = 0;

			foreach (highDomain,idx)
			{
				if (idx->distanceFromOrigin > Grid::high.grid.CellsPerEdge / 2)
					continue;
				Grid::LCellIdI location(idx->stackID,0);	//first 12 are corner cells
				int h = world.GetStack(idx->stackID).FindMostApplicableGround(Grid::CellStack::TopHeight,true,true);
				if (h == Grid::CellStack::BottomHeight || h == Grid::BadHeight)
					continue;
				terrainRange.Include(h);
				heightSum += h;
				heightCount++;
			}

			if (heightCount > 0)
				avgHeight = M::RoundToInt(double(heightSum) / heightCount);
			else
			{
				if (Grid::CellStack::bottom.content != Grid::Content::Clear)
					avgHeight = Grid::CellStack::BottomHeight+1;
				else
					avgHeight = 0;
			}
			ASSERT_IS_CONSTRAINED__(avgHeight,Grid::CellStack::BottomHeight+1, Grid::CellStack::TopHeight-1);

		}

		static const constexpr UINT32 Max = 15;


		void				PlaceBase(World::Data&world, const Objects::PFaction&nexusFaction)
		{
			if (this->hasPort)
				SpawnNexusPortAtPole(world,nexusFaction,pole);

			if (isBase)
			{
				const auto radarPrototype = Objects::Prototypes::structures.Find({ConstName::Type_Tower,ConstName::Turret_Radar});
				ASSERT__(radarPrototype);
				const auto launcherPrototype = Objects::Prototypes::structures.Find({ConstName::Type_Tower,ConstName::Turret_Launcher});
				ASSERT__(launcherPrototype);
				const auto aaPrototype = Objects::Prototypes::structures.Find({ConstName::Type_Tower,ConstName::Turret_AntiAircraft});
				ASSERT__(aaPrototype);

				Ring innerRing = Ring(pole,lowDomain,2);

				if (Grid::CellStack::bottom.content != Grid::Content::Clear)
				{
					foreach (innerRing,segment)
					{
						Create<Objects::Tower>(nexusFaction,world,Grid::LCellIdI(segment->at(0),Grid::CellStack::BottomHeight+6),radarPrototype);
						Create<Objects::Tower>(nexusFaction,world,Grid::LCellIdI(segment->root,Grid::CellStack::BottomHeight+3),launcherPrototype);
					}
				}

				Ring factoryRing = Ring(pole,lowDomain,3);

				foreach (factoryRing,segment)
				{
					auto gen = Create<Objects::Generator>(nexusFaction,world, Grid::LCellIdI(segment->at(0),avgHeight));
					#ifndef CLIENT
					{
						float t = 0;
						gen->SetEnabled(true,t,false,Objects::WorkingStructure::ForceEnable);
						Objects::RefreshCompoundCoverageIfNeeded(t);
					}
					#endif

					auto plant = Create<Objects::VehiclePlant>(nexusFaction,world,Grid::LCellIdI(segment->at(1),avgHeight));
				}

				Ring defenseRing = Ring(pole,lowDomain,4);

				foreach (defenseRing,segment)
				{
					Create<Objects::Tower>(nexusFaction,world,Grid::LCellIdI(segment->at(0),avgHeight),radarPrototype);
					Create<Objects::Tower>(nexusFaction,world,Grid::LCellIdI(segment->at(1),avgHeight),aaPrototype);
					Create<Objects::Tower>(nexusFaction,world,Grid::LCellIdI(segment->at(2),avgHeight),launcherPrototype);
				}
			}
		}

		void				CarveBaseTop(World::Data::ChangeSequence&seq)
		{
			if (!isBase)
			{
				for (int h = avgHeight; h <= Grid::CellStack::TopHeight; h++)
					Place(seq,Grid::LCellIdI(this->pole,h),Grid::Content::Clear);
				return;
			}
			foreach (highDomain,idx)
			{
				if (idx->distanceFromOrigin <= Max)
				{
					seq.SelectStack(idx->stackID);
					for (int h = avgHeight; h < Grid::CellStack::TopHeight; h++)
						seq.Clear(h);
				}
				else if (idx->distanceFromOrigin <= Max+8)
				{
					seq.SelectStack(idx->stackID);
					int carveToHeight = avgHeight + (idx->distanceFromOrigin - Max - 1)/2;
					for (int h = carveToHeight; h <= Grid::CellStack::TopHeight; h++)
						seq.Clear(h);
				}
			}

		}

		void				CarveBase(World::Data::ChangeSequence&seq)
		{
			if (!isBase)
				return;

			foreach (highDomain,idx)
			{
				if (idx->distanceFromOrigin <= 8)
				{
					seq.SelectStack(idx->stackID);
					seq.Change(avgHeight-1,Grid::Content::Lava);

				}


				if (idx->distanceFromOrigin <= 2)
				{
					for (int h = avgHeight; h < Grid::CellStack::TopHeight; h++)
						seq.Change(h,Grid::Content::Clear);
					int from = Grid::CellStack::BottomHeight;
					if (Grid::CellStack::bottom.content == Grid::Content::Clear)
						from = avgHeight - 7;
					for (int h = from; h < avgHeight; h++)
						seq.Change(h,Grid::Content::Granite);

				}
				else if (idx->distanceFromOrigin <= Max)
				{
					seq.SelectStack(idx->stackID);
					int top = Grid::CellStack::TopHeight;
					int carveBottom = Grid::CellStack::BottomHeight;
					int fillBottom = Grid::CellStack::BottomHeight;
					if (idx->distanceFromOrigin <= 9)
					{
						top = avgHeight-1-(9-idx->distanceFromOrigin);
						fillBottom = carveBottom = Grid::CellStack::BottomHeight + (9-idx->distanceFromOrigin);
						if (idx->distanceFromOrigin == 4)
						{
							fillBottom++;	//ledge for radar
							carveBottom++;
						}
						seq.Change(avgHeight-1,Grid::Content::Granite);
					}
					else
					{
						carveBottom = Grid::CellStack::BottomHeight + (idx->distanceFromOrigin - 9);
					}
					for (int h = carveBottom; h <= top; h++)
						seq.Clear(h);

					for (int h = top+1; h+1 < avgHeight; h++)
						seq.Change(h,Grid::Content::Granite);

					if (Grid::CellStack::bottom.content != Grid::Content::Clear)
						for (int h = Grid::CellStack::BottomHeight; h < fillBottom; h++)
							seq.Change(h,Grid::Content::Granite);

					for (int h = Grid::CellStack::BottomHeight; h < carveBottom; h++)
					{
						switch (seq.GetCellI(h).content)
						{
							case Grid::Content::Clear:
							case Grid::Content::Granite:
							case Grid::Content::Rock:
							case Grid::Content::CrystalForest:
							case Grid::Content::Crystals:
								continue;
							case Grid::Content::Lava:
								seq.Change(h,Grid::Content::Granite);
								break;
							default:
								seq.Change(h,Grid::Content::Rock);
								break;
						}
					}

					if (idx->distanceFromOrigin <= 4)
					{
						seq.Change(avgHeight+2,Grid::Content::Granite);
						if (idx->distanceFromOrigin == 4)
							for (int h = avgHeight; h < avgHeight+2; h++)
								seq.Change(h,Grid::Content::Granite);
					}
					else if (idx->distanceFromOrigin <= 6)
					{
						seq.Change(avgHeight+1,Grid::Content::Granite);
					}


					/*
					if (idx->distanceFromOrigin <= 8 && idx->distanceFromOrigin >= 7)
					{
						seq.Change(avgHeight+1,Grid::Content::Granite);
						if (idx->distanceFromOrigin == 8)
							for (int h = avgHeight; h < avgHeight + 1; h++)
								seq.Change(h,Grid::Content::Granite);
					}

					/*
					if (idx->distanceFromOrigin < 7 && (idx->distanceFromOrigin % 2) == 0)
					{
						seq.Change(avgHeight + 2 + (6 - idx->distanceFromOrigin)/2,Grid::Content::Foundation);
					}
					*/
				}
				else if (idx->distanceFromOrigin == Max+1)
				{
					seq.SelectStack(idx->stackID);
					for (int h = Grid::CellStack::BottomHeight; h <= Grid::CellStack::TopHeight; h++)
					{
						switch (seq.GetCellI(h).content)
						{
							case Grid::Content::Clear:
							case Grid::Content::Granite:
							case Grid::Content::Rock:
							case Grid::Content::CrystalForest:
							case Grid::Content::Crystals:
								continue;
							case Grid::Content::Lava:
								seq.Change(h,Grid::Content::Granite);
								break;
							default:
								seq.Change(h,Grid::Content::Rock);
								break;
						}
					}
				}
				


			}

		}


		void				CheckHeight(int globalAvgHeight, int maxHeightDifference)
		{
			int delta = avgHeight - globalAvgHeight;
			if (abs(delta) <= maxHeightDifference)
				return;

			avgHeight = globalAvgHeight + M::Sign(delta) * maxHeightDifference;
			ASSERT_IS_CONSTRAINED__(avgHeight,Grid::CellStack::BottomHeight+1, Grid::CellStack::TopHeight-1);
		}

		int					GetHeight() const {return avgHeight;}
		index_t				GetPole() const {return pole;}
		bool				IsBase() const {return isBase;}

		void				TracePoleRoutes(const World::Data&world, const ArrayRef<Presence>&others, BasicBuffer<Pathway>&outPathways)
		{
			Buffer0<index_t> neighborPoles;
			const auto&edgeField = Grid::low.grid.GetGeometry().edgeField;
			for (index_t e = 0; e < edgeField.Count(); e+=2)
			{
				auto from = edgeField[e];
				auto to = edgeField[e+1];
				if (from == pole)
					neighborPoles << to;
				if (to == pole)
					neighborPoles << from;
			}


			
			foreach (neighborPoles,p)
			{
				const auto&other = others[*p];
				if (other.pole > pole && (hasPort || other.hasPort))
				{
					Pathway&pathway = outPathways.Append();
					pathway.Init(world, *this,others[*p]);
				}
			}
		}


	private:
		bool				hasPort, isBase;
		Buffer0<Grid::TNeighborCellID>
							lowDomain,highDomain;
		M::TIntRange<>		terrainRange;
		INT64				heightSum;
		count_t				heightCount;
		int					avgHeight;
		UINT32				pole;
	




	};

	void			Place(World::Data::ChangeSequence&seq, const Grid::LCellIdI&loc, BYTE content)
	{
		ASSERT_IS_CONSTRAINED__(loc.height,Grid::CellStack::BottomHeight, Grid::CellStack::TopHeight);


		const auto fine = Grid::ToHigh(loc);
		const auto n = Grid::high.grid.GetCell(fine.stackID).neighborhood;

		seq.Change(fine,content);

		for (int k = 0; k < n.numNeighbors; k++)
		{
			seq.Change(Grid::HCellIdI(n.neighborID[k],fine.height),content);
		}
	}

	void			ClearGround(World::Data::ChangeSequence&seq, const Grid::LCellIdI&loc)
	{
		ASSERT_IS_CONSTRAINED__(loc.height,Grid::CellStack::BottomHeight+1, Grid::CellStack::TopHeight-1);
		const auto fine = Grid::ToHigh(loc);
		const auto n = Grid::high.grid.GetCell(fine.stackID).neighborhood;

		seq.Clear(fine);
		seq.Clear(fine+1);

		for (int k = 0; k < n.numNeighbors; k++)
		{
			seq.Clear(Grid::HCellIdI(n.neighborID[k],fine.height));
			seq.Clear(Grid::HCellIdI(n.neighborID[k],fine.height+1));
		}
	}

	class PathTracer
	{
	public:
		PathTracer(const Presence&origin, index_t pathIndex, sindex_t step, ArrayRef<Objects::PathNode> path)
		{
			this->isBase = origin.IsBase();
			this->startIndex = this->pathIndex = pathIndex;
			initialHeight = lastHeight = origin.GetHeight();
			distance = 1;
			this->step = step;
			this->path = path;
		}


		count_t	GetDistance() const
		{
			return distance;
		}

		bool	operator<(const PathTracer&b) const
		{
			return pathIndex < b.pathIndex;
		}

		void	Progress(const World::Data&world,const PathTracer&other, std::function<void(index_t,int)> outHeight)
		{
			if (isBase && distance <= 3)
			{
				outHeight(pathIndex,lastHeight);
				lastSet = pathIndex;
				pathIndex+=step;
				distance++;
				return;
			}

			


			const auto location = Grid::LCellIdI(path[pathIndex].cell.stackID,lastHeight);
			int terrainHeight = Objects::FindApplicableGround(world, location,true,true); //do not submerge into lava
			if (terrainHeight == Objects::badHeight || terrainHeight == Grid::CellStack::BottomHeight)
			{
				lastHeightTolerance++;
				pathIndex += step;
				distance++;
				return;
			}

			const int distToOther = other.lastSet != InvalidIndex ? abs(int(other.lastSet - pathIndex)) : 10000;
			if (distToOther < 0)
				return;

			terrainHeight = M::Clamp(terrainHeight,lastHeight - lastHeightTolerance, lastHeight+lastHeightTolerance);
			terrainHeight = M::Clamp(terrainHeight,other.lastHeight - distToOther, other.lastHeight + distToOther);
			outHeight(pathIndex,terrainHeight);
			auto prevHeight = lastHeight;
			lastHeight = terrainHeight;
			lastHeightTolerance = 1;

			index_t lastSetNext = lastSet != InvalidIndex ? lastSet + step : startIndex;
			if (lastSetNext != pathIndex)
			{
				int startHeight = prevHeight;
				int endHeight = terrainHeight;
				InterpolateHeight(outHeight, lastSetNext, pathIndex, startHeight, endHeight);
			}

			lastSet = pathIndex;
			pathIndex += step;
			distance++;
		}

		void	CompleteTo(const World::Data&world,const PathTracer&other, std::function<void(index_t,int)> outHeight)
		{
			if (lastSet != InvalidIndex && other.lastSet != InvalidIndex)
			{
				if (lastSet+1 < other.lastSet)
				{
					InterpolateHeight(outHeight,lastSet+1, other.lastSet,lastHeight,other.lastHeight);
				}
			}
			else
			{


				index_t from = lastSet != InvalidIndex ? lastSet+1 : 0;
				index_t to = other.lastSet != InvalidIndex ? other.lastSet : path.Count();

				int fromHeight = lastHeight;
					//lastSet != InvalidIndex ? outHeights[lastSet] : initialHeight;
				int toHeight = other.lastHeight;
					//other.lastSet != InvalidIndex ? outHeights[other.lastSet] : other.initialHeight;
				InterpolateHeight(outHeight,from, to, fromHeight, toHeight);

			}
		}


		static void InterpolateHeight(std::function<void(index_t,int)> outHeight, index_t begin, index_t end, int startHeight, int endHeight)
		{
			ASSERT_IS_CONSTRAINED__(startHeight,Grid::CellStack::BottomHeight+1, Grid::CellStack::TopHeight-1);
			ASSERT_IS_CONSTRAINED__(endHeight,Grid::CellStack::BottomHeight+1, Grid::CellStack::TopHeight-1);

			index_t at = 0;
			const count_t range = abs(sindex_t(end - begin));
			const int heightRange = endHeight - startHeight;
			const sindex_t step = end > begin ? 1 : -1;

			for (index_t j = begin; j != end; j += step)
			{
				int h = int(startHeight + M::RoundToInt( (double(at)+0.5) / range * heightRange ));
				outHeight(j,h);
				at++;
			}
		}

	private:
		ArrayRef<Objects::PathNode> path;
		bool	isBase;
		index_t	pathIndex,startIndex;
		index_t distance;
		sindex_t step;
		int initialHeight;
		int lastHeight;
		int lastHeightTolerance = 0;
		index_t lastSet = InvalidIndex;
	};


	static const constexpr int InvalidHeight = INT_MIN;

	static std::function<void(index_t,int)> SetHeight(MutableArrayRef<int> outHeights)
	{
		return [&outHeights](index_t idx, int height)
		{
			ASSERT_IS_CONSTRAINED__(height,Grid::CellStack::BottomHeight+1, Grid::CellStack::TopHeight-1);
			if (idx > 0)
			{
				int neighborHeight = outHeights[idx-1];
				if (neighborHeight != InvalidHeight)
					ASSERT3__(abs(neighborHeight - height) <= 1, (idx-1),height,neighborHeight);
			}
			if (idx+1 < outHeights.Count())
			{
				int neighborHeight = outHeights[idx+1];
				if (neighborHeight != InvalidHeight)
					ASSERT3__(abs(neighborHeight - height) <= 1, (idx+1),height,neighborHeight);
			}
			outHeights[idx] = height;
		};
	}

	void		Pathway::Init(const World::Data&world, const Presence&start, const Presence&end)
	{
		this->start = &start;
		this->end = &end;
		this->ownerPole = ownerPole;
		this->destinationPole = destinationPole;


		path.Clear();
		Objects::Pathfinding::AppendShortestDirectPath(Grid::LCellIdI(start.GetPole(),0),Grid::LCellIdI(end.GetPole(),0),path);
//		path.Truncate(Grid::low.grid.CellsPerEdge/2);	//no need to process other half
		path.EraseLast();


		heights.SetSize(path.Count());
		heights.Fill(InvalidHeight);
		PathTracer a(start,0,1,path.ToRef()), b(end,path.Count()-1,-1,path.ToRef());


		while (a < b)
		{
			a.Progress(world,b,SetHeight(heights));
			b.Progress(world,a,SetHeight(heights));

		}
		a.CompleteTo(world,b,SetHeight(heights));
		for (index_t i = 0; i < heights.Count(); i++)
			ASSERT2__(heights[i] != InvalidHeight,i, heights.Count());
				

	}


	

	void	Pathway::RenderRoad(World::Data::ChangeSequence&seq)	const
	{
		for (index_t i = 0; i < path.Count(); i++)
		{
			Place(seq, Grid::LCellIdI(path[i].cell.stackID,heights[i]-1), Grid::Content::Road);
		}
	}

	void	Pathway::RenderFoundation(World::Data::ChangeSequence&seq)	const
	{
		BitSet mask,m_1,m0,m1;
		Buffer0<Grid::TNeighborCellID> extNeighborhood;
		for (index_t i = 0; i < path.Count(); i++)
		{
			const auto h = heights[i];
			const auto stackID = path[i].cell.stackID;
			Place(seq, Grid::LCellIdI(stackID,h-2), Grid::Content::Foundation);

			auto finer = Grid::low.grid.GetCell(stackID).higherEquivalent;



			if (((i == 1 && start->IsBase())
				||
				(i+2 == path.Count() && end->IsBase()))
				&& Grid::CellStack::bottom.content != Grid::Content::Clear)
			{
				for (int h = 3; h <= 5; h++)
				{
					Place(seq,Grid::LCellIdI(stackID,h+Grid::CellStack::BottomHeight),Grid::Content::Clear);
				}
			}

			if (Grid::CellStack::bottom.content == Grid::Content::Clear)
			{


			}
			else
			{

				if ((i % 5) == 2 && i+1 < path.Count()
					&& 
					(i > 5 || !start->IsBase())
					&&
					(i+ 6 < path.Count() || !end->IsBase())
					)
					//pillar
				{
					mask.Clear();
					m_1.Clear();
					m0.Clear();
					m1.Clear();
					Grid::high.AddExtendedNeighborhood(Grid::ToHigh(path[i-1].cell).stackID,1,m_1);
					Grid::high.AddExtendedNeighborhood(Grid::ToHigh(path[i].cell).stackID,1,m0);
					Grid::high.AddExtendedNeighborhood(Grid::ToHigh(path[i+1].cell).stackID,1,m1);
					mask.Join(m_1);
					mask.Join(m0);
					mask.Join(m1);

					Grid::high.GetExtendedNeighborhood(Grid::ToHigh(path[i].cell).stackID,3,extNeighborhood);

					

					int b = Grid::CellStack::BottomHeight;
					/*
					seq.SelectStack(path[i].cell.stackID);
					for (int h2 = h-3; h2 >= Grid::CellStack::BottomHeight; h2--)
					{
						auto content = seq.GetCellI(h2).content;
						if (content != Grid::Content::Clear && content != Grid::Content::Foundation)
						{
							b = h2;
							break;
						}
					}
					*/

					foreach(extNeighborhood,n)
					{
						if (mask.IsSet(n->stackID))
						{
							seq.SelectStack(n->stackID);
							const int xDistance = n->distanceFromOrigin;
							const float yDistance = (sqrt(4.f*4 - M::Sqr(4.f - xDistance)));
							const int height = n->distanceFromOrigin <= 1 ? Grid::CellStack::Range : M::RoundToInt(2*( 4.f- yDistance));

							int myH = h;
							if (!m0.IsSet(n->stackID))
							{
								if (m1.IsSet(n->stackID))
									myH = heights[i+1];
								else
									myH = heights[i-1];
							}

							const int bottom = M::Max(myH - 3 - height,b);
							for (int h2 = myH-3; h2 >= bottom; h2--)
							{
								//if (!seq.GetCellI(h2).IsClearOrLiquidIgnoreStructure())
								//	break;
								seq.Change(h2,Grid::Content::Foundation);
							}
						}
					}
				}

			}

		}
	}

	void	Pathway::Clear(World::Data::ChangeSequence&seq)	const
	{
		for (index_t i = 0; i < path.Count(); i++)
		{
			ClearGround(seq, Grid::LCellIdI(path[i].cell.stackID,heights[i]));
		}
	}




	void Create( World::Data&world, const Objects::PFaction&nexusFaction, bool & placeBases, bool & placeAllPorts, bool &placeSatellites )
	{
		placeBases = placeBases;
		placeAllPorts = placeAllPorts ||		//all ports by config
			(
				RuntimeConfig::gameGoal == RuntimeConfig::GameGoal::ConquerNexus && //game is conquer nexus, so we need some
				(
					RuntimeConfig::empZones					//emp zones are on, so we need all ports
					|| 
					!placeBases	//nexus bases don't exist, so it makes sense to place all
				)
			);

		if (placeBases || placeAllPorts)
		{
			LogEvent(EventSignificance::Common, "Spawning Nexus Ports (EMP fields="+String(RuntimeConfig::empZones)+")...");

			M::TIntRange<> hRange = M::TIntRange<>::Invalid;

			Buffer0<Presence>	presences;

			for (int pole = 0; pole < 12; pole++)
			{
				const bool isolated = BaseGrid::IsIsolatedCorner(pole);
				presences.Append().Init(world,pole,placeBases && isolated, placeAllPorts);
			}


			INT64 heightSum = 0;
			foreach (presences,p)
				heightSum += p->GetHeight();

			int avgHeight = M::RoundToInt(double(heightSum) / presences.Count());
			const int maxHeightDifference = (Grid::low.grid.CellsPerEdge-2 - 4)/2;

			Buffer0<Pathway> poleRoutes;

			foreach (presences,p)
			{
				p->CheckHeight(avgHeight,maxHeightDifference);
			}

			{
				auto seq = world.BeginChangeSequence(0);


				foreach (presences,p)
					p->CarveBaseTop(seq);
			}
			
			foreach (presences,p)
			{
				p->TracePoleRoutes(world, presences.ToRef(),poleRoutes);
			}

			auto seq = world.BeginChangeSequence(0);

			foreach (presences,p)
				p->CarveBase(seq);

			foreach (poleRoutes, p)
				p->RenderFoundation(seq);

			foreach (presences,p)
				Place(seq,Grid::LCellIdI(p->GetPole(),p->GetHeight()-2),Grid::Content::Foundation);

			foreach (poleRoutes, p)
				p->RenderRoad(seq);

			foreach (presences,p)
				Place(seq,Grid::LCellIdI(p->GetPole(),p->GetHeight()-1),Grid::Content::Road);

			foreach (poleRoutes, p)
				p->Clear(seq);

			#ifndef CLIENT
				auto lock = nexusFaction->LockSensorUpdates();
			#endif

			foreach (presences,p)
				p->PlaceBase(world,nexusFaction);

		}
	}

	#ifndef CLIENT
		void Create( bool & placeBases, bool & placeAllPorts, bool &placeSatellites )
		{
			Create(World::actual,Objects::Faction::nexus,placeBases,placeAllPorts,placeSatellites);
		}
	#endif
	
}

