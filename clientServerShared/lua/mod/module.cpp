#include "module.h"
#include <io/file_system.h>
#include "luaTypes.h"
#include "../../runtimeConfig.h"
#include <objects/prototypes.h>
#include <objects/structure/tower.h>

using namespace DeltaWorks;

namespace Lua
{
	ModLibrary globalModLibrary;


	bool ModLibrary::AddMod(const PathString&pathGate, const PathString&scriptPath)
	{
		try
		{
			LogEvent(EventClass::Preset::scriptLog,"Loading mod '"+FileSystem::PathToString(scriptPath)+"' ...");
			PMod mod(new Mod(pathGate,includes.ToRef(),scriptPath,typeIdCounter));
			mods.Append(mod);
			LogEvent(EventClass::Preset::scriptLog,"Loaded mod '"+FileSystem::PathToString(scriptPath)+"'. No errors");
			return true;
		}
		catch (const std::exception&ex)
		{
			LogEvent(EventClass::Preset::scriptError,ex.what());
			return false;
		}
	}



	Objects::PWorldEntity CreateLuaType(const Lua::Table&table, const std::function<Objects::PWorldEntity(const String&)>&nested)
	{
		String typeName;

		try
		{
			typeName = table.RequireField<String>("typeName");

		}
		catch (const Except::Exception&ex)
		{
			throw Lua::Exception(ex.hasLocation ? ex.location : CLOCATION, "Unable to create working structure: "+String(ex.what()));
		}
		catch (...)
		{
			throw Lua::Exception(CLOCATION, "Unable to create working structure: 'typeName' not set in table");
		}


		try
		{
			return nested(typeName);
			



		}
		catch (const Except::Exception&ex)
		{
			throw Lua::Exception(ex.hasLocation ? ex.location : CLOCATION,"Unable to create working structure type '"+typeName+"': "+String(ex.what()));
		}
		catch (const std::exception&ex)
		{
			throw Lua::Exception(CLOCATION,"Unable to create working structure type '"+typeName+"': "+String(ex.what()));
		}
		catch (...)
		{
			throw Lua::Exception(CLOCATION,"Unable to create working structure type '"+typeName+"': Unspecified error");
		}



	}



	Objects::PWorldEntity Mod::CreateLuaWorkingStructure(index_t typeId, const Lua::Table&table) const
	{
		return CreateLuaType(table,[&table,typeId,this](const String&typeName)
		{
			return Objects::PWorldEntity(new WorkingStructure(PWorkingStructureType(new WorkingStructureType(api,Objects::EntityType(Objects::TTypeComponent(ConstName::Type_Mod,typeId)),  typeName,table))));
		});
	}

	Mod::Mod(const PathString&gate, const ArrayRef<PathString>&includeRoots, const PathString&scriptPath, index_t&typeIdCounter)
	{
		FileSystem::File file;

		if (!FileSystem::Folder().FindFile(scriptPath,file))
			throw Lua::Exception(CLOCATION,"Mod file not found: "+FileSystem::PathToString(scriptPath));

		FileSystem::Folder folder = file.GetFolder();

		program = ::Lua::Program::Create("(Mod) "+FileSystem::PathToString(FileSystem::ExtractFileName(scriptPath)));

		api.reset(new API());
		api->Create(program);
		api->UpdateResearch();
		api->AddAllFactionsAndAlliances();
		api->AddAllEntities();
		api->AddMyMarkers();
		api->AddObjectAndPathfindingFunctions(false);


		auto mod = program->SetGlobalTable("Module");
		mod.SetFunction("DeclareWorkingStructure",[this,&typeIdCounter](const Lua::Table&typeTable)
		{
			prototypes.Append(CreateLuaWorkingStructure(typeIdCounter++,typeTable));
		});


		HashSet<PathString> included;

		std::function<void(const PathString&,bool)> importHandler;

		importHandler = [&includeRoots, &included,folder,gate,this,&importHandler](const PathString&include, bool isGlobal)
		{
			FileSystem::File f;
			bool found = false;
			if (isGlobal)
			{
				foreach (includeRoots, root)
				{
					if (FileSystem::Folder(*root).FindFile(include,f))
					{
						found = true;
						break;
					}
				}
			}
			else
			{
				found = folder.FindFile(include,f);
				if (found && !f.GetLocation().BeginsWith(gate))
					throw Lua::Exception("Targeted relative include path '"+FileSystem::PathToString(include)+"' is not allowed");
			}

			if (!found)
				throw Lua::Exception("Unable to find include path '"+FileSystem::PathToString(include)+"'");
			if (included.SetNew(f.GetLocation()))
				program->ExecuteFile(f.GetLocation(),importHandler);
		};

		program->ExecuteFile(scriptPath,importHandler);



		foreach (prototypes, p)
		{
			if ((*p)->IsStructure())
			{
				if (!(*p)->IsTower())
					Objects::Prototypes::structures << To<Objects::Structure>::Cast(*p);
				else
				{
					throw Lua::Exception(CLOCATION,"Not supporting mod-driven towers for now");
				}
			}
			else if ((*p)->IsPlane())
				Objects::Prototypes::planes << To<Objects::SubOrbital::Plane>::Cast(*p);
			else
				throw Lua::Exception(CLOCATION,"Created entity prototype "+(*p)->GetTypeName()+" is neither structure not plane");
			Objects::Prototypes::all << *p;
		}


		foreach (turrets, t)
		{
			if ((*t)->mountType == Objects::Component::Turret::MountType::Vehicle)
				Objects::Component::vehicleTurretComponents.Add(t->get());
			else if ((*t)->mountType == Objects::Component::Turret::MountType::TowerOnly)
			{
				Objects::Tower::PProto proto(new Objects::Tower::Prototype(t->get()));
				Objects::Tower::globalPrototypes.Append(proto);
				Objects::Prototypes::structures << proto->instance;
				Objects::Prototypes::all << proto->instance;
			}

			Objects::Component::allTurretComponents.Add(t->get());
			Objects::Component::allComponents.Add(t->get());
		}


		foreach (vehicleBases, t)
		{
			Objects::Component::baseComponents.Add(t->get());
			Objects::Component::allComponents.Add(t->get());
		}

	}


}

