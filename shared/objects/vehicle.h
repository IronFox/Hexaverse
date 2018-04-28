#ifndef objectsVehicleH
#define objectsVehicleH

#include "mobileEntity.h"
#include "turretBase.h"
#include "../sharedContainer.h"
#include "entityType.h"

namespace Objects
{

	extern SharedContainer<Vehicle>		vehicles;

	extern Grid::THighDistanceField		vehicleDistanceField;

	#ifndef CLIENT
		extern bool			newVehiclesHaveFullAmmo;
	#endif


	/**
	Instance of a vehicle in the world
	*/
	class Vehicle : public MobileEntity
	{
	private:
		//void					RecompilePath(int stepsPerRound);
		//bool					RecompileTrajectory(M::TVec3<>&impactAt, Grid::HCellIdI&impactCell);
		typedef MobileEntity	Super;
	public:


	
		static count_t			CountTurretOccurrences(const PFaction&,Component::Turret*);




		/**/					Vehicle(const OwnerParameter&owner, const Grid::LCellIdI&location, Component::VehicleBase*base, Component::Turret*turret) :
									MobileEntity(owner,turret,location),base(base)
								{
									ASSERT__(base); ASSERT__(turret); //ASSERT__(turret->mountType == Component::Turret::MountType::Vehicle);
								}

		Component::VehicleBase*	base;








		//void					RotateTurretToFocusAndFire();



		
		#ifndef CLIENT
			virtual bool		DirectTo(mode_t mode, MutableArrayRef<Grid::LCellIdI> waypoints, bool detailedPath, bool leftClick = false) override;

			virtual bool		FrontQueueAutoAttack(TurretBase::TargetChoice&lastChoice)override;
			bool				EnqueueAttack(Grid::LCellIdI destination);
			bool				FrontEnqueueAttack(Grid::LCellIdI destination);






			/**
			Signals that a new round has started and production/movement should continue.
			@return Number of actions left (to be passed into Tick() during a second run), or zero if operations have completed
			*/
			int					Tick(int actionsLeft);


			/**
			Positions the vehicle on the first possible level in or above the specified cell.
			*/
			void				PositionOnTop(const Grid::LCellIdI&, bool signalEvent);

			/**
			Attempts to position the vehicle exactly in the specified cell. The vehicle may fall as a result if the area beneath is free
			*/
			MobilePlacementResult	PlaceExactlyAt(const Grid::LCellIdI&, float timeCost, float minTimeOffset, bool signalEvent=true, bool isCreated=true, bool failIsFatal=false) override;
		
			virtual void		OnInsert( float&atTime, bool freshConstruction ) override;

		#else

		#endif

		

		//virtual bool			CanBuild(const PStructureClass&) const { return true; }

		/**
		Checks whether or not the local object can climb the specified inclination level. Normal vehicles can climb an inclination of 1 up and down
		@param inclination Inclination level. Values above zero indicate ascent, below value indicate a descent
		*/
		bool					CanClimb(int inclination) const { return base->CanClimb(inclination);}
		/**
		Checks whether or not the local vehicle can stand atop the specified cell (not in the cell).
		@param world World to check against
		@param cell The cell the local vehicle is attempting to stand atop of. If relocation is performed the cell one higher than this will be used for the final location
		@param ignoreWater Upon encountering pure water, the method succeeds if either the local vehicle can stand on water, or this value is set true
		@param doFloat Allows the vehicle to stand on any substance. Useful for certain construction jobs
		*/
		bool					CanStandOn(const World::Data&world, const Grid::LCellIdI&cell, bool ignoreWater, bool doFloat) const {return base->CanStandOn(world,cell,ignoreWater, doFloat);}

		bool					IsAutomatedResourceDetector() const {return turret->detectsResources && isAutomated;}
		bool					IsNonAutomatedResourceDetector() const {return turret->detectsResources && !isAutomated;}

		//return abs(inclination) <= 1; }


		static String			GetTypeName(const Component::VehicleBase*base, const Component::Turret*turret) {return base->shortName + turret->shortName;}
		


		#ifdef HAS_WINDOW
			virtual bool		TestIntersection(const M::TVec3<>&b, const M::TVec3<>&d, float & distance) const override;
			virtual const CC*	GetCommands(CC&dynamicStorage) const override;
		#endif

		int						GetPrice() const	{return GetPrice(base,turret, GetResearchState());}
		static int				GetPrice(Component::VehicleBase*, Component::Turret*, const Research::Attributes::Table&);
		static int				GetPrice(Component::VehicleBase&b, Component::Turret&t, const Research::Attributes::Table&table)	{return GetPrice(&b,&t,table);}
		int						GetMaxClimbingHeight() const {return base->maxClimbUp;}
		int						GetMaxFallingHeight() const {return -base->minClimbDown;}


		int						GetTurretOverweight() const;
		float					GetTurretOverweightFactor() const;

		//BaseEntity overrides:
		virtual String			GetTypeName() const override {return GetTypeName(base,turret);}
		virtual EntityType		GetType() const override {return EntityType(base->constName,turret->constName);}

		//WorldEntity overrides:
		virtual int				GetConstructionCost() const override {return GetPrice();}
		#ifndef NO_HIBERNATION
			virtual int			GetUplinkTraffic() const override;
		#endif
		virtual bool			IsUnlockedAtState(const Research::Attributes::Table&researchState) const	override {return base->isUnlocked.GetValue(researchState) && turret->isUnlocked.GetValue(researchState);}
		virtual bool			CanBeUnlocked() const {return base->isUnlocked.IsFunctionOrConstNonZero() && turret->isUnlocked.IsFunctionOrConstNonZero();}
		virtual int				GetMaxHealth() const override;
		virtual String			DescribeMaxHealth() const override;
		virtual PWorldEntity	CreateNewInstance(const OwnerParameter&owner, const Grid::LCellIdI&location, float atTime) override {return std::make_shared<Vehicle>(owner,location,base,turret);};
		virtual float			GetScanningRange() const override;
		virtual String			DescribeScanningRange() const override;
		virtual PTurretBase		GetTurretBase(float&zOffset) override { zOffset = base->height; return To<Vehicle>::Cast(shared_from_this());}
		virtual PConstTurretBase	GetTurretBase(float&zOffset) const override { zOffset = base->height; return To<const Vehicle>::Cast(shared_from_this());}
		virtual bool			IsVehicle() const override {return true;}
		#ifdef HAS_WINDOW
			virtual void		DescribeType(Command::Base::Label&label, UINT32 descFlags) override;
			virtual float		GetSoundLevel() const {return GetActivityLevel()*0.25f;}	//save 75% for motion
		#endif

		//MobileEntity overrides:
		virtual bool			MotionIgnoresAttack() const override {return base->ignoresVehicleAttacks;}
		virtual int				GetMovementStepsPerRound() const override;
		virtual String			DescribeMovementStepsPerRound() const override;
		virtual float			GetBaseMovementInitiative() const override {return base->initiative;}
		virtual BYTE			GetConstructionAcceleration(Component::operation_t op) const override {return base->acceleration[op];}
		#ifndef ONE_COMPOUND_PER_FACTION
			virtual ResourceVector		GetMaxStorageCapacity()	const override;
		#endif


		//TurretBase overrides:
		virtual float			GetFlatAttackRange() const override;
		virtual String			DescribeFlatAttackRange() const override;
		virtual float			GetZOffset() const override {return base->height;}
		virtual int				GetOperationSpeed(Component::operation_t) const override;
		virtual String			DescribeOperationSpeed(Component::operation_t) const override;

		//Hibernating overrides:
		#ifndef NO_HIBERNATION
			virtual int			GetUplinkPriority() const override {return turret->uplinkPriority;}
		#endif






	};





	/**
	Iterates through all vehicles, both stored and displayed in the world. Markers are not iterated
	*/
//	void						ForeachVehicle(const std::function<void(const PVehicle&)>&);

}



#endif
