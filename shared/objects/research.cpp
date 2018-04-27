#include <global_root.h>
#include "research.h"
#include "prototypes.h"
#include "structure.h"
#include "../logEvent.h"
#include "faction.h"
#include "../runtimeConfig.h"
#include <lua/luaGameAPI.h>
#include <io/file_system.h>
#include "structure/laboratory.h"
#include <io/byte_stream.h>
#include "../expectedBehavior.h"

namespace Objects
{



	/*static*/ Research::GlobalState		Research::globalState;


	int						Research::Topic::GetProgress(const Research::FactionState&st) const
	{
		return st.topicProgressVector[serialIndex];
	}

	/*static*/ Research::PTopic	CreateResearch(const Research::PTopic&parent, const String&name, const String&iconName, 
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





	static Array<BaseAttributeTable>	structureTables;
	static String						attributeContext;

	CONSTRUCT_ENUMERATION3(OnAttributeNotFound, Fail, Warn, Ignore);


	template <typename T>
		class LuaAttributes
		{
		private:
			struct ConstOverride
			{
				T			constValue;
				bool		isSet = false;
			};

			struct Attribute
			{
				Lua::Function<T(Lua::Reference)>		luaGetValue;
				Lua::Function<String(Lua::Reference)>	luaDescribeValue;
				ConstOverride							constOverride;

				void		ExportTo( const Lua::Reference&state, typename Research::Attributes::TypeMap<T>::Attrib&outAttrib ) const
				{
					if (constOverride.isSet)
						outAttrib.value = constOverride.constValue;
					else
					{
						if (luaGetValue.IsEmpty())
							outAttrib.value = (T)0;
						else
							outAttrib.value = luaGetValue(state);
					}

					if (luaDescribeValue.IsSet())
						outAttrib.description = luaDescribeValue(state);
					else
					{
						using StringConversion::ToString;
						outAttrib.description = ToString(outAttrib.value);
					}

					outAttrib.flags = GetFlags();
				}

				Research::Attributes::Flags::serial_type_t GetFlags() const
				{
					Research::Attributes::Flags::serial_type_t rs = 0;
					if (luaGetValue.IsSet())
						rs |= Research::Attributes::Flags::HasFunctionFlag;
					if (constOverride.isSet)
						rs |= Research::Attributes::Flags::IsConst;
					return rs;
				}



				void		CheckExportTo( const Lua::Reference&state, Research::Attributes::TypeMap<T>&outAttribs, index_t at ) const
				{
					T value;
					if (constOverride.isSet)
						value = constOverride.constValue;
					else
					{
						if (luaGetValue.IsEmpty())
							value = (T)0;
						else
							value = luaGetValue(state);
					}

					String desc;
					if (luaDescribeValue.IsSet())
						desc = luaDescribeValue(state);
					else
					{
						using StringConversion::ToString;
						desc = ToString(value);
					}

					outAttribs.CheckUpdate(at,value,desc,GetFlags());
				}

				void	SetConst( T value )
				{
					constOverride.constValue = value;
					constOverride.isSet = true;
				}

				bool	IsSet() const
				{
					return luaGetValue.IsSet() || constOverride.isSet;
				}

				bool	IsFunctionOrConstNonZero() const
				{
					return constOverride.isSet ? constOverride.constValue != 0 : luaGetValue.IsSet();
				}

				void	swap(Attribute&other)
				{
					using std::swap;
					swap(constOverride,other.constOverride);
					luaGetValue.swap(other.luaGetValue);
					luaDescribeValue.swap(other.luaDescribeValue);
				}

				friend void swap(Attribute&a, Attribute&b)
				{
					a.swap(b);
				}


			};

		public:

			template <typename F>
				static void Set(const Lua::Table&source, const String&path, const String&name, LuaAttribute<T,F>&a, OnAttributeNotFound onAttributeNotFound = OnAttributeNotFound::Fail)
				{

					a.Clear(list.Count());
					auto&attrib = list.Append();

					#ifndef CLIENT


						Lua::NamedReference ref;
						if (source.IsSet())
							ref = source.GetField(name);
						if (ref.IsEmpty())
						{
							switch (onAttributeNotFound)
							{
								case OnAttributeNotFound::Fail:
									throw Lua::Exception("Field '"+path+"."+name+"' is not defined");
								case OnAttributeNotFound::Warn:
									LogEvent(EventClass::Preset::configurationError,attributeContext+": Field '"+path+"."+name+"' is not defined");
									break;
							}
							return;
						}

						if (ref.IsTable())
						{
							Lua::Table t = ref;
			
							attrib.luaGetValue = t.GetField("Get");
							if (attrib.luaGetValue.IsEmpty())
								throw Lua::Exception("Function '"+path+"."+name+".Get' is not defined");

							attrib.luaDescribeValue = t.GetField("Describe");
						}
						elif (ref.IsFunction())
						{
							attrib.luaGetValue = ref;
						}
						else
						{
							attrib.SetConst(ref.GetAs<T>());
						}
					#endif
				}

			static void Clear()
			{
				list.Clear();
			}

			static bool IsSet( index_t idx )
			{
				if (list.IsEmpty())
					return false;
				return Expect::SafeGet(list,idx,CLOCATION).IsSet();
			}

			static bool IsFunctionOrConstNonZero( index_t idx )
			{
				if (list.IsEmpty())
					return false;
				return Expect::SafeGet(list,idx,CLOCATION).IsFunctionOrConstNonZero();
			}

			static void ExportFunctionMapTo( Array<bool>&outFunctionIsSet )
			{
				outFunctionIsSet.SetSize(list.Count());
				for (index_t i = 0; i < list.Count(); i++)
				{
					outFunctionIsSet[i] = list[i].luaGetValue.IsSet();
				}
			}

			static void ExportTo( const Lua::Reference&state, Research::Attributes::TypeMap<T>&outType )
			{
				if (outType.attributes.Count() != list.Count())
				{
					outType.attributes.SetSize(list.Count());
					for (index_t i = 0; i < list.Count(); i++)
					{
						auto&a = outType.ForceUpdate(i);
						list[i].ExportTo(state,a);
					}
				}
				else
				{
					for (index_t i = 0; i < list.Count(); i++)
						list[i].CheckExportTo(state,outType,i);
				}
			}

		private:




			
			static Buffer0<Attribute,Swap>	list;

		};

	template <typename T>
		/*static*/ Buffer0<typename LuaAttributes<T>::Attribute,Swap>	LuaAttributes<T>::list;

	template <typename T, typename F>
		static void InitAttrib(const Lua::Table&source, const String&path, const String&name, LuaAttribute<T,F>&a, OnAttributeNotFound onAttributeNotFound = OnAttributeNotFound::Fail)
		{
			LuaAttributes<T>::Set(source,path,name,a,onAttributeNotFound);
		}




		


	#ifndef CLIENT
		/*static*/ void UpdateAttributeTable( const Lua::Reference&luaResearchState, Research::Attributes::Table&table )
		{
			table.ClearChanges();
			LuaAttributes<int>::ExportTo(luaResearchState,table.ints);
			LuaAttributes<float>::ExportTo(luaResearchState,table.floats);
			LuaAttributes<bool>::ExportTo(luaResearchState,table.bools);
		}

		void Research::FactionState::UpdateAttributeTable()
		{
			Objects::UpdateAttributeTable(luaResearchState,attributeTable);
			topicProgressChanged = true;	//implied
		}
		/*static*/ bool			Research::LuaIntAttributeIsSet( index_t idx )
		{
			return LuaAttributes<int>::IsSet(idx);
		}

		/*static*/ bool			Research::LuaBoolAttributeIsSet( index_t idx )
		{
			return LuaAttributes<bool>::IsSet(idx);
		}

		/*static*/ bool			Research::LuaFloatAttributeIsSet( index_t idx )
		{
			return LuaAttributes<float>::IsSet(idx);
		}

		/*static*/ bool			Research::LuaIntAttributeIsFunctionOrConstNonZero( index_t idx )
		{
			return LuaAttributes<int>::IsFunctionOrConstNonZero(idx);
		}

		/*static*/ bool			Research::LuaBoolIsFunctionOrConstNonZero( index_t idx )
		{
			return LuaAttributes<bool>::IsFunctionOrConstNonZero(idx);
		}

		/*static*/ bool			Research::LuaFloatAttributeIsFunctionOrConstNonZero( index_t idx )
		{
			return LuaAttributes<float>::IsFunctionOrConstNonZero(idx);
		}
	#else
		/*static*/ bool			Research::LuaIntAttributeIsSet( index_t idx )
		{
			return globalState.zeroTable.ints.IsSet(idx);
		}

		/*static*/ bool			Research::LuaBoolAttributeIsSet( index_t idx )
		{
			return globalState.zeroTable.bools.IsSet(idx);
		}

		/*static*/ bool			Research::LuaFloatAttributeIsSet( index_t idx )
		{
			return globalState.zeroTable.floats.IsSet(idx);
		}

		/*static*/ bool			Research::LuaIntAttributeIsFunctionOrConstNonZero( index_t idx )
		{
			return globalState.zeroTable.ints.IsFunctionOrConstNonZero(idx);
		}

		/*static*/ bool			Research::LuaBoolIsFunctionOrConstNonZero( index_t idx )
		{
			return globalState.zeroTable.bools.IsFunctionOrConstNonZero(idx);
		}

		/*static*/ bool			Research::LuaFloatAttributeIsFunctionOrConstNonZero( index_t idx )
		{
			return globalState.zeroTable.floats.IsFunctionOrConstNonZero(idx);
		}
	#endif


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
		InitAttrib(myTable,path,"SubOrbitalScanningRange",t.subOrbitalScanningRange, OnAttributeNotFound::Ignore);

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
		Lua::Table bomb;
		if (jobs.IsSet())
			bomb = jobs.GetField("Bomb");
		InitAttrib(bomb,"Jobs.Bomb","DoomsdayDamageLevel",Component::Bomb::doomsdayDamageLevel, OnAttributeNotFound::Warn);
		InitAttrib(bomb,"Jobs.Bomb","DoomsdayExplosionRadius",Component::Bomb::doomsdayExplosionRadius, OnAttributeNotFound::Warn);
		InitAttrib(bomb,"Jobs.Bomb","DoomsdayUraniumCost",Component::Bomb::doomsdayUraniumCost, OnAttributeNotFound::Warn);
		InitAttrib(bomb,"Jobs.Bomb","RegularUraniumCost",Component::Bomb::regularUraniumCost, OnAttributeNotFound::Warn);
		{
			Lua::Table bomber;
			if (jobs.IsSet())
				bomber = jobs.GetField("Bomber");
			InitAttrib(bomber,"Jobs.Bomber","DoomsdayBeamAttackCosts",Component::SubOrbital::Bomber::doomsdayBeamAttackCosts, OnAttributeNotFound::Warn);
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

			globalState.researchProgram = Lua::Program::Create("Research");
			ASSERT__(globalState.researchProgram);
			{
				Lua::Table api = Lua::CreateGeneralAPI(globalState.researchProgram);
				api.SetFunction("CreateResearch",CreateResearch);
			}



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
				UpdateAttributeTable(globalState.zeroState,globalState.zeroTable);
			}
			globalState.script = script;

			#ifndef CLIENT
				if (Faction::nexus)
					Faction::nexus->researchState.LoadNexusResearchState();
			#endif

			globalState.onScriptChange();
			//globalState.researchProgram.reset();
		}

		void Research::FactionState::LoadNexusResearchState()
		{
			foreach (globalState.allTopics,_t)
			{
				auto&t = **_t;
				if (t.setProgress.IsSet())
				{
					if (t.maxProgress > 0)
						t.setProgress(luaResearchState,t.maxProgress);
				}
			}
			UpdateAttributeTable();
		}

		void					Research::FactionState::AdvanceNexusResearch()
		{
			DBG_ASSERT__(factionIsNexus);

			foreach (globalState.allTopics,_t)
			{
				auto&t = **_t;
				if (t.setProgress.IsSet())
				{
					if (t.maxProgress < 0)
						topicProgressVector[ t.serialIndex ] ++;
				}
			}
			UpdateAttributeTable();
		}


		void					Research::FactionState::FinalizeRound(const PFaction&f)
		{
			attributeTable.ClearChanges();

			while (researchQueue.IsNotEmpty() && totalContribution)
			{
				int left;
				const auto&res = researchQueue.First();
				int&progress = topicProgressVector[ res->serialIndex ];
				if (res->maxProgress >= 0)
				{
					left = res->maxProgress - progress;
					if (left <= 0)	//already done...?
					{
						LogEvent(EventClass::Preset::important,owner->shared_from_this(),"Research "+res->name+" completed");
						res->setProgress(luaResearchState,res->maxProgress);
						researchQueue.Erase(0);
						queueChanged = true;
						globalState.onQueueChange(owner);
						continue;
					}
					topicProgressChanged = true;
					if (totalContribution < left)	//no chance to get done
					{
						progress += totalContribution;
						res->setProgress(luaResearchState,progress);
						break;
					}
					totalContribution -= left;
					progress = res->maxProgress;
					res->setProgress(luaResearchState,res->maxProgress);
					LogEvent(EventClass::Preset::important,owner->shared_from_this(),"Research "+res->name+" completed");
					researchQueue.Erase(0);
					queueChanged = true;
					globalState.onQueueChange(owner);
					continue;
				}
				progress += totalContribution;
				res->setProgress(luaResearchState,progress);
				topicProgressChanged = true;

				if (researchQueue.Count() > 1)
				{
					LogEvent(EventClass::Preset::common,owner->shared_from_this(),"Multiple research topics defined after "+res->name+". Cycling");
					auto topic = res;
					researchQueue.Erase(0);
					researchQueue.Append(topic);
					queueChanged = true;
					globalState.onQueueChange(owner);
				}

				break;
			}
			lastSpeed = totalContribution;
			totalContribution = 0;
			if (topicProgressChanged)
				UpdateAttributeTable();
		}


		//void					Research::FactionState::ImportResearchState(const Research::TopicProgressVector&archived)
		//{
		//	if (archived.Count() != globalState.allTopics.Count())
		//		return;

		//	topicProgressVector = archived;
		//	luaResearchState = globalState.stateConstructor();

		//	foreach (globalState.allTopics,t)
		//	{
		//		if ((*t)->setProgress.IsSet())
		//			(*t)->setProgress(luaResearchState,archived[t-globalState.allTopics.pointer()]);
		//	}

		//	UpdateAttributeTable();
		//}

	
		void					Research::FactionState::ImportLuaState(const ArrayRef<const BYTE>&serializedState)
		{
			if (!globalState.researchProgram)
			{
				return;
			}
			try
			{
				#ifdef CLIENT
					World::actual.InvalidateSensory();
				#else
					owner->knownWorld.InvalidateSensory();
				#endif

				Lua::Reference deserialized = globalState.researchProgram->DeserializeToReference(ByteReadStream(serializedState));
				globalState.importState(luaResearchState,deserialized);

				ASSERT_EQUAL__(globalState.allTopics.Count(), topicProgressVector.Count());
				foreach (globalState.allTopics,t)
				{
					const index_t at = t - globalState.allTopics.pointer();
					if ((*t)->getProgress.IsSet())
						topicProgressVector[at] = (*t)->getProgress(luaResearchState);
					else
						topicProgressVector[at] = -1;
				}

			}
			catch (const std::exception&e)
			{
				LogEvent(EventClass::Preset::error,e.what());
			}
			UpdateAttributeTable();
		}
	
	
		void					Research::FactionState::ExportLuaState(Array<BYTE>&outSerializedState) const
		{
			//Buffer0<BYTE> data;
			if (globalState.exportState.IsEmpty() || luaResearchState.IsEmpty())
			{
				outSerializedState.Clear();
				return;
			}
			try
			{
				Lua::Reference exported = globalState.exportState(luaResearchState);
				ASSERT__(exported.IsSet());
				ByteStreamBuffer stream;
				exported.Serialize(stream);
				outSerializedState = stream.GetWrittenData();
			}
			catch (const std::exception&e)
			{
				LogEvent(EventClass::Preset::error,e.what());
				outSerializedState.Clear();
			}
		}
	

	#else

		/*static*/ void				Research::FactionState::BeginServerTopicUpdate()
		{
			globalState.rootResearch.Clear();
			globalState.allTopics.Clear();
		}

		/*static*/ void				Research::FactionState::DeclareServerTopic(index_t parentTopicIndex, const String&topicName, const String&iconName, int maxProgress, index_t dependentTopicIndex)
		{
			CreateResearch(parentTopicIndex != InvalidIndex ? Expect::SafeGet(globalState.allTopics,parentTopicIndex,CLOCATION) : nullptr,topicName,iconName,Research::Topic::FSetProgress(),Research::Topic::FGetProgress(), maxProgress,dependentTopicIndex != InvalidIndex ? Expect::SafeGet(globalState.allTopics,dependentTopicIndex,CLOCATION):nullptr);
		}
	#endif


	Research::FactionState::FactionState( Faction*owner, bool isNexus ):owner(owner)
	{
		factionIsNexus = isNexus;
		
		topicProgressVector.SetSize(globalState.allTopics.Count());
		topicProgressVector.Fill(0);
		#ifndef CLIENT
		if (globalState.script)
		{
			luaResearchState = globalState.stateConstructor();
			if (isNexus)
				LoadNexusResearchState();
			else
				UpdateAttributeTable();
		}
		#endif
	}
	
	bool					Research::TopicGroup::Insert(const Research::PTopic&t)
	{
		if (Find(t->name))
			return false;
		map.Set(t->name,t);
		children << t;
		return true;
	}


	Research::PTopic			Research::TopicGroup::Find(const StringRef&name) const
	{
		//Research::PTopic t;
		//map.Query(name,t);
		//return t;
		foreach (children,c)
			if ((*c)->name == name)	//i suspect this is actually faster for slow numbers of topics per family, since it does not force a String copy
				return *c;
		return Research::PTopic();
	}


	static Research::PTopic FindTopicOfPrototypeIn(const Research::PTopic&topic, const PWorldEntity&proto)
	{
		foreach (topic->children,_c)
		{
			const auto&t = *_c;
			if (t->IsGroup())
			{
				const auto&rs = FindTopicOfPrototypeIn(t,proto);
				if (rs)
					return rs;
			}
			if (t->prototypeReference == proto)
				return t;
		}
		return nullptr;
	}

	static Research::PTopic FindTopicOfPrototypeIn(const Research::PTopic&topic, const Component::Base*proto)
	{
		foreach (topic->children,_c)
		{
			const auto&t = *_c;
			if (t->IsGroup())
			{
				const auto&rs = FindTopicOfPrototypeIn(t,proto);
				if (rs)
					return rs;
			}
			if (t->componentReference == proto)
				return t;
		}
		return nullptr;
	}

	#if 0
	/*static*/ Research::PTopic	Research::FactionState::FindTopicOfPrototype(const Component::Base*proto)
	{
		foreach (globalState.allTopics,_t)
		{
			const auto&t = *_t;
			if (t->IsGroup())
			{
				const auto&rs = FindTopicOfPrototypeIn(t,proto);
				if (rs)
					return rs;
			}
			if (t->componentReference == proto)
				return t;
		}
		return nullptr;
	}

	/*static*/ Research::PTopic	Research::FactionState::FindTopicOfPrototype(const PWorldEntity&proto)
	{
		foreach (globalState.allTopics,_t)
		{
			const auto&t = *_t;
			if (t->IsGroup())
			{
				const auto&rs = FindTopicOfPrototypeIn(t,proto);
				if (rs)
					return rs;
			}
			if (t->prototypeReference == proto)
				return t;
		}
		return nullptr;
	}
	#endif

	/*static*/ Research::PTopic	Research::FindTopic(const StringRef&path)
	{
		Ctr::Array<StringRef>	components;
		explode('/',path,components);
		Research::PTopic	current = globalState.rootResearch.Find(components.First());
		for (index_t i = 1; i < components.Count(); i++)
		{
			if (!current)
				return Research::PTopic();
			current = current->Find(components[i]);
		}
		return current;
	}

	/*static*/ Research::PTopic	Research::GetTopic(index_t serialIndex)
	{
		return serialIndex < globalState.allTopics.Count() ? globalState.allTopics[serialIndex] : Research::PTopic();
	}

	bool	Research::FactionState::ResearchIsPlanned(const Research::PTopic&topic) const
	{
		return researchQueue.Contains(topic);
	}

	bool	Research::FactionState::ResearchQueueIsFull() const
	{
		return researchQueue.Count() >= MaxResearchQueueLength;
	}

	bool	Research::FactionState::AppendResearchTask(const Research::PTopic&active)
	{
		if (!active || ResearchQueueIsFull() || !active->CanResearch(topicProgressVector,researchQueue.ToRef()) || researchQueue.Contains(active))
			return false;
		researchQueue << active;
		return true;
	}

	bool					Research::FactionState::DropResearchTask(const Research::PTopic&active)
	{
		if (!active)
			return false;
		index_t at = researchQueue.GetIndexOf(active);
		if (at == InvalidIndex)
			return false;
		researchQueue.Erase(at);
		for (index_t i = at; i < researchQueue.Count();)
		{
			if (!researchQueue[i]->CanResearch(topicProgressVector,researchQueue.SubRef(0,i)))
			{
				LogEvent(EventClass::Preset::operationAborted,owner->shared_from_this(),"Unable to continue research "+researchQueue[i]->GetFullName()+". Preconditions lost. Aborting operation");
				researchQueue.Erase(i);
			}
			else
				i++;
		}
		return true;
	}


	void		Research::FactionState::RestoreResearchQueue(const String&source)
	{
		researchQueue.Clear();
		Ctr::Array<StringRef> paths;
		explode(';',source,paths);
		foreach (paths,p)
			AppendResearchTask(FindTopic(*p));
		globalState.onQueueChange(owner);
	}

	String		Research::FactionState::ExportResearchQueue()
	{
		if (researchQueue.IsEmpty())
			return "";
		StringBuffer buffer;
		foreach (researchQueue,q)
		{
			if (buffer.IsNotEmpty())
				buffer << ';';
			buffer << (*q)->GetPath();
		}
		return buffer.CopyToString();
	}

	void		Research::FactionState::ExportResearchQueue(Ctr::Array<UINT32>&outRS)
	{
		outRS.SetSize(researchQueue.Count());
		for (index_t i = 0; i < researchQueue.Count(); i++)
			outRS[i] = ((UINT32)(researchQueue[i]->serialIndex));
	}
	void					Research::FactionState::RestoreResearchQueue(const Ctr::Array<UINT32>&q)
	{
		researchQueue.Clear();
		foreach (q,e)
			AppendResearchTask(GetTopic(*e));
		globalState.onQueueChange(owner);
	}






}

