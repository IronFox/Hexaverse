#ifndef objectsMobileEntityH
#define objectsMobileEntityH

#include "entity.h"
#include "../cast.h"
#include "jobs.h"
#include "hibernating.h"
#include "../sharedContainer.h"
#include <structure/observable.h>
#include "../roundNumber.h"


namespace Objects
{
	CONSTRUCT_ENUMERATION7(FocusChangeType,
		NoChange,
		FullChange,
		SituationChanged,
		FirstSegmentChanged,
		FocusAppended,
		LastFocusDropped,
		FirstFocusDropped
	);




	struct PathNode
	{
		Grid::LCellIdI	cell;
		int				movementCostsFromPrevNode;
		//bool			reachedInProcessingLimit = true;	//not useful, even for client

		PathNode():movementCostsFromPrevNode(0)	{}
		PathNode(const Grid::LCellIdI&cell, int costs) : cell(cell),movementCostsFromPrevNode(costs)	{}
	};


	/**
	List of all mobile entities that are currently in storage.
	These are not stored in any other global list (e.g. vehicles,satellites,grids, etc.).
	Hangar entities may/should reference them however.
	A link to a hangar structure (storedIn) must be set and not expired.
	*/
	extern SharedContainer<MobileEntity>			storedMobileEntities;

	/**
	Checks all mobile entities, both in storage and not, for various consistency aspects.
	Violations trigger a fatal exception that terminates execution.
	*/
	void						CheckMobileEntitySanity();

	/**
	@brief Movable entity (e.g. a vehicle, or satellite).

	Can enqueue and partially execute common commands. Can (theoretically) be automated
	*/
	class MobileEntity : public WorldEntity, public TurretBase
		#ifndef NO_HIBERNATION
			, public Hibernating
		#endif
	{
		typedef WorldEntity	Super;
	protected:
		void		HandleMotion(MobilePlacementResult&rs, bool signalEvent, const Grid::LCellIdI&oldLocation, float timeCost);
	public:
		typedef CursorMode	mode_t;

		struct TaskData
		{
			enum type_t
			{
				Wait,
				MoveTo,
				Mode,
				Job,
			}						type;
			CursorMode				mode;
		



			/**/					TaskData():type(Wait),mode(CursorMode::Idle)
			{}
			/**/					TaskData(type_t type, mode_t mode):type(type),mode(mode)
			{}

		};

		#ifndef CLIENT
			RoundNumber				lastDeployment;
		#endif


		struct Task : public TaskData
		{
			struct Focus
			{
				Grid::LCellIdI		target;
				#ifndef CLIENT
					bool			isDetailed=false,		//!< Indicates that this node represents exactly one step from the previous. Detailed paths imply explicit computing, and fail if steps between nodes exceed one step
									isIntermediate=false;	//!< Indicates that this node is an automatic waypoint and implicitly computed. Will be removed automatically if the last task is dropped
				#endif

				bool				operator==(const Focus&other) const
				{
					return target == other.target
						#ifndef CLIENT
							&& isDetailed == other.isDetailed
							&& isIntermediate == other.isIntermediate
						#endif
					;
				}

				bool				operator!=(const Focus&other) const
				{
					return !operator==(other);
				}
			};


			Ctr::Queue<Focus>		foci;	//function depends on current mode. movement destination(s) if idle, firing target(s) if in attack mode
			
			Ctr::Buffer0<PathNode>	path;
			PClientAttachment		visualization;

			String					name;

			PVehicleJob				job;

			static Observable<const MobileEntity&, Task&,const Grid::LCellIdI&>	onVisualizationOutdated;


			/**/					Task()	{}
			/**/					Task(const String&name, const Ctr::ArrayRef<Grid::LCellIdI>&destination, bool isDetailed, type_t type, mode_t mode=CursorMode::Idle) : TaskData(type,mode), name(name)
			{
				ASSERT__(destination.IsNotEmpty());
				for (index_t i = 0; i+1 < destination.GetLength(); i++)
				{
					Focus&f = this->foci.Push();
					#ifndef CLIENT
					f.isDetailed = isDetailed;
					f.isIntermediate = true;
					#endif
					f.target = destination[i];
				}

				{
					Focus&f = this->foci.Push();
					#ifndef CLIENT
					f.isDetailed = isDetailed;
					f.isIntermediate = false;
					#endif
					f.target = destination.Last();
				}
			}


			void					swap(Task&other)
			{
				std::swap((TaskData&)*this, (TaskData&)other);
				foci.swap(other.foci);
				path.swap(other.path);
				name.swap(other.name);
				visualization.swap(other.visualization);
				job.swap(other.job);
			}

			Grid::LCellIdI			GetFocus(const Grid::LCellIdI&fallback)	const
			{
				return foci.IsNotEmpty() ? foci.GetNewest().target : fallback;
			}

			bool					AppendPathSegment(const MobileEntity*v, const Grid::LCellIdI&prev, const Task::Focus&target);
			bool					UpdatePath(const MobileEntity*v, const Grid::LCellIdI&pointOfOrigin, FocusChangeType);

			bool					IsJob() const
			{
				return type == Job && job && job->continuationsToJobCompletion >= 0;
			
			}
			bool					IsMode(mode_t mode) const
			{
				return type == Mode && this->mode == mode;
			}
			bool					ContainsMovement() const
			{
				return (type == Task::MoveTo || 
				(type == Job && job && job->movesToFocusWhenDone)) && foci.IsNotEmpty();
			}


			bool					Replot(const Grid::LCellIdI&pointOfOrigin, const MobileEntity*parent, FocusChangeType, bool recompileVisualization);

		private:
			#ifdef HAS_WINDOW
				void				RecompilePath(int stepsPerRound, float structureSize=vehicleScale);
			#endif
		};


		#ifdef CLIENT
			Grid::LCellIdI		preAnimationLocation;
		#endif
		Ctr::Queue<Task,Swap>	myTasks;

		WHangar					storedIn;

		#ifndef ONE_COMPOUND_PER_FACTION
			ResourceVector		currentStorage;
		#endif

		/**
		Maximum amount of construction action a vehicle can have left in one round.
		A single job consumes power-of-two values, usually the entire 128 at once.

		*/
		static const BYTE		MaxConstructionActionLeft = 128;


		#ifndef CLIENT

			private:
				int				privateLockCounter = 0;
				bool			flushTasksOnUnlock = false;
			public:

			/**
			Signals that changes to the local task list (myTasks) must not happen before UnlockTasks() is called againg.
			Call this method before taking a reference to any task in @a myTasks to assure it remains valid.
			Locking has both debugging purposes and logic effects. The only valid alteration in locked mode is 
			ClearAllTasks() which will memorize that tasks are to be cleared when they are next unlocked.
			LockTasks() may be called recursively. Remember to call UnlockTasks() ONCE for each time LockTasks() was called
			*/
			void				LockTasks()	{privateLockCounter++;}

			Task&				LockedPeekTask() {ASSERT__(myTasks.IsNotEmpty()); LockTasks(); return myTasks.Peek();}

			/**
			Opposite of LockTasks(). Must be called exactly ONCE for each time LockTasks() was called. This operation may
			erase all contents of @a myTasks if the last lock is removed and any process has called ClearAllTasks() in the meantime.
			*/
			void				UnlockTasks();
			/**
			Checks whether or not tasks are currently locked. All modification operations (create new task, erase some task)
			MUST check this method in an ASSERT__ . Methods must be designed such that this event simply does not occur.
			*/
			bool				TasksAreLocked() const {return privateLockCounter > 0;}
			void				EraseFirstTask();

			PVehicleJob			completeOnMove;
			BYTE				numTriedPlottingOnMoveAutoMotion=0;
			bool				jobStarted=false; //!< Tracks whether the currently front-most job has been started
			bool				hasMoved=false;
			BYTE				constructionActionLeft=MaxConstructionActionLeft;
		#else
		#endif

		float					movementActionLeft=1.f,
								attackActionLeft=1.f;

		bool					isAutomated = false;	//certain mobile entity types allow automating a particular functionality (e.g. the surveyor vehicle).
														//Whether this boolean has any effect depends on the type of the local entity


		UINT32					constructionActionCounter=0;	//!< Number of round actions that the entity has been busy building at its current location

		/**/					MobileEntity(const OwnerParameter&owner, Component::Turret*turret, const Grid::LCellIdI&location) : Super(owner,location), TurretBase(turret)
								#ifdef CLIENT
									,preAnimationLocation(location)
								#endif
		{}

		bool					IsBusy() const {return myTasks.IsNotEmpty();}
		bool					IsBuilding() const {return myTasks.IsNotEmpty() && myTasks.Peek().IsJob();}
		bool					IsFinishedBuilding() const {return myTasks.IsNotEmpty() && myTasks.Peek().job && myTasks.Peek().job->continuationsToJobCompletion == 0;}
		std::pair<int,int>		GetBuildingProgress() const;

		#ifndef CLIENT
			bool				NextTaskIsMotion() const {return IsAlive() && ((!HasTasks() && completeOnMove) || (HasTasks() && myTasks.Peek().type == Task::MoveTo));}
		#endif

		#ifndef NO_HIBERNATION
			void				Hibernate(bool hibernate,float atTime)
			{
				#ifdef CLIENT
					Hibernating::UpdateHibernation(this,hibernate,0);
				#else
					float t = std::max(timeAt,atTime);
					if (Hibernating::UpdateHibernation(this,hibernate,t))
						timeAt = t;
				#endif
			}
		#endif


		#ifndef ONE_COMPOUND_PER_FACTION
			/**
			Calculates the maximum storage vector based on the local components
			*/
			virtual ResourceVector		GetMaxStorageCapacity()	const = 0;
		#endif




		Grid::LCellIdI			GetLastPointOfOrigin() const;
		Grid::LCellIdI			GetPrevPointOfOrigin() const;

		/**
		Calculates the number of rounds needed to complete the given number of job continuations
		*/
		float					ContinuationsToRounds(int continuationsToJobCompletion, Component::operation_t op) const;


		/**
		Checks whether or not the local entity has any constructing jobs queued for the specified location
		*/
		bool					ConstructingHere(const Grid::LCellIdI&loc) const;
		/**
		Checks whether or not any of the queued tasks involves moving away from the current location.
		This does not include any automatic movements performed at the end of a construction job
		*/
		bool					HasMotionTasks() const;

		#ifdef CLIENT
			void				Relocate(const Grid::LCellIdI&newLocation, bool updateSystem);

			bool				HasTasks() const {return myTasks.IsNotEmpty();}
		#else

			bool				PlotCourse(MutableArrayRef<Grid::LCellIdI> destination, bool detailedPath, bool virtualPlatformMode=false);
			bool				PlotFrontCourse(MutableArrayRef<Grid::LCellIdI> destination, bool detailedPath);
			virtual	void		SetupSystem() override {UpdateSystem();}
			void				RegisterNewJob(
										const String&localizableShapeIdentifier,
										const PVehicleJob&job, const Grid::LCellIdI*focus);
			void				RegisterNewFrontJob(
										const String&localizableShapeIdentifier,
										const PVehicleJob&job, const PVehicleJob&parent, const Grid::LCellIdI*focus);


			bool				DropLastTask();

			void				BeginMotionPhase(float timeOffset)
			{
				entityTime = std::max(entityTime, timeOffset);	//timeAt is usually 0, but may be bigger if the vehicle fell as a result of an explosion. in that case it might be further along the timeline
				if (attackActionLeft < 1.f)
					RegisterChange(ActionChanged,entityTime);
				attackActionLeft = 1.f;

				hasMoved = false;
				numTriedPlottingOnMoveAutoMotion = 0;
				if (MotionIgnoresAttack())
				{
					if (movementActionLeft < 1.f)
						RegisterChange(ActionChanged,entityTime);
					movementActionLeft = 1.f;
				}
			}
			void				BeginAttackPhase()	//executed before BeginMotionPhase this round
			{
				constructionActionLeft=MaxConstructionActionLeft;
				if (movementActionLeft < 1.f)
					RegisterChange(ActionChanged,entityTime);
				movementActionLeft = 1.f;

				if (AttackAndMotionAreIndependent())
				{
					if (attackActionLeft < 1.f)
						RegisterChange(ActionChanged,entityTime);
					attackActionLeft = 1.f;

				}
			}


			/**
			Clears all entity tasks
			@param isReaction Set true to indicate that the clearing happens as a result of some unexpected event.
							If true @a completeOnMove will NOT be reset
					

			*/
			bool				ClearAllTasks(bool isReaction, const TCodeLocation&caller);

			bool				HasTasks() const
			{
				return myTasks.IsNotEmpty() && (!TasksAreLocked() || !flushTasksOnUnlock);
			}

			/**
			Called when checked out from a vehicle store
			*/
			void				SignalDeployment()
			{
				movementActionLeft = 1.f;
				attackActionLeft = 0.f;
				hasMoved = true;
				constructionActionLeft=MaxConstructionActionLeft;
				RegisterChange(ActionChanged,0);
			}

			/**
			Attempts to position the vehicle exactly in the specified cell. The vehicle may fall as a result if the area beneath is free
			*/
			virtual MobilePlacementResult	PlaceExactlyAt(const Grid::LCellIdI&, float timeCost, float minTimeOffset, bool signalEvent=true, bool isCreated=true, bool failIsFatal=false) = 0;
		
			/**
			Attempts to place the entity exactly whered it already is.
			If its ground beneath has erroded, the entity will fall.
			If the new ground is unsuitable, or the fall is too far, the entity will be destroyed (Remove() is called automatically).
			Check IsAlive() or IsDeadOrGhost() to see if the operation has resulted in entity destruction.

			Currently if effectively invokes 
					HandleNow(PlaceExactlyAt(location,0, atTime));
			*/
			void				CheckGround(float atTime);

			/**
			Handles destructive results of PlaceExactlyAt(), since PlaceExactlyAt() is not allowed to immediately destroy the local entity
			*/
			void				HandleNow(MobilePlacementResult);


			bool				CompleteOnMove(const Grid::LCellIdI&referenceLocation);



			enum class TaskResult
			{
				AllDoneForThisRound,
				FinishedCanDoMore,
				TryingSomething,
//				TerminalFailureEntityDestroyed,
			};

			TaskResult			PerformNonAttack();
		protected:
			//void				ProcessVisibilityOf(const PWorldEntity&e, float&t);
			//bool				ProcessVisibilityOfS(const PWorldEntity&e, float&t);
		private:
			TaskResult			HandleNormalTask();
			TaskResult			AutoMoveComplete();

		public:

		#endif

		/**
		Calculates the number of continuations necessary to complete a job of the given costs and operation affinity.
		@param jobCosts Material cost of the respective job (may be theoretical)
		@param op Operation affinity to calculate speed from
		@return Number of continuations necessary to complete the job (rounded up)
		*/
		UINT32					GetOperationContinuations(int jobCosts, Component::operation_t op) const;
		/**
		Estimates the number of actual rounds necessary to complete a job of the given costs and operation affinity.
		*/
		UINT32					GetOperationCompletionRounds(int jobCosts, Component::operation_t op) const;

		Grid::LCellIdI			GetConstructionLocation() const {return GetConstructionLocation(location);}
		float					GetMovementInitiative()const	{return GetBaseMovementInitiative() * movementActionLeft;}
		bool					AttackAndMotionAreIndependent() const {return turret->independentAttackAndMotion;}
		bool					AttackIsStationary() const {return turret->requiresStationaryVehicle;}
		float					GetAttackExplosionRadius() const override;
		bool					CanRepair() const {return turret->CanRepair();}
		bool					CanAttack() const {return turret->CanAttack();}
		virtual BYTE			GetConstructionAcceleration(Component::operation_t op) const {return 0;}

		int						GetCurrentMineralConsumption() const;

		/**
		Updates @a system and @a invert based on @a location
		Called automatically where needed.
		*/
		void					UpdateSystem();

		/**
		Constructs a mobile entity system.
		The old location stored in the specified system is used to define the direction of the final system,
		so don't pass an uninitialized system.
		@param[in,out] inOutSys System to use for direction calculation, and to put the new vehicle system matrix into. Must be initialized
		@param location New vehicle location
		*/
		static void				UpdateSystem(M::TMatrix4<>&inOutSys,const Grid::LCellIdI&location);


		/**
		Retrieves the amount of minerals that can be converted into structure by the local entity
		@return Amount of minerals that can be converted per round by the local entity. 0 for entities that cannot construct
		*/
		virtual int				GetOperationSpeed(Component::operation_t op) const {return turret->GetOperationSpeed(op,owner);}
		virtual Grid::LCellIdI	GetConstructionLocation(const Grid::LCellIdI&focus) const {return focus;}
		virtual bool			MotionIgnoresAttack() const=0;
		virtual int				GetMovementStepsPerRound() const =0;
		virtual String			DescribeMovementStepsPerRound() const =0;
		virtual float			GetBaseMovementInitiative() const {return 100;}
		/**
		Retrieves the visual breath of compiled paths. Purely cosmetic.
		Effective only if HAS_WINDOW is defined.
		*/
		virtual float			GetPathSize() const {return vehicleScale;}
		/**
		Changes the @a location member of the local entity such that it can be written to file.
		@a location is expected to be in runtime state.
		So far only satellites do this in case residence height changes
		*/
		virtual void			TranslateLocationForExport() {}
		/**
		Changes the @a location member of the local entity such that it can be used during runtime.
		@a location is expected to be in a state read from file.
		So far only satellites do this in case residence height changes
		*/
		virtual void			TranslateLocationFromExport(){};


		//TurretBase overrides:
		virtual OwnerParameter	GetOwner() const override {return owner;};
		virtual float			GetAttackActionLeft() const override {return attackActionLeft;};
		virtual bool			NextTaskIsAttack() const override {return IsAlive() && HasTasks() && myTasks.Peek().IsMode(CursorMode::AttackMode);}
		virtual WorldEntity&	ToEntity() override {return *this;}
		virtual float			GetZOffset() const override {return 0;}
		#ifndef CLIENT
			virtual void		FlushAutoAttacks() override;
			virtual bool		AttackNext(const Grid::LCellIdI&target, bool autoTarget) override;
			virtual void		Attack(float&currentTime)	override;
		#endif
		virtual	bool			HasAmmoForShot() override;


		//WorldEntity overrides:
		//virtual bool			IsActive() const override {return !IsHibernating() && Super::IsActive();}
		#ifndef CLIENT
			virtual void		ClearSafeTasks() override;
		#endif

		/**
		Checks whether there is a task out front that has consumed resources and can not be removed without irreversible losses
		*/
		bool					HasUnsafeTask() const;

		virtual bool			IsMobile() const override {return true;}
		#ifndef NO_HIBERNATION
			virtual const Hibernating*ToHibernating() const override {return this;}
		#endif
		virtual void			VisitSensors(const Grid::LCellIdI&loc, const std::function<void(const World::Sensor&)>&visitor, bool appearance = false) const override;
		virtual float			GetScanningRange() const override;
		virtual String			DescribeScanningRange() const override;
		#ifndef CLIENT
			virtual void		OnDestroy(RemovalType, float&atTime) override;	
		#endif

	protected:
	};



	inline PMobileEntity		ToMobileEntity(const PWorldEntity&e)
	{
		if (!e || !e->IsMobile())
			return PMobileEntity();
		return To<MobileEntity>::Cast(e);
	}
}




#endif
