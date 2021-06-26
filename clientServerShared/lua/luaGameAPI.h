#pragma once

#include "luaInterface.h"
#include "objects/types.h"
#include <container/hashtable.h>
#include "../uniqueID.h"
#include "../grid/stack.h"
//#include "../objects/research.h"
#include "../objects/types.h"
#include "../objects/mobileEntity.h"
#include "../objects/pathfinding.h"
#include <functional>
#include "../objects/entityType.h"
#include "../objects/nexus/nexusThreatEstimation.h"

namespace Lua
{

	/**
	Centralized management structure for Lua script execution.
	Provides tables and functions as requested by invoking the Add*() methods.

	On thread-safety:
	This structure is designed to be set up by the main thread.
	Creation or invokation of Add*() methods by any other non-synchronized thread
	will result in unexpected behavior or crashes.

	Execution of the script itself is mostly thread-safe.
	Provided functions guarantee the following conditions during script runtime:
		*	No access to GetEntity(id) or the likes (since these lists are not thread-safe)
		*	Local reference to entities via weak pointers. If they have expired the respective
			Lua call will fail

	Possibly inconsistent but assumed crash-free operations:
		*	Read-only access to entity grids GetEntity(location) or the likes
		*	Read-only access to terrain grid
		*	Read-only access to resource distribution

	If execution is performed by separate thread, it is mandatory that not external
	manipulation occurs during runtime of this thread, as Lua itself is not designed to be
	thread-safe.
	*/
	struct API
	{
		Lua::PProgram		program;
		Lua::Table			gameTable,//Game.X
							aiTable,	//Game.AI.X
							markTable,	//Game.DebugMarkers.X
							researchTable,	//Game.Research.X
							typeTable,		//Game.Type.X
							terrainTable, //Game.Terrain.X
							terrainContentTable,	//Game.Terrain.Content.X
							gridTable;	//Game.Grid.X
		Lua::Table			entityIDMap,
							factionIDMap,
							allianceIDMap,
							allTypes,
							unitTypes,
							turretTypes,
							vbaseTypes,
							researchTopicList;
		
		#ifdef CLIENT	
			Lua::Table		myEntityIDMap,
							alliedEntityIDMap,
							hostileEntityIDMap,
							myMarkerIDMap,
							myResearchQueue;
		#endif
		Array<Lua::Table>	lowCellGrid,
							highCellGrid;


		Objects::Pathfinding::ReachabilityMapper	mapper;


		struct TAlliance
		{
			Lua::Table		table;
			Lua::Table		factionTable;
		};

		struct TFullResourceBalance
		{
			Lua::Table		table, storageTable, balanceTable, demandTable, supplyTable;

			void			AllocateIn(Lua::Table&parent, const String&name);
		};

		struct TFaction
		{
			Lua::Table		table;
			Lua::Table		entityTable;
			TFullResourceBalance	current,max;

			void			AddEntity(const Lua::Table&);
		};


		struct TSharedHangar
		{
			Objects::WHangar	hangar;
			Lua::Table			sharedDeployables;
			bool				rebuild = true;
		};


		typedef Lua::Function<void(const Lua::Table&)>	LFOnDeploy;

		TwisterRandom							random = TwisterRandom(time(NULL) + Time::Stamp().ToTicks());
		Random									simpleRandom = Random(time(NULL) + Time::Stamp().ToTicks());

		HashTable<Grid::LCellIdI,Lua::Table>	locationMap;
		HashTable<TUniqueID,LFOnDeploy>			onDeployedCallbacks;
		Time::Stamp								lastDeploymentCallbackRegistered;
		Buffer0<TSharedHangar>					sharedHangars;

		HashTable<TUniqueID,TAlliance>			loadedAllianceTableMap;
		HashTable<TUniqueID,TFaction>			loadedFactionTableMap;
		HashTable<TUniqueID,std::pair<Lua::Table,bool> >	loadedEntityTableMap;
		HashTable<TUniqueID,Objects::WWorldEntity>	entityMap;
		HashTable<String,Lua::Table>				typeTableMap;
		HashTable<Objects::EntityType,Lua::Table>	typeTableMap2;
		HashSet<TUniqueID>						entityMappedToTable, 
												factionMappedToTable, 
												allianceMappedToTable;
		Objects::Pathfinding::ShortestPath		shortestPath;
		bool									myFactionIsSet = false;

		//feedback functions:
		std::function<void(const TUniqueID&)>	onMarkerTouch,
												onMarkerDestroy, 
												onRemoveEntity,
												onDropAllTasks,
												onDropSafeTasks,
												onDropLastTask
												;
		std::function<void()>					onRequireGridSensory;
		std::function<index_t(const Grid::LCellIdI&, const M::TVec3<>&color, const String&msg, bool seeThrough)>	onVisualMarkerCreate;
		std::function<void(index_t)>			onVisualMarkerDelete;
		std::function<void()>					onVisualMarkerFlush;

		std::function<void(const TUniqueID&, bool)>	onEnableWorkingStructure,
													onUpdateMobileAutomation,
													onUpdateSleep;
		std::function<void(const TUniqueID&mID,Objects::MarkerType,const Objects::PWorldEntity&, const Grid::LCellIdI&)> onMarkerCreate;
		std::function<void(const TUniqueID&, Array<Grid::LCellIdI>&)> onAttackTargets;	//!< Given array is fleeting
		std::function<void(const TUniqueID&, const ArrayRef<Grid::LCellIdI>&, Objects::CursorMode)> onDetailedDirectTo;
		std::function<void(const TUniqueID&, const Objects::EntityType&)>	onBeginProduction, onBeginStructureConstruction;
		std::function<void(const TUniqueID&producer, const TUniqueID&task)>	onAbortProduction;
		std::function<void(const TUniqueID&eId, const TUniqueID&deploymentedID, const Grid::LCellIdI&loc, const TUniqueID&referenceID, Objects::CursorMode mode)> onChoiceIntersection;
		std::function<void(index_t,bool)>	onQueueResearch;
		std::function<void(const TUniqueID&, ConstName, ConstName)>	onSimpleJobStart;
		std::function<void(const TUniqueID&, Objects::CursorMode, const ArrayRef<Grid::HCellIdI>&)> onGridBatchJobStart; 

		struct TExportResult
		{
			CONSTRUCT_ENUMERATION5(ErrorType,NoError,TooLarge,TableLoop,NotSupported,Other);

			ErrorType	error;
			String		errorExt;
			size_t		serialSize=0, maxSerialSize=1;
		};

		std::function<TExportResult(const Lua::Reference&)>		onCheckExportValidity;

		/**/		API():mapper(true)	{}

		void		Create(const Lua::PProgram&);
		void		Create(const String&programNameToCreate);
		void		Destroy();
		bool		IsCreated() const {return program != nullptr;}

		/**
		Adds members to Game.Type.X table, including construction functions
		*/
		void		AddTypes();

		/**
		Adds the regular unit (low) grid cells, and functions
		*/
		void		AddGrid();

		/**
		Adds the high resolution terrain grid cells, and functions
		*/
		void		AddTerrain();

		/**
		Clears Game.units, Game.myUnits, Game.myMarkers, Game.factions, Game.alliances, Game.selectedUnit, Game.Research.topics, Game.Research.myQueue, Game.myFaction
		Unless clearCaches is set, locally cached Lua tables are left unchanged, and will be reused if an entity, faction, marker, or alliance is re-added.
		Potentially script-added variables are preserved this way
		@param clearCaches If set true, re-added entities will receive new Lua tables, thus flushing any script-added values. Table member variables will be updated either way
		*/
		void		FlushAllObjects(bool clearCaches=false);
		void		AddEntity(const Objects::PWorldEntity&e);
		Lua::Table	MapEntity(const Objects::PWorldEntity&e, bool isReal, bool updateIfFound);
		TFaction*	AddFaction(const Objects::PFaction&f, bool updateIfFound);
		TAlliance*	AddAlliance(const Objects::PAlliance&a, bool updateIfFound, bool updateMembers);
		void		AddObjectAndPathfindingFunctions(bool priveleged);
		#ifdef CLIENT
			void	AddMyMarker(const Objects::PMarker&m);
			void	AddMyMarkers();
			void	SetSelectedUnit(const Objects::PWorldEntity&e, bool updateIfFound);
		#endif
		void		UpdateResearch();

		Lua::Table ToTable(const Grid::LCellIdI&cell);
		Lua::Table TurretToTable(const Objects::TurretBase&b, float zOffset, const Objects::Research::Attributes::Table&researchState);
		Lua::Table ToTable(const Objects::MobileEntity::Task&task, const Objects::PMobileEntity&entity, bool isFirst);

		Objects::PVehicle	ParameterToVehicle(const Lua::Table&luaEntity, Grid::LCellIdI&outLoc, const String&parameterName) const;

		void		AddFactionEntities(const Objects::PFaction&);
		void		AddAllEntities();
		void		AddAllFactionsAndAlliances();
		void		AddAllAlliances();
		void		SignalDeployment(const TUniqueID&deploymentID, const Objects::PWorldEntity&e);
	private:
		Lua::Table	CreateThreatTable(Objects::NexusThreat::TLevel);
		Lua::Table	CreateSupportTable(Objects::NexusThreat::TSupport);

		Lua::Table	LuaFindShortestPathFrom(const Objects::PMobileEntity&v, const Grid::LCellIdI&origin, const Grid::LCellIdI&dest, const Lua::Function<int(Lua::Table, int)>&costModifier, bool ignoreFriendlyMobilesOnRoute);
		Lua::Table	LuaTraceTrajectoryFrom(const Objects::TrajectoryEndPoint&origin, const Objects::TrajectoryEndPoint&target, Objects::TrajectoryType trajectory, float flatAttackRange, const Objects::WFaction&owner);
		Lua::Table	LuaFindHighestAvailableResourceLocation(const String&resourceType);
		Lua::Table	LuaFindHighestAvailableResourceLocationExt(const String&resourceType,bool checkLinkPath, bool checkMobiles);
		Objects::PWorldEntity LuaUnitToEntity(const Lua::Table&luaUnit, const String&funcParameterName)	const;
		Lua::Table	LuaGetReachFrom(const Objects::PMobileEntity&m,const Grid::LCellIdI&origin,
									const Lua::Function<bool(Lua::Table)>&isBlocked, 
									int maxReach, bool forceReversible, bool allowAccessibleHangars, bool ignoreOwnedVehiclesOnRoute, 
									bool cannotStandOnUnknown);


		Lua::Table	LuaMapType(const String&typeName);

		Lua::Table	AddOrGetType(Objects::EntityType type);
		Lua::Table	AddType(Objects::EntityType t);
		void		RefreshType(Lua::Table&t, Objects::EntityType type);
		void		AddSimpleJob(Lua::Table&t, const TUniqueID&id, const String&name, ConstName jobCName, ConstName parameterCName=ConstName::Empty);
		void		AddGridBatchJob(Lua::Table&t, const TUniqueID&id, const String&name, Objects::CursorMode cursorMode);
		void		AddRoutedTerraformingJob(Lua::Table&t, const TUniqueID&id, const String&name, Objects::CursorMode cursorMode);
		void		SetResearchProperties(Lua::Table&t, const Objects::Component::Base*component);
		void		RefreshTypes();
		void		RefreshType(Objects::EntityType t);


		void	Setup();
	};
}

