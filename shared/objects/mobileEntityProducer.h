#ifndef objectMobileEntityProducerH
#define objectMobileEntityProducerH

#include "../grid/stack.h"
#include "types.h"
#include "../uniqueID.h"
#ifndef CLIENT
	#include "resource.h"
#endif
#include "entityCommand.h"
#include "../observer.h"

namespace Objects
{


	class GatheringPoint
	{
	public:
		#ifdef HAS_WINDOW
			static Grid::LCellIdI	selected;			//!< Gathering point of selected entity (if any, or invalid)
			static TMatrix4<>		selectedVisual;		//!< Point where the respective gathering point should be visualized at. (same cell; may be different height)

			void					UpdateVisuals();
		#endif

		Grid::LCellIdI				current = Grid::LCellIdI::Invalid;

		bool						Set(const Grid::LCellIdI&point, bool isSelected);

	};

	
	/**
	Member class used by all structures that can produce mobile entities.
	Rather than inheriting this class, they store it as a member and override the ToProducer() methods
	*/
	class MobileEntityProducer
	{
	protected:
		
		#ifdef HAS_WINDOW
			int2					commandOffset = int2(4,2);
		#endif
		
		String						JobName(index_t index, const String&itemName, UINT32 roundsLeft)const;
		UINT32						ProductionRounds( const PMobileEntity&v, bool isActive) const;

	public:
	
		GatheringPoint				gatheringPoint;

		struct WorkItem
		{
			TUniqueID				id;
			#ifdef CLIENT
				#ifdef HAS_WINDOW
					std::pair<int2,Command::PBase>	commandToTerminate;
				#endif
				String				name, queueName;
				int					cost, roundsToCompletion;
				Component::VehicleBase*		base=nullptr;	//set for vehicles only
				Component::Turret*		turret=nullptr;	//set for vehicles only
				PMobileEntity		prototype;	//set for non-vehicles only
			#else
				PMobileEntity		producing;

				/**
				Checks whether or not the local work item produces anything. null-pointer sensitive (can be called on a null-object)
				*/
				bool				IsEmpty() const	{return this != nullptr && !producing;}
			#endif

			#ifndef NO_SATELLITES
				bool				IsSatellite() const;
			#endif
			bool					IsPlane() const;
			bool					IsVehicle() const;
			bool					IsPureVehicle() const;
			Component::VehicleBase*	GetVehicleBase() const;
			bool					CanSpawnInGrid() const;
			bool					CanSpawnAboveGrid(int&) const;
			const String&			GetName() const;
		};
		//typedef std::pair<PVehicle,Command>		WorkItem;
		Ctr::Buffer0<WorkItem>		queue;	//must use buffer here, because elements can be erased and changes are rare

		UINT32						productionPerRound,	//!< Amount of minerals converted per round. Effectively the production speed of the local instance
									productionDone=0,	//!< Total amount of minerals invested into the current production. Reset to 0 upon completion
									productionRoundsLeft=0;


		static Observable<MobileEntityProducer&,WorkItem&>	onAppendTask;

		/**/						MobileEntityProducer(UINT32 productionPerRound):productionPerRound(productionPerRound)
									{}

		UINT32						ProductionRounds(int cost, bool isActive) const;

		bool						IsBuilding(std::pair<int,int>&outProgress) const
		{
			if (queue.IsEmpty())
				return false;
			UINT32 max = ProductionRounds(queue.First(),false);
			outProgress = std::pair<int,int>(max - productionRoundsLeft , max);
			return true;
		}


		void					AppendTask(const TUniqueID&id, ConstName baseName, ConstName turretName);

		#ifdef HAS_WINDOW
			const int2&				GetCommandOffset() const {return commandOffset;}
			void					VisitAllCommands(FCommandVisitor visitor,bool force);
			void					SetCommandOffset(const int2&);
		#else
			void					SetCommandOffset(const int2&)	{};

		#endif
		bool						AwaitsUserInteraction() const;

		#ifndef CLIENT
			WorkItem*				EnqueueProductionOf(const PMobileEntity&e, index_t at = InvalidIndex);
			WorkItem*				EnqueueProductionOfVehicle(ConstName baseName, ConstName turretName);
			WorkItem*				EnqueueProductionOfVehicle(Component::VehicleBase*selectedBase, Component::Turret*selectedTurret);
			#ifndef NO_SATELLITES
				WorkItem*			EnqueueProductionOfSatellite(ConstName typeName);
			#endif

			void					Tick(ResourceVector&v, float&t);
			UINT32					ProductionRounds(const WorkItem&e, bool isActive) const	{return ProductionRounds(e.producing,isActive);}

			PMobileEntity			SpawnFinishedProductionAnywhere(float&t);
			PMobileEntity			CompleteProduction(float &t);
			PMobileEntity			GetActiveProduction() const;
			bool					AbortConstruction(const TUniqueID&);
			void					UpdateGatheringPoint(const Grid::LCellIdI&);

		#else
			UINT32					ProductionRounds(const WorkItem&e, bool isActive) const	{return ProductionRounds(e.cost,isActive);}
			Component::VehicleBase*	GetActiveProductionBase() const;

		#endif


		//new overriables:
		virtual void				SignalLastProductionDone(float&t) {}
		virtual void				SignalNewProductionEnqueued() {}
		virtual bool				CanProduce(const PWorldEntity&proto) const=0;

	};



}




#endif
