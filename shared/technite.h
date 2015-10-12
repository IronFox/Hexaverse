#ifndef techniteH
#define techniteH

#include "config.h"

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


	class Technite
	{
	public:

		static const BYTE	MoveEnergyCost = 1,
							ConsumeEnergyCost = 2,
							GnawAtEnergyCost = 1,
							SplitEnergyCost = 5,
							SplitMatterCost = 5,
							EnergyPackageSize = 5,
							MatterPackageSize = 5;

		static const count_t NumGroundTypes = 13;
		static_assert(NumGroundTypes == Grid::Content::NumberOfPossibilities, "Count mismatch. Adjust array size");

		static const BYTE	MatterYield[NumGroundTypes];
		static const BYTE	DegradeTo[NumGroundTypes];


		static bool			HeldBy(Grid::Content content)
		{
			return content != Grid::Content::Clear && content != Grid::Content::Water && content != Grid::Content::Lava;
		}

		class CompressedLocation
		{
		private:
			UINT32		data;
			static_assert(Grid::HighSetup::GridType::CellCount < 0x1000000, "Number of grid possibilities exceeds compression capacity");
			static_assert(Grid::CellStack::Range < 0x100, "Number of grid layers exceeds compression capacity");
		public:
			Grid::HCellIdI	GetI()	const
			{
				return Grid::HCellIdI(data >> 8, GetHeight());
			}
			Grid::HCellIdU	GetU()	const
			{
				return Grid::HCellIdU(data >> 8, GetLayer());
			}

			int			GetHeight() const
			{
				return Grid::CellStack::ToHeight(GetLayer());
			}

			UINT		GetLayer() const
			{
				return data & 0xFF;
			}
			UINT32		GetStackID() const
			{
				return data >> 8;
			}

			void		operator=(const Grid::HCellIdI&cellID)
			{
				data = (cellID.stackID << 8) | Grid::CellStack::ToLayer(cellID.height);
			}

			void		operator=(const Grid::HCellIdU&cellID)
			{
				data = (cellID.stackID << 8) | cellID.layer;
			}
		};

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

			String		ToString() const
			{
				return "Energy="+String(energy)+", Matter="+String(matter);
			}


			static void Increase(BYTE&local, BYTE by)
			{
				local = (BYTE)std::min<int>((int)local + (int)by, 255);
			}

		};


		static const Resources SplitCost, MoveCost, ConsumeCost, GnawAtCost;// EnergyPackage, MatterPackage;


		enum class Task : BYTE
		{
			None,
			ConsumeSurroundingCell,
			GnawAtSurroundingCell,
			GrowTo,
			TransferEnergyTo,
			TransferMatterTo,
			SelfTransformToType,

			Count
		};

		enum class TaskResult : BYTE
		{
			NothingToDo,
			BadCommand,
			CannotEatThis,
			CannotGnawAtThis,
			CannotStandThere,
			TechniteLacksResources,
			NoTechniteAtDestination,
			MoreWorkNeeded,
			OperationWindowMissed,
			Again,
			Success
		};

		class CompressedTarget
		{
		private:
			BYTE		data = 0;
		public:
			static const BYTE SelfNeighbor = 0xF;

			/**/		CompressedTarget()	{}
			/**/		CompressedTarget(BYTE neighborIndex, int heightDelta)
			{
				Set(neighborIndex, heightDelta);
			}

			unsigned	GetNeighborIndex() const
			{
				return data&0xF;
			}
			int			GetHeightDelta() const
			{
				return ((int)(data >> 4)) -1;
			}

			void		Set(BYTE neighborIndex, int heightDelta)
			{
				data = neighborIndex | (BYTE)((heightDelta+1) << 4);
			}
		};

		class CompressedState
		{
		private:
			BYTE		data = 0;
		public:
			/**/		CompressedState(){}
			/**/		CompressedState(bool lit, BYTE ttl)
			{
				Set(lit,ttl);
			}

			bool		IsLit() const
			{
				return (data & 0x80) != 0;
			}
			BYTE		GetTTL() const
			{
				return (data & 0x7f);
			}

			void		SetLit(bool b)
			{
				if (b)
					data |= 0x80;
				else
					data &= ~0x80;
			}

			void		SetTTL(BYTE ttl)
			{
				data &= 0x80;
				data |= (ttl & 0x7f);
			}

			void		Set(bool lit, BYTE ttl)
			{
				data = (BYTE)((ttl & 0x7F) | (lit ? 0x80 : 0x0));
			}
		};

		struct TCompactState
		{
			CompressedLocation	location;	//4 bytes
			Resources		resources;		//2 bytes
			TaskResult		lastResult;		//1 byte
			CompressedState	state;			//1 byte
		};	//8 bytes


		struct TCompactInstruction
		{
			//CompressedLocation	location;	//4 bytes
			Task				nextTask;	//1 byte
			CompressedTarget	taskTarget;	//1 byte
			BYTE				taskParameter;	//1 byte
		};




		bool				DecodeAbsoluteTarget(Grid::HCellIdU&target) const;
		bool				DecodeAbsoluteTarget(Grid::HCellIdU&myLocation, Grid::HCellIdU&target) const;

		Resources			resources;							// 2
		Task				nextTask = Task::None;				// 1
		TaskResult			taskResult = TaskResult::Again;		// 1
		CompressedLocation	location;							// 4
		CompressedState		state = CompressedState(false,64);	// 1
		CompressedTarget	target;								// 1
		AtomicBool			isDead = false;						// 1
		BYTE				taskParameter = 0;					// 1

		#ifdef CLIENT
			TVec3<BYTE>		color;
			bool			usesColor = false;
		#endif


		/**/				Technite() {}


		#ifdef CLIENT
			void			CompileTendency(TVec3<>&outDirection, float&outMagnitude, TVec3<>&inOutColor) const;
		#endif

		void				Set(const TCompactInstruction&instruction)
		{
			nextTask = instruction.nextTask;
			taskParameter = instruction.taskParameter;
			target = instruction.taskTarget;
		}

		void				ExportState(TCompactState&outState) const
		{
			outState.location = location;
			outState.resources = resources;
			outState.lastResult = taskResult;
			outState.state = state;
		}

		void				ImportState(const TCompactState&inState)
		{
			location = inState.location;
			resources = inState.resources;
			taskResult = inState.lastResult;
			state = inState.state;
		}

		BYTE				GetLifeCost() const
		{
			return GetLifeCostAtHeight(location.GetHeight());
		}

		static BYTE		GetLifeCostAtHeight(int height)
		{
			return (BYTE)std::max(height, 1);
		}


		BYTE			GetLightEnergyYield() const
		{
			return GetLightEnergyYieldAtHeight(location.GetHeight());
		}

		static BYTE		GetLightEnergyYieldAtHeight(int height)
		{
			UINT32 layer = Grid::CellStack::ToLayer(height);
			return (BYTE) (layer * layer / Grid::CellStack::Range/4) + 1;
				//(BYTE)std::max(height, 0)+1;
		}


		void			RegisterFactionSensory(Grid::VisibilityState&, float t) const;

	};

	static_assert(sizeof(Technite::TCompactState) == 8,"Packing error");
	static_assert(sizeof(Technite::TCompactInstruction) == 3,"Packing error");

	static const size_t ResourceSize = sizeof(Technite::Resources);
	static const size_t LocationSize = sizeof(Technite::CompressedLocation);
	static const size_t TechniteSize = sizeof(Technite);
	static const size_t ClientInstructionSize = sizeof(Technite::TCompactInstruction);

	static_assert(ResourceSize == 2,"Packing error");
	static_assert(LocationSize == 4,"Packing error");
	#ifndef CLIENT
		static_assert(TechniteSize == 12,"Packing error");
	#else
		static_assert(TechniteSize == 16,"Packing error");	//+color
	#endif

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
			void				Clear()	{if (volumeCells.IsNotEmpty()) volumeCells.Fill(Empty); numEntries = 0;}
		};

		count_t					totalEntries = 0;
		Cluster					cluster[ClusterCount];

		index_t					Map(const Grid::HCellIdU&, const Cluster*&outCluster) const;
		index_t					Map(const Grid::HCellIdU&, Cluster*&outCluster, bool initIfEmpty);
		//index_t					Map(const Grid::HCellIdU&, const Cluster*&outCluster) const;
		//index_t					Map(const Grid::HCellIdU&, Cluster*&outCluster, bool initIfEmpty);
		//index_t					MapCluster(const Grid::HCellIdU&loc, index_t&outFamily, UINT32&heightRemainder) const;
		index_t					MapCluster(const Grid::HCellIdU&loc, index_t&outFamily, UINT32&heightRemainder) const;

		static const UINT32	Empty = 0xFFFFFFFF;
	public:
		/**/					BroadTechniteMap() {}

		void					Unset(const Grid::HCellIdU& id);
		//void					Unset(const Grid::HCellIdU& id);
		void					Set(const Grid::HCellIdU&id, index_t to);
		//void					Set(const Grid::HCellIdU&id, index_t to);
		bool					Query(const Grid::HCellIdU&id, index_t&out) const;
		//bool					Query(const Grid::HCellIdU&id, index_t&out) const;
		bool					IsSet(const Grid::HCellIdU&id) const;
		//bool					IsSet(const Grid::HCellIdU&id) const;
		bool					SetNew(const Grid::HCellIdU&id, index_t to);
		//bool					SetNew(const Grid::HCellIdU&id, index_t to);
		count_t					Count() const;
		count_t					ThoroughCount() const;
		void					Clear()		{for (index_t i = 0; i < ClusterCount; i++)	cluster[i].Clear(); totalEntries = 0;}
	};


	void						BeginProcessing(float t0);
	bool						ProcessNext();
	float						GetAnimationTime();

	#ifndef CLIENT
		void					VerifyGrid();
	#endif


	class Faction : public Objects::Faction
	{
		typedef Objects::Faction	Super;
	public:
//		Grid::WorldData			knownWorld;
	//	Grid::VisibilityState	visibleWorld;
		static AtomicGrid		atomicGrid;

		Buffer<Technite>		technites;

		float3					color = float3(0.2,0.3,0.9);	//!< Color of technites in the world grid. Unique, red for testing purposes

		BroadTechniteMap		techniteMap;

		BYTE					gridID;				//!< Identifier in the grid. 255/0xFF is invalid. Up to 254 different cell factions on one planet.


		static Buffer0<PFaction>allTechniteFactions;
		static PFaction			gridIDMap[0xFF];


		static PFaction			Create(bool copyInitialWorld = false);
		static PFaction			CreateFromFile(BYTE gridID, bool copyInitialWorld = false);

		void					Destroy(float t);

		void					GetTechniteDirection(UINT32 stackIndex, UINT32 layer, TVec3<>&outDirection)	const;

		index_t					CreateSingleTechnite(const Grid::HCellIdU&, float t);


		static bool				ValidTechniteLocation(const Grid::HCellIdU&location);
		static bool				ValidGlobalTechniteLocation(const Grid::HCellIdU&location);

		virtual bool			IsTechniteFaction() const override { return true; }
		virtual void			OnKill() override;

		#ifndef CLIENT
			virtual bool		SurvivesByOtherMeans()const	override {return technites.IsNotEmpty();}
		#else
			void				ClearTechnites();
			void				AppendTechnites(const ArrayData<Technite::TCompactState>&techs);
			void				SetTechniteColor(index_t techniteIndex, const TVec3<BYTE>&color);

		#endif

		static PFaction			GetFactionOf(BYTE gridFactionID);
		Technite*				GetOwnTechnite(const Grid::HCellIdU&location);


		void					VerifyIntegrity();

		typedef Technite::TaskResult	ExecResult;
	private:
		friend void BeginProcessing(float);
		friend bool ProcessNext();

		struct WorkerData
		{
			#ifdef _DEBUG
			struct LoggedEvent
			{
				CONSTRUCT_ENUMERATION8(Type, EatTechnite,EatCellContent, DegradeMatter, TransferMatter, TransferEnergy, SelfTransform, SpawnTechnite, GrowTo);

				Type			type;
				index_t			workerIndex;
				Grid::HCellIdU	source, target;
				Grid::Content	from,to;
				bool			targetingOwnTechniteFaction,destructive;
				String			result;
				index_t			iteration;

				Timer::Time		stamp = timer.Now();

				bool			operator<(const LoggedEvent&other) const {return stamp < other.stamp;}
				bool			operator>(const LoggedEvent&other) const {return stamp > other.stamp;}
			};


			Buffer<LoggedEvent>		events;
			#endif


			Buffer<Grid::HCellIdU>	newTechnites;
			//Buffer<std::pair<Grid::HCellIdU, Grid::HCellIdU> > moveInstructions;
			Buffer<std::pair<Grid::HCellIdU, Technite::Resources> > increaseResources;
		};


		/**/					Faction():Super() {};

		#if defined CLIENT && defined HAS_WINDOW
			void				ChangeVision(const Grid::HCellIdU&center, short change);
		#endif


		ExecResult				GrowTo(Technite&technite, const Grid::HCellIdU&target, index_t iteration, WorkerData&data);
		//ExecResult				MoveTo(Technite&technite, const Grid::HCellIdU&target, index_t iteration, WorkerData&data);
		ExecResult				Eat(Technite&technite, const Grid::HCellIdU&target, index_t iteration, WorkerData&data, bool destroy);
		bool					AnyEntitiesIn(const Grid::HCellIdU&target);
		Technite::Resources		EatEntitiesIn(const Grid::HCellIdU&target, bool&cantEatThis);
		Technite::Resources		EatTechnite(Technite&local, const Grid::HCellIdU&target, index_t iteration, WorkerData&data, bool factionIsActive);
		bool					SpawnNewTechnite(const Grid::HCellIdU&source, const Grid::HCellIdU&target, WorkerData&data, index_t iteration);
		//bool					MoveTechnite(Technite&technite, const Grid::HCellIdU&target, WorkerData&data);
		bool					ReduceResources(Technite&technite, const Technite::Resources&by);
		void					IncreaseResources(Technite&technite, const Technite::Resources&by);
		void					IncreaseRemoteResources(Technite&technite, const Technite::Resources&by, WorkerData&data);
		Grid::Content			DegradeMatter(const Grid::HCellIdU&source, const Grid::HCellIdU&target, WorkerData&data, index_t iteration, Technite::Resources&outYield, bool destroy);

		ExecResult				ExecuteTask(Technite&cell, index_t iteration, WorkerData&data);
		void					CheckState(Technite&technite);
		void					ProcessLighting(Technite&technite);


		/**
		Queries the currently stored atomic content of the world grid in its background copy (must be called during processing)
		*/
		static Grid::Content	GetLastContent(const Grid::HCellIdU&cellID);
		//static Grid::Content	GetLastContent(const Grid::HCellIdU&cellID);

		/**
		Queries the currently stored non-atomic content of the world grid in its global state (should not be called during processing)
		*/
		static Grid::Content	GetGlobalGridContent(const Grid::HCellIdU&cellID);
		//static Grid::Content	GetGlobalGridContent(const Grid::HCellIdU&cellID);

		#ifndef CLIENT
			void				Process(float t);
			void				ProcessLighting();
			#ifdef _DEBUG
				static void		VerifyAtomicGrid();
				static bool		VerifyAtomicIsVisibleAndMatches(const Objects::PFaction&f, const Grid::HCellIdU&cellID, const Grid::HCellIdU&techniteIn);
			#endif
		#endif

		#ifdef _DEBUG
			static void			ReportLocation(const Grid::HCellIdU&loc);
		#endif

		static Array<WorkerData>	workerData;

		//static WorkPipe<Grid::HCellIdU>	newTechnites;
		//static WorkPipe<Grid::HCellIdU> moveInstructions;
		//static WorkPipe<std::pair<Grid::HCellIdU,Technite::Resources> > increaseResources;
		bool					anyDied;
	};



	void		Create();
	void		Destroy();





}





#endif



