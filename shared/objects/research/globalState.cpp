#include "globalState.h"
#include "../prototypes.h"
#include "../faction.h"
#include "attributeDetails.h"
#include <io/file_system.h>
#include "../../runtimeConfig.h"
#include "../structure.h"

namespace Objects
{

	/*static*/ Research::GlobalState		Research::globalState;


	int						Research::Topic::GetProgress(const Research::FactionState&st) const
	{
		return st.topicProgressVector[serialIndex];
	}

	Research::PTopic		Research::Topic::Create(const Research::PTopic&parent, const String&name, const String&iconName, 
												const Research::Topic::FSetProgress&setProgress, const Research::Topic::FGetProgress&getProgress, 
												int maxProgress,
												const Research::PTopic&dependency
											)
	{
		LogEvent(EventClass::Preset::debug,"Lua created new research '"+name+"'");

		const String&iconSource = iconName.IsEmpty() ? name : iconName;

		PWorldEntity proto = Prototypes::all.FindByName(iconSource);
		if (proto)
			ASSERT__(!proto->owner);	//this should never be true, but i'm growing paranoid
		Component::Base*component = proto ? nullptr : Component::allComponents.FindByName(iconSource);
		bool hasIcon = !proto && !component;


		Research::PTopic rs(new Research::Topic(parent, name,hasIcon ? iconSource : "",setProgress,getProgress,maxProgress,dependency));

		rs->componentReference = component;
		rs->prototypeReference = proto;
		rs->serialIndex = Research::globalState.allTopics.Count();
		Research::globalState.allTopics << rs;


		if (!parent)
		{
			if (!Research::globalState.rootResearch.Insert(rs))
				throw Lua::Exception("Research '"+name+"' is not unique in global namespace");
		}
		else
		{
			if (!parent->Insert(rs))
				throw Lua::Exception("Research '"+name+"' is not unique in '"+parent->GetFullName()+"'");
			if (parent->maxProgress != 0)
			{
				throw Lua::Exception("Trying to convert regular research '"+parent->name+"' to group when adding '"+name+"' as child. '"+parent->name+"'.maxProgress must be 0 when used as a group");
			}
		}

		if (dependency)
		{
			if (dependency->maxProgress <= 0)
			{
				throw Lua::Exception("Trying to register research dependency of '"+name+"' to unresearchable topic '"+dependency->name+"'");
			}

		}
		return rs;
	}

	Research::Topic::Topic(const Research::PTopic&parent, const String&name, const String&iconName, 
								const FSetProgress&setProgress, const FGetProgress&getProgress, 
								int maxProgress,
								const Research::PTopic&dependency
								)
								:Super(name), parent(parent),iconName(iconName),
							#ifndef CLIENT
								setProgress(setProgress),getProgress(getProgress),
							#endif
								maxProgress(maxProgress),
								dependency(dependency)
	{}



	Research::Topic::~Topic()
	{
		bool brk = true;

	}

	bool			Research::Topic::CanResearch(const PFaction&owner) const
	{
		if (IsGroup())
			return false;
		//auto st = owner ? owner->researchState.researchState : Lua::Reference();
		if (owner && ResearchCompleted(owner->researchState.topicProgressVector))
			return false;
		Research::PTopic p = dependency.lock();
		if (!p)
			return true;
		if (!owner)
			return false;
		return p->ResearchCompleted(owner->researchState.topicProgressVector) || owner->researchState.GetResearchQueue().Contains(p);
	}



	namespace Research
	{

		static Array<BaseAttributeTable>	structureTables;
	


		



		static void InitStructureTable(String baseName, BaseAttributeTable&t)
		{
			baseName.EraseCharacter(' ');
			baseName.EraseCharacter('-');
		
			#ifdef CLIENT
				Lua::Table source;
			#else
				static Lua::Table configTable;
				if (configTable.IsEmpty())
				{
					configTable = Research::globalState.researchProgram->FindGlobal("StructureConfig");
					if (configTable.IsEmpty())
						throw Lua::Exception("Table 'StructureConfig' is not defined");
				}

				Lua::Table source = configTable.GetField(baseName);
				if (source.IsEmpty())
					throw Lua::Exception("Table 'StructureConfig."+baseName+"' is not defined");
			#endif
			String path =  "StructureConfig."+baseName;
			InitAttrib(source,path,  "Cost",t.constructionCost);
			InitAttrib(source,path,  "Health",t.maxHealth);
			InitAttrib(source,path,  "ScanningRange",t.maxScanningRange);
			InitAttrib(source,path, "IsUnlocked", t.isUnlocked);
		}


		static Lua::Table LookupComponentTable(const Lua::Table&parent, String&path, Component::Base&b)
		{

			Lua::Table baseTable;

			#ifndef CLIENT
				String baseName = b.typeName;
				baseName.EraseCharacter(' ');
				baseName.EraseCharacter('-');

				path += '.'+baseName;

				baseTable = parent.GetField(baseName);
				if (baseTable.IsEmpty())
					throw Lua::Exception("Table '"+path+"' is not defined");
			#endif
			return baseTable;

		}

		static Lua::Table InitComponent(const Lua::Table&parent, String&path, Component::Base&b)
		{
			auto baseTable = LookupComponentTable(parent,path,b);
			InitAttrib(baseTable,path,"Cost",b.productionCosts);
			InitAttrib(baseTable,path,"IsUnlocked",b.isUnlocked);
			return baseTable;
		}

		static void InitVehicleBase(Component::VehicleBase&b)
		{
			static Lua::Table configTable;
			#ifndef CLIENT
				if (configTable.IsEmpty())
				{
					configTable = Research::globalState.researchProgram->FindGlobal("VehicleBaseConfig");
					if (configTable.IsEmpty())
						throw Lua::Exception("Table 'VehicleBaseConfig' is not defined");
				}
			#endif
		
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
			#ifndef CLIENT
				if (configTable.IsEmpty())
				{
					configTable = Research::globalState.researchProgram->FindGlobal("TurretConfig");
					if (configTable.IsEmpty())
						throw Lua::Exception("Table 'TurretConfig' is not defined");
				}
			#endif

			String path = "TurretConfig";
			Lua::Table myTable = InitComponent(configTable,path,t);

			InitAttrib(myTable,path,"HealthMultiplier",t.healthMultiplier);
			InitAttrib(myTable,path,"ScanningRange",t.scanningRange);
			InitAttrib(myTable,path,"Weight",t.weight);

			for (index_t i = 0; i < Component::OperationCount; i++)
			{
				InitAttrib(myTable,path,String("Op_")+Component::OperationToString((Component::operation_t)i),t.operationSpeed[i], OnAttributeNotFound::Ignore);
			}

			InitAttrib(myTable,path,"GroundScannerPenetration",t.groundScannerPenetration, OnAttributeNotFound::Ignore);
			InitAttrib(myTable,path,"GroundScanningRange",t.groundScanningRange, OnAttributeNotFound::Ignore);

			InitAttrib(myTable,path,"AmmoConsumption",t.ammoNeedPerShot, OnAttributeNotFound::Ignore);
			InitAttrib(myTable,path,"AttacksPerRound",t.attacksPerRound, OnAttributeNotFound::Ignore);
			InitAttrib(myTable,path,"HorizontalAttackRange",t.horizontalAttackRange, OnAttributeNotFound::Ignore);
			InitAttrib(myTable,path,"ExplosionRadius",t.explosionRadius, OnAttributeNotFound::Ignore);	//can be independent
			InitAttrib(myTable,path,"DamageLevel",t.damageLevel, OnAttributeNotFound::Ignore);
		}

		static void RemapAttributes( PathString filename )
		{
			const auto instDir = FileSystem::Folder().ParentLocation()->GetLocation();
			const auto documents = RuntimeConfig::ConfigurationScope::GetMyDocuments();
			if (filename.BeginsWith(instDir))
			{
				filename.EraseLeft(instDir.Length());
				filename = "[InstallDir]" + filename;
			}
			elif (filename.BeginsWith(documents))
			{
				filename.EraseLeft(documents.Length());
				filename = "[MyDocuments]" + filename;
			}


			attributeContext = FileSystem::PathToString(filename);
			LuaAttributes<int>::Clear();
			LuaAttributes<bool>::Clear();
			LuaAttributes<float>::Clear();

			for (index_t i = 0; i < Prototypes::structures.Count(); i++)
			{
				const PStructure&s = Prototypes::structures[i];
				auto&t = structureTables[i];
				InitStructureTable(s->GetTypeName(),t);
			}

			foreach (Component::allTurretComponents,turret)
				InitTurret(**turret);
			foreach (Component::baseComponents,base)
				InitVehicleBase(**base);

			Lua::Table jobs;
			#ifndef CLIENT
				jobs = Research::globalState.researchProgram->FindGlobal("Jobs");
			#endif
			{
				Lua::Table bomb;
				if (jobs.IsSet())
					bomb = jobs.GetField("Bomb");
				InitAttrib(bomb,"Jobs.Bomb","DoomsdayDamageLevel",Component::Bomb::doomsdayDamageLevel, OnAttributeNotFound::Warn);
				InitAttrib(bomb,"Jobs.Bomb","DoomsdayExplosionRadius",Component::Bomb::doomsdayExplosionRadius, OnAttributeNotFound::Warn);
				InitAttrib(bomb,"Jobs.Bomb","DoomsdayUraniumCost",Component::Bomb::doomsdayUraniumCost, OnAttributeNotFound::Warn);
				InitAttrib(bomb,"Jobs.Bomb","RegularUraniumCost",Component::Bomb::regularUraniumCost, OnAttributeNotFound::Warn);
			}
		}
	}


	/*static*/ void		Research::StaticCreate()
	{
		structureTables.SetSize(Prototypes::structures.Count());
		for (index_t i = 0; i < Prototypes::structures.Count(); i++)
		{
			const PStructure&s = Prototypes::structures[i];
			BaseAttributeTable&t = structureTables[i];
			s->DefineAttributeTable(&t);
		}

		#ifndef CLIENT
			LoadDefaultProgram();
		#else
			RemapAttributes("");
		#endif
	}




	#ifndef CLIENT
		/*static*/ void			Research::LoadDefaultProgram()
		{
			#define RESEARCH_SCRIPT_NAME "attributesAndResearch.lua"

			PathString factoryDefault = "../lua/" RESEARCH_SCRIPT_NAME;
			#if 0 && defined _DEBUG
				PathString final = factoryDefault;
			#else
				FileSystem::CreateDirectory(RuntimeConfig::myConfig.Lookup("config/lua"));
				FileSystem::CreateDirectory(RuntimeConfig::sharedConfig.Lookup("config/lua"));
				PathString final = RuntimeConfig::myConfig.Lookup("config/lua/" RESEARCH_SCRIPT_NAME);
				if (!FileSystem::IsFile(final))
					final = RuntimeConfig::sharedConfig.Lookup("config/lua/" RESEARCH_SCRIPT_NAME);
				if (!FileSystem::IsFile(final))
				{
					final = factoryDefault;
				}
			#endif
			globalState.factoryDefault = ScriptCache::Load(factoryDefault); 
			ActivateProgram(ScriptCache::Load(final));
		}

		static void			InnerLoad(const ScriptCache::PScript&script);
		static void			RequireLoad(const ScriptCache::PScript&script);


		/*static*/ void		Research::ActivateProgram(const ScriptCache::PScript&script)
		{
			//#if defined _DEBUG && defined HAS_WINDOW
			//	if (globalState.script)
			//		return;		//prevent reloading
			//#endif
			if (!script || script == globalState.script)
				return;

			if (script != globalState.factoryDefault)	//otherwise force-load
			{
				LogEvent(EventClass::Preset::common,"Activating research script "+Bin2Str::ConvertPOD(script->identifier));

				ScriptCache::PScript current = globalState.script;
				try
				{
					InnerLoad(script);
					return;
				}
				catch (const std::exception&e)
				{
					if (script->filename.IsNotEmpty())
						LogEvent(EventClass(EventSignificance::NonFatalError,EventFlavor::ServerPrivate),String(script->filename)+": "+e.what());
					else
						LogEvent(EventClass(EventSignificance::NonFatalError,EventFlavor::ServerPrivate),e.what());
				};

				//fall back
				LogEvent(EventClass(EventSignificance::NonFatalError,EventFlavor::ServerPrivate),"Unable to load script. Falling back to factory default");
			}
			RequireLoad(globalState.factoryDefault);
		}

		static void RequireLoad(const ScriptCache::PScript&script)
		{
			try
			{
				InnerLoad(script);
				return;
			}
			catch (const std::exception&e)
			{
				FATAL__("Fallback research script failed to load: "+String( e.what() ));
			};
		}
		
		struct TSerialState
		{
			Array<BYTE>		serialState;
			String			researchQueue;

			void			swap(TSerialState&other)
			{
				serialState.swap(other.serialState);
				researchQueue.swap(other.researchQueue);
			}

			friend void		swap(TSerialState&a,TSerialState&b)
			{
				a.swap(b);
			}
		};

		/*static*/ void	InnerLoad( const ScriptCache::PScript&script )
		{
			using namespace Research;

			if (!script)
				throw Lua::Exception(CLOCATION, Except::globalString("Script is NULL"));
			LogEvent(EventClass(EventSignificance::Common,EventFlavor::ServerPrivate),"Attempting to activate research program "+Bin2Str::ConvertPOD(script->identifier));


			Ctr::Buffer0<TSerialState,Swap> taskLists;
			ForeachFaction([&taskLists](const PFaction&f)
			{
				auto&entry = taskLists.Append();
				f->researchState.ExportLuaState(entry.serialState);
				entry.researchQueue = f->researchState.ExportResearchQueue();
			});

			globalState.rootResearch.Clear();
			globalState.allTopics.Clear();

			globalState.researchAPI.Destroy();
			globalState.researchProgram = Lua::Program::Create("Research");
			globalState.researchAPI.Create(globalState.researchProgram);
			ASSERT__(globalState.researchProgram);
			globalState.researchAPI.gameTable.SetFunction("CreateResearch",Topic::Create);



			globalState.researchProgram->Execute(script->content,String(script->filename));

		

			Lua::Table researchTable = globalState.researchProgram->FindGlobal("ResearchState");
			if (researchTable.IsEmpty())
			{
				throw Lua::Exception("Table 'ResearchState' is not defined");
			}
			globalState.stateConstructor = researchTable.GetField("New");
			if (globalState.stateConstructor.IsEmpty())
			{
				throw Lua::Exception("Function 'ResearchState.New' is not defined");
			}

			globalState.exportState = researchTable.GetField("Export");
			if (globalState.exportState.IsEmpty())
			{
				throw Lua::Exception("Method 'ResearchState:Export' is not defined");
			}
			globalState.importState = researchTable.GetField("Import");
			if (globalState.importState.IsEmpty())
			{
				throw Lua::Exception("Method 'ResearchState:Import' is not defined");
			}

			RemapAttributes(script->filename);


			index_t at = 0;
			ForeachFaction([&at,&taskLists](const PFaction&f)
			{
				const auto&entry = taskLists[at];
				f->researchState.luaResearchState = globalState.stateConstructor();
				if (entry.serialState.IsNotEmpty())
					f->researchState.ImportLuaState(entry.serialState);
				else
				{
					f->researchState.topicProgressVector.SetSize(globalState.allTopics.Count());
					f->researchState.topicProgressVector.Fill(0);
					f->researchState.UpdateAttributeTable();
				}
				f->researchState.RestoreResearchQueue(entry.researchQueue);
				at++;
			});
			{
				globalState.zeroState = globalState.stateConstructor();
				globalState.zeroTable.Update(globalState.zeroState);
			}
			globalState.script = script;

			#ifndef CLIENT
				if (Faction::nexus)
					Faction::nexus->researchState.LoadNexusResearchState();
			#endif

			globalState.onScriptChange();
			//globalState.researchProgram.reset();
		}
	#endif

	namespace Research
	{

	

		bool					Topic::ResearchCompleted(const TopicProgressVector&researchState) const
		{
			return maxProgress > 0 && researchState[serialIndex] >= maxProgress;
		}
	}
}
