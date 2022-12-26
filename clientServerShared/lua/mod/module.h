#pragma once
#include <global_root.h>
#include <objects/types.h>
#include <container/array.h>
#include <container/vectorList.h>
#include <lua/luaInterface.h>
//#include "../entityCommand.h"
#include <objects/components.h>
#include "../luaGameAPI.h"


namespace Lua
{
	DECLARE_POINTER_CLASS(Mod);


	class Mod
	{
	public:
		/**/	Mod()=delete;
		/**/	Mod(const PathString&gate, const ArrayRef<PathString>&includeRoots, const PathString&scriptPath, index_t&typeIdCounter);


		ArrayRef<Objects::Component::PTurret> GetAllTurrets() const
		{
			return turrets.ToRef();
		}
		
		ArrayRef<Objects::Component::PVehicleBase> GetAllVehicleBases() const
		{
			return vehicleBases.ToRef();
		}


		ArrayRef<Objects::PWorldEntity> GetAllPrototypes() const
		{
			return prototypes.ToRef();
		}

	private:
		Objects::PWorldEntity CreateLuaWorkingStructure(index_t typeId, const Lua::Table&table) const;
		void MakeImportHandler(const ArrayRef<PathString>&includeRoots, const PathString&gate, HashSet<PathString>&included);

		Vector<Objects::PWorldEntity> prototypes;
		Vector<Objects::Component::PTurret> turrets;
		Vector<Objects::Component::PVehicleBase> vehicleBases;

		Lua::FImportHandler importHandler;

		PProgram program;
		std::shared_ptr<API>		api;
	};


	class ModLibrary
	{
	public:
		void	AddGlobalInclude(const PathString&include)
		{
			if (!includes.Contains(include))
				includes.Append(include);
		}

		bool	AddMod(const PathString&pathGate, const PathString&scriptPath);


	private:
		Vector<PathString> includes;
		Vector<PMod>	mods;
		index_t			typeIdCounter = 0;
	};

	extern ModLibrary globalModLibrary;









}
