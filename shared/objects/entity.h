#ifndef objectEntityH
#define objectEntityH

#include "../uniqueID.h"
#include "../grid/stack.h"

#include "turretBase.h"
#include "types.h"
#include "hibernating.h"
#include "luaAttribute.h"
#include "description.h"
#include "entityCommand.h"
#include "geometry.h"

#ifdef HAS_WINDOW
	#include <al/openal.h>
#endif

struct SharedPtrHash
{
	template <typename T>
	static hash_t	ComputeHash(const std::shared_ptr<T>&ptr)
	{
		return (hash_t)ptr.get();
	}

};


namespace Objects
{

	static const float timeEpsilon = 0.001f;	//time in seconds between subsequent events such that they are sorted correctly on the client
	typedef SharedPtrHash	OwnerHash;

	extern Ctr::GenericHashTable<TUniqueID,PWorldEntity>	idMap;


	#ifdef HAS_WINDOW


		float	GetLowLighting(const Grid::LCellIdI&location);


	#endif



	/**
	Root-class for all entity-like objects.
	Every derivative of this class inherits a globally unique identifier, and a world-location (including matrix).
	Derivatives must provide type name method overrides.
	BaseEntity's may or may not be consistent/persistent to all players
	*/
	class BaseEntity 
	{
	protected:

		/**/						BaseEntity(const Grid::LCellIdI&at) : location(at), system(Matrix<>::eye4), inverse(Matrix<>::eye4)
		{
			#ifndef CLIENT
			{
				//if (isSatellite)
				//	ASSERT_GREATER__(location.height,Grid::CellStack::TopHeight);
				//else
				//	ASSERT_IS_CONSTRAINED__(location.height, Grid::CellStack::BottomHeight, Grid::CellStack::TopHeight);

				id.Generate();
			}
			#endif

		}

		#ifdef CLIENT
			String					cachedNiceName;

		#endif

		struct TEmpty{};
		/**/						BaseEntity(TEmpty) : location(Grid::LCellIdI::Invalid), system(Matrix<>::eye4), inverse(Matrix<>::eye4)
		{
			id = TUniqueID::Empty();
		}
	public:
		/**
		Cell location of the local entity. Structures may be destroyed, but they do not move. Vehicles may move.
		Some entity types may not use this variable (so far all do, even markers)
		*/
		Grid::LCellIdI				location;


		/**
		Globally unique id of this entity
		*/
		TUniqueID					id;


		TMatrix4<>					system;		//!< system matrix. Structures: affecting @a localizableShape . If @a localizableShape is NULL then this matrix has no effect
		TMatrix4<>					inverse;	//!< inverse of system


		const TVec3<>&				GetWorldPosition() const {return system.w.xyz;}
		static TVec3<>				GetWorldPosition(const Grid::LCellIdI&loc) {return Grid::ToWorld(loc);}	//might change in the future. Have to make sure that the various system build functions in systems.cpp match

		/**
		Assembles a name composed of the parent management class name and the local instance id
		*/
		String						GetName() const;

		#ifdef CLIENT


			void					UpdateNiceName();
			/**
			Similar to GetName() but tries to avoid hash numbers by using word combinations (duplicates may occur)
			*/
			const String&			GetNiceName() const;
		#else
			String					GetNiceName() const;
		#endif
		/**
		Retrieves the type name of the local instance
		*/
		virtual String				GetTypeName() const = 0;
		/**
		Retrieves the ConstName-based type of the local instance
		Remember that these name indices may change during version updates and can therefore
		NOT be used for disk IO.
		*/
		virtual EntityType			GetType() const = 0;
	};


	class BaseAttributeCache
	{
	public:
		static index_t			versionCounter;

	};

	template <typename T>
		class LuaAttributeCache : public BaseAttributeCache
		{
			typedef BaseAttributeCache	Super;
		public:
			index_t				versionCounter=InvalidIndex;
			T					cached;
			template <typename F1, typename F2>
				T				GetEMPModified(const WorldEntity&e,const LuaAttribute<T,F1>&source,const LuaAttribute<T,F2>&bonus);
			template <typename F1>
				T				GetEMPModified(const WorldEntity&e,const LuaAttribute<T,F1>&source)	{return GetEMPModified(e,source,LuaAttribute<T,NoFilter>());}
			template <typename F1, typename F2>
				String			DescribeEMPModified(const WorldEntity&e,const LuaAttribute<T,F1>&source,const LuaAttribute<T,F2>&bonus);
			template <typename F1>
				String			DescribeEMPModified(const WorldEntity&e,const LuaAttribute<T,F1>&source)	{return DescribeEMPModified(e,source,LuaAttribute<T,NoFilter>()); }
			template <typename T2, typename F1, typename F2>
				T				Get(const Lua::Reference&researchState, const LuaAttribute<T,F1>&source, const LuaAttribute<T2,F2>&multiplier, T minimum);
			bool				IsCurrent() const {return versionCounter == Super::versionCounter;}
			T					Update(T newValue) {cached = newValue; versionCounter = Super::versionCounter; return cached;}
			void				Invalidate() { versionCounter = InvalidIndex; }
		};

	class CachedAttributes
	{
	public:
		LuaAttributeCache<float>	groundScanningRange,
									maxScanningRange,
									horizontalAttackRange;
		LuaAttributeCache<int>		maxHealth,constructionCost;
	};


	class BaseAttributeTable
	{
	public:
		//all functions here take the research state of an owning faction into account
		LuaAttribute<int,NoFilter>			maxHealth,
											constructionCost;
		LuaAttribute<float,UnitScaleFilter>	maxScanningRange;
		LuaAttribute<bool,NoFilter>			isUnlocked;
	};

	
	/**
	World entity base class.
	Everything that consistently resides in the scene such that it can be seen by its owner,
	and others in scanning range derives from WorldEntity

	*/
	class WorldEntity : public BaseEntity, public std::enable_shared_from_this<WorldEntity>
	{
	protected:
		/**/						WorldEntity(const OwnerParameter&owner, const Grid::LCellIdI&at) : BaseEntity(at), owner(owner),unstable(false), health(-1)
		{
			#ifndef CLIENT
			{
				ResetAllChange();
			}
			#endif

		}

		#ifdef HAS_WINDOW
			bool					TestIntersection(const Geometry&geo, const TVec3<>&b, const TVec3<>&d, float & distance) const;
		#endif


	private:
		bool						sleeping = false;	//!< Use SetSleep() instead. Indicates that this entity is asleep. Sleeping entities are not considered in the client-side IsIdle() iteration. Sleeping mobile entities will not execute any jobs


	public:
		struct TRegisteredSensor
		{
			Grid::LCellIdI			location = Grid::LCellIdI::Invalid;
			int						empDamageLevel=0;

			bool					IsSet() const {return location != Grid::LCellIdI::Invalid;}
			bool					operator==(const TRegisteredSensor&other) const {return location == other.location && empDamageLevel == other.empDamageLevel;}
			bool					operator!=(const TRegisteredSensor&other) const {return !operator==(other);}
			String					ToString() const {return location.ToString()+"_e"+String(empDamageLevel);}
		};
		#ifndef CLIENT
			float					entityTime=0.f;		//!< Time (in seconds) into the animation phase to calculate event times
			Random					entityRandom;
			
			void					ReSeedRandom();

			void					ResetEntityTime()	{entityTime = 0;}

		#endif

		mutable CachedAttributes	cache;	//less wrong to make this mutable in const methods, than changing all const methods that don't actually change their respective object

		Lua::Reference				GetResearchState() const;
		virtual BaseAttributeTable*	GetTable() const {FATAL__("Attempting to query attribute table on "+GetTypeName());  return nullptr;}

		inline void					SyncTime(float&remoteTime, float timeOffset)
		{
			#ifndef CLIENT
				remoteTime = entityTime = std::max(entityTime,remoteTime) + timeOffset;
			#endif
		}
		inline void					SyncTime(float&remoteTime)
		{
			#ifndef CLIENT
				remoteTime = entityTime = std::max(entityTime,remoteTime);
			#endif
		}

		inline void					SyncTime(WorldEntity&other, float&remoteTime, float timeOffset)
		{
			#ifndef CLIENT
				other.entityTime = remoteTime = entityTime = std::max(entityTime,std::max(remoteTime,other.entityTime)) + timeOffset;
			#endif
		}
		inline void					SyncTime(WorldEntity&other, float&remoteTime)
		{
			#ifndef CLIENT
				other.entityTime = remoteTime = entityTime = std::max(entityTime,std::max(remoteTime,other.entityTime));
			#endif
		}



		virtual						~WorldEntity();

		#ifdef HAS_WINDOW
			//typedef Objects::Command::Base		Command;
//			typedef Objects::FCommandVisitor	FCommandVisitor;

			Engine::OpenAL::Source	ambientSound;
		#endif

		/**
		Reference to the owner of this entity
		*/
		OwnerParameter				owner;

		bool						isInserted = false,	//tracks if the local entity has been inserted into the scene
									isInsertedIntoGrid = false;//tracks if the local entity has been inserted into the grid

		#ifdef CLIENT
			bool					isOwnedByMe;
			int						ignoreThreatRounds = 0;	//used by ai to ignore threat level if the ai assumes it can win
		#endif

		int							health;
		int							empDamageLevel = 0;	//!< Number of rounds that scanners are disabled due to emp-damage. Also affects extent of damage

		bool						unstable;	//!< Indicates that something is wrong with the consumption vector of the local network. Or the vehicle state is unstable. Again, DO NOT CHANGE.

		#ifndef CLIENT
			enum changeType_t
			{
				Unchanged = 0x0,
				HealthChanged = 0x1,
				PositionChanged = 0x2,
				ActionChanged = 0x4,
				TasksChanged = 0x8,
				StorageChanged = 0x10,
				EnableChanged = 0x20,
				EMPDamageChanged = 0x40,
				AutomationChanged = 0x80,
				NexusControlChanged = 0x100,
				CanEnableChanged = 0x200,
				HibernationChanged = 0x400,
				OrbitalConnectionChanged = 0x800,
				SleepChange = 0x1000,
			};
			float					changeTimer;	//Earliest change time. Reduced if an earlier even occurs
			UINT32					changeFlags;	//Everything that has changed since the last reset (usually once per server-frame)



			void					ResetAllChange()
			{
				changeFlags = 0;
				changeTimer = -std::numeric_limits<float>::max();
			}

			void					RegisterChange(changeType_t type, float timeAt)
			{
				changeFlags |= type;
				changeTimer = std::max(changeTimer,timeAt);
			}
			void					SignalChange(changeType_t type, float timeAt)	{RegisterChange(type,timeAt);}


			//Buffer0<PWorldEntity>	seenBy,
				//					seeing;
			void					SetSleep(bool doSleep, float&t, bool signalToStructure=true)
			{
				if (sleeping == doSleep)
					return;
				LogEvent(EventSignificance::Common,this,"Sleeping changed to "+String(doSleep)+" at t="+String(t));

				sleeping = doSleep;
				RegisterChange(SleepChange,t);
				if (signalToStructure)
					OnSleepChange(t);
			}
			


			virtual void			SetupSystem()=0;	//used to setup the matrices once after creation

			virtual void			OnSleepChange(float&t) {};
		#else
			void					ApplySleepChangeFromServer(bool doSleep)
			{
				sleeping = doSleep;
				#ifdef HAS_WINDOW
					ambientSound.SetGain(GetSoundLevel());
				#endif
			}
		#endif
		//virtual bool				CanSee(const PWorldEntity&other)	const;
		//virtual bool				CanSee(const TVec3<>&center, float radius)	const;


		/**
		Used for visualization: active entities have bright, pulsing key colors, dull dark colors otherwise.
		@return 0 (completely inactive) to 1 (active)
		*/
		virtual float				GetActivityLevel() const {return IsHibernating() ? 0.f : (sleeping ? 0.5f : 1.f);}
		#ifdef HAS_WINDOW
			virtual float			GetSoundLevel() const {return GetActivityLevel()*0.35f;}
		#endif



		virtual int					GetConstructionCost() const;	//!< Retrieves the minerals required to construct a new instance of this structure

		/**
		Queries the entity height in layers. Most entities have height=1
		*/
		virtual int					GetHeight() const {return 1;}
		/**
		Queries if the local entity is broad (one lowres-cell wide), as opposed to just one highres-cell wide.
		Connectors are not broad, everything else (ATM) is broad.
		*/
		virtual bool				IsBroad() const {return true;}

		#ifndef NO_HIBERNATION
			/**
			Queries the bandwidth required for the local entity.
			Loosly evaluates the amount of attention a player must give to this type.
			Connectors are the least attention-grabbing type, which return 1
			*/
			virtual int					GetUplinkTraffic() const = 0;

			virtual const Hibernating*	ToHibernating() const {return nullptr;}
		#else
			bool					IsHibernating() const {return false;}
		#endif


		/**
		Spawns a new instance of the local type at the specified location. The method is called on a location that has previously passed CheckPlacement()
		*/
		virtual PWorldEntity		CreateNewInstance(const OwnerParameter&owner, const Grid::LCellIdI&location, float atTime) = 0;


		/**
		Signals that the local entity is being removed. Overrides of this method should always call the respective base method
		This method is called once the local entity has been unregistered from the object grid, entity lists, and faction.
		Structures should handle compound disconnections and struct count decreases.
		@param atTime Processing time stamp of the event, or 0 if outside server processing phases
		*/
		virtual void				OnDestroy(RemovalType, float&atTime)	{};	

		/**
		Signals that the local entity has been created.
		Structures should handle compound disconnections and struct count decreases.
		@param atTime Processing time stamp of the event, or 0 if outside server processing phases
		*/
		virtual void				OnConstructionFinished(float&atTime);

		/**
		Called exactly once on the prototype instance only, once instantiation of all prototype objects is completed (other prototypes may be references, but might not be set up yet).
		*/
		virtual void				PrototypeSetup()	{};


		virtual int					GetMaxHealth() const;
		virtual String				DescribeMaxHealth() const;
		/**
		Queries the current scanning radius of the local entity, including research and EMP effects.
		Currently this is overridden by MobileEntity, Vehicle, and Tower
		*/
		virtual float				GetScanningRange() const;
		virtual float3				GetScanningCenter(const Grid::LCellIdI&loc) const;
		/**
		Explicitly describes how the scanning range is computed (only has to explicitly name research, not EMP, but has to include EMP in the resulting numbers)
		*/
		virtual String				DescribeScanningRange() const;
		static float				StaticEMPModify(float raw, float min, int empDamageLevel);
		float						EMPModify(float raw, float min) const;
		std::function<float(float)>	BindEMPModifyFunction() const {return std::bind(&WorldEntity::EMPModify,this,std::placeholders::_1,2.f);}
		
		virtual PTurretBase			GetTurretBase(float &zOffset) { return PTurretBase();}
		inline PTurretBase			GetTurretBase() { float dummy; return GetTurretBase(dummy);}
		virtual PConstTurretBase	GetTurretBase(float &zOffset) const { return PConstTurretBase();}
		inline PConstTurretBase		GetTurretBase() const { float dummy; return GetTurretBase(dummy);}

		TurretTrajectory			TraceTrajectory(const TrajectoryEndPoint&dest, Ctr::Buffer0<TVec4<> >&outSteps, TVec3<>&impactAt, Grid::HCellIdI&impactCell, float&outAngle) const;
		TurretTrajectory			TraceTrajectory(const TrajectoryEndPoint&dest, TVec3<>&impactAt) const;
		TurretTrajectory			TraceTrajectoryFrom(const Grid::LCellIdI&source, const TrajectoryEndPoint&dest, Ctr::Buffer0<TVec4<> >&outSteps, TVec3<>&impactAt, Grid::HCellIdI&impactCell, float&outAngle) const;
		TurretTrajectory			TraceTrajectoryFrom(const Grid::LCellIdI&source, const TrajectoryEndPoint&dest, TVec3<>&impactAt) const;
		virtual bool				CanHitFrom(const Grid::LCellIdI&origin, const TrajectoryEndPoint&dest, float&outAngle) const;
		virtual bool				CanHitFrom(const Grid::LCellIdI&origin, const TrajectoryEndPoint&dest) const;
		bool						CanHit(const TrajectoryEndPoint&dest, float&outAngle) const;
		bool						CanHit(const TrajectoryEndPoint&dest) const;
		bool						IsSleeping() const {return sleeping;}

		virtual bool				AwaitsUserInteraction() const {return false;}


		#ifndef CLIENT
			virtual bool			DirectTo(CursorMode, Ctr::ArrayRef<Grid::LCellIdI> waypoints, bool detailedPath, bool leftClick = false) = 0;

			/**
			Forces all tasks to be dropped that do not result in resource loss. Other tasks must remain
			*/
			virtual void			ClearSafeTasks()	{};
		#endif


		//various type check methods. technically, a dynamic cast into the respective structure would be sufficient but i am led to believe that these are faster
		virtual bool				IsWorkingStructure() const {return false;}
		virtual bool				IsMobile() const {return false;}
		virtual bool				IsTower() const {return false;}

		virtual bool				IsUnlockedAtState(const Lua::Reference&researchState) const	{return GetTable()->isUnlocked.GetValue(researchState);}
		bool						IsUnlocked() const {return IsUnlockedAtState(GetResearchState());}
		virtual bool				IsVehicle() const {return false;}
		#ifndef NO_SATELLITES
			virtual bool			IsSatellite() const {return false;}
		#endif
		virtual bool				IsStructure() const {return false;}
		//virtual bool				IsVehiclePlant() const {return false;}
		//these are performance optimizations. Some visualizations may behave unexpectedly if they are not
		//overidden by classes implementing MobileEntityProducer
		//dynamic cast is not suitable for high frequency queries:
		//http://stackoverflow.com/questions/9959315/why-virtual-function-call-is-faster-than-dynamic-cast
		virtual const MobileEntityProducer*	CastToProducer() const {return nullptr;}


		/**
		Casts the local entity to an appropriate hangar structure if it can store the given mobile entity @a e
		If storage is not possible due to type constraints (NOT capacity constraints), the return value must be
		empty.
		*/
		virtual PHangar				CastToDestinationForType(const MobileEntity&e) {return PHangar();}
		virtual bool				IsHangar() const {return false;}

		virtual bool				IsIndestructible() const;

		/**
		Checks if the local entity is currently above the regular grid (e.g. a satellite)
		*/
		inline bool					IsAboveGrid() const	{return IsAboveGrid(location);}
		static inline bool			IsAboveGrid(const Grid::LCellIdI&location) {return location.height > Grid::CellStack::TopHeight;}

		/**
		Checks whether or not a new instance of the local class can be spawned within the world grid
		*/
		virtual bool				CanSpawnInGrid() const {return true;}
		/**
		Checks whether or not a new instance of the local class can be spawned above grid
		@param[out] outHeight Height that a new instance can be spawned at. Not updated if the method returns false
		@return true, if a new instance can be spawned above grid. @a outHeight will be updated to a valid non-grid height in this case
		*/
		virtual bool				CanSpawnAboveGrid(int&outHeight) const {return false;}

		/**
		Sentry entities automatically fill their attack queue
		*/
		virtual bool				IsSentry() const {return false;}

		#ifdef HAS_WINDOW
			virtual void			OnSelect()	{};
			virtual void			OnDeselect(){};
		#endif


		/**
		Iterates through all sensors of the local entity
		@param loc Location to use as entity location (the actual entity location is ignored)
		@param visitor Function to call
		@param appearance If true the method only returns sensors that represent its visual appearance
		*/
		virtual void			VisitSensors(const Grid::LCellIdI&loc, const std::function<void(const World::Sensor&)>&visitor, bool appearance = false) const;

		void					RegisterSensory(float&atTime) {registeredSensor = RegisterSensory(location,atTime);}
		void					UnregisterSensory(float&atTime)  {UnregisterSensory(registeredSensor,atTime);}
		bool					SensoryIsRegistered() const {return registeredSensor.IsSet();}

		/**
		Re-registers the current sensory with the faction.
		Assumes that grid sensory has been previously flushed
		*/
		void					ReapplySensory(float&atTime);
		TRegisteredSensor		RegisterSensory(const Grid::LCellIdI&loc, float&atTime, Ctr::Buffer0<PWorldEntity>*newContacts = nullptr);
		void					UnregisterSensory(TRegisteredSensor&, float&atTime);
		void					MoveSensoryToCurrentLocation(float&atTime, Ctr::Buffer0<PWorldEntity>*newContacts = nullptr);

		#ifdef HAS_WINDOW
			/**
			Stores a description of the local type in the specified label
			@param[out] label Label to store the description in
			@param deskFlags Combination of descFlag_t to fine-tune the generation process
			*/
			virtual void	DescribeType(Description&label, UINT32 descFlags);
		#endif
		void				AppendDeploymentCount(Description&outResult, const OwnerParameter&overrideOwner)	const;


		#ifdef CLIENT
			#ifdef HAS_WINDOW
				enum descFlag_t
				{
					DescIncludeName = 0x1,
					DescIncludePrice = 0x2,
					DescIncludeCurrentHealth = 0x4,
					DescIncludeMaxHealth = 0x8,
					DescIncludeFuelDemandModifier = 0x10,	//!< effective only for satellites
					DescIncludeUnitCount = 0x20,
				};
				//old: includeName, includePrice

				/**
				Appends deployment count in the form " Deployed: cnt/max", if any limit is set.
				Otherwise nothing will be appended.
				*/
				String				CompileDeploymentCount(const OwnerParameter&overrideOwner)	const;

				/**
				Visits each visual command the local entity provides
				@param visitor Callback function to call for every command
				@param force Set true to also visit commands that are currently hidden. False will visit only the currently selected command tree branch
				*/
				virtual void		VisitAllCommands(FCommandVisitor visitor, bool force) {};
				virtual bool		TestIntersection(const TVec3<>&b, const TVec3<>&d, float & distance) const=0;
			#endif
	
			/**
			Tests ray-intersection with the local structure
			*/
			void					UpdateEMPDamage(int damageLevel, bool isCreated=true);
		#else

			void					HandleEMPDamage(int damageLevel, float&atTime)
			{
				int newDamage = std::max(empDamageLevel,damageLevel);
				if (newDamage == empDamageLevel)
					return;
				UnregisterSensory(atTime);
				empDamageLevel = newDamage;
				RegisterChange(EMPDamageChanged,atTime);
				RegisterSensory(atTime);
			}
			/**
			Removes all present emp damage. Should generally only be done for nexus entities
			*/
			void					HealAllEMPDamage(float&atTime)
			{
				if (empDamageLevel > 0)
				{
					UnregisterSensory(atTime);
					empDamageLevel = 0;
					RegisterChange(EMPDamageChanged,atTime);
					RegisterSensory(atTime);
				}
			}
			void					HealEMPDamage(float&atTime)
			{
				if (empDamageLevel > 0)
				{
					UnregisterSensory(atTime);
					empDamageLevel--;
					RegisterChange(EMPDamageChanged,atTime);
					RegisterSensory(atTime);
				}
			}

		private:
			//static void				MapVisibilityBetween(const PWorldEntity&, const PWorldEntity&, float&atTime);

		#endif

		private:
			TRegisteredSensor		registeredSensor;

	};




	template <typename T>
		template <typename F1, typename F2>
			T	LuaAttributeCache<T>::GetEMPModified(const WorldEntity&e,const LuaAttribute<T,F1>&source,const LuaAttribute<T,F2>&bonus)
			{
				if (versionCounter != Super::versionCounter)
				{
					versionCounter = Super::versionCounter;
					T val = source.GetValue(e.GetResearchState());
					if (bonus.IsSet())
						val += bonus.GetValue(e.GetResearchState());
					cached = val;
				}
				return WorldEntity::StaticEMPModify(cached,2.f * Grid::UnitScale,e.empDamageLevel);;
			}
	template <typename T>
		template <typename F1, typename F2>
			String	LuaAttributeCache<T>::DescribeEMPModified(const WorldEntity&e,const LuaAttribute<T,F1>&source,const LuaAttribute<T,F2>&bonus)
			{
				if (bonus.IsSet())
				{
					String bonusStr = bonus.Describe(e.GetResearchState());
					if (bonusStr != "0")
					{
						T val = source.GetUnfilteredValue(e.GetResearchState())
							+ bonus.GetUnfilteredValue(e.GetResearchState());

						return String(val)+" [="+source.Describe(e.GetResearchState()) + "+"+bonusStr+" (bonus)]";
					}
				}
			
				return source.Describe(e.GetResearchState());
			}
	template <typename T>
		template <typename T2,typename F1, typename F2>
			T	LuaAttributeCache<T>::Get(const Lua::Reference&researchState, const LuaAttribute<T,F1>&source, const LuaAttribute<T2,F2>&multiplier, T minimum)
			{
				if (versionCounter != Super::versionCounter)
				{
					versionCounter = Super::versionCounter;
					T val = source.GetValue(researchState);
					if (multiplier.IsSet())
						val*=multiplier.GetValue(researchState);
					
					val = Math::Max(val, minimum);
					cached = val;
				}
				return cached;
			}


	PWorldEntity							GetEntity(const TUniqueID&id);

}



#endif

