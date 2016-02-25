#ifndef objectMobileEntityProducerH
#define objectMobileEntityProducerH

#include "../sphereGrid.h"
#include "types.h"
#include "../uniqueID.h"
#ifdef HAS_WINDOW
	#include "visualCommands.h"
#endif
#ifndef CLIENT
	#include "resource.h"
#endif


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

		bool						Set(const Grid::LCellIdI&point, const PWorldEntity&parent);

	};

	
	/**
	Member class used by all structures that can produce mobile entities.
	Rather than inheriting this class, they store it as a member and override the ToProducer() methods
	*/
	class MobileEntityProducer
	{
	protected:
		String						JobName(index_t index, const String&itemName, UINT32 roundsLeft)const;
		UINT32						ProductionRounds( const PMobileEntity&v, bool isActive) const;

	public:
	
		GatheringPoint				gatheringPoint;

		struct WorkItem
		{
			TUniqueID				id;
			#ifdef CLIENT
				#ifdef HAS_WINDOW
					EntityConstructionCommand	commandToTerminate;
				#endif
				String				name;
				int					cost, roundsToCompletion;
				Component::VehicleBase*		base=nullptr;	//set for vehicles only
			#else
				PMobileEntity		producing;

				/**
				Checks whether or not the local work item produces anything. null-pointer sensitive (can be called on a null-object)
				*/
				bool				IsEmpty() const	{return this != nullptr && !producing;}
			#endif

			bool					IsSatellite() const;
		};
		//typedef std::pair<PVehicle,Command>		WorkItem;
		Buffer0<WorkItem>			queue;	//must use buffer here, because elements can be erased and changes are rare

		UINT32						productionPerRound,	//!< Amount of minerals converted per round. Effectively the production speed of the local instance
									productionDone=0,	//!< Total amount of minerals invested into the current production. Reset to 0 upon completion
									productionRoundsLeft=0;


		/**/						MobileEntityProducer(UINT32 productionPerRound):productionPerRound(productionPerRound)
									{}

		UINT32						ProductionRounds(int cost, bool isActive) const;

		bool						IsBuilding(std::pair<int,int>&outProgress) const
		{
			if (queue.IsEmpty())
				return false;
			UINT32 max = ProductionRounds(queue.first(),false);
			outProgress = std::pair<int,int>(max - productionRoundsLeft , max);
			return true;
		}

		void						AppendTask(const TUniqueID&id, ConstName baseName, ConstName turretName);

		#ifdef HAS_WINDOW
			void					VisitAllCommands(FCommandVisitor visitor);
			void					VisitAllCommands_Reverse(FCommandVisitor visitor);
		#endif
		bool						AwaitsUserInteraction() const;

		#ifndef CLIENT
			WorkItem*				EnqueueProductionOf(const PMobileEntity&e, index_t at = InvalidIndex);
			WorkItem*				EnqueueProductionOfVehicle(ConstName baseName, ConstName turretName);
			WorkItem*				EnqueueProductionOfVehicle(Component::VehicleBase*selectedBase, Component::Turret*selectedTurret);
			WorkItem*				EnqueueProductionOfSatellite(ConstName typeName);

			void					Tick(ResourceVector&v);
			UINT32					ProductionRounds(const WorkItem&e, bool isActive) const	{return ProductionRounds(e.producing,isActive);}

			PMobileEntity			SpawnFinishedProductionAnywhere();
			PMobileEntity			CompleteProduction();
			PMobileEntity			GetActiveProduction() const;
			bool					AbortConstruction(const TUniqueID&);
			void					UpdateGatheringPoint(const Grid::LCellIdI&);

		#else
			UINT32					ProductionRounds(const WorkItem&e, bool isActive) const	{return ProductionRounds(e.cost,isActive);}
			Component::VehicleBase*	GetActiveProductionBase() const;

		#endif


		//new overriables:
		virtual void				SignalLastProductionDone() {}
		virtual void				SignalNewProductionEnqueued() {}

	};



}




#endif
