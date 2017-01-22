#ifndef objectJobsH
#define objectJobsH

#include <io/riff_handler.h>
#include "../sphereGrid.h"
#include "cursorMode.h"
#include "types.h"
#include "../uniqueID.h"
#include "components.h"

namespace Objects
{


	namespace VehicleJob
	{

		

		static const int	DigHorizontalMineralCosts = -15,//virtual costs for time calculation. not actually consumed
							ClearAboveMineralCosts = -15,//virtual costs for time calculation. not actually consumed
							DigDownMineralCosts = -15,//virtual costs for time calculation. not actually consumed
							BuildFoundationMineralCosts = 14,
							BuildRoadMineralCosts = 7,
							BuildWallMineralCosts = 2,
							RegularSelfDetonationCosts = -100,
							DoomsdaySelfDetonationCosts = 200;


		class BaseJob;
		typedef std::shared_ptr<BaseJob>	PJob;
		typedef std::weak_ptr<BaseJob>		WJob;


		class BaseJob : public std::enable_shared_from_this<BaseJob>, public ISerializable
		{
		public:
			int						roundsToJobCompletion,
									resourceCostLeft;

			#ifndef CLIENT
				UINT32				ioID,		//transitory. used during io operations
									ioParentID;		//transitory. used during io operations
				WJob				parent;
				const 
			#endif
			bool					movesToFocusWhenDone;

			enum class Autocomplete
			{
				Never,
				Always,
				IfSatellite
			};

			Autocomplete			jobAutoCompletion;	//must be non-const because it can be updated from deserialization


			/**/					BaseJob(Autocomplete jobAutoCompletion, bool movesToFocusWhenDone):jobAutoCompletion(jobAutoCompletion),movesToFocusWhenDone(movesToFocusWhenDone)	{}
			/**/					BaseJob(int costs, const MobileEntity&v, Component::operation_t op, Autocomplete jobAutoCompletion, bool movesToFocusWhenDone);
			virtual					~BaseJob()	{}
			
			BYTE					GetConstructionActionCostPerRound(const MobileEntity&v) const;
			void					ResetResourceAndRoundsLeft(int costs, const MobileEntity&v, Component::operation_t op);

			/**
			Expected to reset resources and rounds left based on the local job composition for the given vehicle.
			Should internally call ResetResourceAndRoundsLeft() with appropriate parameters.
			*/
			virtual void			Restart(const MobileEntity&v) = 0;
			virtual Component::operation_t	GetOperationAffinity() const = 0;
			virtual Resource		GetConsumedResourceType() const {return Resource::Minerals;}
			virtual bool			DelayedBySleep() const {return true;}

			#ifndef CLIENT
				enum begin_t
				{
					Started,
					Delayed,
					CannotStartAbort
				};
			
				enum continue_t
				{
					JobDone,
					JobDelayed,
					ContinueJob,
					TerminalFailure,
					TerminalFailureVehicleDestroyed
				};


				virtual begin_t		BeginHere(MobileEntity&v, const Grid::LCellIdI&, bool hasToMoveFirst)	{return hasToMoveFirst ? CannotStartAbort : Started;}
				virtual continue_t	Continue(MobileEntity&v, const Grid::LCellIdI&, bool freshRound);
				virtual bool		CanContinue(MobileEntity&v, const Grid::LCellIdI&, String&outError)	{return true;}
				virtual bool		IsImplicitlyDone(MobileEntity&v, const Grid::LCellIdI&_destination) const {return false;}
				virtual continue_t	Finish(MobileEntity&v, const Grid::LCellIdI&)=0;
				virtual String		GetName() const = 0;
				virtual Riff::TID	GetDataKey() const = 0;
				virtual PJob		Instantiate() const =0;
				virtual ConstName	GetCTypeName() const = 0;

				static Buffer0<PJob>allJobPrototypes;

			#endif
		};



		class PathJob : public BaseJob
		{
		public:
			typedef BaseJob	Super;

			Buffer0<Grid::LCellIdI>		path;
			bool						pathChanged;

			/**/						PathJob(Autocomplete jobAutoCompletion, bool movesToFocusWhenDone):Super(jobAutoCompletion, movesToFocusWhenDone)	{}
			/**/						PathJob(int costs, const MobileEntity&v, Component::operation_t op, Autocomplete jobAutoCompletion, bool movesToFocusWhenDone):
										Super(costs, v, op, jobAutoCompletion, movesToFocusWhenDone), pathChanged(false)	{}


			virtual bool				UpdatePath(const MobileEntity&v, const Grid::LCellIdI&origin, Grid::LCellIdI&destination) =0;
			virtual	serial_size_t		GetSerialSize(bool export_size) const override {return (serial_size_t)(path.GetContentSize() + (export_size ? GetSerialSizeOfSize((serial_size_t)path.length()):0));}
			virtual	bool				Serialize(IWriteStream&outStream, bool exportSize) const override
			{
				if (exportSize)
					if (!outStream.WriteSize(path.length()))
						return false;
				return outStream.WritePrimitives(path.pointer(),path.length());
			}
			virtual	bool				Deserialize(IReadStream&inStream, serial_size_t fixedSize) override
			{
				if (fixedSize == EmbeddedSize)
				{
					if (!inStream.ReadSize(fixedSize))
						return false;
				}
				else
					fixedSize /= sizeof(Grid::LCellIdI);
				path.Clear();
				return inStream.ReadPrimitives(path.AppendRow(fixedSize),fixedSize);
			}
		};

		#ifdef CLIENT

			class DummyPathJob : public PathJob
			{
			public:
				typedef PathJob	Super;

				/**/					DummyPathJob(const MobileEntity&v) : Super(1,v,GetOperationAffinity(),Autocomplete::Always,false) {}
				virtual void			Restart(const MobileEntity&v) override {Super::ResetResourceAndRoundsLeft(1,v,GetOperationAffinity());}
				virtual Component::operation_t	GetOperationAffinity() const override {return Component::TerrainRemove;}


				virtual bool			UpdatePath(const MobileEntity&v, const Grid::LCellIdI&origin, Grid::LCellIdI&destination)	override
				{
					return true;	//dont change anything
				}
			};

			class DummyJob : public BaseJob
			{
			public:
				typedef BaseJob	Super;
				bool					delayedBySleep = true;

				/**/					DummyJob(const MobileEntity&v) : Super(1,v,GetOperationAffinity(),Autocomplete::Always,false) {}
				virtual void			Restart(const MobileEntity&v) override {Super::ResetResourceAndRoundsLeft(1,v,GetOperationAffinity());}
				virtual Component::operation_t	GetOperationAffinity() const override {return Component::TerrainRemove;}
				virtual bool			DelayedBySleep() const override {return delayedBySleep;}
				//virtual Riff::TID		GetDataKey() const override {return "BLDF";};
				//virtual PJob	Instantiate() const override {return PJob(new BuildFoundation());}
				virtual	serial_size_t	GetSerialSize(bool exportSize) const override	{return 0;};
				virtual	bool			Serialize(IWriteStream&outStream, bool exportSize) const override {return true;};
				virtual	bool			Deserialize(IReadStream&inStream, serial_size_t fixedSize) override {return true;};

			};
		#else

			class BuildRoad : public BaseJob
			{
			public:
				typedef BaseJob			Super;
				UINT32					prevCell,nextCell;


				/**/					BuildRoad():Super(Autocomplete::Always,false)	{}
				/**/					BuildRoad(MobileEntity&v) : BaseJob(BuildRoadMineralCosts,v,GetOperationAffinity(),Autocomplete::Always,false),prevCell(-1),nextCell(-1) {}
				/**/					BuildRoad(MobileEntity&v, UINT32 from, UINT32 to) : BaseJob(BuildRoadMineralCosts,v,GetOperationAffinity(),Autocomplete::Always,false),prevCell(from),nextCell(to) {}
				virtual void			Restart(const MobileEntity&v) override {Super::ResetResourceAndRoundsLeft(BuildRoadMineralCosts,v,GetOperationAffinity());}
				virtual Component::operation_t	GetOperationAffinity() const override {return Component::TerrainAdd;}


				virtual String			GetName() const override {return "build roads";}


				virtual Riff::TID		GetDataKey() const override {return "BLDR";};
				virtual ConstName		GetCTypeName() const override {return ConstName::Job_BuildRoad;}
				virtual PJob			Instantiate() const override {return PJob(new BuildRoad());}
				virtual	serial_size_t	GetSerialSize(bool export_size) const override {return 8;};
				virtual	bool			Serialize(IWriteStream&outStream, bool exportSize) const override {return outStream.WritePrimitive(prevCell) && outStream.WritePrimitive(nextCell);}
				virtual	bool			Deserialize(IReadStream&inStream, serial_size_t fixedSize) override {return inStream.ReadPrimitive(prevCell) && inStream.ReadPrimitive(nextCell);}


				virtual continue_t		Finish(MobileEntity&v, const Grid::LCellIdI&focus)	override;
			};


			class BuildWallUp : public BaseJob
			{
			protected:
			
				/**/					BuildWallUp(Autocomplete jobAutoCompletion, bool movesToFocusWhenDone):Super(jobAutoCompletion, movesToFocusWhenDone)	{}
				/**/					BuildWallUp(MobileEntity&v,Autocomplete jobAutoCompletion, bool movesToFocusWhenDone) : BaseJob(BuildWallMineralCosts,v,Component::TerrainAdd,jobAutoCompletion, movesToFocusWhenDone) {}

				virtual Grid::LCellIdI	FocusToBuildLocation(const Grid::LCellIdI&focus) const {return focus-1;}

				static bool				ConnectTo(BYTE content)
				{
					return Grid::CellStack::Cell::IsSolid(content);
					//return content != Grid::Content::Clear;	//everything not clear
					//return Grid::CellStack::Cell::typeStrength[content] >= 4;	//grass strength
				}

			public:
				typedef BaseJob			Super;
				

				/**/					BuildWallUp():Super(Autocomplete::Always,true)	{}
				/**/					BuildWallUp(MobileEntity&v) : BaseJob(BuildWallMineralCosts,v,GetOperationAffinity(),Autocomplete::Always,true) {}
				virtual void			Restart(const MobileEntity&v) override {Super::ResetResourceAndRoundsLeft(BuildWallMineralCosts,v,GetOperationAffinity());}
				virtual Component::operation_t	GetOperationAffinity() const override {return Component::TerrainAdd;}


				virtual begin_t			BeginHere(MobileEntity&v, const Grid::LCellIdI&_destination, bool hasToMoveFirst)	override;
				virtual String			GetName() const override {return "build wall";}
				virtual bool			CanContinue(MobileEntity&v, const Grid::LCellIdI&focus, String&outError)	override;


				virtual Riff::TID		GetDataKey() const override {return "BLDW";};
				virtual ConstName		GetCTypeName() const override {return ConstName::Job_BuildWallUp;}
				virtual PJob			Instantiate() const override {return PJob(new BuildWallUp());}
				virtual	serial_size_t	GetSerialSize(bool export_size) const override {return 0;};
				virtual	bool			Serialize(IWriteStream&outStream, bool exportSize) const override {return true;}
				virtual	bool			Deserialize(IReadStream&inStream, serial_size_t fixedSize) override {return true;}


				virtual continue_t		Finish(MobileEntity&v, const Grid::LCellIdI&focus)	override;
			};

			class BuildHorizontalWall : public BuildWallUp
			{
			protected:
				virtual Grid::LCellIdI	FocusToBuildLocation(const Grid::LCellIdI&focus) const override {return focus;}

			public:
				typedef BuildWallUp		Super;
				

				/**/					BuildHorizontalWall():Super(Autocomplete::Never,false)	{}
				/**/					BuildHorizontalWall(MobileEntity&v) : Super(v,Autocomplete::Never,false) {}

				virtual bool			CanContinue(MobileEntity&v, const Grid::LCellIdI&focus, String&/*outError*/)	override	{return true;};
				virtual begin_t			BeginHere(MobileEntity&v, const Grid::LCellIdI&, bool hasToMoveFirst) override	{return hasToMoveFirst ? CannotStartAbort : Started;}


				virtual String			GetName() const override {return "build wall (horizontal)";}
				//virtual bool			CanContinue(MobileEntity&v, const Grid::LCellIdI&focus)	override;


				virtual Riff::TID		GetDataKey() const override {return "BLDH";};
				virtual ConstName		GetCTypeName() const override {return ConstName::Job_BuildHorizontalWall;}
				virtual PJob			Instantiate() const override {return PJob(new BuildHorizontalWall());}
			};
			//class BuildFoundation : public BaseJob
			//{
			//public:
			//	typedef BaseJob	Super;
			//	/**/					BuildFoundation():Super(true,false) {}
			//	/**/					BuildFoundation(MobileEntity&v) : BaseJob(BuildFoundationMineralCosts,v,true,true,false)
			//	{}

			//	virtual String			GetName() const override {return "build foundations";}
			//	virtual bool			CanContinue(MobileEntity&v, const Grid::LCellIdI&focus)	override;
			//	virtual continue_t		Finish(MobileEntity&v, const Grid::LCellIdI&focus)	override;

			//	virtual Riff::TID		GetDataKey() const override {return "BLDF";};
			//	virtual PJob	Instantiate() const override {return PJob(new BuildFoundation());}
			//	virtual	serial_size_t	GetSerialSize(bool exportSize) const override	{return 0;};
			//	virtual	bool			Serialize(IWriteStream&outStream, bool exportSize) const override {return true;};
			//	virtual	bool			Deserialize(IReadStream&inStream, serial_size_t fixedSize) override {return true;};
			//};





			class AlterTerrain : public BaseJob
			{
			public:
				typedef BaseJob			Super;
				CursorMode		mode;
				Array<Grid::HCellIdI>	selection;
				BYTE					maxHeightDifference;

				/**/					AlterTerrain():Super(Autocomplete::Always,false),maxHeightDifference(1)	{}
				/**/					AlterTerrain(MobileEntity&v, CursorMode mode, const Array<Grid::HCellIdI>&selection) : BaseJob((int)selection.Count() * BuildWallMineralCosts,v,mode == CursorMode::BuildBlockMode ? Component::TerrainAdd : Component::TerrainRemove,Autocomplete::Always,false),mode(mode),selection(selection),maxHeightDifference(1) {}
				virtual void			Restart(const MobileEntity&v) override {Super::ResetResourceAndRoundsLeft((int)selection.Count() * BuildWallMineralCosts,v,GetOperationAffinity());}
				static Component::operation_t	GetOperationAffinity(CursorMode mode) {return mode == CursorMode::BuildBlockMode ? Component::TerrainAdd : Component::TerrainRemove;}
				virtual Component::operation_t	GetOperationAffinity() const override {return GetOperationAffinity(mode);}

				virtual String			GetName() const override {return mode == CursorMode::BuildBlockMode ? "deposit earth" : "remove terrain";}


				virtual continue_t		Finish(MobileEntity&v, const Grid::LCellIdI&focus)	override;

				virtual Riff::TID		GetDataKey() const override {return "ALTT";};
				virtual ConstName		GetCTypeName() const override {return ConstName::Job_AlterTerrain;}
				virtual PJob			Instantiate() const override {return PJob(new AlterTerrain());}
				virtual	serial_size_t	GetSerialSize(bool exportSize) const override {return 3 + selection.GetSerialSize(exportSize);};
				virtual	bool			Serialize(IWriteStream&outStream, bool exportSize) const override
				{
					return outStream.WritePrimitive((UINT16)mode)
							&& outStream.WritePrimitive(maxHeightDifference)
							 && selection.Serialize(outStream,exportSize);
				}
				virtual	bool			Deserialize(IReadStream&inStream, serial_size_t fixedSize) override
				{
					UINT16 i16;
					if (!inStream.ReadPrimitive(i16) || !inStream.ReadPrimitive(maxHeightDifference) )
						return false;
					mode = (CursorMode)i16;
					if (fixedSize != EmbeddedSize)
						fixedSize -= 2;
					return selection.Deserialize(inStream,fixedSize);
				}
			};



			#ifndef ONE_COMPOUND_PER_FACTION

				class TransferJob : public BaseJob
				{
				public:
					typedef BaseJob			Super;

					CursorMode		mode;

					/**/					TransferJob():Super(Autocomplete::Always,false)	{}
					/**/					TransferJob(MobileEntity&v, CursorMode mode) : BaseJob(0,v,GetOperationAffinity(),Autocomplete::Always,false),mode(mode) {}
					virtual void			Restart(const MobileEntity&v) override {Super::ResetResourceAndRoundsLeft(0,v,GetOperationAffinity());}
					virtual Component::operation_t	GetOperationAffinity() const override {return Component::Construct;}

					virtual String			GetName() const override {return mode == CursorMode::RestockMode ? "Restock from complex/vehicle" : "Transfer to complex/vehicle";}
					virtual bool			CanContinue(MobileEntity&v, const Grid::LCellIdI&focus, String&outError)	override;
					virtual continue_t		Finish(MobileEntity&v, const Grid::LCellIdI&focus)	override;
					virtual Riff::TID		GetDataKey() const override {return "TRAN";};
					virtual ConstName		GetCTypeName() const override {return ConstName::Job_Transfer;}
					virtual PJob			Instantiate() const override {return PJob(new TransferJob());}
					virtual	serial_size_t	GetSerialSize(bool exportSize) const override {return 2;};
					virtual	bool			Serialize(IWriteStream&outStream, bool exportSize) const override	{return outStream.WritePrimitive((UINT16)mode);}
					virtual	bool			Deserialize(IReadStream&inStream, serial_size_t fixedSize) override
					{
						UINT16 i16;
						if (!inStream.ReadPrimitive(i16))
							return false;
						mode = (CursorMode)i16;
						return true;
					};
				};

			#endif			

			class RepairJob : public BaseJob
			{
			public:
				typedef BaseJob			Super;

				/**/					RepairJob():Super(Autocomplete::Always,false)	{}
				/**/					RepairJob(MobileEntity&v) : BaseJob(0,v,GetOperationAffinity(),Autocomplete::Always,false) {}
				virtual void			Restart(const MobileEntity&v) override {Super::ResetResourceAndRoundsLeft(0,v,GetOperationAffinity());}
				virtual Component::operation_t	GetOperationAffinity() const override {return Component::Repair;}

				virtual String			GetName() const override {return "Repair complex/vehicle";}
				virtual bool			CanContinue(MobileEntity&v, const Grid::LCellIdI&focus, String&outError)	override;
				virtual continue_t		Finish(MobileEntity&v, const Grid::LCellIdI&focus)	override;
				virtual Riff::TID		GetDataKey() const override {return "REPA";};
				virtual ConstName		GetCTypeName() const override {return ConstName::Job_Repair;}
				virtual PJob			Instantiate() const override {return PJob(new RepairJob());}
				virtual	serial_size_t	GetSerialSize(bool exportSize) const override {return 0;};
				virtual	bool			Serialize(IWriteStream&outStream, bool exportSize) const override	{return true;}
				virtual	bool			Deserialize(IReadStream&inStream, serial_size_t fixedSize) override {return true;}
			};

			class ClearAbove : public BaseJob
			{
			public:
				typedef BaseJob			Super;

				/**/					ClearAbove():Super(Autocomplete::Always,false)	{}
				/**/					ClearAbove(MobileEntity&v) : BaseJob(ClearAboveMineralCosts,v,GetOperationAffinity(),Autocomplete::Always,false) {}
				virtual void			Restart(const MobileEntity&v) override {Super::ResetResourceAndRoundsLeft(ClearAboveMineralCosts,v,GetOperationAffinity());}
				virtual Component::operation_t	GetOperationAffinity() const override {return Component::TerrainRemove;}

				virtual bool			CanContinue(MobileEntity&v, const Grid::LCellIdI&focus, String&outError)	override;
				virtual String			GetName() const override {return "clear upper terrain";}
				virtual continue_t		Finish(MobileEntity&v, const Grid::LCellIdI&focus)	override;

				virtual Riff::TID		GetDataKey() const override {return "CLRA";};
				virtual ConstName		GetCTypeName() const override {return ConstName::Job_ClearAbove;}
				virtual PJob			Instantiate() const override {return PJob(new ClearAbove());}
				virtual	serial_size_t	GetSerialSize(bool exportSize) const override {return 0;};
				virtual	bool			Serialize(IWriteStream&outStream, bool exportSize) const override {return true;};
				virtual	bool			Deserialize(IReadStream&inStream, serial_size_t fixedSize) override {return true;};

			};

			class DigHorizontal : public BaseJob
			{
			public:
				typedef BaseJob			Super;

				/**/					DigHorizontal():Super(Autocomplete::Always,true)	{}
				/**/					DigHorizontal(MobileEntity&v) : BaseJob(DigHorizontalMineralCosts,v,GetOperationAffinity(),Autocomplete::Always,true) {}
				virtual void			Restart(const MobileEntity&v) override {Super::ResetResourceAndRoundsLeft(DigHorizontalMineralCosts,v,GetOperationAffinity());}
				virtual Component::operation_t	GetOperationAffinity() const override {return Component::TerrainRemove;}

				virtual bool			CanContinue(MobileEntity&v, const Grid::LCellIdI&focus, String&outError)	override;
				virtual continue_t		Finish(MobileEntity&v, const Grid::LCellIdI&focus)	override;
				virtual String			GetName() const override {return "dig horizontally";}
				virtual Riff::TID		GetDataKey() const override {return "DIGH";};
				virtual ConstName		GetCTypeName() const override {return ConstName::Job_DigHorizontal;}
				virtual PJob			Instantiate() const override {return PJob(new DigHorizontal());}
				virtual	serial_size_t	GetSerialSize(bool exportSize) const override {return 0;};
				virtual	bool			Serialize(IWriteStream&outStream, bool exportSize) const override {return true;};
				virtual	bool			Deserialize(IReadStream&inStream, serial_size_t fixedSize) override {return true;};


			};

			class DigDown : public BaseJob
			{
				static bool				Succeeded(MobilePlacementResult rs);

			public:
				typedef BaseJob	Super;
				/**/					DigDown():Super(Autocomplete::Always,true) {}

				/**/					DigDown(MobileEntity&v) : BaseJob(DigDownMineralCosts,v,GetOperationAffinity(),Autocomplete::Always,true) {}
				virtual void			Restart(const MobileEntity&v) override {Super::ResetResourceAndRoundsLeft(DigDownMineralCosts,v,GetOperationAffinity());}
				virtual Component::operation_t	GetOperationAffinity() const override {return Component::TerrainRemove;}

				virtual String			GetName() const override {return "dig down";}

				virtual bool			CanContinue(MobileEntity&v, const Grid::LCellIdI&focus, String&outError)	override;
				virtual continue_t		Finish(MobileEntity&v, const Grid::LCellIdI&)	override;
				virtual Riff::TID		GetDataKey() const override {return "DIGD";};
				virtual ConstName		GetCTypeName() const override {return ConstName::Job_DigDown;}

				virtual PJob			Instantiate() const override {return PJob(new DigDown());}
				virtual	serial_size_t	GetSerialSize(bool exportSize) const override {return 0;};
				virtual	bool			Serialize(IWriteStream&outStream, bool exportSize) const override {return true;};
				virtual	bool			Deserialize(IReadStream&inStream, serial_size_t fixedSize) override {return true;};


			};


			class Construct;
			typedef std::shared_ptr<Construct>	PConstruct;

			class Construct : public BaseJob
			{
			public:
				typedef BaseJob			Super;

				PStructure				constructWhat;
				TUniqueID				useID;

				/**/					Construct():Super(Autocomplete::IfSatellite,false)	{useID.Generate();}
				/**/					Construct(MobileEntity&v, const PStructure&c);

				virtual String			GetName() const override;
				virtual ConstName		GetCTypeName() const override {return ConstName::Job_Construct;}


				virtual begin_t			BeginHere(MobileEntity&v, const Grid::LCellIdI&, bool hasToMoveFirst)	override;
				virtual bool			CanContinue(MobileEntity&v, const Grid::LCellIdI&, String&outError) override;
				virtual continue_t		Finish(MobileEntity&v, const Grid::LCellIdI&) override;
				virtual Riff::TID		GetDataKey() const override {return "CONS";};
				virtual PJob			Instantiate() const override {return PJob(new Construct());}
				virtual	serial_size_t	GetSerialSize(bool exportSize) const override;
				virtual	bool			Serialize(IWriteStream&outStream, bool exportSize) const override;
				virtual	bool			Deserialize(IReadStream&inStream, serial_size_t fixedSize) override;

				virtual void			Restart(const MobileEntity&v) override;
				virtual Component::operation_t	GetOperationAffinity() const override {return Component::Construct;}
			};


			class Sequence;
			typedef std::shared_ptr<Sequence>	PSequence;
		
			class Sequence : public PathJob
			{
			public:
				typedef PathJob	Super;

				CursorMode		mode;
				Grid::LCellIdI			lastLocation = Grid::LCellIdI::Invalid;
				bool					first;
				/**/					Sequence():Super(Autocomplete::Always,true)	{};

				/**/					Sequence(MobileEntity&v, CursorMode mode) : Super(1,v,Component::TerrainRemove,Autocomplete::Always,/*false*/true),mode(mode),first(true) {}
				virtual void			Restart(const MobileEntity&v) override {Super::ResetResourceAndRoundsLeft(1,v,Component::TerrainRemove);}
				virtual Component::operation_t	GetOperationAffinity() const override {return Component::TerrainRemove;}	//we don't want this job itself to cost any kind of resources, hence TerrainRemove. This job actually does TerrainAdd or Construct, but those are child-jobs

				virtual String			GetName() const override
				{
					switch (mode)
					{
						case CursorMode::BuildRoadChainMode:
							return "Build Road to...";
						case CursorMode::BuildConnectorChainMode:
							return "Build Connectors to...";
						case CursorMode::BuildWallChainMode:
							return "Build Wall to...";
						case CursorMode::BuildBridgeMode:
							return "Build Bridge to...";
					}
					return "???...";
				}

				virtual bool			CanContinue(MobileEntity&v, const Grid::LCellIdI&, String&outError) override
				{
					//if (deadBecause.IsNotEmpty())
					//{
					//	outError = deadBecause;
					//	return false;
					//}
					return true;
				}


				void					Guess(const MobileEntity&v, const Grid::LCellIdI&origin, index_t myJobIndex, UINT16 & roundsToJobCompletion, UINT16 & resourceCostLeft)	const;

				bool					DoneHere(const Grid::WorldData&world,const Grid::LCellIdI&cell, const Grid::LCellIdI&nextCell)	const;
				virtual bool			UpdatePath(const MobileEntity&v, const Grid::LCellIdI&origin, Grid::LCellIdI&destination)	override;


				virtual begin_t			BeginHere(MobileEntity&v, const Grid::LCellIdI&_destination, bool hasToMoveFirst)	override;

				virtual bool			IsImplicitlyDone(MobileEntity&v, const Grid::LCellIdI&_destination) const override;

				virtual continue_t		Finish(MobileEntity&v, const Grid::LCellIdI&) override
				{
					return JobDone;
				}
				virtual Riff::TID		GetDataKey() const override {return "SEQC";};
				virtual ConstName		GetCTypeName() const override {return ConstName::Job_Sequence;}
				virtual PJob			Instantiate() const override {return PJob(new Sequence());}
				virtual	serial_size_t	GetSerialSize(bool exportSize) const override
				{
					return 2 + sizeof(Grid::LCellIdI) + 1 + Super::GetSerialSize(true);
				}
				virtual	bool			Serialize(IWriteStream&outStream, bool exportSize) const override
				{
					return outStream.WritePrimitive((UINT16)mode) && outStream.WritePrimitive(lastLocation)
							&&outStream.WritePrimitive(first)
							&&Super::Serialize(outStream,true);
				}
				virtual	bool			Deserialize(IReadStream&inStream, serial_size_t fixedSize) override;
			};


			class SelfDetonate : public BaseJob
			{
			public:
				typedef BaseJob			Super;

				bool					doomsday=false;

				/**/					SelfDetonate():Super(Autocomplete::Always,false)	{}
				/**/					SelfDetonate(MobileEntity&v, ConstName parameter);

				virtual String			GetName() const override;
				virtual ConstName		GetCTypeName() const override {return ConstName::Job_SelfDetonate;}
				virtual bool			DelayedBySleep() const override {return false;}

				virtual begin_t			BeginHere(MobileEntity&v, const Grid::LCellIdI&, bool hasToMoveFirst)	{return Started;}
				virtual bool			CanContinue(MobileEntity&v, const Grid::LCellIdI&, String&outError) override;
				virtual continue_t		Finish(MobileEntity&v, const Grid::LCellIdI&) override;
				virtual Riff::TID		GetDataKey() const override {return "SLFD";};
				virtual PJob			Instantiate() const override {return PJob(new SelfDetonate());}
				virtual	serial_size_t	GetSerialSize(bool exportSize) const override;
				virtual	bool			Serialize(IWriteStream&outStream, bool exportSize) const override;
				virtual	bool			Deserialize(IReadStream&inStream, serial_size_t fixedSize) override;

				virtual void			Restart(const MobileEntity&v) override {Super::ResetResourceAndRoundsLeft(doomsday ? DoomsdaySelfDetonationCosts  : RegularSelfDetonationCosts,v,GetOperationAffinity());}
				virtual Component::operation_t	GetOperationAffinity() const override {return Component::Nuclear;}
				virtual Resource		GetConsumedResourceType() const override {return Resource::Uranium;}
			};


		#endif
	}


	typedef VehicleJob::PJob	PVehicleJob;

}





#endif
