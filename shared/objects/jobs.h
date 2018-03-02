#ifndef objectJobsH
#define objectJobsH

#include <io/riff_handler.h>
#include "../grid/stack.h"
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
							MinBuildRoadMineralCosts = 7,	//dynamic based on free space beneath road	(bridge)
							MinBuildWallMineralCosts = 2,
							HeightPeneltyMineralCosts = 5,
							RegularSelfDetonationCosts = -100,
							DoomsdaySelfDetonationCosts = 200;


		class BaseJob;
		typedef std::shared_ptr<BaseJob>	PJob;
		typedef std::weak_ptr<BaseJob>		WJob;


		class BaseJob : public std::enable_shared_from_this<BaseJob>, public ISerializable
		{
		public:
			/**
			The number of times Continue() must be called on the local job to complete it
			*/
			int						continuationsToJobCompletion;
			/**
			The amount of resources that must be supplied to finish the job (not all jobs actually consume resources).
			Base for calculating buildActionsToJobCompletion and serverRoundsToJobCompletion
			*/
			int						resourceCostLeft;

			Grid::LCellIdI			location;	//!< Last known build location. Set by constructor and Update()

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
			/**/					BaseJob(int costs, const MobileEntity&v, const Grid::LCellIdI&loc, Component::operation_t op, Autocomplete jobAutoCompletion, bool movesToFocusWhenDone);
			virtual					~BaseJob()	{}
			
			BYTE					GetConstructionActionCostPerContinuation(const MobileEntity&v) const;
			void					ResetResourceAndRoundsLeft(int costs, const MobileEntity&v, Component::operation_t op);

			/**
			Expected to recompute resources and rounds left based on the local job composition and stored location for the given vehicle.
			Should internally call ResetResourceAndRoundsLeft() with appropriate parameters.
			*/
			virtual void			UpdateEstimations(const MobileEntity&v) =0;
			void					UpdateLocationAndEstimations(const MobileEntity&v, const Grid::LCellIdI&loc) {location = loc; UpdateEstimations(v);}

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

				static Ctr::Buffer0<PJob>allJobPrototypes;


			#endif
		protected:
		
		};



		class PathJob : public BaseJob
		{
		public:
			typedef BaseJob	Super;

			Ctr::Buffer0<Grid::LCellIdI>path;
			bool						pathChanged;

			/**/						PathJob(Autocomplete jobAutoCompletion, bool movesToFocusWhenDone):Super(jobAutoCompletion, movesToFocusWhenDone)	{}
			/**/						PathJob(int costs, const MobileEntity&v, Component::operation_t op, Autocomplete jobAutoCompletion, bool movesToFocusWhenDone):
										Super(costs, v, Grid::LCellIdI::Invalid,op, jobAutoCompletion, movesToFocusWhenDone), pathChanged(false)	{}


			virtual bool				UpdatePath(const MobileEntity&v, const Grid::LCellIdI&origin, Grid::LCellIdI&destination) =0;
			virtual	void				Serialize(IWriteStream&outStream) const override
			{
				outStream.WriteSize(path.GetLength());
				outStream.WritePrimitives(path.pointer(),path.GetLength());
			}
			virtual	void				Deserialize(IReadStream&inStream) override
			{
				serial_size_t fixedSize = inStream.ReadSize();
				path.Clear();
				inStream.ReadPrimitives(path.AppendRow(fixedSize),fixedSize);
			}
		};

		#ifdef CLIENT

			class DummyPathJob : public PathJob
			{
			public:
				typedef PathJob	Super;

				/**/					DummyPathJob(const MobileEntity&v) : Super(1,v,GetOperationAffinity(),Autocomplete::Always,false) {}
				virtual void			UpdateEstimations(const MobileEntity&v) override {Super::ResetResourceAndRoundsLeft(1,v,GetOperationAffinity());}
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
				Component::operation_t	affinity;

				/**/					DummyJob(const MobileEntity&v, Component::operation_t affinity);
				virtual void			UpdateEstimations(const MobileEntity&v) override {Super::ResetResourceAndRoundsLeft(1,v,GetOperationAffinity());}
				virtual Component::operation_t	GetOperationAffinity() const override {return affinity;}
				virtual bool			DelayedBySleep() const override {return delayedBySleep;}
				//virtual Riff::TID		GetDataKey() const override {return "BLDF";};
				//virtual PJob	Instantiate() const override {return PJob(new BuildFoundation());}
				virtual	void			Serialize(IWriteStream&outStream) const override {};
				virtual	void			Deserialize(IReadStream&inStream) override {};

			};
		#else

			class BuildRoad : public BaseJob
			{
			public:
				typedef BaseJob			Super;
				//UINT32					prevCell,nextCell;
				bool					bridgeMode=false;

				static int				GetConstructionCostsAt(const Grid::LCellIdI&);

				/**/					BuildRoad():Super(Autocomplete::Always,false)	{}
				/**/					BuildRoad(const MobileEntity&v, const Grid::LCellIdI&loc) : BaseJob(GetConstructionCostsAt(loc),v,loc,GetOperationAffinity(),Autocomplete::Always,false) {}
				/**/					BuildRoad(const MobileEntity&v, bool bridgeMode);
				virtual void			UpdateEstimations(const MobileEntity&v) override;
				virtual Component::operation_t	GetOperationAffinity() const override {return Component::TerrainAdd;}


				virtual String			GetName() const override {return "build roads";}


				virtual Riff::TID		GetDataKey() const override {return "BLDR";};
				virtual ConstName		GetCTypeName() const override {return ConstName::Job_BuildRoad;}
				virtual PJob			Instantiate() const override {return PJob(new BuildRoad());}
				virtual	void			Serialize(IWriteStream&outStream) const override {outStream.WritePrimitive(bridgeMode);}
				virtual	void			Deserialize(IReadStream&inStream) override;


				virtual continue_t		Finish(MobileEntity&v, const Grid::LCellIdI&focus)	override;
			};


			class BuildWallUp : public BaseJob
			{
			protected:

				/**/					BuildWallUp(Autocomplete jobAutoCompletion, bool movesToFocusWhenDone):Super(jobAutoCompletion, movesToFocusWhenDone)	{}
				/**/					BuildWallUp(const MobileEntity&v, const Grid::LCellIdI&loc,Autocomplete jobAutoCompletion, bool movesToFocusWhenDone) : BaseJob(GetConstructionCostsAt(loc),v,loc,Component::TerrainAdd,jobAutoCompletion, movesToFocusWhenDone) {}

				virtual Grid::LCellIdI	FocusToBuildLocation(const Grid::LCellIdI&focus) const {return focus-1;}

				static bool				ConnectTo(BYTE content)
				{
					return Grid::CellStack::Cell::IsSolid(content);
					//return content != Grid::Content::Clear;	//everything not clear
					//return Grid::CellStack::Cell::typeStrength[content] >= 4;	//grass strength
				}

				void					SetWall(World::Data::ChangeSequence&seq, const BaseGrid::TCellNeighborhood&n, const Grid::HCellIdI&center) const;


			public:
				typedef BaseJob			Super;
				
				static int				GetConstructionCostsAt(const Grid::HCellIdI&);
				static int				GetConstructionCostsAt(const Grid::LCellIdI&);


				/**/					BuildWallUp():Super(Autocomplete::Always,true)	{}
				/**/					BuildWallUp(const MobileEntity&v, const Grid::LCellIdI&loc) : BaseJob(GetConstructionCostsAt(loc),v,loc,GetOperationAffinity(),Autocomplete::Always,true) {}
				virtual void			UpdateEstimations(const MobileEntity&v) override {Super::ResetResourceAndRoundsLeft(GetConstructionCostsAt(location),v,GetOperationAffinity());}
				virtual Component::operation_t	GetOperationAffinity() const override {return Component::TerrainAdd;}


				virtual begin_t			BeginHere(MobileEntity&v, const Grid::LCellIdI&_destination, bool hasToMoveFirst)	override;
				virtual String			GetName() const override {return "build wall";}
				virtual bool			CanContinue(MobileEntity&v, const Grid::LCellIdI&focus, String&outError)	override;


				virtual Riff::TID		GetDataKey() const override {return "BLDW";};
				virtual ConstName		GetCTypeName() const override {return ConstName::Job_BuildWallUp;}
				virtual PJob			Instantiate() const override {return PJob(new BuildWallUp());}
				virtual	void			Serialize(IWriteStream&outStream) const override {}
				virtual	void			Deserialize(IReadStream&inStream) override {}


				virtual continue_t		Finish(MobileEntity&v, const Grid::LCellIdI&focus)	override;
			};

			class BuildHorizontalWall : public BuildWallUp
			{
			protected:
				virtual Grid::LCellIdI	FocusToBuildLocation(const Grid::LCellIdI&focus) const override {return focus;}

			public:
				typedef BuildWallUp		Super;
				

				/**/					BuildHorizontalWall():Super(Autocomplete::Never,false)	{}
				/**/					BuildHorizontalWall(const MobileEntity&v, const Grid::LCellIdI&loc) : Super(v,loc,Autocomplete::Never,false) {}

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
			//	virtual	serial_size_t	GetSerialSize() const override	{return 0;};
			//	virtual	bool			Serialize(IWriteStream&outStream) const override {return true;};
			//	virtual	bool			Deserialize(IReadStream&inStream) override {return true;};
			//};





			class AlterTerrain : public BaseJob
			{
			public:
				typedef BaseJob			Super;
				CursorMode				mode;
				Array<Grid::HCellIdI>	selection;
				BYTE					maxHeightDifference;

				static int				GetConstructionCostsOf(const ArrayRef<Grid::HCellIdI>&, CursorMode mode);

				/**/					AlterTerrain():Super(Autocomplete::Always,false),maxHeightDifference(1)	{}
				/**/					AlterTerrain(const MobileEntity&v, CursorMode mode, const Ctr::Array<Grid::HCellIdI>&selection);
				virtual void			UpdateEstimations(const MobileEntity&v) override;
				static Component::operation_t	GetOperationAffinity(CursorMode mode) {return mode == CursorMode::BuildBlockMode ? Component::TerrainAdd : Component::TerrainRemove;}
				virtual Component::operation_t	GetOperationAffinity() const override {return GetOperationAffinity(mode);}

				virtual String			GetName() const override {return mode == CursorMode::BuildBlockMode ? "build concrete" : "remove terrain";}


				virtual continue_t		Finish(MobileEntity&v, const Grid::LCellIdI&focus)	override;

				virtual Riff::TID		GetDataKey() const override {return "ALTT";};
				virtual ConstName		GetCTypeName() const override {return ConstName::Job_AlterTerrain;}
				virtual PJob			Instantiate() const override {return PJob(new AlterTerrain());}
				virtual	void			Serialize(IWriteStream&outStream) const override
				{
					outStream.WritePrimitive((UINT16)mode);
					outStream.WritePrimitive(maxHeightDifference);
					using Serialization::Serialize;
					Serialize(outStream,selection);
				}
				virtual	void			Deserialize(IReadStream&inStream) override
				{
					UINT16 i16;
					inStream.ReadPrimitive(i16);
					inStream.ReadPrimitive(maxHeightDifference);
					mode = (CursorMode)i16;
					using Serialization::Deserialize;
					Deserialize(inStream,selection);
				}
			};



			#ifndef ONE_COMPOUND_PER_FACTION

				class TransferJob : public BaseJob
				{
				public:
					typedef BaseJob			Super;

					CursorMode		mode;

					/**/					TransferJob():Super(Autocomplete::Always,false)	{}
					/**/					TransferJob(const MobileEntity&v, CursorMode mode) : BaseJob(0,v,GetOperationAffinity(),Autocomplete::Always,false),mode(mode) {}
					virtual void			Restart(const MobileEntity&v) override {Super::ResetResourceAndRoundsLeft(0,v,GetOperationAffinity());}
					virtual Component::operation_t	GetOperationAffinity() const override {return Component::Construct;}

					virtual String			GetName() const override {return mode == CursorMode::RestockMode ? "Restock from complex/vehicle" : "Transfer to complex/vehicle";}
					virtual bool			CanContinue(MobileEntity&v, const Grid::LCellIdI&focus, String&outError)	override;
					virtual continue_t		Finish(MobileEntity&v, const Grid::LCellIdI&focus)	override;
					virtual Riff::TID		GetDataKey() const override {return "TRAN";};
					virtual ConstName		GetCTypeName() const override {return ConstName::Job_Transfer;}
					virtual PJob			Instantiate() const override {return PJob(new TransferJob());}
					virtual	serial_size_t	GetSerialSize() const override {return 2;};
					virtual	bool			Serialize(IWriteStream&outStream) const override	{return outStream.WritePrimitive((UINT16)mode);}
					virtual	bool			Deserialize(IReadStream&inStream) override
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
				/**/					RepairJob(const MobileEntity&v, const Grid::LCellIdI&loc) : BaseJob(0,v,loc,GetOperationAffinity(),Autocomplete::Always,false) {}
				virtual void			UpdateEstimations(const MobileEntity&v) override {Super::ResetResourceAndRoundsLeft(0,v,GetOperationAffinity());}
				virtual Component::operation_t	GetOperationAffinity() const override {return Component::Repair;}

				virtual String			GetName() const override {return "Repair complex/vehicle";}
				virtual bool			CanContinue(MobileEntity&v, const Grid::LCellIdI&focus, String&outError)	override;
				virtual continue_t		Finish(MobileEntity&v, const Grid::LCellIdI&focus)	override;
				virtual Riff::TID		GetDataKey() const override {return "REPA";};
				virtual ConstName		GetCTypeName() const override {return ConstName::Job_Repair;}
				virtual PJob			Instantiate() const override {return PJob(new RepairJob());}

				virtual	void			Serialize(IWriteStream&outStream) const override	{}
				virtual	void			Deserialize(IReadStream&inStream) override {}
			};

			class ClearAbove : public BaseJob
			{
			public:
				typedef BaseJob			Super;

				/**/					ClearAbove():Super(Autocomplete::Always,false)	{}
				/**/					ClearAbove(const MobileEntity&v, const Grid::LCellIdI&loc) : BaseJob(ClearAboveMineralCosts,v,loc,GetOperationAffinity(),Autocomplete::Always,false) {}
				virtual void			UpdateEstimations(const MobileEntity&v) override {Super::ResetResourceAndRoundsLeft(ClearAboveMineralCosts,v,GetOperationAffinity());}
				virtual Component::operation_t	GetOperationAffinity() const override {return Component::TerrainRemove;}

				virtual bool			CanContinue(MobileEntity&v, const Grid::LCellIdI&focus, String&outError)	override;
				virtual String			GetName() const override {return "clear upper terrain";}
				virtual continue_t		Finish(MobileEntity&v, const Grid::LCellIdI&focus)	override;

				virtual Riff::TID		GetDataKey() const override {return "CLRA";};
				virtual ConstName		GetCTypeName() const override {return ConstName::Job_ClearAbove;}
				virtual PJob			Instantiate() const override {return PJob(new ClearAbove());}

				virtual	void			Serialize(IWriteStream&outStream) const override	{}
				virtual	void			Deserialize(IReadStream&inStream) override {}

			};

			class DigHorizontal : public BaseJob
			{
			public:
				typedef BaseJob			Super;

				/**/					DigHorizontal():Super(Autocomplete::Always,true)	{}
				/**/					DigHorizontal(const MobileEntity&v, const Grid::LCellIdI&loc) : BaseJob(DigHorizontalMineralCosts,v,loc,GetOperationAffinity(),Autocomplete::Always,true) {}
				virtual void			UpdateEstimations(const MobileEntity&v) override {Super::ResetResourceAndRoundsLeft(DigHorizontalMineralCosts,v,GetOperationAffinity());}
				virtual Component::operation_t	GetOperationAffinity() const override {return Component::TerrainRemove;}

				virtual bool			CanContinue(MobileEntity&v, const Grid::LCellIdI&focus, String&outError)	override;
				virtual continue_t		Finish(MobileEntity&v, const Grid::LCellIdI&focus)	override;
				virtual String			GetName() const override {return "dig horizontally";}
				virtual Riff::TID		GetDataKey() const override {return "DIGH";};
				virtual ConstName		GetCTypeName() const override {return ConstName::Job_DigHorizontal;}
				virtual PJob			Instantiate() const override {return PJob(new DigHorizontal());}

				virtual	void			Serialize(IWriteStream&outStream) const override	{}
				virtual	void			Deserialize(IReadStream&inStream) override {}
			};

			class DigDown : public BaseJob
			{
				static bool				Succeeded(MobilePlacementResult rs);

			public:
				typedef BaseJob	Super;
				/**/					DigDown():Super(Autocomplete::Always,true) {}

				/**/					DigDown(const MobileEntity&v, const Grid::LCellIdI&loc) : BaseJob(DigDownMineralCosts,v,loc,GetOperationAffinity(),Autocomplete::Always,true) {}
				virtual void			UpdateEstimations(const MobileEntity&v) override {Super::ResetResourceAndRoundsLeft(DigDownMineralCosts,v,GetOperationAffinity());}
				virtual Component::operation_t	GetOperationAffinity() const override {return Component::TerrainRemove;}

				virtual String			GetName() const override {return "dig down";}

				virtual bool			CanContinue(MobileEntity&v, const Grid::LCellIdI&focus, String&outError)	override;
				virtual continue_t		Finish(MobileEntity&v, const Grid::LCellIdI&)	override;
				virtual Riff::TID		GetDataKey() const override {return "DIGD";};
				virtual ConstName		GetCTypeName() const override {return ConstName::Job_DigDown;}

				virtual PJob			Instantiate() const override {return PJob(new DigDown());}

				virtual	void			Serialize(IWriteStream&outStream) const override	{}
				virtual	void			Deserialize(IReadStream&inStream) override {}
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
				/**/					Construct(const MobileEntity&v, const Grid::LCellIdI&loc, const PStructure&c);

				virtual String			GetName() const override;
				virtual ConstName		GetCTypeName() const override {return ConstName::Job_Construct;}


				virtual begin_t			BeginHere(MobileEntity&v, const Grid::LCellIdI&, bool hasToMoveFirst)	override;
				virtual bool			CanContinue(MobileEntity&v, const Grid::LCellIdI&, String&outError) override;
				virtual continue_t		Finish(MobileEntity&v, const Grid::LCellIdI&) override;
				virtual Riff::TID		GetDataKey() const override {return "CONS";};
				virtual PJob			Instantiate() const override {return PJob(new Construct());}

				virtual void			UpdateEstimations(const MobileEntity&v) override;
				virtual Component::operation_t	GetOperationAffinity() const override {return Component::Construct;}

				virtual	void			Serialize(IWriteStream&outStream) const override;
				virtual	void			Deserialize(IReadStream&inStream) override;
			};


			class Sequence;
			typedef std::shared_ptr<Sequence>	PSequence;
		
			class Sequence : public PathJob
			{
			public:
				typedef PathJob	Super;

				CursorMode				mode;
				Grid::LCellIdI			lastLocation = Grid::LCellIdI::Invalid,
										doneIn = Grid::LCellIdI::Invalid;
				bool					first;
				/**/					Sequence():Super(Autocomplete::Always,true)	{};

				/**/					Sequence(const MobileEntity&v, CursorMode mode) : Super(1,v,Component::TerrainRemove,Autocomplete::Always,/*false*/true),mode(mode),first(true) {}
				virtual void			UpdateEstimations(const MobileEntity&v) override {Super::ResetResourceAndRoundsLeft(1,v,Component::TerrainRemove);}
				virtual Component::operation_t	GetOperationAffinity() const override {return Component::TerrainRemove;}	//we don't want this job itself to cost any kind of resources, hence TerrainRemove. This job actually does TerrainAdd or Construct, but those are child-jobs

				virtual String			GetName() const override;

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

				bool					DoneHere(const World::Data&world,const Grid::LCellIdI&cell, const Grid::LCellIdI&nextCell)	const;
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

				virtual	void			Serialize(IWriteStream&outStream) const override
				{
					outStream.WritePrimitive((UINT16)mode);
					outStream.WritePrimitive(lastLocation);
					outStream.WritePrimitive(first);
					Super::Serialize(outStream);
				}
				virtual	void			Deserialize(IReadStream&inStream) override;
			};


			class SelfDetonate : public BaseJob
			{
			public:
				typedef BaseJob			Super;

				bool					doomsday=false;

				/**/					SelfDetonate():Super(Autocomplete::Always,false)	{}
				/**/					SelfDetonate(const MobileEntity&v, const Grid::LCellIdI&loc, ConstName parameter);

				virtual String			GetName() const override;
				virtual ConstName		GetCTypeName() const override {return ConstName::Job_SelfDetonate;}
				virtual bool			DelayedBySleep() const override {return false;}

				virtual begin_t			BeginHere(MobileEntity&v, const Grid::LCellIdI&, bool hasToMoveFirst)	{return Started;}
				virtual bool			CanContinue(MobileEntity&v, const Grid::LCellIdI&, String&outError) override;
				virtual continue_t		Finish(MobileEntity&v, const Grid::LCellIdI&) override;
				virtual Riff::TID		GetDataKey() const override {return "SLFD";};
				virtual PJob			Instantiate() const override {return PJob(new SelfDetonate());}

				virtual void			UpdateEstimations(const MobileEntity&v) override {Super::ResetResourceAndRoundsLeft(doomsday ? DoomsdaySelfDetonationCosts  : RegularSelfDetonationCosts,v,GetOperationAffinity());}
				virtual Component::operation_t	GetOperationAffinity() const override {return Component::Nuclear;}
				virtual Resource		GetConsumedResourceType() const override {return Resource::Uranium;}
			
				virtual	void			Serialize(IWriteStream&outStream) const override;
				virtual	void			Deserialize(IReadStream&inStream) override;
			};


		#endif
	}


	typedef VehicleJob::PJob	PVehicleJob;

}





#endif
