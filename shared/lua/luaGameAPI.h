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
	struct API
	{
		Lua::PProgram		program;
		Lua::Table			gameTable,//Game.X
							pathfindingTable, //Game.Pathfinding.X
							gridTable;	//Game.Grid.X
		Lua::Table			entityIDMap,
							factionIDMap,
							allianceIDMap;
		#ifdef CLIENT	
			Lua::Table		myEntityIDMap,
							myMarkerIDMap,
							myResearch;
		#endif
		Array<Lua::Table>	lowCellGrid;


		Objects::Pathfinding::ReachabilityMapper	mapper;


		struct TAlliance
		{
			Lua::Table		table;
			Lua::Table		factionTable;
		};

		struct TFaction
		{
			Lua::Table		table;
			Lua::Table		entityTable;

			void			AddEntity(const Lua::Table&);
		};

		HashTable<Grid::LCellIdI,Lua::Table>	locationMap;

		HashTable<TUniqueID,TAlliance>	loadedAllianceTableMap;
		HashTable<TUniqueID,TFaction>	loadedFactionTableMap;
		HashTable<TUniqueID,std::pair<Lua::Table,bool> >	loadedEntityTableMap;
		HashTable<TUniqueID,Objects::WWorldEntity>	entityMap;
		HashSet<TUniqueID>	entityMappedToTable, factionMappedToTable, allianceMappedToTable;
		Objects::Pathfinding::ShortestPath	shortestPath;

		//feedback functions:
		std::function<void(const TUniqueID&)>	onMarkerTouch,onMarkerDestroy, onRemove,onDropAllTasks;
		std::function<void(const TUniqueID&, bool)>	onEnableWorkingStructure;
		std::function<void(const TUniqueID&mID,Objects::MarkerType,const TUniqueID&eID, const Grid::LCellIdI&)> onMarkerCreate;
		std::function<void(const TUniqueID&, Array<Grid::LCellIdI>&)> onAttackTargets;	//!< Given array is fleeting
		std::function<void(const TUniqueID&, const ArrayRef<Grid::LCellIdI>&)> onDetailedDirectTo;

		/**/		API():mapper(true)	{}

		void		Create(const Lua::PProgram&);
		void		Create(const String&programNameToCreate);
		void		Destroy();
		bool		IsCreated() const {return program != nullptr;}


		void		AddGrid();
		/**
		Clears Game.units, Game.myUnits, Game.myMarkers, Game.factions, Game.alliances, Game.selectedUnit, Game.myResearch, Game.myCurrentResearch
		Unless clearCaches is set, locally cached Lua tables are left unchanged, and will be reused if an entity, faction, marker, or alliance is re-added.
		Potentially script-added variables are preserved this way
		@param clearCaches If set true, re-added entities will receive new Lua tables, thus flushing any script-added values. Table member variables will be updated either way
		*/
		void		FlushAllObjects(bool clearCaches=false);
		void		AddEntity(const Objects::PWorldEntity&e);
		Lua::Table	MapEntity(const Objects::PWorldEntity&e, bool isReal, bool updateIfFound);
		TFaction*	AddFaction(const Objects::PFaction&f, bool updateIfFound);
		TAlliance*	AddAlliance(const Objects::PAlliance&a, bool updateIfFound);
		#ifdef CLIENT
			void	AddMyMarker(const Objects::Marker&m);
			void	SetSelectedUnit(const Objects::PWorldEntity&e, bool updateIfFound);
			void	UpdateMyResearch();
		#endif

		Lua::Table ToTable(const Grid::LCellIdI&cell);
		Lua::Table TurretToTable(const Objects::TurretBase&b, float zOffset, const Objects::Research::Attributes::Table&researchState);
		Lua::Table ToTable(const Objects::MobileEntity::Task&task);
		Objects::PVehicle	ParameterToVehicle(const Lua::Table&luaEntity, Grid::LCellIdI&outLoc, const String&parameterName) const;

		void	AddFactionEntities(const Objects::PFaction&);
		void	AddAllEntities();

	private:
		void	Setup();
	};
}

