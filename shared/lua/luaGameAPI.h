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
							researchTable,	//Game.Research.X
							terrainTable, //Game.Terrain.X
							terrainContentTable,	//Game.Terrain.Content.X
							gridTable;	//Game.Grid.X
		Lua::Table			entityIDMap,
							factionIDMap,
							allianceIDMap,
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

		struct TFaction
		{
			Lua::Table		table;
			Lua::Table		entityTable, storageTable, balanceTable, demandTable, supplyTable, maxStorageTable;

			void			AddEntity(const Lua::Table&);
		};


		struct TSharedHangar
		{
			Objects::WHangar	hangar;
			Lua::Table			sharedDeployables;
			bool				rebuild = true;
		};


		typedef Lua::Function<void(const Lua::Table&)>	LFOnDeploy;

		HashTable<Grid::LCellIdI,Lua::Table>	locationMap;
		HashTable<TUniqueID,LFOnDeploy>			onDeployedCallbacks;
		Timer::Time								lastDeploymentCallbackRegistered;
		Buffer0<TSharedHangar>					sharedHangars;

		HashTable<TUniqueID,TAlliance>			loadedAllianceTableMap;
		HashTable<TUniqueID,TFaction>			loadedFactionTableMap;
		HashTable<TUniqueID,std::pair<Lua::Table,bool> >	loadedEntityTableMap;
		HashTable<TUniqueID,Objects::WWorldEntity>	entityMap;
		HashSet<TUniqueID>	entityMappedToTable, factionMappedToTable, allianceMappedToTable;
		Objects::Pathfinding::ShortestPath	shortestPath;

		//feedback functions:
		std::function<void(const TUniqueID&)>	onMarkerTouch,
												onMarkerDestroy, 
												onRemoveEntity,
												onDropAllTasks,
												onDropLastTask
												;
		std::function<void(const TUniqueID&, bool)>	onEnableWorkingStructure;
		std::function<void(const TUniqueID&mID,Objects::MarkerType,const Objects::PWorldEntity&, const Grid::LCellIdI&)> onMarkerCreate;
		std::function<void(const TUniqueID&, Array<Grid::LCellIdI>&)> onAttackTargets;	//!< Given array is fleeting
		std::function<void(const TUniqueID&, const ArrayRef<Grid::LCellIdI>&)> onDetailedDirectTo;
		std::function<void(const TUniqueID&, const Objects::EntityType&)>	onBeginProduction, onBeginStructureConstruction;
		std::function<void(const TUniqueID&producer, const TUniqueID&task)>	onAbortProduction;
		std::function<void(const TUniqueID&producer, const Grid::LCellIdI&, const TUniqueID&deploymentID)>	onDeployCompletedProduction;
		std::function<void(const TUniqueID&hangar, const TUniqueID&mobile, const Grid::LCellIdI&location, const TUniqueID&deploymentID)> onDeployStored;
		std::function<void(index_t,bool)>	onQueueResearch;

		/**/		API():mapper(true)	{}

		void		Create(const Lua::PProgram&);
		void		Create(const String&programNameToCreate);
		void		Destroy();
		bool		IsCreated() const {return program != nullptr;}

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
		void		AddObjectAndPathfindingFunctions();
		#ifdef CLIENT
			void	AddMyMarker(const Objects::Marker&m);
			void	AddMyMarkers();
			void	SetSelectedUnit(const Objects::PWorldEntity&e, bool updateIfFound);
		#endif
		void		UpdateResearch();

		Lua::Table ToTable(const Grid::LCellIdI&cell);
		Lua::Table TurretToTable(const Objects::TurretBase&b, float zOffset, const Objects::Research::Attributes::Table&researchState);
		Lua::Table ToTable(const Objects::MobileEntity::Task&task);
		Objects::PVehicle	ParameterToVehicle(const Lua::Table&luaEntity, Grid::LCellIdI&outLoc, const String&parameterName) const;

		void		AddFactionEntities(const Objects::PFaction&);
		void		AddAllEntities();
		void		AddAllFactionsAndAlliances();
		void		AddAllAlliances();
		void		SignalDeployment(const TUniqueID&deploymentID, const Objects::PWorldEntity&e);
	private:

		Lua::Table	LuaFindShortestPathFrom(const Objects::PMobileEntity&v, const Grid::LCellIdI&origin, const Grid::LCellIdI&dest, const Lua::Function<int(Lua::Table, int)>&costModifier, bool ignoreFriendlyMobilesOnRoute);
		Lua::Table	LuaTraceTrajectoryFrom(const Objects::TrajectoryEndPoint&origin, const Objects::TrajectoryEndPoint&target, Objects::TrajectoryType trajectory, float flatAttackRange, const Objects::WFaction&owner);
		Lua::Table	LuaFindHighestAvailableResourceLocation(const String&resourceType);
		Objects::PWorldEntity LuaUnitToEntity(const Lua::Table&luaUnit, const String&funcParameterName)	const;

		void	Setup();
	};
}

