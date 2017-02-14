#include <global_root.h>
#include <global_string.h>
#include <math/basic.h>
#include "reflection.h"
#include "state.h"
#include "journalSaveGame.h"
#include <io/config.h>
#include <io/file_system.h>
#include "aiProcess.h"
#include "configurationMode.h"

namespace Reflection
{
	CLI::Interpreter	state;

	static CLI::PFolder factionFolder;

	namespace Variable
	{
		PBase	planetType;	
	
		void	Walk(const CLI::PFolder&f, const Variable::FCallback&callback)
		{
			for (const auto&var : f->variables)
			{
				Reflection::Variable::PBase&v = To<Reflection::Variable::Base>::DynamicCast(var);
				if (v)
				{
					callback(v);
				}
			}
			for (const auto&dir : f->folders)
				Walk(dir,callback);
		}

		/*virtual override*/ bool		Base::IsWriteProtected() const
		{
			if (persistence == Persistence::PlanetConfig)
				return !ConfigurationMode::IsActive() || Super::IsWriteProtected();
			return Super::IsWriteProtected();
		}
		

		void				Base::OnChanged()
		{
			if (extraOnChange)
				extraOnChange(*this);
			if (WriteToSavegame())
			{
				Journal::currentRound.RegisterPersistentVariableChange(false,To<Base>::Cast(shared_from_this()));
			}
			lastValue = ToString();
		};

		void				Base::SetupBase()
		{
			lastValue = ToString();
		}


		/**
		Container for regular string variables.

		*/
		class StringVar : public Base
		{
			typedef Base	Super;
		public:
			String	value,
					defaultValue;


			/**/		StringVar(const String&name, Persistence persistence, const String&value, unsigned protection=CLI::EraseProtection) : Super(name,"String",persistence, 1,protection),value(value),defaultValue(value)
			{}
			virtual String	ToString()		const	override	{return value;}
			virtual	bool	Set(const String&value)	override
			{
				if (this->value != value)
				{
					this->value = value;
					OnChanged();
				}
				return true;
			}
			virtual String	GetDefault() const override
			{
				return defaultValue;
			}

			virtual void		SetCurrentToDefault() override
			{
				defaultValue = value;
			}
		};


		template <typename T>
			class TypedVariable : public Base
			{
				typedef Base	Super;
			public:
				T				*const reference;
				T				defaultValue;
				const TFloatRange<T>	range;
				
				/**/			TypedVariable(const String&name, Persistence p, T&reference, T min, T max, unsigned protection=CLI::EraseProtection):Super(name,TypeInfo<T>::name,p,1,protection),reference(&reference),range(FloatRange(min,max))
				{
					defaultValue = reference;
				}
				virtual String	ToString()		const	override	{return String(*reference);}
				virtual	bool	Set(const String&value)	override
				{
					T old = *reference;
					if (convert(value.c_str(),value.length(), *reference))
					{
						*reference = range.Clamp(*reference);
						if (old != *reference)
							OnChanged();
						return true;
					}
					return false;
				}
				virtual String	GetDefault() const override
				{
					return String(defaultValue);
				}
				virtual String		GetSupportedValues() const override 
				{
					return range.ToString();
				}

				virtual void		SetCurrentToDefault() override
				{
					defaultValue = *reference;
				}
			};


		
		template <typename T, typename RefT>
			class MappedEnumVariable : public Base
			{
				typedef Base	Super;
			public:
				RefT			*const reference,
								defaultValue;

				static constexpr String	MakeTypeName()
				{
					return T::GetName();
				}
				
				/**/			MappedEnumVariable(const String&name, Persistence p, RefT&reference, unsigned protection=CLI::EraseProtection):Super(name,MakeTypeName(),p,1,protection),reference(&reference),defaultValue((RefT)T())	{}
				virtual String	ToString()		const	override	{return T::Reinterpret(*reference).ToString();}
				virtual	bool	Set(const String&value)	override
				{
					T temp;
					RefT old = *reference;
					if (!temp.Decode(value.c_str()))
					{
						return false;
					}
					(*reference) = (RefT)temp;
					if (*reference != old)
						OnChanged();
					return true;
				}
				virtual String		GetSupportedValues() const override 
				{
					static StringBuffer allowed;
					allowed.Clear();
					for (index_t i = 0; i < T::NumberOfPossibilities; i++)
					{
						if (i)
							allowed << ", ";
						allowed << T::Reinterpret(i).ToString();
					}
					//allowed += "'";
					return allowed.ToStringRef();
				}

				virtual String	GetDefault() const override
				{
					return T::Reinterpret(defaultValue).ToString();
				}
				virtual void		SetCurrentToDefault() override
				{
					defaultValue = *reference;
				}

			};


		template <typename T>
			class EnumVariable : public MappedEnumVariable<T,T>
			{
				typedef MappedEnumVariable<T,T>	Super;
			public:

				/**/			EnumVariable(const String&name, Persistence p, T&reference, unsigned protection=CLI::EraseProtection):Super(name,p,reference,protection)	{}
				virtual String	ToString()		const	override	{return reference->ToString();}

				virtual String	GetDefault() const override
				{
					return defaultValue.ToString();
				}

			};


		class NexusAIEnabled : public Base
		{
			typedef Base	Super;
		public:
			String			defaultValue = "true";

			/**/			NexusAIEnabled(const String&name, unsigned protection=CLI::EraseProtection):Super(name, "bool",Persistence::RestoreFromSavegame,1,protection)	{}
			virtual String	ToString()		const	override	{return String(Game::Config::Nexus::enableAI);}
			virtual	bool	Set(const String&value)	override
			{
				String old = ToString();
				bool val;
				bool rs = convert(value.c_str(),val);
				if (!rs)
					return false;
				Game::Config::Nexus::enableAI = val;
				if (old != ToString())
				{
					if (Network::SessionIsStarted())
					{
						if (!val)
							AIProcess::End();
						else
							AIProcess::Begin(Objects::Faction::nexus->accessToken);
					}
					OnChanged();
				}
				return rs;
			}
			virtual String		GetSupportedValues() const override 
			{
				return "[false,true]";
			}
			virtual String	GetDefault() const override
			{
				return defaultValue;
			}
			virtual void		SetCurrentToDefault() override
			{
				defaultValue = ToString();
			}
		};


		class SupportedFactions : public Base
		{
			typedef Base	Super;
		public:
			String			defaultValue = "All";

			/**/			SupportedFactions(const String&name, unsigned protection=CLI::EraseProtection):Super(name, "Flagset",Persistence::RestoreFromSavegame,1,protection)	{}
			virtual String	ToString()		const	override	{return Game::Config::FactionSupportFlagString();}
			virtual	bool	Set(const String&value)	override
			{
				String old = ToString();
				Game::Config::SetFactionSupportFlagString(value);
				if (old != ToString())
				{
					OnChanged();
				}
				return true;
			}
			virtual String		GetSupportedValues() const override 
			{
				return "'All'"
						", 'Spectator'"
						", 'Regular'"
						;
			}
			virtual String	GetDefault() const override
			{
				return defaultValue;
			}
			virtual void		SetCurrentToDefault() override
			{
				defaultValue = ToString();
			}
		};
		
		class FactionsPerEMPZone : public Base
		{
			typedef Base	Super;
		public:
			#ifdef LOWER_RESOLUTION_PLANET
				UINT32		defaultValue = 1;
			#else
				UINT32		defaultValue = 3;
			#endif

			/**/			FactionsPerEMPZone(const String&name, unsigned protection=CLI::EraseProtection):Super(name, "UInt32",Persistence::RestoreFromSavegame,1,protection)	{}
			virtual String	ToString()		const	override	{return String(Game::Config::factionsPerZone);}
			virtual	bool	Set(const String&value)	override
			{
				UINT32 old = Game::Config::factionsPerZone;
				if (!convert(value.c_str(),value.length(),Game::Config::factionsPerZone))
					return false;
				if (Game::Config::factionsPerZone < 1 || Game::Config::factionsPerZone > 5)
				{
					Game::Config::factionsPerZone = old;
					return false;
				}
				if (old < Game::Config::factionsPerZone && Objects::Faction::all.Count() > 0)
				{
					LogEvent(EventClass::Preset::commandRejected,"Unable to increase factionsPerEMPZone while factions exist.");
					Game::Config::factionsPerZone = old;
					return false;
				}
				if (old != Game::Config::factionsPerZone)
				{
					OnChanged();
					World::UpdateEMPZones(0);
				}
				return true;
			}
			virtual String		GetSupportedValues() const override 
			{
				return "[1,5]"
						;
			}
			virtual String	GetDefault() const override
			{
				return defaultValue;
			}
			virtual void		SetCurrentToDefault() override
			{
				defaultValue = Game::Config::factionsPerZone;
			}
		};

		class EMPZones : public Base
		{
			typedef Base	Super;
		public:
			bool			defaultValue = true;

			/**/			EMPZones(const String&name, unsigned protection=CLI::EraseProtection):Super(name, "Bool",Persistence::RestoreFromSavegame,1,protection)	{}
			virtual String	ToString()		const	override	{return String(RuntimeConfig::empZones);}
			virtual	bool	Set(const String&value)	override
			{
				bool old = RuntimeConfig::empZones;
				if (!convert(value.c_str(),value.length(),RuntimeConfig::empZones))
					return false;
				if (!old && RuntimeConfig::empZones && Objects::Faction::all.Count() > 0)
				{
					LogEvent(EventClass::Preset::commandRejected,"Unable to enable empZones while factions exist.");
					RuntimeConfig::empZones = old;
					return false;
				}
				if (old != RuntimeConfig::empZones)
				{
					OnChanged();
					World::UpdateEMPZones(0);
				}
				return true;
			}
			virtual String		GetSupportedValues() const override 
			{
				return "[1,5]"
						;
			}
			virtual String	GetDefault() const override
			{
				return defaultValue;
			}
			virtual void		SetCurrentToDefault() override
			{
				defaultValue = RuntimeConfig::empZones;
			}
		};

		class NexusVehicleTurrets : public Base
		{
			typedef Base	Super;
		public:
			String			defaultValue = "Cannon, Artillery, Radar";

			/**/			NexusVehicleTurrets(const String&name, unsigned protection=CLI::EraseProtection):Super(name,"Turretset",Persistence::RestoreFromSavegame,1,protection)	{}
			virtual String	ToString()		const	override
			{
				static StringBuffer buffer;
				buffer.Clear();
				foreach (Game::Config::Nexus::vehicleTurrets,t)
				{
					if (buffer.IsNotEmpty())
						buffer << ',';
					buffer << (*t)->typeName;
				}
				return buffer.ToString();
			}
			virtual	bool	Set(const String&value)	override
			{
				String old = ToString();
				static Array<String>vehicleTurretNames;

				explode(',', value, vehicleTurretNames);
				foreach (vehicleTurretNames,t)
				{
					t->trimThis();
					Objects::Component::Turret*ct = Objects::Component::GetTurretByName(*t);
					if (!ct || ct->mountType != Objects::Component::Turret::MountType::Vehicle)
						return false;
				}
				Game::Config::Nexus::vehicleTurrets.SetSize(vehicleTurretNames.Count());
				for (index_t i = 0; i < vehicleTurretNames.Count(); i++)
					Game::Config::Nexus::vehicleTurrets[i] = Objects::Component::GetTurretByName(vehicleTurretNames[i]);
				if (ToString() != old)
				{
					OnChanged();
				}
				return true;

			}
			virtual String		GetSupportedValues() const override 
			{
				String rs;
				foreach (Objects::Component::allTurretComponents,tc)
					if ((*tc)->mountType == Objects::Component::Turret::MountType::Vehicle)
					{
						if (rs.IsNotEmpty())
							rs += ",";
						rs += (*tc)->typeName;
					}
				return "{"+rs+"}*";
			}
			virtual String	GetDefault() const override
			{
				return defaultValue;
			}
			virtual void		SetCurrentToDefault() override
			{
				defaultValue = ToString();
			}
		};

		class ConfigMode : public Base
		{
			typedef Base	Super;
		public:
			bool defaultValue = false;

			/**/			ConfigMode(unsigned protection=CLI::EraseProtection):Super("configMode","bool",Persistence::Temporary,1,protection)	{}
			virtual String	ToString()		const	override
			{
				return ConfigurationMode::IsActive();
			}
			virtual	bool	Set(const String&value)	override
			{
				bool n;
				if (!convert(value.c_str(),n))
					return false;
				if (n != ConfigurationMode::IsActive())
				{
					if (n)
						ConfigurationMode::Begin();
					else
						ConfigurationMode::End();
					OnChanged();
				}
				return true;

			}
			virtual String		GetSupportedValues() const override 
			{
				return "true,false";
			}
			virtual String	GetDefault() const override
			{
				return defaultValue;
			}
			virtual void		SetCurrentToDefault() override
			{
				defaultValue = ConfigurationMode::IsActive();
			}
		};

		class UnitLimit : public Base
		{
			typedef Base	Super;
		public:
			const ConstName	key;
			UINT16			defaultValue;

			UINT16			Get() const
			{
				return (UINT16)RuntimeConfig::UnitLimits::Get(key,true);
			}

			/**/			UnitLimit(const String&name, ConstName key, unsigned protection=CLI::EraseProtection):Super(name,"Count16",Persistence::RestoreFromSavegame,1,protection),key(key)
			{}
			virtual String	ToString()		const	override	{return String(Get());}
			virtual	bool	Set(const String&value)	override
			{
				int v;
				UINT16 old = Get();
				if (convert(value.c_str(),v) && RuntimeConfig::UnitLimits::Set(key,v))
				{
					if (old != Get())
						OnChanged();
					return true;
				}
				return false;
			}
			virtual String		GetSupportedValues() const override 
			{
				return "[0,65535]";
			}
			virtual String	GetDefault() const override
			{
				return String(defaultValue);
			}
			virtual void		SetCurrentToDefault() override
			{
				defaultValue = Get();
			}

		};


		class UnitLimitEnabled : public Base
		{
			typedef Base	Super;
		public:
			bool			defaultValue =true;

			/**/			UnitLimitEnabled(const String&name, unsigned protection=CLI::EraseProtection):Super(name,"bool",Persistence::RestoreFromSavegame,1,protection)
			{}
			virtual String	ToString()		const	override	{return String(RuntimeConfig::UnitLimits::IsEnabled());}
			virtual	bool	Set(const String&value)	override
			{
				bool enable;

				bool old = RuntimeConfig::UnitLimits::IsEnabled();
				if (convert(value.c_str(),enable))
				{
					RuntimeConfig::UnitLimits::Enable(enable);
					if (old != RuntimeConfig::UnitLimits::IsEnabled())
						OnChanged();
					return true;
				}
				return false;
			}
			virtual String		GetSupportedValues() const override 
			{
				return "[false,true]";
			}
			virtual String	GetDefault() const override
			{
				return String(defaultValue);
			}
			virtual void		SetCurrentToDefault() override
			{
				defaultValue = RuntimeConfig::UnitLimits::IsEnabled();
			}

		};


		class FactionAttachment : public CLI::Folder::Attachable
		{
		public:
			TUniqueID	id;

			/**/		FactionAttachment() :id(TUniqueID::Empty()) {}
			/**/		FactionAttachment(const TUniqueID&id) :id(id) {}
		};
		typedef std::shared_ptr<FactionAttachment>	PFactionAttachment;

		class PersistenceAttachment :public CLI::Folder::Attachable
		{

		};

		class FileReferenceAttachment: public PersistenceAttachment
		{
		public:
			const CLI::PVariable	sourceVariable;
			const String	base;

			/**/			FileReferenceAttachment(const String&base, const CLI::PVariable&source):sourceVariable(source),base(base)	{}
		};



		PBase	SetString(const String&name, const String&value, Persistence p)
		{
			PBase rs (new StringVar(name,p, value, CLI::EraseProtection));
			state.SetVariable(rs);
			return rs;
		}


		template <typename T>
		PBase	Set(const String&name, T&reference, T min, T max, Persistence p)
		{
			PBase rs (new TypedVariable<T>(name,p,reference,min,max,CLI::EraseProtection));
			state.SetVariable(rs);
			return rs;
		}


		template <typename T>
		PBase	SetEnum(const String&name, T&enumReference, Persistence p)
		{
			PBase rs (new EnumVariable<T>(name,p,enumReference,CLI::EraseProtection));
			state.SetVariable(rs);
			return rs;
		}
		template <typename T, typename RefT>
		PBase	SetMappedEnum(const String&name, RefT&enumReference, Persistence p)
		{
			PBase rs (new MappedEnumVariable<T,RefT>(name,p,enumReference,CLI::EraseProtection));
			state.SetVariable(rs);
			return rs;
		}
	}



	namespace FactionCommands
	{
		CLI::PCommand	terminate;



		void		TerminateCommand(const String&reason)
		{
			CLI::PFolder f = state.GetExecutionContext();
			Variable::PFactionAttachment attach = To<Variable::FactionAttachment>::DynamicCast(f->attachment);
			if (!attach)
			{
				LogEvent(EventClass::Preset::remoteControlError, "Error: This folder is not a faction folder");
				return;
			}
			String r = reason.trimRef();
			if (r.IsEmpty())
			{
				LogEvent(EventClass::Preset::remoteControlError, "Error: No reason given");
				return;
			}
			Objects::PFaction faction = Objects::Faction::FindByID(attach->id);
			if (!faction)
			{
				LogEvent(EventClass::Preset::remoteControlError, "Error: Faction no longer exists");
				SignalFactionsChanged();
				return;
			}
			LogEvent(EventClass::Preset::factionDeath, "Terminating "+faction->GetLogName()+" ("+r+")");
			Objects::Faction::Kill(faction);
			Network::PClient client;
			if (Network::playerMap.Query(faction,client))
			{
				client->SendProgressUpdate(true, "");
			}
		}

		void		ListNames()
		{
			Objects::ForeachFaction([](const Objects::PFaction&f)
			{
				CLI::println(f->name + " - " + f->id.ToString());
			});
		}

		void		FindName(const String&name)
		{
			Objects::ForeachFaction([name](const Objects::PFaction&f)
			{
				if (f->name.findIgnoreCase(name))
					CLI::println(f->name + " - " + f->id.ToString());
			});
		}

		void		GotoName(const String&name)
		{


		}

		void		Create()
		{
			terminate.reset(new CLI::Command(CLI::Command::Name("terminate reason"), &TerminateCommand, CLI::NoCompletion));
			terminate->SetUniqueHelp("Terminates this faction and all of its destructible units. If the local faction is the Nexus faction, then it'll lose all its units but won't be deleted."
							" Unless you have some backup savegame, this operation is irreversible. Don't use this on live ");

		}





		
		class Name : public CLI::Variable
		{
		public:
			Objects::WFaction faction;

			/**/				Name(const Objects::PFaction&f):CLI::Variable("name","String",1,CLI::FullProtection),faction(f)
			{}

			virtual	String		ToString()			const override	{Objects::PFaction f = faction.lock(); if (f) return f->name; return "<null>";}
			virtual	bool		Set(const String&value)	{return false;}
		};

		class Client : public CLI::Variable
		{
		public:
			Objects::WFaction faction;

			/**/				Client(const Objects::PFaction&f):CLI::Variable("client","Network Address",1,CLI::FullProtection),faction(f)
			{}

			virtual	String		ToString()			const override
			{
				Objects::PFaction f = faction.lock(); 
				if (!f) 
					return "<null>";
				Network::PClient client;
				if (!Network::playerMap.Query(f,client))
				{
					return "offline";
				}
				TCP::PPeer peer = client->peer.lock();
				if (!peer)
				{
					return "offline";
				}
				return peer->ToString();
			}
			virtual	bool		Set(const String&value)	{return false;}
		};

	}


	static void MapConfig(Grid::TPlanetConfig::TLayer&layer, const CLI::PFolder&context)
	{
		state.PushAndReplaceFocus(context);
			Variable::Set("baseHeight",layer.baseHeight,Grid::CellStack::BottomHeight, Grid::CellStack::TopHeight, Variable::Persistence::PlanetConfig);
			Variable::Set("noiseHeightScale",layer.heightScale,0.f, 10.f, Variable::Persistence::PlanetConfig);
			Variable::Set("frequency",layer.frequency,0.f, 10.f, Variable::Persistence::PlanetConfig);
			Variable::Set("dampening",layer.dampening,0, 10, Variable::Persistence::PlanetConfig);
		state.PopFocus();
	}

	static bool DeclareFolder(const XML::Node&node, const CLI::PFolder&context)
	{
		bool allGood = true;
		foreach (node.children,child)
		{
			String name;
			if (!child->Query("name",name))
			{
				LogEvent(EventClass::Preset::configurationError,"Unable to retrieve name attribute of XML variable node in "+context->GetPath());
				allGood = false;
				continue;
			}
			if (child->name == "variable")
			{
				CLI::PVariable v = context->variables.Query(name);
				if (!v)
				{
					LogEvent(EventClass::Preset::configurationError,"Addressed variable '"+name+"' does not exist in "+context->GetPath());
					allGood = false;
					continue;
				}
				String help = child->inner_content.trim();
				if (help.IsNotEmpty())
				{
					static Array<String>	lines;
					static StringBuffer		fused;
					fused.Clear();
					explodeCallback(isNewline<char>,help,lines);
					//wrap(help,80,lines);
					foreach (lines,line)
					{
						StringRef trimmed = line->trimRef();
						if (trimmed.length())
						{
							if (fused.IsNotEmpty())
								fused << nl;
							fused << trimmed;
						}
					}
					//if (fused.ToString().find("The higher, the thinner, sharper and"))
					//{
					//	bool brk = true;
					//}
					v->SetUniqueHelp(fused.ToStringRef());
				}


				String value;
				if (child->Query("default",value) && value.IsNotEmpty())
				{
					if (v->Set(value))
						v->SetCurrentToDefault();
					else
					{
						LogEvent(EventClass::Preset::configuration,"Failed to set '"+v->name+"' to '"+value+"' in "+context->GetPath());
						allGood = false;
					}
				}

				continue;
			}
			if (child->name == "command")
			{
				CLI::PCommand v = context->commands.Query(name);
				if (!v)
				{
					LogEvent(EventClass::Preset::configuration,"Addressed command '"+name+"' does not exist in "+context->GetPath());
					allGood = false;
					continue;
				}
				if (child->inner_content.trim().IsNotEmpty())
					v->SetUniqueHelp(child->inner_content.trim());

				continue;
			}

			if (child->name == "context")
			{
				CLI::PFolder f = context->folders.Query(name);
				if (!f)
				{
					LogEvent(EventClass::Preset::configuration,"Addressed context '"+name+"' does not exist in "+context->GetPath());
					allGood = false;
					continue;
				}
				if (child->inner_content.trim().IsNotEmpty())
					f->SetUniqueHelp(child->inner_content.trim());
				if (!DeclareFolder(*child,f))
					allGood = false;
				continue;
			}

			{
				LogEvent(EventClass::Preset::configuration,"Unexpected XML node type '"+child->name+"' found in "+context->GetPath());
				continue;
			}
		}
		return allGood;
	}

	static void ExportFolder(XML::Node&node, const CLI::PFolder&context)
	{
		for (const auto&cmd : context->commands)
		{
			XML::Node&xcmd = node.Create("command");
			xcmd.Set("name",cmd->name);
			if (cmd->help)
				xcmd.inner_content  = *cmd->help;
		}
		for (const auto&var : context->variables)
		{
			XML::Node&xcmd = node.Create("variable");
			xcmd.Set("name",var->name);
			if (var->help)
				xcmd.inner_content  = *var->help;
			String def = var->GetDefault();
			//if (def.IsNotEmpty())
			xcmd.Set("default",def);
		}
		for (const auto&ctx : context->folders)
		{
			XML::Node&xentry = node.Create("context");
			xentry.Set("name",ctx->name);
			if (ctx->help)
				xentry.inner_content  = *ctx->help;
			ExportFolder(xentry,ctx);
		}
	}

	static void SetupBase(const CLI::PFolder&context)
	{
		for (const auto&var : context->variables)
		{
			Variable::PBase base = To<Variable::Base>::DynamicCast(var);
			if (base)
				base->SetupBase();
		}
		for (const auto&ctx : context->folders)
		{
			SetupBase(ctx);
		}
	}

	void	CreatePrototypeDeclaration()
	{
		XML::Container xml;
		xml.root_node.name = "declaration";
		ExportFolder(xml.root_node,state.GetRoot());
		xml.SaveToFile("./config/declaration.xml");
	}

	

	
	void PrintLn(const String&line)
	{
		LogEvent(EventClass::Preset::remoteControlCommand,"Response: '"+line+"'");
	}


	void	Create()
	{
		#undef VAR
		#define VAR(_NAME_,_CONTEXT_,_MIN_,_MAX_,_PERSISTENCE_)	Variable::Set(#_NAME_,_CONTEXT_::_NAME_,_MIN_,_MAX_,_PERSISTENCE_)
		#undef ENUMVAR
		#define ENUMVAR(_NAME_,_CONTEXT_,_PERSISTENCE_)	Variable::SetEnum(#_NAME_,_CONTEXT_::_NAME_,_PERSISTENCE_)

		CLI::InitDefaults(state,PrintLn);

		FactionCommands::Create();

		factionFolder = state.EnterNewFolder("Factions");
		state.DefineCommand("names", FactionCommands::ListNames);
		state.DefineCommand("find name", FactionCommands::FindName);
		state.ExitFolder();


		CLI::PAttachment persistenceAttachment(new Variable::PersistenceAttachment());

		state.EnterNewFolder("World")->attachment = persistenceAttachment;
			VAR(simulateLiquids,World::Config,false,true,Variable::Persistence::RestoreFromSavegame);
			ENUMVAR(worldDiscovery,World::Config,Variable::Persistence::PlanetConfig);


			state.EnterNewFolder("Planet")->attachment = persistenceAttachment;
				Variable::planetType = Variable::SetString("Type","LushGreen",Variable::Persistence::PlanetConfig);
				state.EnterNewFolder("Config")->attachment.reset(new Variable::FileReferenceAttachment("config/planets",Variable::planetType));
				{
					/*
					Important:
					Keep this in sync with client/components/configurationMode.cpp
					*/
					Variable::Set("atmosphereDensity",Grid::currentPlanetConfig.atmosphereColor.a,0.f,1.f,Variable::Persistence::PlanetConfig);
					Variable::SetMappedEnum<Grid::Content,UINT32>("innerCoreType",Grid::currentPlanetConfig.innerCoreType,Variable::Persistence::PlanetConfig);
					state.EnterNewFolder("ExtendedCore")->attachment = persistenceAttachment;
						Variable::SetMappedEnum<Grid::Content,BYTE>("type",Grid::currentPlanetConfig.extendedCoreType,Variable::Persistence::PlanetConfig);
					state.ExitFolder();
					
					
					CLI::PFolder mountains = state.EnterNewFolder("Mountains");
						mountains->attachment = persistenceAttachment;
//						Variable::SetMappedEnum<Grid::Content,BYTE>("top",Grid::currentPlanetConfig.topType,Variable::Persistence::PlanetConfig)->extraOnChange = [](Variable::Base&) {Grid::currentPlanetConfig.GenerateAtmosphereColor();};
						Variable::SetMappedEnum<Grid::Content,BYTE>("type",Grid::currentPlanetConfig.type,Variable::Persistence::PlanetConfig)->extraOnChange = [](Variable::Base&) {Grid::currentPlanetConfig.GenerateAtmosphereColor();};
						Variable::Set("snowHeight",Grid::currentPlanetConfig.snowHeight,Grid::CellStack::BottomHeight, Grid::CellStack::TopHeight,Variable::Persistence::PlanetConfig)->extraOnChange = [](Variable::Base&) {Grid::currentPlanetConfig.GenerateAtmosphereColor();};
					state.ExitFolder();

					state.EnterNewFolder("Liquid")->attachment = persistenceAttachment;
						Variable::SetMappedEnum<Grid::Content,BYTE>("type",Grid::currentPlanetConfig.liquidCoverage,Variable::Persistence::PlanetConfig)->extraOnChange = [](Variable::Base&) {Grid::currentPlanetConfig.GenerateAtmosphereColor();};
						Variable::Set("level",Grid::currentPlanetConfig.liquidHeight,Grid::CellStack::BottomHeight, Grid::CellStack::TopHeight,Variable::Persistence::PlanetConfig)->extraOnChange = [](Variable::Base&) {Grid::currentPlanetConfig.GenerateAtmosphereColor();};
						Variable::SetMappedEnum<Grid::Content,BYTE>("outline",Grid::currentPlanetConfig.liquidOutline,Variable::Persistence::PlanetConfig);
					state.ExitFolder();

					MapConfig(Grid::currentPlanetConfig.extendedCore,state.GetFocus()->CreateOrGetChild("ExtendedCore"));
					MapConfig(Grid::currentPlanetConfig.mountains,mountains);

					state.EnterNewFolder("Resources")->attachment = persistenceAttachment;
						for (index_t i = 0; i < Objects::Resource::NumberOfPossibilities; i++)
						{
							Objects::Resource res = Objects::Resource::Reinterpret(i);
							state.EnterNewFolder(res.ToString())->attachment = persistenceAttachment;
								Variable::Set("density",Grid::Resource::density[i],0.f,1.f,Variable::Persistence::PlanetConfig);
								//Variable::Set("probability",Grid::Resource::probability[i],0.f,1.f,Variable::Persistence::GameInvariant);
								Variable::Set("max",Grid::Resource::maximum[i],(Grid::Resource::coverage_t)1,(Grid::Resource::coverage_t)32,Variable::Persistence::PlanetConfig);
							state.ExitFolder();
						}
					state.ExitFolder();
				}
				state.ExitFolder();
			state.ExitFolder();
		state.ExitFolder();
		state.EnterNewFolder("Rules")->attachment = persistenceAttachment;

			ENUMVAR(gameGoal,RuntimeConfig,Variable::Persistence::RestoreFromSavegame);

			state.SetVariable(CLI::PVariable(new Variable::SupportedFactions("supportedFactions")));

			VAR(closedGame,Game::Config,false,true,Variable::Persistence::RestoreFromSavegame);
			VAR(roundTimer,Game::Config,-1,INT_MAX,Variable::Persistence::RestoreFromSavegame);
			VAR(bombsExplodeWhenDestroyed,RuntimeConfig,false,true,Variable::Persistence::RestoreFromSavegame);
			VAR(startBases,Game::Config,false,true,Variable::Persistence::RestoreFromSavegame);
			VAR(basesRequired,Game::Config,false,true,Variable::Persistence::RestoreFromSavegame);
			VAR(exposeActionLeftToHostileFactions,Game::Config,false,true,Variable::Persistence::RestoreFromSavegame);
			VAR(newVehiclesHaveFullAmmo,Objects,false,true,Variable::Persistence::RestoreFromSavegame);
			VAR(uplinkBandwidth,Game::Config,1U,1000000U,Variable::Persistence::RestoreFromSavegame);
			ENUMVAR(doomsdayWeapons,RuntimeConfig,Variable::Persistence::RestoreFromSavegame);
			
			state.SetVariable(CLI::PVariable(new Variable::FactionsPerEMPZone("factionsPerEMPZone")));
			state.SetVariable(CLI::PVariable(new Variable::EMPZones("empZones")));

			state.EnterNewFolder("UnitLimits")->attachment = persistenceAttachment;
				RuntimeConfig::UnitLimits::Reset();

				state.SetVariable(CLI::PVariable(new Variable::UnitLimitEnabled("Enabled")));

				state.EnterNewFolder("Structures")->attachment = persistenceAttachment;
					for (const Objects::PStructure&p:Objects::structurePrototypes)
					{
						String key = p->GetTypeName();
						key.replace(' ','_');
						state.SetVariable(CLI::PVariable(new Variable::UnitLimit(key,p->GetCTypeName())));
					}
				state.ExitFolder();

				state.EnterNewFolder("MobileTurrets")->attachment = persistenceAttachment;
					for (const auto&c:Objects::Component::allTurretComponents)
					{
						String key = c->typeName;
						key.replace(' ','_');
						state.SetVariable(CLI::PVariable(new Variable::UnitLimit(key,c->constName)));
					}
				state.ExitFolder();
			state.ExitFolder();

			state.EnterNewFolder("Nexus")->attachment = persistenceAttachment;
				VAR(haveBases,Game::Config::Nexus,false,true,Variable::Persistence::RestoreFromSavegame);
				#ifndef NO_SATELLITES
					VAR(haveSatellitesAtopBases,Game::Config::Nexus,false,true,Variable::Persistence::RestoreFromSavegame);
				#endif
				state.SetVariable(CLI::PVariable(new Variable::NexusAIEnabled("enableAI")));
				//VAR(enableAI,Game::Config::Nexus,false,true,Variable::Persistence::RestoreFromSavegame)
				VAR(threatEstimationFactor,Game::Config::Nexus,0.001f,100.f,Variable::Persistence::RestoreFromSavegame);
				VAR(roundsPerResearchPoint,Game::Config::Nexus,0U,10000U,Variable::Persistence::RestoreFromSavegame);
				state.SetVariable(CLI::PVariable(new Variable::NexusVehicleTurrets("vehicleTurrets")));
			state.ExitFolder();
		state.ExitFolder();

		state.EnterNewFolder("Server")->attachment = persistenceAttachment;
			VAR(saveEveryNRounds,FileIO::Config,-1,100,Variable::Persistence::ConfigOnly);
			VAR(saveWhenAllQuit,FileIO::Config,false,true,Variable::Persistence::ConfigOnly);
			
			state.SetVariable(CLI::PVariable(new Variable::ConfigMode()));
		state.ExitFolder();

		SetupBase(state.GetRoot());

		{
			XML::Container xdecl("./config/declaration.xml");
			if (!DeclareFolder(xdecl.root_node,state.GetRoot()))
			{
				CreatePrototypeDeclaration();
			}
		}


	}

	static void Feed(const CLI::PFolder&folder, const PathString&subConfigName, bool testMode, const std::function<bool(const Variable::PBase&)>&filterFunction, bool readOriginal=false);
	static bool InnerFeed(const CLI::PFolder&folder,const Config::Context*ctx, bool testMode, const std::function<bool(const Variable::PBase&)>&filterFunction);

	static bool FeedSubDirectory(const CLI::PFolder&dir, const Config::Context*ctx, bool testMode, const std::function<bool(const Variable::PBase&)>&filterFunction)
	{
		bool allGood = true;
		std::shared_ptr<Variable::PersistenceAttachment> a = To<Variable::PersistenceAttachment>::DynamicCast(dir->attachment);
		if (!a)	//externally created
		{
			DBG_ASSERT__(dir != state.GetRoot());	//should never be in here for root
			return true;
		}

		std::shared_ptr<Variable::FileReferenceAttachment> ref = To<Variable::FileReferenceAttachment>::DynamicCast(dir->attachment);
		if (ref)
		{
			if (testMode)
				Variable::planetType->Set("LushGreen");

			//PathString str = RuntimeConfig::ConfigFile( PathString(ref->base + FOLDER_SLASH + ref->sourceVariable->ToString() + ".ini"));
			Feed(dir,PathString(ref->base + FOLDER_SLASH + ref->sourceVariable->ToString() + ".ini"),testMode,filterFunction);
		}
		else
		{
			const Config::Context*sub = ctx ? ctx->GetContext(dir->name) : nullptr;
			if (ctx && !sub)
			{
				allGood = false;
			}
			bool innerGood = InnerFeed(dir,sub,testMode,filterFunction);
			if (!innerGood)
				allGood = false;

		}
		return allGood;
	}

	static bool InnerFeed(const CLI::PFolder&folder,const Config::Context*ctx, bool testMode, const std::function<bool(const Variable::PBase&)>&filterFunction)
	{
		bool allGood = true;
		for (const auto&var : folder->variables)
		{
			Variable::PBase b = To<Variable::Base>::DynamicCast(var);
			if (!b || b->persistence == Reflection::Variable::Persistence::Temporary)
				continue;
			if (filterFunction)
			{
				if (!filterFunction(b))
					continue;

			}
			const Config::Context::Attribute*attrib = ctx ? ctx->GetAttrib(var->name) : nullptr;
			if (attrib)
			{
				if (!var->Set(attrib->value))
				{
					String support = var->GetSupportedValues();
					if (support.IsNotEmpty())
						support = "Supported values are "+var->GetSupportedValues()+". ";
					LogEvent(EventClass::Preset::configurationError,"Unable to set configuration "+folder->GetPath()+"/"+var->name+" to '"+attrib->value+"'. "+support+"Defaulting to "+var->GetDefault());
					String varPath = folder->GetPath()+"/"+var->name;
					ASSERT2__(var->Set(var->GetDefault()),varPath,var->GetDefault());
					allGood = false;
				}
			}
			else
			{
				if (ctx)
					LogEvent(EventClass::Preset::configurationError,"Attribute missing in configuration: "+folder->GetPath()+"/"+var->name+". Defaulting to "+var->GetDefault());
				String varPath = folder->GetPath()+"/"+var->name;
				ASSERT2__(var->Set(var->GetDefault()),varPath,var->GetDefault());
				allGood = false;
			}

		}
		for (const auto&dir : folder->folders)
		{
			if (!FeedSubDirectory(dir,ctx,testMode,filterFunction))
				allGood = false;
		}
		return allGood;
	}


	static void WriteIndent(StringFile&outFile, int indent)
	{
		for (int i = 0; i < indent; i++)
			outFile << '\t';
	}

	static void WriteHelp(StringFile&outFile, const std::shared_ptr<CLI::NamedItem>&item, int indent, const String&prefix)
	{
		static StringBuffer outBuffer;
		outBuffer.Clear();
		outBuffer << prefix;
		if (item->help && item->help->IsNotEmpty())
			outBuffer << *item->help;
		outBuffer << nl;


		Variable::PBase var = To<Variable::Base>::DynamicCast(item);
		if (var)
		{
			String values = var->GetSupportedValues();

			switch (var->persistence)
			{
				case Variable::Persistence::ConfigOnly:
					outBuffer << "Loaded from configuration each time the server is restarted. Not written to savegames."<<nl;
				break;
				//case Variable::Persistence::ConfigOverridesSavegame:
				//	outBuffer << "Written to savegames and applied during loading, but overridden by settings found in configuration files afterwards."<<nl;
				//break;
				case Variable::Persistence::GameInvariant:
				case Variable::Persistence::PlanetConfig :	//for now, write the same thing into the config
					outBuffer << "Written to savegames and restored from there, but cannot be altered once a game has started."<<nl;
				break;
				case Variable::Persistence::RestoreFromSavegame:
					outBuffer << "Written to savegames and restored from there, overriding any settings found in configuration files. May be changed by the server during runtime."<<nl;
				break;
				default:
					FATAL__("Unknown or unsupported type for export");
				break;
			}


			outBuffer << var->type;
			if (values.IsNotEmpty())
			{
				outBuffer <<"; range=" << values;
			}
			String def = var->GetDefault();
			if (def.IsNotEmpty())
			{
				outBuffer << "; default=" << def;
			}
			outBuffer <<'.'<<nl;
		}

		static Array<String>	lines;
		wrap(outBuffer.ToString(),80,lines);
		foreach (lines,line)
		{
			String trimmed = line->trim();
			if (trimmed.IsNotEmpty())
			{
				WriteIndent(outFile,indent);
				outFile << ';' << trimmed<<nl;
			}
		}
	}

	static void DefineConfig(StringFile&outFile,const CLI::PFolder&folder, int indent)
	{
		for (const auto&var : folder->variables)
		{
			Variable::PBase b = To<Variable::Base>::DynamicCast(var);
			if (!b || b->persistence == Variable::Persistence::Temporary)
				continue;
			WriteHelp(outFile,var,indent,var->name+": ");
			WriteIndent(outFile,indent);
			outFile << var->name << '='<<var->ToString() << nl;
		}
		for (const auto&dir : folder->folders)
		{
			std::shared_ptr<Variable::PersistenceAttachment> a = To<Variable::PersistenceAttachment>::DynamicCast(dir->attachment);
			if (!a)	//externally created
				continue;

			std::shared_ptr<Variable::FileReferenceAttachment> ref = To<Variable::FileReferenceAttachment>::DynamicCast(dir->attachment);
			if (ref)	//file-reference
				continue;
			WriteHelp(outFile,dir,indent,"");
			WriteIndent(outFile,indent);
			outFile << '[' << dir->name<<']'<<nl;
			DefineConfig(outFile,dir,indent+1);
		}
	}

	static void Feed(const CLI::PFolder&folder, const PathString&subConfigName, bool testMode, const std::function<bool(const Variable::PBase&)>&filterFunction, bool readOriginal/*=false*/)
	{
		if (!readOriginal)
			Feed(folder,subConfigName,testMode,filterFunction,true);	//read original first
		Configuration	config;
		Config::Context*	ctx = nullptr;
		
		const PathString configName = readOriginal ? "./"+subConfigName :  RuntimeConfig::ConfigFile(PathString(subConfigName));

		bool rewrite = true;
		if (FileSystem::IsFile(configName))
		{
			if (!config.LoadFromFile(configName))
			{
				LogEvent(EventClass::Preset::configurationError,"Unable to parse "+String(configName)+":\n"+config.Report());
			}
			else
			{
				ctx = &config;
				rewrite = false;
			}
		}
		else
		{
			if (FileSystem::IsFile(subConfigName))
			{
				LogEvent(EventClass::Preset::configurationPrint,"Restoring missing configuration file '"+String(subConfigName)+"' from base data");
				if (FileSystem::Copy(subConfigName,configName,true))
				{
					if (!config.LoadFromFile(configName))
					{
						LogEvent(EventClass::Preset::configurationError,"Unable to parse "+String(configName)+":\n"+config.Report());
					}
					else
					{
						ctx = &config;
						rewrite = false;
					}
				}
			}	
		}

		if (!ctx)
			LogEvent(EventClass::Preset::configuration,"Default configuration loaded");
		bool allGood = InnerFeed(folder,ctx,testMode,filterFunction);
		if (!readOriginal && (!allGood || rewrite))
		{
			//Feed(folder,subConfigName,testMode,filterFunction,true);
			StringFile outFile;
			if (!outFile.Create(configName))
			{
				LogEvent(EventClass::Preset::configurationError,"Unable to overwrite '"+String(configName)+"'");
			}
			else
			{
				LogEvent(EventClass::Preset::configurationPrint,"Writing '"+String(configName)+"'...");
				DefineConfig(outFile,folder, 0);
				outFile.Close();
			}
		}
		else
			if (readOriginal && !allGood && !rewrite)
			{
				FATAL__("There were errors while trying to parse original configuration file "+String(configName)+". Fix the errors and restart the application.\nCheck your server logs (My Documents/My Games/Hexaverse/server/logs) for details. Redownloading the application should also fix the issue.")
			}
	}



	void	LoadOverrides()
	{
		#ifdef _DEBUG
			const char*localServerINI = "config/debug_server.ini";
		#else
			const char*localServerINI = "config/server.ini";
		#endif

		Feed(state.GetRoot(),localServerINI, false, [](const Variable::PBase&base)->bool
		{
			return false;//base->persistence == Variable::Persistence::ConfigOverridesSavegame;
		});

		Grid::currentPlanetConfig.GenerateAtmosphereColor();


	}

	static CLI::PFolder FindFolderWithAttachmentSource(const CLI::PVariable&v, const CLI::PFolder&current)
	{
		std::shared_ptr<Variable::PersistenceAttachment> a = To<Variable::PersistenceAttachment>::DynamicCast(current->attachment);
		if (!a && current != state.GetRoot())	//externally created
			return CLI::PFolder();

		std::shared_ptr<Variable::FileReferenceAttachment> ref = To<Variable::FileReferenceAttachment>::DynamicCast(current->attachment);
		if (ref && ref->sourceVariable == v)
			return current;

		for (const auto&f : current->folders)
		{
			CLI::PFolder rs = FindFolderWithAttachmentSource(v,f);
			if (rs)
				return rs;
		}
		return CLI::PFolder();

	}

	static CLI::PFolder FindFolderWithAttachmentSource(const CLI::PVariable&v)
	{
		return FindFolderWithAttachmentSource(v,state.GetRoot());
	}

	void	ReloadPlanet()
	{
		CLI::PFolder f = FindFolderWithAttachmentSource(Variable::planetType);
		ASSERT__(f);
		if (f)
		{
			FeedSubDirectory(f, nullptr, false, std::function<bool(const Variable::PBase&)>());
		}
	}


	void Load(bool testMode)
	{
		FileSystem::Folder	root;
		ASSERT__(root.IsValidLocation());
		FileSystem::Folder planetPrototypes = root.FindFolder("config/planets");
		

		if (planetPrototypes.IsValidLocation())
		{
			FileSystem::CreateDirectory(RuntimeConfig::ConfigFile("config/planets"));
			planetPrototypes.Rewind();
			FileSystem::File file;
			while (planetPrototypes.NextFile(".ini",file))
			{
				PathString dest = RuntimeConfig::ConfigFile("config/planets/"+file.GetName());
				if (!FileSystem::IsFile(dest))
				{
					if (!FileSystem::Copy(file.GetLocation(),dest,true))
					{
						LogEvent(EventClass::Preset::configurationError,"Warning: Unable to restore default planet type '"+String(file.GetInnerName())+"'");
					}
					else
						LogEvent(EventClass::Preset::configurationError,"Restored default planet type '"+String(file.GetInnerName())+"'");
				}
			}

		}
		else
			LogEvent(EventClass::Preset::configurationError,"Warning: Unable to find base planet source folder from '"+String(root.GetLocation()));


		#ifdef _DEBUG
			const char*localServerINI = "config/debug_server.ini";
		#else
			const char*localServerINI = "config/server.ini";
		#endif

		Feed(state.GetRoot(),localServerINI, testMode,std::function<bool(const Variable::PBase&)>());

		Grid::currentPlanetConfig.GenerateAtmosphereColor();
	}


	void	VisitAllVariables(const Variable::FCallback&callback)
	{
		Variable::Walk(state.GetRoot(),callback);
	}


	void	CheckAllSaveGameVariables()
	{
		VisitAllVariables([](const Variable::PBase&v)
		{
			if (v->WriteToSavegame())
			{
				v->CheckChanges();
			}
		});

	}


	void SignalFactionsChanged()
	{
		bool anyDeleted = false;
		for (index_t i = 0; i < factionFolder->folders.Count(); i++)
		{
			const auto&f = factionFolder->folders[i];
			Variable::PFactionAttachment attach = To<Variable::FactionAttachment>::DynamicCast(f->attachment);
			if (!attach)
			{
				LogEvent(EventClass::Preset::internalError, "Found faction folder '"+f->name+"' without attachment");
				state.EraseFolder(f);
				i--;
				anyDeleted = true;
				continue;
			}

			if (!Objects::Faction::FindByID(attach->id))
			{
				state.EraseFolder(f);
				i--;
				anyDeleted = true;
				continue;
			}
		}
//		if (anyDeleted)
	//		RemoteControl::SignalFoldersErased();
		Objects::ForeachFaction([](const Objects::PFaction&faction)
		{
			String idName = faction->id.ToFullString();
			if (factionFolder->folders.Query(idName))
				return;
			CLI::PFolder f = factionFolder->CreateOrGetChild(idName);

			f->attachment.reset(new Variable::FactionAttachment(faction->id));

			f->commands.Insert("terminate", FactionCommands::terminate);

			f->variables.Insert("name",CLI::PVariable( new FactionCommands::Name(faction) ));
			f->variables.Insert("client",CLI::PVariable( new FactionCommands::Client(faction) ));
		});
	}



}


