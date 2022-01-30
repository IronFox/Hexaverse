#include "luaTypes.h"
#include "../../objects/resource.h"
#include <objects/gridAndChoices.h>


namespace Lua
{

	void StripIcons(String&desc)
	{
		for (index_t i = 0; i < Objects::Resource::table.Count(); i++)
		{
			auto type = Objects::Resource::table[i];
			desc.FindAndReplaceIgnoreCase('%'+type.name+'%',type.GetTagStr());
		}


	}

	/**/	RootType::RootType(const PAPI&api, const Objects::EntityType&type, const String&name, const Lua::Table&table)
		:name(name),type(type),api(api),self(table)
	{
		height = table.RequireField<int>("height");
		isBroad = table.RequireField<bool>("isBroad");
		RequireFunction("Instantiate", instantiateFunction);
		destroyFunction = table.GetField("OnDestroy");
		onSelect = table.GetField("OnSelect");
		onDeselect = table.GetField("OnDeselect");
		getCommands = table.GetField("GetCommands");

		if (table.GetField("briefDescription",briefDescription))
			StripIcons(briefDescription);
		if (table.GetField("textDescription",textDescription))
			StripIcons(textDescription);



	}

	const Objects::CC* RootType::GetCommands(const Lua::Reference&instance, Objects::CC&dynamicStorage) const
	{
		#ifdef HAS_WINDOW	
			if (!getCommands)
				return nullptr;


			Lua::Table t = getCommands(instance);

			if (!t || t.GetLength() == 0)
				return nullptr;


			for (index_t i = 1; i <= t.GetLength(); i++)
			{
				Lua::Table cmdTable = t.GetElement(i);
				//Objects::Command::PBase cmd(new Objects::Command::Base());
				Lua::Function<void(Lua::Reference)> onClick = cmdTable.GetField("onClick");
				Lua::Function<Lua::Table(Lua::Reference)> getCost = cmdTable.GetField("getCost");
				Lua::Function<Lua::Table(Lua::Reference)> createLabel = cmdTable.GetField("createLabel");
				Lua::Function<int(Lua::Reference)> getStatus = cmdTable.GetField("getFlags");
				String cmdName = cmdTable.RequireField<String>("name");

				if (!getStatus)
					throw Lua::Exception(CLOCATION, name+ " Command '"+cmdName+"' does not define getFlags");


				ApplicationInterface::TCompileCommandArguments args;
				args.name = cmdName;
				args.customIconName = cmdTable.RequireField<String>("customIcon");
				args.onClick = [onClick, instance]() 
				{
					if (onClick)
						onClick(instance);
				};
				args.getCost = [getCost, instance,this,cmdName](int&costOut, Objects::Resource::ID&costTypeOut, int&timeCostOut)
				{
					if (getCost)
					{
						Lua::Table t = getCost(instance);

						costOut = t.RequireField<int>("resCount");
						auto resTypeName = t.RequireField<String>("resType");
						auto type = Objects::Resource::table.FindByName(resTypeName);
						if (!type)
							throw new Lua::Exception(CLOCATION,name+ " Command '"+cmdName+"' returned unsupported resource type '"+resTypeName+"'");
						costTypeOut = type->GetID();
						timeCostOut = t.RequireField<int>("opCount");
					}
					else
					{
						costOut = 0;
						timeCostOut = 0;
					}
				};
				args.getLabel = [createLabel, instance](Objects::Command::Base::Label&outResult)
				{
					if (createLabel)
					{
						Lua::Table t = createLabel(instance);
						String brief, extended;
						if (t.GetField("brief",brief) && brief.IsNotEmpty())
						{
							outResult.BeginBrief();
							Array<StringRef> lines;
							Explode('\n', brief, lines);
							foreach (lines, line)
								outResult << *line;
						}
						if (t.GetField("extended",extended) && extended.IsNotEmpty())
						{
							outResult.MarkBriefEnd();
							Array<StringRef> lines;
							Explode('\n', brief, lines);
							foreach (lines, line)
								outResult << *line;
						}

					}
				};
				args.getStatus = [getStatus, instance]()
				{
					return (Objects::Command::Base::Flags)getStatus(instance);
				};
			
				auto cmd = ApplicationInterface::CreateScriptedCommand(args);
				if (cmd)
					dynamicStorage.Add(cmd);
			}

			return &dynamicStorage;
		#else
			return nullptr;
		#endif
	}


	Lua::Reference RootType::Instantiate(const Objects::WorldEntity*entity) const
	{
		Lua::Reference rs;
		TryWithAPI([entity,this,&rs](const API& api)
		{
			auto unitTable = api.loadedEntityTableMap.Require(entity->id).first;
			auto result = instantiateFunction(self,Lua::nil,unitTable);
			unitTable.SetField("scripted",result);
			rs = result;
		});
		return rs;
	}



	

	/**/	StructureType::StructureType(const PAPI&api, const Objects::EntityType&type, const String&name, const Lua::Table&table):Super(api,type,name,table)
	{
		hasFoundation = table.RequireField<bool>("hasFoundation");

		String rawCategory = table.RequireField<String>("category");
		if (!category.Decode(rawCategory.c_str()))
		{
			StringBuffer builder;
			for (index_t i = 0; i < Objects::Structure::Category::N; i++)
			{
				if (i > 0)
					builder << ",";
				builder << Objects::Structure::Category::Reinterpret(i).GetName();
			}

			throw Lua::Exception(CLOCATION,"Structure category '"+rawCategory+"' does not exist. Supported categories are "+builder.ToStringRef());
		}

		onLostNeighbor = table.GetField("OnLostNeighbor");
		onNewConnection = table.GetField("OnNewConnection");
		onConnectionLost = table.GetField("OnConnectionLost");
		onEnvironmentChange = table.GetField("OnEnvironmentChange");
		connectionPossibleTo = table.GetField("ConnectionPossibleTo");

		#ifdef HAS_WINDOW
		//LoadVisualStructureHull(const String&geometryName, Objects::Geometry&staticGeometry, PClientAttachment&staticShape);
			ApplicationInterface::LoadVisualStructureHull(table.RequireField<String>("meshName"),geometry,shape);
		#endif

	}



	void StructureType::OnLostNeighbor(const Lua::Reference&instance, const Objects::PStructure&s) const
	{
		if (onLostNeighbor)
			TryWithAPI([&instance,&s,this](const API&api)
			{
				Lua::Table other = api.loadedEntityTableMap.Require(s->id).first;
				onLostNeighbor(instance,other);
			});
	}

	void StructureType::OnNewConnection(const Lua::Reference&instance, const Objects::PStructure&s, UINT32 acrossHighCell) const
	{
		if (onNewConnection)
			TryWithAPI([&instance,&s,this,acrossHighCell](const API&api)
			{
				Lua::Table other = api.loadedEntityTableMap.Require(s->id).first;
				Lua::Table cell = api.highCellGrid[acrossHighCell];
				onNewConnection(instance,other,cell);
			});
	}

	void StructureType::OnConnectionLost(const Lua::Reference&instance, const Objects::PStructure&s, UINT32 acrossHighCell) const
	{
		if (onConnectionLost)
			TryWithAPI([&instance,&s,this,acrossHighCell](const API&api)
			{
				Lua::Table other = api.loadedEntityTableMap.Require(s->id).first;
				Lua::Table cell = api.highCellGrid[acrossHighCell];
				onConnectionLost(instance,other,cell);
			});

	}

	bool StructureType::ConnectionPossibleTo(const Lua::Reference&instance, const Objects::PStructure&s, UINT32 acrossHighCell) const
	{
		bool possible = true;
		if (connectionPossibleTo)
			TryWithAPI([&possible,&instance,&s,this,acrossHighCell](const API&api)
			{
				Lua::Table other = api.loadedEntityTableMap.Require(s->id).first;
				Lua::Table cell = api.highCellGrid[acrossHighCell];
				possible = connectionPossibleTo(instance,other,cell);
			});
		return possible;
	}

	void StructureType::OnEnvironmentChange(const Lua::Reference&instance, const Objects::TStructureNeighborhood&neighborhood) const
	{
		if (onEnvironmentChange)
			TryWithAPI([&instance,&neighborhood,this](const API&api)
			{
				auto nTable = api.program->CreateTable();
				for (const auto&s : neighborhood)
				{
					Lua::Table other = api.loadedEntityTableMap.Require(s->id).first;
					nTable.AppendElement(other);
				}
				onEnvironmentChange(instance,nTable);
			});

	}




	bool WorkingStructureType::CanProduce(const Lua::Reference&ref, const Objects::PWorldEntity&e) const
	{
		if (!isMobileEntityProducer)
			return false;
		bool rs = false;
		TryWithAPI([ref,e,this,&rs](const API& api)
		{
			Lua::Table typeTable;
			if (!api.typeTableMap2.Query(e->GetType(),typeTable))
				throw Lua::Exception(CLOCATION,"Executing Lua::WorkingStructureType::CanProduct() on scripted type '"+name+"' but type "+e->GetTypeName()+" is not mapped by API");
			rs = canProduce(ref,typeTable);
		});
		return rs;
	}


	void WorkingStructureType::GetResourceVector(const Lua::Reference&instance, Objects::Resource::Vector&balance, Objects::Resource::Vector&storageCurrent, Objects::Resource::Vector&storageMax) const
	{
		TryWithAPI([this,&instance,&balance,&storageCurrent,&storageMax](const API&api)
		{
			Lua::Table lBalance = api.program->CreateTable();
			Lua::Table lStorageCurrent = api.program->CreateTable();
			Lua::Table lStorageMax = api.program->CreateTable();

			API::ResourceVectorToLua(balance,lBalance);
			API::ResourceVectorToLua(storageCurrent,lStorageCurrent);
			API::ResourceVectorToLua(storageMax,lStorageMax);

			getResourceVector(instance,lBalance,lStorageCurrent,lStorageMax);

			API::LuaToResourceVector(lBalance,balance);
			API::LuaToResourceVector(lStorageCurrent,storageCurrent);
			API::LuaToResourceVector(lStorageMax,storageMax);
		});
	}

	void WorkingStructureType::SetResourceStorage(const Lua::Reference&instance, const Objects::Resource::Vector&vec) const
	{
		TryWithAPI([this,&instance,&vec](const API&api)
		{
			Lua::Table lVec = api.program->CreateTable();

			API::ResourceVectorToLua(vec,lVec);

			setResourceStorage(instance,lVec);
		});
	}



}