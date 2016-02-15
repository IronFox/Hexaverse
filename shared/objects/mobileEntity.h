#ifndef objectsMobileEntityH
#define objectsMobileEntityH

#include "entity.h"
#include "../cast.h"
#include "jobs.h"
#include "hibernating.h"


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




	

	class MobileResourceEnvironment
	{
	public:
		#ifdef WIRELESS_VEHICLES
			#ifndef ONE_COMPOUND_PER_FACTION
				Grid::LowSetup::TVolumeDescriptor desc;
				Buffer0<std::pair<UINT32, PStructure> >		sourceCompounds;

				bool			ContainsCompound(UINT32 compoundID)	const;

				void			GetOwnCompoundsIn(const OwnerParameter&p, const Grid::LowSetup::TVolumeDescriptor&desc);
				void			AddCompound(const PStructure&ref);
			#endif

			#ifndef CLIENT
				static void		ChangeCompoundStorage(Compound&compound, MobileEntity*originatingAt, float atTime, Resource res, int&change);
				static void		ForceChangeCompoundStorage(Compound&compound, MobileEntity*originatingAt, float atTime, Resource res, int change);
				static void		WithdrawOverproduction(Compound&compound, MobileEntity*originatingAt, Resource res, int&change);
			#endif


			struct TCacheID
			{
				UINT32			round=0xFFFFFFFF;
				UINT16			stackID=0xFFFF,numCompounds=0;
				UINT8			layerID=0xFF;
				UINT8			phase=0xFF;

				/**/			TCacheID()	{}
				/**/			TCacheID(const MobileEntity&);
				/**/			TCacheID(const MobileEntity&, const Grid::LCellIdI&);

				bool			operator==(const TCacheID&other) const {return round == other.round && stackID == other.stackID && layerID == other.layerID && phase == other.phase && numCompounds == other.numCompounds;}
				bool			operator!=(const TCacheID&other) const {return !operator==(other);}
			};

			TCacheID			cacheID;

			void	CheckRefresh(const MobileEntity&);
			void	CheckRefresh(const MobileEntity&, const Grid::LCellIdI&);
		#else
			inline void	CheckRefresh(const MobileEntity&)	{};

		#endif

		bool		CanWithdraw(const MobileEntity&owner, Resource res, int demand)	const;
		#ifndef CLIENT
			void	ChangeStorage(MobileEntity&owner, Resource res, int&change);
			void	ForceChangeStorage(MobileEntity&owner, Resource res, int change);
		#endif
		int			GetStorage(const MobileEntity&owner, Resource res)	const;
	};
		


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
	extern Buffer0<PMobileEntity>			storedMobileEntities;

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
		MobileResourceEnvironment	resourceEnvironment;
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
			CursorMode		mode;
		
			#ifdef HAS_WINDOW
				DX11::InstructionBlock32			visualization;
			#endif



			/**/					TaskData():type(Wait),mode(CursorMode::Idle)
			{}
			/**/					TaskData(type_t type, mode_t mode):type(type),mode(mode)
			{}

		};




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


			Queue<Focus>	foci;	//function depends on current mode. movement destination(s) if idle, firing target(s) if in attack mode
			
			Buffer0<PathNode>		path;

			String					name;

			PVehicleJob				job;
			#ifdef HAS_WINDOW
				struct Shape
				{
					DX11::InstructionBlock32	instruction;
					TMatrix4<>		system;
					bool			depthTest;
				};

				Buffer0<Shape>		localizableShapes;

				void				SetLocalizableShape(ConstName structureIdentifier, const Grid::LCellIdI&location);
			#endif


			/**/					Task()	{}
			/**/					Task(const String&name, const ArrayRef<Grid::LCellIdI>&destination, bool isDetailed, type_t type, mode_t mode=CursorMode::Idle) : TaskData(type,mode), name(name)
			{
				ASSERT__(destination.IsNotEmpty());
				for (index_t i = 0; i+1 < destination.length(); i++)
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
				job.swap(other.job);
				#ifdef HAS_WINDOW
					localizableShapes.swap(other.localizableShapes);
				#endif
			}

			Grid::LCellIdI			GetFocus(const Grid::LCellIdI&fallback)	const
			{
				return foci.IsNotEmpty() ? foci.GetNewest().target : fallback;
			}

			bool					AppendPathSegment(const MobileEntity*v, const Grid::LCellIdI&prev, const Task::Focus&target);
			bool					UpdatePath(const MobileEntity*v, const Grid::LCellIdI&pointOfOrigin, FocusChangeType);

			bool					IsJob() const
			{
				return type == Job && job && job->roundsToJobCompletion >= 0;
			
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
		Queue<Task,Swap>		myTasks;

		WHangarStructure		storedIn;

		#ifndef ONE_COMPOUND_PER_FACTION
			ResourceVector		currentStorage;
		#endif


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
			bool				hasTriedPlottingOnMoveAutoMotion=false;
			bool				jobStarted=false;
			bool				hasMoved=false;
			float				timeAt=0.f;		//!< Time (in seconds) into the motion phase to calculate event times
			bool				hasConstructedThisRound=false;
		#else
		#endif

		float					movementActionLeft=1.f,
								attackActionLeft=1.f;

		bool					isAutomated = false;	//certain mobile entity types allow automating a particular functionality (e.g. the surveyor vehicle).
														//Whether this boolean has any effect depends on the type of the local entity


		UINT32					constructionRoundCounter=0;	//!< Number of rounds that the entity has been busy building at its current location

		/**/					MobileEntity(const OwnerParameter&owner, Component::Turret*turret, const Grid::LCellIdI&location) : Super(owner,location), TurretBase(turret)
								#ifdef CLIENT
									,preAnimationLocation(location)
								#endif
		{}

		bool					IsBusy() const {return myTasks.IsNotEmpty();}
		bool					IsBuilding() const {return myTasks.IsNotEmpty() && myTasks.Peek().job && myTasks.Peek().job->roundsToJobCompletion >= 0;}
		bool					IsFinishedBuilding() const {return myTasks.IsNotEmpty() && myTasks.Peek().job && myTasks.Peek().job->roundsToJobCompletion == 0;}
		std::pair<int,int>		GetBuildingProgress() const;

		#ifndef CLIENT
			bool				NextTaskIsMotion() const {return health > 0 && ((!HasTasks() && completeOnMove) || (HasTasks() && myTasks.Peek().type == Task::MoveTo));}
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

		#ifdef CLIENT
			void				Relocate(const Grid::LCellIdI&newLocation, bool updateSystem);

			bool				HasTasks() const {return myTasks.IsNotEmpty();}
		#else

			bool				PlotCourse(ArrayRef<Grid::LCellIdI> destination, bool detailedPath, bool virtualPlatformMode=false);
			bool				PlotFrontCourse(ArrayRef<Grid::LCellIdI> destination, bool detailedPath);
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
				timeAt = std::max(timeAt, timeOffset);	//timeAt is usually 0, but may be bigger if the vehicle fell as a result of an explosion. in that case it might be further along the timeline
				if (attackActionLeft < 1.f)
					RegisterChange(ActionChanged,timeAt);
				attackActionLeft = 1.f;

				hasMoved = false;
				hasTriedPlottingOnMoveAutoMotion = false;
				if (MotionIgnoresAttack())
				{
					if (movementActionLeft < 1.f)
						RegisterChange(ActionChanged,timeAt);
					movementActionLeft = 1.f;
				}
			}
			void				BeginAttackPhase()	//executed before BeginMotionPhase this round
			{
				hasConstructedThisRound = false;
				timeAt = 0;
				if (movementActionLeft < 1.f)
					RegisterChange(ActionChanged,timeAt);
				movementActionLeft = 1.f;

				if (AttackIgnoresMotion())
				{
					if (attackActionLeft < 1.f)
						RegisterChange(ActionChanged,timeAt);
					attackActionLeft = 1.f;

				}
			}


			/**
			Clears all entity tasks
			@param isReaction Set true to indicate that the clearing happens as a result of some unexpected event.
							If true @a completeOnMove will NOT be reset
					

			*/
			bool				ClearAllTasks(bool isReaction, const TCodeLocation&caller)
			{
				LogEvent(EventSignificance::Debug,this,"ClearAllTasks("+String(isReaction)+", "+String(caller.ToString())+")");
				if (TasksAreLocked())
				{
					if (flushTasksOnUnlock)
						return false;
					timeAt += timeEpsilon;
					flushTasksOnUnlock = true;
					LogEvent(EventSignificance::Debug,this," ...set to clear on unlock");
					return true;
				}
				
				if (myTasks.IsNotEmpty() || completeOnMove)
				{
					timeAt += timeEpsilon;
					jobStarted = false;
					if (!isReaction)
						completeOnMove.reset();
					RegisterChange(TasksChanged,timeAt);
					//tasksChanged = true;
					myTasks.Clear();
					LogEvent(EventSignificance::Debug,this," ...cleared");
					return true;
				}
				return false;
			}

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
				hasConstructedThisRound = false;
				RegisterChange(ActionChanged,0);
			}

			/**
			Attempts to position the vehicle exactly in the specified cell. The vehicle may fall as a result if the area beneath is free
			*/
			virtual MobilePlacementResult	PlaceExactlyAt(const Grid::LCellIdI&, float timeCost, float minTimeOffset, bool signalEvent=true, bool isCreated=true, bool failIsFatal=false) = 0;

			bool				CompleteOnMove(const Grid::LCellIdI&referenceLocation);


			bool				PerformNonAttack();
		protected:
			//void				ProcessVisibilityOf(const PWorldEntity&e, float&t);
			//bool				ProcessVisibilityOfS(const PWorldEntity&e, float&t);
		private:
			bool				HandleNormalTask();
		public:

		#endif

		UINT32					GetOperationRounds(int jobCosts, Component::operation_t op) const
		{
			if (!jobCosts)
				return 0;
			int speed = GetOperationSpeed(op);
			if (!speed)
				return 0xFFFFFFFF;
			int rs = jobCosts / speed;
			if (jobCosts % speed)
				rs ++;
			return rs;
		}
		Grid::LCellIdI			GetConstructionLocation() const {return GetConstructionLocation(location);}
		float					GetMovementInitiative()const	{return GetBaseMovementInitiative() * movementActionLeft;}
		bool					AttackIgnoresMotion() const {return turret->ignoresVehicleMotion;}
		bool					AttackIsStationary() const {return turret->requiresStationaryVehicle;}
		float					GetAttackExplosionRadius() const {return turret->explosionRadius;}
		bool					CanRepair() const {return turret->CanRepair();}
		bool					CanAttack() const {return turret->CanAttack();}

		int						GetCurrentMineralConsumption() const;

		/**
		Attempts to withdraw upkeep from locally available resources.
		@return true, if the required amount has successfully been withdrawn, false otherwise
		@param[out] outMissing Vector containing missing resources. Always valid, even if the method returns true (will be all 0 then)
		*/
		bool					TrySelfMaintain(float&atTime, ResourceVector&outMissing);


		/**
		Updates @a system and @a invert based on @a location
		Called automatically where needed.
		*/
		void					UpdateSystem();

		/**
		Retrieves the amount of minerals that can be converted into structure by the local entity
		@return Amount of minerals that can be converted per round by the local entity. 0 for entities that cannot construct
		*/
		virtual int				GetOperationSpeed(Component::operation_t op) const {return turret->operationSpeed[op];}
		virtual Grid::LCellIdI	GetConstructionLocation(const Grid::LCellIdI&focus) const {return focus;}
		virtual bool			MotionIgnoresAttack() const=0;
		virtual int				GetMovementStepsPerRound() const =0;
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


		//TurretBase overrides:
		virtual float			GetAttackActionLeft() const override {return attackActionLeft;};
		virtual bool			NextTaskIsAttack() const override {return health > 0 && HasTasks() && myTasks.Peek().IsMode(CursorMode::AttackMode);}
		#ifndef CLIENT
			virtual void		Attack(float&currentTime)	override;
		#endif


		//WorldEntity overrides:
		virtual void			GetUpkeep(ResourceVector&v) const override {Super::GetUpkeep(v); v.level[Resource::Minerals] = 0;}	//no mineral upkeep for mobiles
		//virtual bool			IsActive() const override {return !IsHibernating() && Super::IsActive();}
		virtual bool			IsMobile() const override {return true;}
		#ifndef NO_HIBERNATION
			virtual const Hibernating*ToHibernating() const override {return this;}
		#endif
		virtual float			GetMaxScanningRange() const override	{return turret->scanningRange;};	//returns the effective scanning range of the local entity


	protected:
		inline void				OffsetTime(float&t)
		{
			#ifndef CLIENT
				timeAt = std::max(timeAt,t);
				timeAt += timeEpsilon;
				t = timeAt;
			#endif
		}
	};


}




#endif
