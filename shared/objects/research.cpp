#include <global_root.h>
#include "research.h"
#include "prototypes.h"
#include "structure.h"
#include "../logEvent.h"

namespace Objects
{



	/*static*/ ResearchState::GlobalState		ResearchState::globalState;


	PResearchTopic	ResearchState::CreateResearch(const PResearchTopic&parent, const String&name, const String&iconName, 
												const ResearchTopic::FAdvance&adv, 
												const ResearchTopic::FGetProgress&currentProgress, int maxProgress,
												const PResearchTopic&dependency
											)
	{
		LogEvent(EventClass::Preset::important,"Lua created new research '"+name+"'");

		const String&iconSource = iconName.IsEmpty() ? name : iconName;

		PWorldEntity proto = GetNonVehiclePrototypeByName(iconSource);
		Component::Base*component = proto ? nullptr : Component::GetByName(iconSource);
		bool hasIcon = !proto && !component;


		PResearchTopic rs(new ResearchTopic(parent, name,hasIcon ? iconSource : "",adv,currentProgress,maxProgress,dependency));

		rs->serialIndex = globalState.allTopics.Count();
		globalState.allTopics << rs;


		if (!parent)
			globalState.rootResearch << rs;
		else
		{
			parent->children << rs;
			if (parent->maxProgress > 0)
			{
				throw Lua::Exception("Trying to convert regular research '"+parent->name+"' to group when adding '"+name+"' as child. '"+parent->name+"'.maxProgress must be 0 when used as a group");
			}
		}
		return rs;
	}

	ResearchTopic::ResearchTopic(const PResearchTopic&parent, const String&name, const String&iconName, 
								const FAdvance&adv, 
								const FGetProgress&currentProgress, int maxProgress,
								const PResearchTopic&dependency
								)
								:parent(parent),name(name),iconName(iconName),
								advance(adv),currentProgress(currentProgress),maxProgress(maxProgress),
								dependency(dependency)
	{}



	ResearchTopic::ResearchTopic::~ResearchTopic()
	{
		bool brk = true;

	}




	static Array<BaseAttributeTable>	structureTables;


	template <typename T>
		static void InitAttrib(const Lua::Table&source, const String&path, const String&name, LuaAttribute<T>&a, bool allowEmpty = false)
		{
			Lua::Reference ref = source.GetField(name);
			if (ref.IsEmpty())
			{
				if (!allowEmpty)
					FATAL__("research.lua does not define "+path+"."+name);
				return;
			}

			if (ref.IsTable())
			{
				Lua::Table t = source.GetField(name);
			
				a.luaGetValue = t.GetField("Get");
				if (a.luaGetValue.IsEmpty())
					FATAL__("research.lua does not define "+path+"."+name+".Get");

				a.luaDescribeValue = t.GetField("Describe");
			}
			elif (ref.IsFunction())
			{
				a.luaGetValue = ref;
			}
			else
			{
				a.SetConst(ref.GetAs<T>());
			}
		}

	static void InitTable(String baseName, BaseAttributeTable&t)
	{
		baseName.eraseCharacter(' ');
		baseName.eraseCharacter('-');
		
		static Lua::Table configTable;
		if (configTable.IsEmpty())
		{
			configTable = ResearchState::globalState.researchProgram->FindGlobal("StructureConfig");
			if (configTable.IsEmpty())
				FATAL__("research.lua does not define StructureConfig table");
		}

		Lua::Table source = configTable.GetField(baseName);
		if (source.IsEmpty())
			FATAL__("research.lua does not define StructureConfig."+baseName+" table");

		String path =  "StructureConfig."+baseName;
		InitAttrib(source,path,  "Cost",t.constructionCost);
		InitAttrib(source,path,  "Health",t.maxHealth);
		InitAttrib(source,path,  "ScanningRange",t.maxScanningRange);
		InitAttrib(source,path, "IsUnlocked", t.isUnlocked);
	}


	static Lua::Table InitComponent(const Lua::Table&parent, String&path, Component::Base&b)
	{
		String baseName = b.typeName;
		baseName.eraseCharacter(' ');
		baseName.eraseCharacter('-');

		path += '.'+baseName;

		Lua::Table baseTable = parent.GetField(baseName);
		if (baseTable.IsEmpty())
			FATAL__("research.lua does not define "+path+" table");
		InitAttrib(baseTable,path,"Cost",b.productionCosts);
		InitAttrib(baseTable,path,"IsUnlocked",b.isUnlocked);
		return baseTable;
	}

	static void InitVehicleBase(Component::VehicleBase&b)
	{
		static Lua::Table configTable;
		if (configTable.IsEmpty())
		{
			configTable = ResearchState::globalState.researchProgram->FindGlobal("VehicleBaseConfig");
			if (configTable.IsEmpty())
				FATAL__("research.lua does not define VehicleBaseConfig table");
		}
		
		String path = "VehicleBaseConfig";
		Lua::Table myTable = InitComponent(configTable,path,b);

		InitAttrib(myTable,path,"TurretRangeModifier",b.turretRangeModifier);
		InitAttrib(myTable,path,"OperationSpeedMultiplier",b.operationSpeedMultiplier);
		InitAttrib(myTable,path,"MovementSpeed",b.movementSpeed);
		InitAttrib(myTable,path,"CarryingCapacity",b.carryingCapacity);
		InitAttrib(myTable,path,"Health",b.health);
	}

	static void InitTurret(Component::Turret&t)
	{
		static Lua::Table configTable;
		if (configTable.IsEmpty())
		{
			configTable = ResearchState::globalState.researchProgram->FindGlobal("TurretConfig");
			if (configTable.IsEmpty())
				FATAL__("research.lua does not define TurretConfig table");
		}

		String path = "TurretConfig";
		Lua::Table myTable = InitComponent(configTable,path,t);

		InitAttrib(myTable,path,"HealthMultiplier",t.healthMultiplier);
		InitAttrib(myTable,path,"ScanningRange",t.scanningRange);
		InitAttrib(myTable,path,"Weight",t.weight);

		for (index_t i = 0; i < Component::OperationCount; i++)
		{
			InitAttrib(myTable,path,String("Op_")+Component::OperationToString((Component::operation_t)i),t.operationSpeed[i],true);
		}

		InitAttrib(myTable,path,"GroundScannerPenetration",t.groundScannerPenetration,true);
		InitAttrib(myTable,path,"GroundScanningRange",t.groundScanningRange,true);

		InitAttrib(myTable,path,"AmmoConsumption",t.ammoNeedPerShot,true);
		InitAttrib(myTable,path,"AttacksPerRound",t.attacksPerRound,true);
		if (t.attacksPerRound.IsSet())
		{
			InitAttrib(myTable,path,"DamageLevel",t.damageLevel);
			InitAttrib(myTable,path,"ExplosionRadius",t.explosionRadius);
			InitAttrib(myTable,path,"HorizontalAttackRange",t.horizontalAttackRange);
		}
	}


	/*static*/ void		ResearchState::StaticCreate()
	{
		globalState.researchProgram = Lua::Program::Create();
		ASSERT__(globalState.researchProgram);
		{
			Lua::Table api = globalState.researchProgram->CreateTable();

			api.SetFunction("CreateResearch",CreateResearch);
			api.SetGlobal("API");
		}



		try
		{
			globalState.researchProgram->ExecuteFile("../lua/research.lua");
		}
		catch (const std::exception&e)
		{
			FATAL__(e.what());
		}
		Lua::Table researchTable = globalState.researchProgram->FindGlobal("ResearchState");
		if (researchTable.IsEmpty())
		{
			FATAL__("Research Lua script does not define ResearchState class");
		}
		globalState.stateConstructor = researchTable.GetField("New");
		if (globalState.stateConstructor.IsEmpty())
		{
			FATAL__("Research Lua script does not define ResearchState.New method");
		}

		
		structureTables.SetSize(structurePrototypes.Count());
		for (index_t i = 0; i < structurePrototypes.Count(); i++)
		{
			const PStructure&s = structurePrototypes[i];
			BaseAttributeTable&t = structureTables[i];
			InitTable(s->GetTypeName(),t);
			s->DefineAttributeTable(&t);
		}

		foreach (Component::allTurretComponents,turret)
			InitTurret(**turret);
		foreach (Component::baseComponents,base)
			InitVehicleBase(**base);


		//globalState.researchProgram.reset();
	}

	ResearchState::ResearchState(const TUniqueID&id, bool isNexus)
	{
		factionID = id;
		factionIsNexus = isNexus;

		researchState = globalState.stateConstructor();

	}

	/*static*/ PResearchTopic	ResearchState::FindTopic(const String&name)
	{
		foreach (globalState.allTopics,t)
			if ((*t)->name == name)
				return *t;
		return PResearchTopic();
	}

	/*static*/ PResearchTopic	ResearchState::GetTopic(index_t serialIndex)
	{
		return globalState.allTopics[serialIndex];
	}

	bool	ResearchState::SetActiveTopic(const PResearchTopic&active)
	{
		if (!active || !active->CanResearch(researchState))
			return false;
		activeResearch = active;
		return true;
	}


	void					ResearchState::FinalizeRound(const PFaction&f)
	{
		if (activeResearch && totalContribution)
		{
			int left = activeResearch->maxProgress - activeResearch->currentProgress(researchState);
			if (left <= 0)
				return;
			totalContribution = vmin(totalContribution,left);
			activeResearch->advance(researchState,totalContribution);
			changed = true;
			totalContribution = 0;
			if (totalContribution == left)
			{
				ASSERT_EQUAL__(activeResearch->currentProgress(researchState),activeResearch->maxProgress);
				activeResearch.reset();
			}
		}
		BaseAttributeCache::versionCounter++;
	}

	void					ResearchState::Import(const ArrayData<BYTE>&serializedState)
	{
		
		ASSERT__(researchState.DeserializeMerge(ByteReadStream(serializedState)));

		BaseAttributeCache::versionCounter++;

	}
	
	
	void					ResearchState::Export(Array<BYTE>&outSerializedState) const
	{
		//Buffer0<BYTE> data;
		ByteStream stream;

		ASSERT__(researchState.Serialize(stream,true));
		stream.CopyToArray(outSerializedState);
	}



}

