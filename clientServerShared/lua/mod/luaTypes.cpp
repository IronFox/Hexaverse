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

	/**/	RootType::RootType(const PAPI&api, const Objects::EntityType&type, const String&name, const Lua::Table&table):name(name),type(type),api(api),self(table)
	{
		height = table.RequireField<int>("height");
		isBroad = table.RequireField<bool>("isBroad");
		RequireFunction("Instantiate", instantiateFunction);
		destroyFunction = table.GetField("OnDestroy");
		onSelect = table.GetField("OnSelect");
		onDeselect = table.GetField("OnDeselect");

		if (table.GetField("briefDescription",briefDescription))
			StripIcons(briefDescription);
		if (table.GetField("textDescription",textDescription))
			StripIcons(textDescription);
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