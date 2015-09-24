#ifndef techniteH
#define techniteH

#include "config.h"

#if TECHNITES

#include "sphereGrid.h"
#include "typeDecl.h"
#include "uniqueID.h"

#include "objects/faction.h"

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


	ALIGN4
	class Technite
	{
	public:

		static const BYTE	MoveEnergyCost = 1,
							EatEnergyCost = 1,
							SplitEnergyCost = 5,
							SplitMatterCost = 5,
							EnergyPackageSize = 5,
							MatterPackageSize = 5;

		static const count_t NumGroundTypes = 13;
		static_assert(NumGroundTypes == Grid::Content::NumberOfPossibilities, "Count mismatch. Adjust array size");

		static const BYTE	MatterYield[NumGroundTypes];
		static const BYTE	ChangeTypeTo[NumGroundTypes];


		static bool			HeldBy(Grid::Content content)
		{
			return content != Grid::Content::Clear && content != Grid::Content::Water && content != Grid::Content::Lava;
		}

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

			Count
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

		struct TState
		{
			bool		lit;
			count_t		ttl;

			/**/		TState() {}
			/**/		TState(bool lit, count_t ttl) : lit(lit), ttl(ttl) {}
			/**/		TState(BYTE compressed) { Decompress(compressed); }

			BYTE		Compress() const
			{
				return (BYTE)((ttl & 0x7F) | (lit ? 0x80 : 0x0));
			}

			void		Decompress(BYTE compressed)
			{
				lit = (compressed & 0x80) != 0;
				ttl = (compressed & 0x7f);
			}
		};


		bool			DecodeAbsoluteTarget(Grid::HCellIdI&target) const;

		Task			nextTask = Task::None;
		TaskResult		taskResult = TaskResult::Again;
		Grid::HCellIdI	location;

		BYTE			compressedTarget = 0,
						compressedState = TState(false,64).Compress();
		AtomicBool		isDead = false;


		/**/			Technite() {}


		TState			DecodeState() const { return TState(compressedState); }

		BYTE			GetLifeCost() const
		{
			return GetLifeCostAtHeight(location.height);
		}

		static BYTE		GetLifeCostAtHeight(int height)
		{
			return (BYTE)std::max(height, 0);
		}


		BYTE			GetLightEnergyYield() const
		{
			return GetLightEnergyYieldAtHeight(location.height);
		}

		static BYTE		GetLightEnergyYieldAtHeight(int height)
		{
			UINT32 layer = Grid::CellStack::ToLayer(height);
			return (BYTE) (layer * layer / Grid::CellStack::Range/4) + 1;
				//(BYTE)std::max(height, 0)+1;
		}

	};

	//sstatic const size_t Size = sizeof(Technite);

	static_assert(sizeof(Technite) == 8 + sizeof(Grid::HCellIdI),"Packing error");

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


	void						BeginProcessing();
	bool						ProcessNext(float t);



	class Faction : public Objects::Faction
	{
		typedef Objects::Faction	Super;
	public:
//		Grid::WorldData			knownWorld;
	//	Grid::VisibilityState	visibleWorld;
		static AtomicGrid		atomicGrid;

		Buffer<Technite>		technites;
		//GenericHashTable<Grid::HCellIdI,index_t, Grid::HCellIdI>	techniteMap;

		BroadTechniteMap		techniteMap;

		BYTE					gridID;				//!< Identifier in the grid. 255/0xFF is invalid. Up to 254 different cell factions on one planet.


		static Buffer0<PFaction>allTechniteFactions;
		static PFaction			gridIDMap[0xFF];


		static PFaction			Create(const String&name = "", bool copyInitialWorld = false);
		static PFaction			CreateFromFile(BYTE gridID, const String&name="", bool copyInitialWorld = false);

		void					Destroy(float t);



		index_t					CreateSingleTechnite(const Grid::HCellIdI&, float t);


		static bool				ValidTechniteLocation(const Grid::HCellIdI&location);
		static bool				ValidGlobalTechniteLocation(const Grid::HCellIdI&location);

		virtual bool			IsTechniteFaction() const override { return true; }


		typedef Technite::TaskResult	ExecResult;
	private:
		friend void BeginProcessing();
		friend bool ProcessNext(float);

		struct WorkerData
		{
			Buffer<Grid::HCellIdI>	newTechnites;
			Buffer<std::pair<Grid::HCellIdI, Grid::HCellIdI> > moveInstructions;
			Buffer<std::pair<Grid::HCellIdI, Technite::Resources> > increaseResources;
		};


		/**/					Faction(const String&name=""):Super(name) {};

		ExecResult				GrowTo(Technite&technite, const Grid::HCellIdI&target, index_t iteration, WorkerData&data);
		ExecResult				MoveTo(Technite&technite, const Grid::HCellIdI&target, index_t iteration, WorkerData&data);
		ExecResult				Eat(Technite&technite, const Grid::HCellIdI&target, index_t iteration, WorkerData&data);


		Technite::Resources		EatEntitiesIn(const Grid::HCellIdI&target, bool&cantEatThis);
		Technite::Resources		EatTechnite(Technite&local, const Grid::HCellIdI&target, bool factionIsActive);
		bool					SpawnNewTechnite(const Grid::HCellIdI&target, WorkerData&data);
		bool					MoveTechnite(Technite&technite, const Grid::HCellIdI&target, WorkerData&data);
		bool					ReduceResources(Technite&technite, const Technite::Resources&by);
		void					IncreaseResources(Technite&technite, const Technite::Resources&by);
		void					IncreaseRemoteResources(Technite&technite, const Technite::Resources&by, WorkerData&data);
		Grid::Content			DegradeMatter(const Grid::HCellIdI&target, Technite::Resources&outYield);
		Technite*				GetOwnTechnite(const Grid::HCellIdI&location);

		PFaction				GetFactionOf(BYTE gridFactionID);
		ExecResult				ExecuteTask(Technite&cell, index_t iteration, WorkerData&data);
		void					CheckState(Technite&technite);
		void					ProcessLighting(Technite&technite);


		/**
		Queries the currently stored atomic content of the world grid in its background copy (must be called during processing)
		*/
		static Grid::Content	GetLastContent(const Grid::HCellIdI&cellID);
		static Grid::Content	GetLastContent(const Grid::HCellIdU&cellID);

		/**
		Queries the currently stored non-atomic content of the world grid in its global state (should not be called during processing)
		*/
		static Grid::Content	GetGlobalGridContent(const Grid::HCellIdI&cellID);

		void					Process(float t);
		void					ProcessLighting();


		//static WorkPipe<Grid::HCellIdI>	newTechnites;
		//static WorkPipe<Grid::HCellIdI> moveInstructions;
		//static WorkPipe<std::pair<Grid::HCellIdI,Technite::Resources> > increaseResources;
		bool					anyDied;
	};



	void		Create();
	void		Destroy();





}





#endif










#endif
