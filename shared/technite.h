#ifndef techniteH
#define techniteH

#include "config.h"

#include "sphereGrid.h"
#include "typeDecl.h"
#include "uniqueID.h"

#include <net/tcp.h>


namespace Technites
{
	extern bool limitTechniteLife;
	extern const EventClass techniteError;

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

	struct ExecutionLayer
	{
		enum T
		{
			Messages,
			Scan,
			Transfer,
			End,
			Kill,
			ReplaceMoveSpawn,

			Count
		};
	};


	class Technite : public TCP::Peer::Attachment
	{
	public:

		enum class Task : BYTE
		{
			None,		//->TaskResult::Success
			Scan,		//->TaskResult::Success/InsufficientResources
			Message,	//->TaskResult::Success/TargetOutOfRange/TargetNoTechnite/TargetOffline
			Move,		//->TaskResult::Success/TargetBlocked/TargetOutOfRange/InsufficientResources/CannotStandHere
			Transfer,	//->TaskResult::Success/TargetOutOfRange/TargetNoTechnite/InsufficientResources
			Replace,	//->TaskResult::Success/TargetBlocked/TargetOutOfRange/InsufficientResources
			End,		//->TaskResult::Success/InsufficientResources
			Spawn,		//->TaskResult::Success/TargetBlocked/InsufficientResources
			Kill,		//->TaskResult::Success/TargetOutOfRange/TargetNoTechnite

			Count
		};

		static const count_t TaskCount = (count_t)Task::Count;


		static FixedArray<BYTE,TaskCount>				energyCost;
		static FixedArray<ExecutionLayer::T,TaskCount>	layer;

		static const count_t NumGroundTypes = 13;
		static_assert(NumGroundTypes == Grid::Content::NumberOfPossibilities, "Count mismatch. Adjust array size");



		static bool			SupportedBy(Grid::Content content)
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

			UINT32			GetData() const {return data;}

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
			BYTE		energy = 0;


			/**/		Resources() {}
			/**/		Resources(BYTE energy) :energy(energy) {}



			Resources	operator/(int div) const
			{
				return Resources(energy / div);

			}

			void		operator+=(const Resources&by)
			{
				Increase(energy, by.energy);
			}

			bool		operator-=(const Resources&by)
			{
				if (energy < by.energy)
					return false;
				energy -= by.energy;
				return true;
			}
			bool		operator<(const Resources&by)
			{
				return energy < by.energy;	//if multiple resource types must use ||
			}

			String		ToString() const
			{
				return "Energy="+String(energy);
			}


			static void Increase(BYTE&local, BYTE by)
			{
				local = (BYTE)std::min<int>((int)local + (int)by, 255);
			}

		};


		enum class TaskResult : BYTE
		{
			Success,
			BadCommand,
			CannotStandThere,
			TargetOutOfRange,
			TargetNoTechnite,
			TargetBlocked,
			TargetOffline,
			InsufficientResources,
			NoTechniteAtDestination,
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



		struct Instruction
		{
			TUniqueID			targetTechnite;
			String				payload;
			Task				nextTask;	//1 byte
			CompressedTarget	taskTarget;	//1 byte
			BYTE				taskParameter;	//1 byte
		};

		struct StartOptions
		{
			String				repo,
								initMessage;
			TUniqueID			parent;
		};


		bool				DecodeAbsoluteTarget(Grid::HCellIdU&target) const;
		bool				DecodeAbsoluteTarget(Grid::HCellIdU&myLocation, Grid::HCellIdU&target) const;
		
		#ifndef CLIENT
			std::thread		dockerProcessThread;
			TCP::WPeer		peer;
			AtomicBool		isDead = false;						// 1
			CryptographicID	secret;								// 32
		#endif

		Instruction			nextInstruction;
		Grid::HighSetup::TVolumeDescriptor	hFieldOfVision;
		Grid::LowSetup::TVolumeDescriptor	lFieldOfVision;
		float				visionRadius = -1.f;

		TUniqueID			id;									// 16
		Resources			resources;							// 2
		TaskResult			taskResult = TaskResult::Success;	// 1
		CompressedLocation	location;							// 4
		CompressedState		state = CompressedState(false,64);	// 1
		StartOptions		startOptions;


		/**/				Technite() {}

		#ifndef CLIENT
			/**/			Technite(Technite&&) = default;
			/**/			Technite(const Technite&) = delete;
			Technite&		operator=(const Technite&) = delete;
		#endif

		String				GetDockerName() const {return "tech"+id.ToFullString();}

		/**
		Configures fieldOfVision to default technite vision.
		Update does NOT try and check whether the current field of vision already matches
		*/
		void				SetStandardFieldOfVision();
		/**
		Configures fieldOfVision to map a spherical area around the local technite.
		Update does NOT try and check whether the current field of vision already matches
		*/
		void				SetSphericalFieldOfVision(float radius);


		BYTE				GetInitialTTL() const
		{
			return GetInitialTTLAtHeight(location.GetHeight());
		}

		static BYTE		GetInitialTTLAtHeight(int height)
		{
			return GetInitialTTLAtLayer(Grid::CellStack::ToLayer(height));
		}
		static BYTE		GetInitialTTLAtLayer(UINT layer)
		{
			return (sqr(Grid::CellStack::Range) - sqr(layer)) / Grid::CellStack::Range * 2;
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

		#ifndef CLIENT
			void		StartClient();
			void		ShutdownClient();
		#endif
	};


	static const size_t ResourceSize = sizeof(Technite::Resources);
	static const size_t LocationSize = sizeof(Technite::CompressedLocation);
	static const size_t TechniteSize = sizeof(Technite);

	static_assert(ResourceSize == 1,"Packing error");
	static_assert(LocationSize == 4,"Packing error");


	typedef Sphere<>	TSensor;


	class AtomicGrid
	{
	public:
		typedef BYTE Content;
		struct CellStack
		{
			struct Cell
			{
				std::atomic<Content>	content;
			};
			Cell	volumeCell[Grid::CellStack::Range];

			/**/	CellStack()
			{
				memset(volumeCell, 0, sizeof(volumeCell));
			}

			void	Fill(Content content)	{for (index_t i = 0; i < Grid::CellStack::Range; i++) volumeCell[i].content = content;}
		};
		Array<CellStack>	cellStack;

		volatile bool		familiesChanged[BaseGrid::NumFamilies];



		/**/				AtomicGrid() : cellStack(Grid::HighSetup::GridType::CellCount)
		{}
		//void				Import(const Grid::WorldData&data);
		//void				ExportChanged(Grid::WorldData&outData, float t);
		//void				SignalStackChange(UINT32 stackID);
		void				Fill(Content content)	{foreach (cellStack,stack) stack->Fill(content);}
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
		index_t					MapCluster(const Grid::HCellIdU&loc, index_t&outFamily, UINT32&heightRemainder) const;

		static const UINT32	Empty = 0xFFFFFFFF;
	public:
		/**/					BroadTechniteMap() {}

		index_t					Unset(const Grid::HCellIdU& id);
		void					Set(const Grid::HCellIdU&id, index_t to);
		bool					Query(const Grid::HCellIdU&id, index_t&out) const;
		bool					IsSet(const Grid::HCellIdU&id) const;
		bool					SetNew(const Grid::HCellIdU&id, index_t to);
		count_t					Count() const;
		count_t					ThoroughCount() const;
		void					Clear()		{for (index_t i = 0; i < ClusterCount; i++)	cluster[i].Clear(); totalEntries = 0;}
	};


	void						BeginProcessing(float t0);
	bool						ProcessNextLayer();
	float						GetAnimationTime();

	#ifndef CLIENT
		void					VerifyGrid();
	#endif


	extern AtomicGrid			atomicGrid;
	extern Buffer<Technite>		technites;
	extern BroadTechniteMap		techniteMap;

	#ifdef CLIENT
		extern Array<String>	supportedTypes;
	#endif

	struct TNewTechnite : public Technite::StartOptions
	{
		TUniqueID				id;
		Grid::HCellIdU			location;

		bool					operator<(const TNewTechnite&other)	const
		{
			return parent < other.parent;
		}
		bool					operator>(const TNewTechnite&other)	const
		{
			return parent > other.parent;
		}
	};

	index_t						Create(const TNewTechnite&, float t);
	bool						CanStandHere(const Grid::HCellIdU&tecLoc);

	void						VerifyIntegrity();


	struct WorkerData
	{
		Buffer<Grid::HCellIdU>	newTechnites;
		//Buffer<std::pair<Grid::HCellIdU, Grid::HCellIdU> > moveInstructions;
		Buffer<std::pair<Grid::HCellIdU, Technite::Resources> > increaseResources;
		Buffer0<std::pair<Grid::HCellIdU, BYTE> >	addSensor;
	};


	void					ProcessLighting();


	void					Clear();

	void					SetupAndStartService(USHORT port);
	void					StopService();
	bool					ServiceIsOnline();
	void					DestroyService();

	void					Resolve();

	index_t					GetTechniteIndex(const TUniqueID&);
	Technite*				GetTechnite(const TUniqueID&);

	index_t					GetTechniteIndex(const Grid::HCellIdU&);
	Technite*				GetTechnite(const Grid::HCellIdU&);
}





#endif



