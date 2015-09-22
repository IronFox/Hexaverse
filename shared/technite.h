#ifndef techniteH
#define techniteH

#include "config.h"

#if TECHNITES

#include "sphereGrid.h"
#include "typeDecl.h"
#include "uniqueID.h"

namespace Technites
{
	DECLARE_POINTER_CLASS(Faction);



	struct AtomicBool
	{
		volatile char	value;

		/**/	AtomicBool(bool v = false) :value(v) {}

		bool	Set(bool expectedCurrent, bool setToIfMatch)
		{
			return _InterlockedCompareExchange8(&value, setToIfMatch, expectedCurrent) == (char)expectedCurrent;
		}

		operator bool() const { return value != 0; }

		void operator=(bool b)
		{
			value = b;
		}


	};

	static_assert(sizeof(AtomicBool) == 1, "Struct alignment out of wack");



	class Technite
	{
	public:

		static const BYTE	MoveEnergyCost = 1,
							EatEnergyCost = 1,
							SplitEnergyCost = 5,
							SplitMatterCost = 5,
							EnergyPackageSize = 5,
							MatterPackageSize = 5;

		struct Resources
		{
			BYTE		energy = 0,
						matter = 0;


			/**/		Resources() {}
			/**/		Resources(BYTE energy, BYTE matter) :energy(energy), matter(matter) {}



			Resources	operator/(int div) const
			{
				return Resources(energy / div, matter / div);

			}

			void		operator+=(const Resources&by)
			{
				Increase(energy, by.energy);
				Increase(matter, by.matter);
			}

			bool		operator-=(const Resources&by)
			{
				if (matter < by.matter || energy < by.energy)
					return false;
				matter -= by.matter;
				energy -= by.energy;
				return true;
			}


			static void Increase(BYTE&local, BYTE by)
			{
				local = (BYTE)std::min<int>((int)local + (int)by, 255);
			}

		}				resources;


		static const Resources SplitCost, MoveCost, EatCost, EnergyPackage, MatterPackage;


		enum class Task : BYTE
		{
			None,
			EatMoveTo,
			GrowTo,
			TransferEnergyTo,
			TransferMatterTo,
		};

		enum class TaskResult : BYTE
		{
			NothingToDo,
			BadCommand,
			CannotEatThis,
			CannotStandThere,
			TechniteLacksResources,
			NoTechniteAtDestination,
			MoreWorkNeeded,
			Again,
			Success
		};

		struct TTarget
		{
			unsigned	neighborIndex;
			int			heightDelta;


			BYTE		Compress()	const
			{
				BYTE base = (BYTE)neighborIndex;
				base |= (BYTE)((heightDelta+1) << 3);
				return base;
			}

			void		Decompress(BYTE compressed)
			{
				neighborIndex = (compressed&0x7);
				heightDelta = ((int)(compressed >> 3)) -1;
			}
		};


		bool			DecodeAbsoluteTarget(Grid::HCellIdI&target) const;


		Task			nextTask = Task::None;
		TaskResult		taskResult = TaskResult::Again;
		BYTE			compressedTarget=0;
		AtomicBool		isDead = false;
		Grid::HCellIdI	location;


		/**/			Technite() {}
		/**/			Technite(const Technite&other) :nextTask(other.nextTask), taskResult(other.taskResult), 
							compressedTarget(other.compressedTarget),location(other.location),
							isDead((bool)other.isDead)
		{
			//bool current = other.isDead;
			//bool n = other.isDead;
			ASSERT__(isDead.Set(other.isDead, other.isDead));
			
		}



	};

	class AtomicGrid
	{
	public:
		struct CellStack
		{
			struct Cell
			{
				std::atomic<BYTE>	content;
			};
			Cell	volumeCell[Grid::CellStack::Range];

			/**/	CellStack()
			{
				memset(volumeCell, 0, sizeof(volumeCell));
			}
		};
		Array<CellStack>	cellStack;

		volatile bool		familiesChanged[BaseGrid::NumFamilies];



		/**/				AtomicGrid() : cellStack(Grid::HighSetup::GridType::CellCount)
		{}
		void				Import(const Grid::WorldData&data);
		void				ExportChanged(Grid::WorldData&outData, float t);
		void				SignalStackChange(UINT32 stackID);
	};


	class BroadTechniteMap
	{
	private:
		static const count_t HeightSections = 4;
		static const count_t SectionLayers = Grid::CellStack::Range / HeightSections;
		static const count_t ClusterCount = BaseGrid::NumFamilies * HeightSections;	//four height domains each

		struct Cluster
		{
			Array<UINT32>		volumeCells;
			count_t				numEntries = 0;
		};

		count_t					totalEntries = 0;
		Cluster					cluster[ClusterCount];

		index_t					Map(const Grid::HCellIdI&, const Cluster*&outCluster) const;
		index_t					Map(const Grid::HCellIdI&, Cluster*&outCluster, bool initIfEmpty);
		index_t					MapCluster(const Grid::HCellIdI&loc, index_t&outFamily, UINT32&heightRemainder) const;
		index_t					MapCluster(const Grid::HCellIdU&loc, index_t&outFamily, UINT32&heightRemainder) const;

		static const UINT32	Empty = 0xFFFFFFFF;
	public:
		/**/					BroadTechniteMap() {}

		void					Unset(const Grid::HCellIdI& id);
		void					Set(const Grid::HCellIdI&id, index_t to);
		bool					Query(const Grid::HCellIdI&id, index_t&out) const;
		bool					IsSet(const Grid::HCellIdI&id) const;
		bool					SetNew(const Grid::HCellIdI&id, index_t to);
		count_t					Count() const;
	};


	class Faction : public std::enable_shared_from_this<Faction>
	{
	public:

		Grid::WorldData			knownWorld;
		Grid::VisibilityState	visibleWorld;
		static AtomicGrid		atomicGrid;

		Buffer<Technite>		technites;
		//GenericHashTable<Grid::HCellIdI,index_t, Grid::HCellIdI>	techniteMap;

		BroadTechniteMap		techniteMap;

		BYTE					gridID;				//!< Identifier in the grid. 255/0xFF is invalid. Up to 254 different cell factions on one planet.
		TUniqueID				globalID;


		static Buffer0<PFaction>all;
		static PFaction			gridIDMap[0xFF];


		static PFaction			Create();

		void					Destroy(float t);

		void					Process(float t);


		index_t					CreateSingleTechnite(const Grid::HCellIdI&, float t);

		static void				Begin();
		static bool				ProcessNext(float t);

		static bool				ValidTechniteLocation(const Grid::HCellIdI&location);
		static bool				ValidTechniteLocation(BYTE groundContent, BYTE targetContent, BYTE targetStructureLevel);

		typedef Technite::TaskResult	ExecResult;
	private:
		/**/					Faction() {};

		ExecResult				GrowTo(Technite&technite, const Grid::HCellIdI&target, index_t iteration);
		ExecResult				MoveTo(Technite&technite, const Grid::HCellIdI&target, index_t iteration);
		ExecResult				Eat(Technite&technite, const Grid::HCellIdI&target, index_t iteration);


		Technite::Resources		EatEntitiesIn(const Grid::HCellIdI&target, bool&cantEatThis);
		Technite::Resources		EatTechnite(const Grid::HCellIdI&target, bool factionIsActive);
		void					SpawnNewTechnite(const Grid::HCellIdI&target);
		bool					MoveTechnite(Technite&technite, const Grid::HCellIdI&target);
		bool					ReduceResources(Technite&technite, const Technite::Resources&by);
		void					IncreaseResources(Technite&technite, const Technite::Resources&by);
		void					IncreaseRemoteResources(Technite&technite, const Technite::Resources&by);
		Grid::Content			DegradeMatter(const Grid::HCellIdI&target, Technite::Resources&outYield);
		Technite*				GetOwnTechnite(const Grid::HCellIdI&location);

		PFaction				GetFactionOf(BYTE gridFactionID);
		ExecResult				ExecuteTask(Technite&cell, index_t iteration);
		void					CheckState(Technite&technite);


		Grid::Content			GetLastContent(const Grid::HCellIdI&cellID);


		static WorkPipe<Grid::HCellIdI>	newTechnites;
		static WorkPipe<Grid::HCellIdI> moveInstructions;
		static WorkPipe<std::pair<Grid::HCellIdI,Technite::Resources> > increaseResources;
		bool					anyDied;
	};



	void		Create();
	void		Destroy();





}





#endif










#endif
