#include <global_root.h>
#include "research.h"
#include "prototypes.h"
#include "structure.h"
#include "../logEvent.h"
#include "../scriptCache.h"
#include "faction.h"
#include "../runtimeConfig.h"
#include "../luaGameAPI.h"
#include <io/file_system.h>
#include "structure/laboratory.h"
#include <io/byte_stream.h>
#include "../expectedBehavior.h"

namespace Objects
{



	/*static*/ ResearchState::GlobalState		ResearchState::globalState;


	int						ResearchTopic::GetProgress(const ResearchState&st) const
	{
		return st.topicProgressVector[serialIndex];
	}

	/*static*/ PResearchTopic	ResearchState::CreateResearch(const PResearchTopic&parent, const String&name, const String&iconName, 
												const ResearchTopic::FSetProgress&setProgress, const ResearchTopic::FGetProgress&getProgress, 
												int maxProgress,
												const PResearchTopic&dependency
											)
	{
		LogEvent(EventClass::Preset::debug,"Lua created new research '"+name+"'");

		const String&iconSource = iconName.IsEmpty() ? name : iconName;

		PWorldEntity proto = Prototypes::all.FindByName(iconSource);
		if (proto)
			ASSERT__(!proto->owner);	//this should never be true, but i'm growing paranoid
		Component::Base*component = proto ? nullptr : Component::allComponents.FindByName(iconSource);
		bool hasIcon = !proto && !component;


		PResearchTopic rs(new ResearchTopic(parent, name,hasIcon ? iconSource : "",setProgress,getProgress,maxProgress,dependency));

		rs->componentReference = component;
		rs->prototypeReference = proto;
		rs->serialIndex = globalState.allTopics.Count();
		globalState.allTopics << rs;


		if (!parent)
		{
			if (!globalState.rootResearch.Insert(rs))
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

	ResearchTopic::ResearchTopic(const PResearchTopic&parent, const String&name, const String&iconName, 
								const FSetProgress&setProgress, const FGetProgress&getProgress, 
								int maxProgress,
								const PResearchTopic&dependency
								)
								:Super(name), parent(parent),iconName(iconName),
							#ifndef CLIENT
								setProgress(setProgress),getProgress(getProgress),
							#endif
								maxProgress(maxProgress),
								dependency(dependency)
	{}



	ResearchTopic::ResearchTopic::~ResearchTopic()
	{
		bool brk = true;

	}

	bool			ResearchTopic::CanResearch(const PFaction&owner) const
	{
		if (IsGroup())
			return false;
		//auto st = owner ? owner->researchState.researchState : Lua::Reference();
		if (owner && ResearchCompleted(owner->researchState.topicProgressVector))
			return false;
		PResearchTopic p = dependency.lock();
		if (!p)
			return true;
		if (!owner)
			return false;
		return p->ResearchCompleted(owner->researchState.topicProgressVector) || owner->researchState.GetResearchQueue().Contains(p);
	}





	static Array<BaseAttributeTable>	structureTables;


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

				void		ExportTo( const Lua::Reference&state, typename ResearchAttributeType<T>::Attrib&outAttrib ) const
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

				ResearchAttributeFlags::serial_type_t GetFlags() const
				{
					ResearchAttributeFlags::serial_type_t rs = 0;
					if (luaGetValue.IsSet())
						rs |= ResearchAttributeFlags::HasFunctionFlag;
					if (constOverride.isSet)
						rs |= ResearchAttributeFlags::IsConst;
					return rs;
				}



				void		CheckExportTo( const Lua::Reference&state, ResearchAttributeType<T>&outAttribs, index_t at ) const
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
				static void Set(const Lua::Table&source, const String&path, const String&name, LuaAttribute<T,F>&a, bool allowEmpty = false)
				{

					a.Clear(list.Count());
					auto&attrib = list.Append();

					#ifndef CLIENT


						Lua::NamedReference ref = source.GetField(name);
						if (ref.IsEmpty())
						{
							if (!allowEmpty)
								throw Lua::Exception("Field '"+path+"."+name+"' is not defined");
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

			static void ExportTo( const Lua::Reference&state, ResearchAttributeType<T>&outType )
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
		static void InitAttrib(const Lua::Table&source, const String&path, const String&name, LuaAttribute<T,F>&a, bool allowEmpty = false)
		{
			LuaAttributes<T>::Set(source,path,name,a,allowEmpty);
		}




		


	#ifndef CLIENT
		/*static*/ void ResearchState::UpdateAttributeTable( const Lua::Reference&luaResearchState, ResearchAttributeTable&table )
		{
			table.ClearChanges();
			LuaAttributes<int>::ExportTo(luaResearchState,table.ints);
			LuaAttributes<float>::ExportTo(luaResearchState,table.floats);
			LuaAttributes<bool>::ExportTo(luaResearchState,table.bools);
		}

		void ResearchState::UpdateAttributeTable()
		{
			UpdateAttributeTable(luaResearchState,attributeTable);
			topicProgressChanged = true;	//implied
		}
		/*static*/ bool			ResearchState::LuaIntAttributeIsSet( index_t idx )
		{
			return LuaAttributes<int>::IsSet(idx);
		}

		/*static*/ bool			ResearchState::LuaBoolAttributeIsSet( index_t idx )
		{
			return LuaAttributes<bool>::IsSet(idx);
		}

		/*static*/ bool			ResearchState::LuaFloatAttributeIsSet( index_t idx )
		{
			return LuaAttributes<float>::IsSet(idx);
		}

		/*static*/ bool			ResearchState::LuaIntAttributeIsFunctionOrConstNonZero( index_t idx )
		{
			return LuaAttributes<int>::IsFunctionOrConstNonZero(idx);
		}

		/*static*/ bool			ResearchState::LuaBoolIsFunctionOrConstNonZero( index_t idx )
		{
			return LuaAttributes<bool>::IsFunctionOrConstNonZero(idx);
		}

		/*static*/ bool			ResearchState::LuaFloatAttributeIsFunctionOrConstNonZero( index_t idx )
		{
			return LuaAttributes<float>::IsFunctionOrConstNonZero(idx);
		}
	#else
		/*static*/ bool			ResearchState::LuaIntAttributeIsSet( index_t idx )
		{
			return globalState.zeroTable.ints.IsSet(idx);
		}

		/*static*/ bool			ResearchState::LuaBoolAttributeIsSet( index_t idx )
		{
			return globalState.zeroTable.bools.IsSet(idx);
		}

		/*static*/ bool			ResearchState::LuaFloatAttributeIsSet( index_t idx )
		{
			return globalState.zeroTable.floats.IsSet(idx);
		}

		/*static*/ bool			ResearchState::LuaIntAttributeIsFunctionOrConstNonZero( index_t idx )
		{
			return globalState.zeroTable.ints.IsFunctionOrConstNonZero(idx);
		}

		/*static*/ bool			ResearchState::LuaBoolIsFunctionOrConstNonZero( index_t idx )
		{
			return globalState.zeroTable.bools.IsFunctionOrConstNonZero(idx);
		}

		/*static*/ bool			ResearchState::LuaFloatAttributeIsFunctionOrConstNonZero( index_t idx )
		{
			return globalState.zeroTable.floats.IsFunctionOrConstNonZero(idx);
		}
	#endif


	static void InitTable(String baseName, BaseAttributeTable&t)
	{
		baseName.EraseCharacter(' ');
		baseName.EraseCharacter('-');
		
		#ifdef CLIENT
			Lua::Table source;
		#else
			static Lua::Table configTable;
			if (configTable.IsEmpty())
			{
				configTable = ResearchState::globalState.researchProgram->FindGlobal("StructureConfig");
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


	static Lua::Table InitComponent(const Lua::Table&parent, String&path, Component::Base&b)
	{
		String baseName = b.typeName;
		baseName.EraseCharacter(' ');
		baseName.EraseCharacter('-');

		path += '.'+baseName;

		Lua::Table baseTable;

		#ifndef CLIENT
			baseTable = parent.GetField(baseName);
			if (baseTable.IsEmpty())
				throw Lua::Exception("Table '"+path+"' is not defined");
		#endif

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
				configTable = ResearchState::globalState.researchProgram->FindGlobal("VehicleBaseConfig");
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
				configTable = ResearchState::globalState.researchProgram->FindGlobal("TurretConfig");
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
			InitAttrib(myTable,path,String("Op_")+Component::OperationToString((Component::operation_t)i),t.operationSpeed[i],true);
		}

		InitAttrib(myTable,path,"GroundScannerPenetration",t.groundScannerPenetration,true);
		InitAttrib(myTable,path,"GroundScanningRange",t.groundScanningRange,true);

		InitAttrib(myTable,path,"AmmoConsumption",t.ammoNeedPerShot,true);
		InitAttrib(myTable,path,"AttacksPerRound",t.attacksPerRound,true);
		InitAttrib(myTable,path,"HorizontalAttackRange",t.horizontalAttackRange,true);
		InitAttrib(myTable,path,"ExplosionRadius",t.explosionRadius,true);	//can be independent
		InitAttrib(myTable,path,"DamageLevel",t.damageLevel,true);
	}


	/*static*/ void		ResearchState::StaticCreate()
	{
		structureTables.SetSize(Prototypes::structures.Count());
		for (index_t i = 0; i < Prototypes::structures.Count(); i++)
		{
			const PStructure&s = Prototypes::structures[i];
			BaseAttributeTable&t = structureTables[i];
			s->DefineAttributeTable(&t);
		}

		#ifndef CLIENT

			globalState.program0 = ScriptCache::LoadContent(
	"--[[ API:\n"
"\n"
"Handle\tGame.CreateResearch(Handle group, String name, String iconName, advance(ResearchState,int by), getProgress(ResearchState)->int, int maxProgress, Handle dependency)\n"
"\n"
"\tCreates a new research topic or topic group.\n"
"\tLeave maxProgress at 0 to indicate that the research is a topic group\n"
"\n"
"\t@param group Topic group to insert this research into. Set nil to declare the research a root topic\n"
"\t@param name String identifier of the research topic\n"
"\t@param iconName Name of the component, prototype, or image to display. Leave empty to reuse the content of [name] instead\n"
"\t@param advance Function/method that accepts an instance of ResearchState and an integer containing research points (>0), \n"
"\t\t\t\t\tto be assigned to this research topic. Leave empty if this is supposed to be a topic group\n"
"\t@param getProgress Function/method that accepts an instance of ResearchState and returns the current progress of this research topic. \n"
"\t\t\t\t\tLeave empty if this is supposed to be a topic group\n"
"\t@param maxProgress Maximum possible amount of research progress that can be invested into this topic. Use negative values to indicate\n"
"\t\t\t\t\tan infinitely researchable topic. Leave at 0 if this is supposed to be a topic group\n"
"\t@param dependency Research topic handle that this research depends on. Researching this topic will only be allowed once the specified\n"
"\t\t\t\t\tdependency is completed\n"
"\t@return Handle to the new topic group\n"
"\n"
"\t\n"
"void\tGame.Log(msg)\t\t\t\t\tPrints any message to Hexaverse log (client or server-private)\n"
"\n"
"bool\tGame.LowerResolutionPlanet\t\tSet to true if the planet has reduced grid resolution (1/4 of cells, each cell twice as high)\n"
"float\tGame.UnitScale\t\t\t\t\tSet to a global unit scale corresponding to [Game.LowerResolutionPlanet]. This scale value is to be\n"
"\t\t\t\t\t\t\t\t\t\tapplied too units (hence the name). 1 if the planet is not of lowered resolution, 2 otherwise.\n"
"\n"
"\n"
"Hexaverse expects the following classes and methods to be defined:\n"
"\tResearchState - table, arbitrary content. Expected but not required to mimic a class\n"
"\tResearchState.New() - constructs a new instance (expected to be of its own type, but not required)\n"
"\tResearchState:Export() - expected to return the serializable content of the local research state.\n"
"\t\t\t\t\t\t\tExpected but not required to operate on instances of type ResearchState. State is passed as first function\n"
"\t\t\t\t\t\t\tparameter (e.g. [self] in a method).\n"
"\t\t\t\t\t\t\tResult must not contain non-serializable content. Serializable content includes tables, ints, floats, and strings, \n"
"\t\t\t\t\t\t\tboth as keys and values.\n"
"\tResearchState:Import(exported) - expected to integrate a previously exported, deserialized research state. Should only accept recognized data,\n"
"\t\t\t\t\t\t\t\t\tand discard the rest. Data may come from older savegames containing outdated research formats.\n"
"\t\t\t\t\t\t\t\t\tExpected but not required to operate on instances of type ResearchState. State is passed as first function\n"
"\t\t\t\t\t\t\t\t\tparameter (e.g. [self] in a method).\n"
"]]\n"
"\n"
"\n"
"\n"
"\n"
"researchables = {}\n"
"\n"
"\n"
"ResearchEntry = {}\n"
"\n"
"function ResearchEntry.New(limit)\n"
"\tlocal rs = {}\n"
"\trs.limit = limit\n"
"\treturn rs\n"
"end\n"
"function ResearchEntry.NewBool(limit)\n"
"\treturn ResearchEntry.New(limit)\n"
"end\n"
"function ResearchEntry.NewFloat()\n"
"\treturn ResearchEntry.New(-1)\n"
"end\n"
"\n"
"\n"
"\n"
"ResearchState = {}\n"
"function ResearchState.New()\n"
"\tlocal rs = {}\n"
"\trs.researchProgress = {}\n"
"\tfor k,v in pairs(researchables) do\n"
"\t\trs.researchProgress[k] = 0;\n"
"\tend\n"
"\treturn rs\n"
"end\n"
"\n"
"function ResearchState.Export(self)\n"
"\treturn self.researchProgress;\n"
"end;\n"
"\n"
"function ResearchState.GetProgress(self, key--[[string]])\n"
"\tif type( self.researchProgress[key] ) == nil then\n"
"\t\tGame.Log(\"Research state lacks key '\"..tostring(key)..\"'. Instantiating new\");\n"
"\t\tself.researchProgress[key] = 0;\n"
"\tend;\n"
"\treturn self.researchProgress[key];\n"
"end;\n"
"\n"
"function ResearchState.SetProgress(self, key--[[string]], progress--[[int]])\n"
"\tself.researchProgress[key] = progress;\n"
"end;\n"
"\n"
"\n"
"function ResearchState.GetLimit(self, key--[[string]])\n"
"\tif type( researchables[key] ) == nil then\n"
"\t\treturn 0;\n"
"\tend;\n"
"\treturn researchables[key].limit;\n"
"end;\n"
"\n"
"\n"
"\n"
"function ResearchState.Import(self, exported)\n"
"\tif type(exported) ~= 'table' then\n"
"\t\treturn;\n"
"\tend;\n"
"\tself.researchProgress = {};\n"
"\tfor k,v in pairs(exported) do\n"
"\t\tif type(researchables[k]) ~= 'nil' then\n"
"\t\t\tif type(v) == 'table' then\n"
"\t\t\t\tself.researchProgress[k] = v.progress;\n"
"\t\t\t\tGame.Log(\"Research '\"..tostring(k)..\"' is old table format. Converting...\");\n"
"\t\t\telse\n"
"\t\t\t\tself.researchProgress[k] = v;\n"
"\t\t\tend;\n"
"\t\telse\n"
"\t\t\tGame.Log(\"Research '\"..tostring(k)..\"' is not supported by this research script. Ignoring...\");\n"
"\t\tend;\n"
"\tend;\n"
"\tfor k,v in pairs(researchables) do\n"
"\t\tif type(self.researchProgress[k]) == 'nil' then\n"
"\t\t\tGame.Log(\"Imported research state lacks key '\"..tostring(k)..\"'. Setting to 0 progress\");\n"
"\t\t\tself.researchProgress[k] = 0;\n"
"\t\tend;\n"
"\tend;\n"
"end\n"
"\n"
"\n"
"\n"
"\n"
"\n"
"\n"
"\n"
"\n"
"\n"
"UnlockProgress = {}\n"
"function UnlockProgress.New()\n"
"\tlocal rs = {}\n"
"\trs.progress = 0;\n"
"\treturn rs\n"
"end\n"
"\n"
"\n"
"--[[ Helper functions ]]\n"
"\n"
"\n"
"function Round(value--[[float]], digits--[[optional int]])\n"
"\tlocal factor = 10^(digits or 0);\n"
"\tlocal result = math.floor(value * factor + 0.5) / factor;\n"
"\tlocal floored = math.floor(value);\n"
"\tif (floored==value) then\n"
"\t\treturn floored;\n"
"\tend\n"
"\treturn result;\n"
"end\n"
"\n"
"function DefaultHealthAtCost(cost--[[float]]) --float\n"
"\treturn cost /5\n"
"end\n"
"\n"
"function ExplicitStructure(cost, health, scanningRange, isUnlocked)\n"
"\treturn\n"
"\t{\n"
"\t\tCost = cost,\n"
"\t\tHealth = health,\n"
"\t\tScanningRange = scanningRange,\n"
"\t\tIsUnlocked = isUnlocked\n"
"\t}\n"
"end\n"
"\n"
"function Structure(cost, scanningRange)\n"
"\treturn ExplicitStructure(cost, DefaultHealthAtCost(cost),scanningRange, true)\n"
"end\n"
"\n"
"function DefaultStructure(cost)\n"
"\treturn Structure(cost,2)\n"
"end\n"
"\n"
"function Get(attribute, state)\n"
"\tif (attribute == nil) then\n"
"\t\treturn 0\n"
"\tend\n"
"\tt = type(attribute)\n"
"\tif t == 'table' then\n"
"\t\treturn attribute.Get(state)\n"
"\tend\n"
"\tif t == 'function' then\n"
"\t\treturn attribute(state)\n"
"\tend\n"
"\treturn attribute\n"
"end\n"
"\n"
"function Describe(attribute, state)\n"
"\tif (attribute == nil) then\n"
"\t\treturn '0'\n"
"\tend\n"
"\tt = type(attribute)\n"
"\tif t == 'table' then\n"
"\t\treturn attribute.Describe(state)\n"
"\tend\n"
"\tif t == 'function' then\n"
"\t\treturn attribute(state)\n"
"\tend\n"
"\treturn attribute\n"
"end\n"
"\n"
"\n"
"--[[ Turret configurations ]]\n"
"TurretConfig = \n"
"{\n"
"\tMissileLauncher = \n"
"\t{\n"
"\t\tCost = 40,\n"
"\t\tWeight = 13,\n"
"\t\tHealthMultiplier = 1.5,\n"
"\t\tScanningRange = 4,\n"
"\t\tAttacksPerRound = 1,\n"
"\t\tDamageLevel = 10,\n"
"\t\tExplosionRadius = 1,\n"
"\t\tHorizontalAttackRange = 8,\n"
"\t\tIsUnlocked = true,\n"
"\t},\n"
"\tRocketLauncher = \n"
"\t{\n"
"\t\tCost = 15,\n"
"\t\tWeight = 3,\n"
"\t\tHealthMultiplier = 2,\n"
"\t\tScanningRange = 5,\n"
"\t\tAttacksPerRound = 2,\n"
"\t\tDamageLevel = 7,\n"
"\t\tExplosionRadius = 1,\n"
"\t\tHorizontalAttackRange = 4,\n"
"\t\tIsUnlocked = true,\n"
"\t},\n"
"\tCannon = \n"
"\t{\n"
"\t\tCost = 15,\n"
"\t\tWeight = 3,\n"
"\t\tHealthMultiplier = 2,\n"
"\t\tScanningRange = 5,\n"
"\t\tAttacksPerRound = 2,\n"
"\t\tDamageLevel = 7,\n"
"\t\tExplosionRadius = 1,\n"
"\t\tHorizontalAttackRange = 5,\n"
"\t\tIsUnlocked = true,\n"
"\t},\n"
"\tRadar =\n"
"\t{\n"
"\t\tCost = 10,\n"
"\t\tWeight = 2,\n"
"\t\tHealthMultiplier = 1,\n"
"\t\tScanningRange = 7,\n"
"\t\tIsUnlocked = true,\n"
"\t},\n"
"\tSurveyor =\n"
"\t{\n"
"\t\tCost = 10,\n"
"\t\tWeight = 4,\n"
"\t\tHealthMultiplier = 1,\n"
"\t\tScanningRange = 4,\n"
"\t\tIsUnlocked = true,\n"
"\t},\n"
"\tBomb =\n"
"\t{\n"
"\t\tOp_Nuclear = 50,\n"
"\t\tCost = 40,\n"
"\t\tWeight = 5,\n"
"\t\tHealthMultiplier = 1,\n"
"\t\tScanningRange = 1,\n"
"\t\tExplosionRadius = 4,\n"
"\t\tDamageLevel = 10,\n"
"\t\tHorizontalAttackRange = 0,\n"
"\t\tIsUnlocked = true,\n"
"\t},\n"
"\tEMP =\n"
"\t{\n"
"\t\tCost = 100,\n"
"\t\tWeight = 5,\n"
"\t\tHealthMultiplier = 1,\n"
"\t\tScanningRange = 3,\n"
"\t\tAttacksPerRound = 1,\n"
"\t\tDamageLevel = 5,\n"
"\t\tExplosionRadius = 3,\n"
"\t\tHorizontalAttackRange = 15,\n"
"\t\tAmmoConsumption = 50,\n"
"\t\tIsUnlocked = true,\n"
"\t},\n"
"\tArtillery =\n"
"\t{\n"
"\t\tCost = 25,\n"
"\t\tWeight = 3,\n"
"\t\tHealthMultiplier = 1.5,\n"
"\t\tScanningRange = 4,\n"
"\t\tAttacksPerRound = 1,\n"
"\t\tDamageLevel = 7,\n"
"\t\tExplosionRadius = 2,\n"
"\t\tHorizontalAttackRange = 7,\n"
"\t\tIsUnlocked = false,\n"
"\t},\n"
"\tTerraformer =\n"
"\t{\n"
"\t\tCost = 40,\n"
"\t\tWeight = 2,\n"
"\t\tHealthMultiplier = 1,\n"
"\t\tScanningRange = 4,\n"
"\t\tOp_Construct = 10,\n"
"\t\tOp_TerrainRemove = 10,\n"
"\t\tOp_TerrainAdd = 10,\n"
"\t\tIsUnlocked = true,\n"
"\t},\n"
"\tMechanic =\n"
"\t{\n"
"\t\tCost = 30,\n"
"\t\tWeight = 2,\n"
"\t\tHealthMultiplier = 1,\n"
"\t\tScanningRange = 3,\n"
"\t\tOp_Repair = 10,\n"
"\t\tIsUnlocked = true,\n"
"\t},\n"
"}\n"
"\n"
"\n"
"VehicleBaseConfig = \n"
"{\n"
"\tHoverbase = \n"
"\t{\n"
"\t\tCost = 30,\n"
"\t\tMovementSpeed = 20,\n"
"\t\tCarryingCapacity = 1,\n"
"\t\tHealth = 4,\n"
"\t\tOperationSpeedMultiplier = 1,\n"
"\t\tTurretRangeModifier = 0,\n"
"\t\tIsUnlocked = true,\n"
"\t},\n"
"\tHeavyBase = \n"
"\t{\n"
"\t\tCost = 45,\n"
"\t\tMovementSpeed = 15,\n"
"\t\tCarryingCapacity = 10,\n"
"\t\tHealth = 12,\n"
"\t\tOperationSpeedMultiplier = 2,\n"
"\t\tTurretRangeModifier = 0,\n"
"\t\tIsUnlocked = true,\n"
"\t},\n"
"\tCatamaranBase = \n"
"\t{\n"
"\t\tCost = 60,\n"
"\t\tMovementSpeed = 8,\n"
"\t\tCarryingCapacity = 15,\n"
"\t\tHealth = 12,\n"
"\t\tOperationSpeedMultiplier = 2,\n"
"\t\tTurretRangeModifier = 1,\n"
"\t\tIsUnlocked = true,\n"
"\t},\n"
"}\n"
"\n"
"\n"
"--[[\n"
"Declares an item as unlock-researchable.\n"
"Manipulates the IsUnlocked attribute of the specified item.\n"
"Automatically registers everything\n"
"\n"
"@param researchGroup\t\tResearch-group handle (folder) that this new research should be filed under. May be nil, causing this to be filed as a root research topic\n"
"@param researchDependency\tResearch handle (actual research) that must be completed before this research may commence. Must be a non-improve research. May be nil\n"
"@param typeTable\t\t\tHandle to the table that the specified type is a member in\n"
"@param typeName\t\t\t\tName (string) of the type the attribute belongs to\n"
"@param limit\t\t\t\tMax research (int). Progress must reach this value for the item to become unlocked\n"
"\n"
"@return \t\t\t\t\tResulting research handle, as returned from Hexaverse\n"
"]]\n"
"function MakeUnlockResearchable(researchGroup, researchDependency, typeTable, typeName, limit)\n"
"\t--entry = ResearchEntry.New(original)\n"
"\tlocal key = 'unlock'..typeName\n"
"\tresearchables[key] = ResearchEntry.NewBool(limit)\n"
"\tlocal typeHandle = typeTable[typeName]\n"
"\ttypeHandle.IsUnlocked =\n"
"\t{\n"
"\t\tGet = function(state)\n"
"\t\t\tif state == nil then\n"
"\t\t\t\t--Game.Log(key..\": no state. false\");\n"
"\t\t\t\treturn false\n"
"\t\t\tend\n"
"\t\t\t--Game.Log(key..\": \"..ResearchState.GetProgress(state,key)..'/'..state:GetLimit(key));\n"
"\t\t\treturn ResearchState.GetProgress(state,key) >= ResearchState.GetLimit(state, key);\n"
"\t\tend,\n"
"\t\tDescribe = function(state)\n"
"\t\t\tif state == nil then\n"
"\t\t\t\treturn false\n"
"\t\t\tend\n"
"\t\t\treturn ResearchState.GetProgress(state,key) >= ResearchState.GetLimit(state, key)\n"
"\t\tend\n"
"\t}\n"
"\t\n"
"\treturn Game.CreateResearch(researchGroup,typeName,nil,\n"
"\t\tfunction(state,progress) ResearchState.SetProgress(state, key, progress) end,\n"
"\t\tfunction(state) return ResearchState.GetProgress(state, key); end, \n"
"\t\tresearchables[key].limit, researchDependency\n"
"\t\t);\n"
"\t\n"
"end\n"
"\n"
"\n"
"--[[\n"
"Calculates the factor that is to be multiplied with the base attribute to get the final attribute\n"
"\n"
"@param researchProgress\t\tActual progress (float) in the chosen research topic. Can be negative causing degeneration of the attribute towards zero\n"
"@param progressMultiplier\tPositive speed modifier (float) that manipulates how quickly research progresses. 1 leaves research speed unmodified\n"
"\n"
"@return \t\t\t\t\tResulting attribute modifier\n"
"]]\n"
"function GetResearchImprovementFactor(researchProgress--[[float]],progressMultiplier--[[float]])  --[[float]]\n"
"\tlocal totalProgress =  math.sqrt(math.abs(researchProgress)) * progressMultiplier;\n"
"\tif (researchProgress < 0) then\n"
"\t\treturn 1 / (1 + totalProgress);\n"
"\tend\n"
"\treturn (1 + totalProgress);\n"
"end\n"
"\n"
"\n"
"--[[\n"
"Declares a currently non-researchable attribute as improvement-researchable.\n"
"Works on all floating point attributes.\n"
"Improvement-research goes on indefinitely.\n"
"Automatically registers everything\n"
"\n"
"@param researchGroup\t\tResearch-group handle (folder) that this new research should be filed under. May be nil, causing this to be filed as a root research topic\n"
"@param researchDependency\tResearch handle (actual research) that must be completed before this research may commence. Must be a non-improve research. May be nil\n"
"@param typeTable\t\t\tHandle to the table that the specified type is a member in\n"
"@param typeName\t\t\t\tName (string) of the type the attribute belongs to\n"
"@param attributeName\t\tName (string) of the attribute to research\n"
"@param speed\t\t\t\tFloating point speed modifier. The closer to zero, the slower. \n"
"\t\t\t\t\t\t\tPositive values affect the resulting factor linearly. \n"
"\t\t\t\t\t\t\tIf 10 rounds are needed to reach 1.2 at factor 0.02, then another 10 rounds will produce 1.4.\n"
"\t\t\t\t\t\t\tNegative values affect the resulting factor inversely.\n"
"\t\t\t\t\t\t\tIf 10 rounds are needed to reach 1/(1.2)=83% at factor -0.02, then another 10 rounds will produce 1/(1.4)=71%\n"
"\t\t\t\t\t\t\t\n"
"@return \t\t\t\t\tResulting research handle, as returned from Hexaverse\n"
"]]\n"
"function MakeImproveResearchable(researchGroup, researchDependency, typeTable, typeName, attributeName, speed)\n"
"\tlocal key = 'improve'..typeName;\n"
"\tlocal typeHandle = typeTable[typeName];\n"
"\t--at this point we assume that typeHandle[attributeName] is still a simple variable. we can add a check here but then ResearchEntry.NewFloat would get really messy\n"
"\tlocal original = typeHandle[attributeName];\n"
"\tresearchables[key] = ResearchEntry.NewFloat();\n"
"\ttypeHandle[attributeName] =\n"
"\t{\n"
"\t\tGet = function(state)\n"
"\t\t\tif state == nil then\n"
"\t\t\t\treturn original;\n"
"\t\t\tend\n"
"\t\t\tlocal progress = ResearchState.GetProgress(state, key);\n"
"\t\t\tlocal factor = GetResearchImprovementFactor(progress,speed);\n"
"\t\t\tlocal result = original * factor;\n"
"\t\t\treturn result;\n"
"\t\tend,\n"
"\t\t\n"
"\t\tDescribe = function(state)\n"
"\t\t\tif state == nil then\n"
"\t\t\t\treturn original;\n"
"\t\t\tend\n"
"\t\t\tlocal progress = ResearchState.GetProgress(state,key);\n"
"\t\t\tif (progress == 0) then\n"
"\t\t\t\treturn original;\n"
"\t\t\tend\n"
"\t\t\tlocal factor = GetResearchImprovementFactor(progress,speed);\n"
"\t\t\treturn Round(original * factor,2)..' [='.. original..'*'..Round(factor,2)..']';\n"
"\t\tend\n"
"\t}\n"
"\t\n"
"\treturn Game.CreateResearch(researchGroup,typeName,nil,\n"
"\t\tfunction(state,progress) ResearchState.SetProgress(state, key, progress) end,\n"
"\t\tfunction(state) return ResearchState.GetProgress(state,key) end, -1, researchDependency\n"
"\t\t);\n"
"end\n"
"\n"
"\n"
"\n"
"function CanAttack(turret--[[Table]])\n"
"\treturn Get(turret.AttacksPerRound,nil) > 0\n"
"end\n"
"\n"
"function TowerScanningModifier(turret--[[Table]])\n"
"\tif CanAttack(turret) then\n"
"\t\treturn -2.0\t--penalty for offensive towers\n"
"\tend\n"
"\treturn 2.0\t--boost non-offensive towers\n"
"end\n"
" \n"
"\n"
"--[[\n"
"Constructs a tower structure configuration from a given turret entry.\n"
"Defines both Get and Describe functions\n"
"]]\n"
"function Tower(turret--[[Table]]) --Table\n"
"\tlocal cost = function(state) return 40 + Get(turret.Cost,state) end;\n"
"\tlocal rs =\n"
"\t{\n"
"\t\tCost =\n"
"\t\t{\n"
"\t\t\tGet = cost,\n"
"\t\t\tDescribe = \n"
"\t\t\t\tfunction(state) \n"
"\t\t\t\t\treturn '40+'.. Describe(turret.Cost,state)\n"
"\t\t\t\tend,\n"
"\t\t},\n"
"\t\tHealth = \n"
"\t\t{\n"
"\t\t\tGet = \n"
"\t\t\t\tfunction(state) \n"
"\t\t\t\t\treturn DefaultHealthAtCost(cost()) * Get(turret.HealthMultiplier,state) \n"
"\t\t\t\tend,\n"
"\t\t\tDescribe = \n"
"\t\t\t\tfunction(state) \n"
"\t\t\t\t\tlocal multiplier = Describe(turret.HealthMultiplier,state);\n"
"\t\t\t\t\tlocal health = DefaultHealthAtCost(cost());\n"
"\t\t\t\t\tif (multiplier == 1) then\n"
"\t\t\t\t\t\treturn Round(health);\n"
"\t\t\t\t\tend\n"
"\t\t\t\t\treturn Round(health * Get(turret.HealthMultiplier,state),2)..' [='.. Round(health)..'*'.. multiplier..' (turret health multiplier)]';\n"
"\t\t\t\tend,\n"
"\t\t},\n"
"\t\tScanningRange =\n"
"\t\t{\n"
"\t\t\tGet = \n"
"\t\t\t\tfunction(state) \n"
"\t\t\t\t\treturn Get(turret.ScanningRange,state) + TowerScanningModifier(turret)\n"
"\t\t\t\tend,\n"
"\t\t\tDescribe = \n"
"\t\t\t\tfunction(state) \n"
"\t\t\t\t\tlocal modifier =  TowerScanningModifier(turret);\n"
"\t\t\t\t\tlocal base = Describe(turret.ScanningRange,state);\n"
"\t\t\t\t\tif (modifier == 0) then\n"
"\t\t\t\t\t\treturn base;\n"
"\t\t\t\t\tend\n"
"\t\t\t\t\tif (modifier > 0) then\n"
"\t\t\t\t\t\treturn Round(Get(turret.ScanningRange,state)+modifier,2)..' [='..  base ..'+'..Round(modifier,2)..' (tower modifier)]';\n"
"\t\t\t\t\tend\n"
"\t\t\t\t\treturn Round(Get(turret.ScanningRange,state)+modifier,2)..' [='..  base ..'-'..Round(-modifier,2)..' (tower modifier)]';\n"
"\t\t\t\tend,\n"
"\t\t},\n"
"\t\tIsUnlocked = \n"
"\t\t\tfunction(state) \n"
"\t\t\t\treturn Get(turret.IsUnlocked,state) \n"
"\t\t\tend,\n"
"\t}\n"
"\treturn rs;\n"
"end\n"
"\n"
"\n"
"--[[\n"
"Constructs a tower structure configuration from a given turret entry.\n"
"Defines both Get and Describe functions.\n"
"It is declared as locked\n"
"]]\n"
"function DeprecatedTower(turret--[[Table]]) --Table\n"
"\tlocal rs = Tower(turret);\n"
"\trs.IsUnlocked = false;\t--make sure it's not a function. If it's a function, the game assumes it can be unlocked, and displays it as a construction choice\n"
"\treturn rs;\n"
"end\n"
"\n"
"\n"
"StructureConfig =\n"
"{\n"
"\tMine = DefaultStructure(40),\n"
"\tGenerator = DefaultStructure(40),\n"
"\tReactorCore = DefaultStructure(120),\n"
"\tVehicleStore = DefaultStructure(40),\n"
"\tVehiclePlant = DefaultStructure(80),\n"
"\tLaboratory = DefaultStructure(80),\n"
"\tMunitionFactory = DefaultStructure(40),\n"
"\tConnector = DefaultStructure(5),\n"
"\tFuelContainer = DefaultStructure(10),\n"
"\tMineralContainer = DefaultStructure(10),\n"
"\tUraniumStorage = DefaultStructure(10),\n"
"\tMunitionStorage = DefaultStructure(10),\n"
"\tNexusPort = ExplicitStructure(0,1,2,false),\n"
"\tMissileLauncherTower = Tower(TurretConfig.MissileLauncher),\n"
"\tRocketLauncherTower = Tower(TurretConfig.RocketLauncher),\n"
"\tRadarTower = Tower(TurretConfig.Radar),\n"
"\tCannonTower = DeprecatedTower(TurretConfig.Cannon),\n"
"\tSurveyorTower = Tower(TurretConfig.Surveyor),\n"
"}\n"
"\n"
"\n"
"\n"
"\n"
"unlockGroup = Game.CreateResearch(nil,\"Unlock\",\"Research_Unlock\");\n"
"improveGroup = Game.CreateResearch(nil,\"Improve\",'Increase');\n"
"\n"
"\n"
"\n"
"artilleryUnlock = MakeUnlockResearchable(unlockGroup, nil, TurretConfig, 'Artillery',20);\n"
"empUnlock = MakeUnlockResearchable(unlockGroup, artilleryUnlock, TurretConfig, 'EMP',40);\n"
"bombUnlock = MakeUnlockResearchable(unlockGroup, artilleryUnlock, TurretConfig, 'Bomb',40);\n"
"launcherUnlock = MakeUnlockResearchable(unlockGroup, nil, TurretConfig, 'MissileLauncher',20);\n"
"--radarUnlock = MakeUnlockResearchable(unlockGroup, nil, TurretConfig, 'Radar',20);\n"
"heavyUnlock = MakeUnlockResearchable(unlockGroup, nil, VehicleBaseConfig, 'HeavyBase',20);\n"
"catUnlock = MakeUnlockResearchable(unlockGroup, nil, VehicleBaseConfig, 'CatamaranBase',20);\n"
"MakeUnlockResearchable(unlockGroup, nil, StructureConfig, 'ReactorCore',10);\n"
"\n"
"attackRangeGroup = Game.CreateResearch(improveGroup,'Attack Range','Research_AttackRange');\n"
"\tMakeImproveResearchable(attackRangeGroup, artilleryUnlock, TurretConfig, 'Artillery', 'HorizontalAttackRange',0.005);\n"
"\tMakeImproveResearchable(attackRangeGroup, launcherUnlock, TurretConfig, 'MissileLauncher', 'HorizontalAttackRange',0.005);\n"
"\tMakeImproveResearchable(attackRangeGroup, nil, TurretConfig, 'RocketLauncher', 'HorizontalAttackRange',0.005);\n"
"\tMakeImproveResearchable(attackRangeGroup, nil, TurretConfig, 'Cannon', 'HorizontalAttackRange',0.005);\n"
"\n"
"scanningRangeGroup = Game.CreateResearch(improveGroup,'Scanning Range','Research_ScanningRange');\n"
"\tMakeImproveResearchable(scanningRangeGroup, radarUnlock, TurretConfig, 'Radar', 'ScanningRange',0.005);\n"
"\n"
"healthGroup = Game.CreateResearch(improveGroup,'Health','Research_Health');\n"
"\tMakeImproveResearchable(healthGroup, nil, VehicleBaseConfig, 'Hoverbase', 'Health',0.05);\n"
"\tMakeImproveResearchable(healthGroup, heavyUnlock, VehicleBaseConfig, 'HeavyBase', 'Health',0.05);\n"
"\tMakeImproveResearchable(healthGroup, catUnlock, VehicleBaseConfig, 'CatamaranBase', 'Health',0.05);\n"
"\n"
"\n"
""
			);

			#ifdef _DEBUG
				ActivateProgram(globalState.program0);	//test fallback program
			#endif

			//.reset(new ScriptCache::Script());
		

			#if defined _DEBUG || !defined CLIENT
				LoadDefaultProgram();
			#endif
		#else
			RemapAttributes();
		#endif
	}


	/*static*/ void ResearchState::RemapAttributes()
	{
		LuaAttributes<int>::Clear();
		LuaAttributes<bool>::Clear();
		LuaAttributes<float>::Clear();

		for (index_t i = 0; i < Prototypes::structures.Count(); i++)
		{
			const PStructure&s = Prototypes::structures[i];
			auto&t = structureTables[i];
			InitTable(s->GetTypeName(),t);
		}

		foreach (Component::allTurretComponents,turret)
			InitTurret(**turret);
		foreach (Component::baseComponents,base)
			InitVehicleBase(**base);
	}


	#ifndef CLIENT
		/*static*/ void			ResearchState::LoadDefaultProgram()
		{
			PathString factoryDefault = "../lua/research.v2.lua";
			#if 0 && defined _DEBUG
				PathString final = factoryDefault;
			#else
				FileSystem::CreateDirectory(RuntimeConfig::myConfig.Lookup("config/lua"));
				FileSystem::CreateDirectory(RuntimeConfig::sharedConfig.Lookup("config/lua"));
				PathString final = RuntimeConfig::myConfig.Lookup("config/lua/research.v2.lua");
				if (!FileSystem::IsFile(final))
					final = RuntimeConfig::sharedConfig.Lookup("config/lua/research.v2.lua");
				if (!FileSystem::IsFile(final))
				{
					final = factoryDefault;
					#if 0	//leave this disabled. do not copy automatically
						if (FileSystem::CopyFile("../lua/research.lua",final,false))
							LogEvent(EventClass::Preset::configuration,"Copied ../lua/research.lua to "+String(final));
						else
							FATAL__("Unable to copy ../lua/research.lua to "+String(final));
					#endif /*0*/
				}
			#endif
			globalState.factoryDefault = ScriptCache::Load(factoryDefault); 
			ActivateProgram(ScriptCache::Load(final));
		}



		/*static*/ void		ResearchState::ActivateProgram(const ScriptCache::PScript&script)
		{
			//#if defined _DEBUG && defined HAS_WINDOW
			//	if (globalState.script)
			//		return;		//prevent reloading
			//#endif
			if (!script || script == globalState.script)
				return;
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

			if (current && current != globalState.program0)
			{
				LogEvent(EventClass(EventSignificance::NonFatalError,EventFlavor::ServerPrivate),"Falling back to previous script");
				RequireLoad(current);
			}
			else
			{
				if (script == globalState.factoryDefault)
				{
					LogEvent(EventClass(EventSignificance::NonFatalError,EventFlavor::ServerPrivate),"Unable to load factory default. Falling back to emergency script");
					RequireLoad(globalState.program0);
				}
				else
				{
					ASSERT__(globalState.factoryDefault);
					LogEvent(EventClass(EventSignificance::NonFatalError,EventFlavor::ServerPrivate),"Falling back to factory default script");
					ActivateProgram(globalState.factoryDefault);
				}
			}
		}

		/*static*/ void ResearchState::RequireLoad(const ScriptCache::PScript&script)
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
		
		/*static*/ void	ResearchState::InnerLoad( const ScriptCache::PScript&script )
		{
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

			RemapAttributes();


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

		void ResearchState::LoadNexusResearchState()
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

		void					ResearchState::AdvanceNexusResearch()
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


		void					ResearchState::FinalizeRound(const PFaction&f)
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


		//void					ResearchState::ImportResearchState(const ResearchProgressVector&archived)
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

	
		void					ResearchState::ImportLuaState(const ArrayRef<const BYTE>&serializedState)
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
	
	
		void					ResearchState::ExportLuaState(Array<BYTE>&outSerializedState) const
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

		/*static*/ void				ResearchState::BeginServerTopicUpdate()
		{
			globalState.rootResearch.Clear();
			globalState.allTopics.Clear();
		}

		/*static*/ void				ResearchState::DeclareServerTopic(index_t parentTopicIndex, const String&topicName, const String&iconName, int maxProgress, index_t dependentTopicIndex)
		{
			CreateResearch(parentTopicIndex != InvalidIndex ? Expect::SafeGet(globalState.allTopics,parentTopicIndex,CLOCATION) : nullptr,topicName,iconName,ResearchTopic::FSetProgress(),ResearchTopic::FGetProgress(), maxProgress,dependentTopicIndex != InvalidIndex ? Expect::SafeGet(globalState.allTopics,dependentTopicIndex,CLOCATION):nullptr);
		}
	#endif


	ResearchState::ResearchState( Faction*owner, bool isNexus ):owner(owner)
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
	
	bool					ResearchGroup::Insert(const PResearchTopic&t)
	{
		if (Find(t->name))
			return false;
		map.Set(t->name,t);
		children << t;
		return true;
	}


	PResearchTopic			ResearchGroup::Find(const StringRef&name) const
	{
		//PResearchTopic t;
		//map.Query(name,t);
		//return t;
		foreach (children,c)
			if ((*c)->name == name)	//i suspect this is actually faster for slow numbers of topics per family, since it does not force a String copy
				return *c;
		return PResearchTopic();
	}


	static PResearchTopic FindTopicOfPrototypeIn(const PResearchTopic&topic, const PWorldEntity&proto)
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

	static PResearchTopic FindTopicOfPrototypeIn(const PResearchTopic&topic, const Component::Base*proto)
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

	/*static*/ PResearchTopic	ResearchState::FindTopicOfPrototype(const Component::Base*proto)
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

	/*static*/ PResearchTopic	ResearchState::FindTopicOfPrototype(const PWorldEntity&proto)
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


	/*static*/ PResearchTopic	ResearchState::FindTopic(const StringRef&path)
	{
		Ctr::Array<StringRef>	components;
		explode('/',path,components);
		PResearchTopic	current = globalState.rootResearch.Find(components.First());
		for (index_t i = 1; i < components.Count(); i++)
		{
			if (!current)
				return PResearchTopic();
			current = current->Find(components[i]);
		}
		return current;
	}

	/*static*/ PResearchTopic	ResearchState::GetTopic(index_t serialIndex)
	{
		return serialIndex < globalState.allTopics.Count() ? globalState.allTopics[serialIndex] : PResearchTopic();
	}

	bool	ResearchState::ResearchIsPlanned(const PResearchTopic&topic) const
	{
		return researchQueue.Contains(topic);
	}

	bool	ResearchState::ResearchQueueIsFull() const
	{
		return researchQueue.Count() >= MaxResearchQueueLength;
	}

	bool	ResearchState::AppendResearchTask(const PResearchTopic&active)
	{
		if (!active || ResearchQueueIsFull() || !active->CanResearch(topicProgressVector,researchQueue.ToRef()) || researchQueue.Contains(active))
			return false;
		researchQueue << active;
		return true;
	}

	bool					ResearchState::DropResearchTask(const PResearchTopic&active)
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


	void		ResearchState::RestoreResearchQueue(const String&source)
	{
		researchQueue.Clear();
		Ctr::Array<StringRef> paths;
		explode(';',source,paths);
		foreach (paths,p)
			AppendResearchTask(FindTopic(*p));
		globalState.onQueueChange(owner);
	}

	String		ResearchState::ExportResearchQueue()
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

	void		ResearchState::ExportResearchQueue(Ctr::Array<UINT32>&outRS)
	{
		outRS.SetSize(researchQueue.Count());
		for (index_t i = 0; i < researchQueue.Count(); i++)
			outRS[i] = ((UINT32)(researchQueue[i]->serialIndex));
	}
	void					ResearchState::RestoreResearchQueue(const Ctr::Array<UINT32>&q)
	{
		researchQueue.Clear();
		foreach (q,e)
			AppendResearchTask(GetTopic(*e));
		globalState.onQueueChange(owner);
	}






}

