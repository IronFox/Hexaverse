#ifdef HAS_WINDOW

#include <global_root.h>
#include "tutorial.h"
#include "dialog.h"
#include <io/xml.h>
#include <container/hashtable.h>
#include "../version.h"
#include <objects.h>
#include <luaGameAPI.h>
#include "clientRuntimeConfig.h"
#include "network.h"
#include "ui.h"
#include <lua.hpp>

namespace Tutorial
{
	Timer::Time	lastDelayingEvent = timer.Now();
	bool			movementTutorialIssued = false,
					endRoundTutorialIssued = false;


	namespace Detail
	{
		class Message
		{
		public:
			String			image;
			Array<String>	lines;
			bool			shown = false;

			void			swap(Message&other)
			{
				image.swap(other.image);
				lines.swap(other.lines);
				std::swap(shown,other.shown);
			}
		};


		StringFile			memory;

		StringTable<Message,Strategy::Swap>	messages;
	}

	namespace Interactive
	{
		Lua::PProgram		tutorialProgram;
		Lua::Table			api,myUnits;
		bool				started = false;

		namespace Trigger
		{
			Lua::Function<void()>	begin,selectionChanged,onMessageConfirm;
		}

		namespace API
		{

			GUI::PWindow	taskWindow;

			struct TTrigger
			{
				float		remainingSeconds;
				Lua::Function<void()>	trigger;
			};

			Buffer0<TTrigger>	activeTriggers;


			//void	NextTask()
			//{
			//	if (taskWindow)
			//		To<GUI::Label>::Cast(taskWindow->GetComponents().First())->SetText("Well done!");
			//}

			void	DisplayMessage(const String&msg, const String&img)
			{
				if (taskWindow)
					GUI::Hide(taskWindow);

				auto w = Dialog::CreateTutorialMessage("", msg,img);
				w->check = []()->Dialog::CheckResult{ return Network::client.IsConnected() ? Dialog::CheckResult::OkayContinue : Dialog::CheckResult::Abort; };
				w->onConfirm = 
				[w]()
				{
					Trigger::onMessageConfirm();
					GUI::Hide(w);
				};
				w->canBeImplicitlyClosed = false;
				GUI::Show(w);
			}

			void	DescribeTask(const String&msg)
			{
				if (taskWindow)
				{
					To<GUI::Label>::Cast(taskWindow->GetComponents().First())->SetText(msg);
					taskWindow->InvalidateSize();
				}
				else
				{
					taskWindow.reset(new GUI::Window());
					taskWindow->canBeImplicitlyClosed = false;

					taskWindow->AddNewComponent(new GUI::Label(msg))->AnchorTop(0).FillHorizontally(0);
					taskWindow->autoHeight = true;
					taskWindow->autoCenter = false;
					taskWindow->anchorConfig.Left().Unset();
					taskWindow->anchorConfig.Right().Set(0.1f);
					taskWindow->anchorConfig.Bottom().Set(0.1f);
					taskWindow->SetSize(float2(0.25));
					taskWindow->isModal = false;
				}
				GUI::Show(taskWindow);
			}

			void	CallDelayed(float delay, Lua::Function<void()> f)
			{
				activeTriggers.Append().remainingSeconds = delay;
				activeTriggers.Last().trigger = f;
			}

		}


		PointerTable<Lua::Reference,Swap>	objectTables;


		Lua::Table RebuildEntity(const Objects::PWorldEntity&e)
		{
			ASSERT__(e);
			Lua::Table t;
			if (!objectTables.Query(e.get(),t))
			{
				t = tutorialProgram->CreateTable();
				objectTables.Set(e.get(),t);
			}

			t.ClearTable();
			Lua::EntityToTable(e,t);
			return t;
		}

		void RebuildSelected()
		{
			const auto&e = Objects::selectedEntity;
			if (!e)
			{
				api.SetField("selectedUnit",Lua::Reference());
				return;
			}
			
			api.SetField("selectedUnit",RebuildEntity(e));
		}

		void RebuildFactionUnits()
		{
			myUnits.ClearTable();
			Objects::ForeachWorldEntityIncludingStored([](const Objects::PWorldEntity&e)
			{
				if (e->isOwnedByMe)
				{
					myUnits.AppendElement(RebuildEntity(e));
				}
			},false);

		}

		void Create()
		{
			try
			{
				objectTables.Clear();
				tutorialProgram = Lua::Program::Create();
				api = Lua::CreateGeneralAPI(tutorialProgram);
				myUnits = tutorialProgram->CreateTable();
				api.SetField("myUnits",myUnits);

				api.SetFunction("DisplayMessage",API::DisplayMessage);
				api.SetFunction("DescribeTask",API::DescribeTask);
				api.SetFunction("CallDelayed",API::CallDelayed);

				tutorialProgram->ExecuteFile("../lua/tutorial.lua");

				Trigger::begin = tutorialProgram->RequireGlobal("OnBegin");
				Trigger::selectionChanged = tutorialProgram->RequireGlobal("OnSelectionChanged");
				Trigger::onMessageConfirm = tutorialProgram->RequireGlobal("OnMessageConfirm");
			}
			catch (const std::exception&ex)
			{
				LogEvent(EventClass::Preset::configurationError,ex.what());
			}
		}
	}

	void	ShowMessage(const String&name, bool forceOpenNow/*=false*/)
	{
		if (RuntimeConfig::aiMode)
			return;
		Detail::Message*msg = Detail::messages.QueryPointer(name);
		if (!msg)
			return;
		if (msg->shown)
			return;
		msg->shown = true;
		Detail::memory << name << nl;
		Detail::memory.Flush();
		Dialog::EnqueueTutorial(name,msg->lines,msg->image);
		if (forceOpenNow)
			Dialog::ShowNextTutorial();
	}

	void	HideMessage(const String&name)
	{
		Dialog::RemoveTutorial(name);
	}



	void	Create()
	{
		XML::Container xtutorials;
		xtutorials.LoadFromFile("resources/tutorial/messages.xml");
		for (const auto&xmessage : xtutorials.root_node.children)
		{
			if (xmessage.name == "message")
			{
				String name;
				ASSERT__(xmessage.Query("name",name));
				Detail::Message*msg = Detail::messages.SetNew(name);
				xmessage.Query("image",msg->image);	//optional
				explode('\n',xmessage.inner_content,msg->lines);
				for (auto&line : msg->lines)
				{
					line.trimThis();
					line.replace("%PRODUCT_NAME%",PRODUCT_NAME);
					line.replace("%FUEL%",Objects::ResourceTag(Objects::Resource::Fuel));
					line.replace("%MINERALS%",Objects::ResourceTag(Objects::Resource::Minerals));
					line.replace("%URANIUM%",Objects::ResourceTag(Objects::Resource::Uranium));
					line.replace("%ENERGY%",Objects::ResourceTag(Objects::Resource::Energy));
					
				}
			}
			else
				FATAL__("Found unexpected entry in tutorial text file");
		
		}

		FileSystem::CreateDirectory("config");

//		#ifndef _DEBUG
		if (Detail::memory.Open(RuntimeConfig::ConfigFile("tutorialMemory.txt")))
		{
			String line;
			while (Detail::memory >> line)
			{
				Detail::Message*msg = Detail::messages.QueryPointer(line);
				if (msg)
					msg->shown = true;
			
			}
		
			Detail::memory.Close();
			Detail::memory.Append(RuntimeConfig::ConfigFile("tutorialMemory.txt"));
		}
		else
	//	#endif
			Detail::memory.Create(RuntimeConfig::ConfigFile("tutorialMemory.txt"));
	


		Interactive::Create();
	}

	void	SignalStart()
	{
		Interactive::started = true;
		Interactive::RebuildFactionUnits();
		Interactive::RebuildSelected();
		Interactive::Trigger::begin();
	}

	void	SignalRoundStart()
	{
		if (!Interactive::started)
			return;
		Interactive::RebuildFactionUnits();
	}


	void	SignalEntitySelectionChange(bool stateChangeOnly)
	{
		if (!Interactive::started)
			return;
		Interactive::RebuildSelected();

		Interactive::Trigger::selectionChanged();
	}

	
	void	SignalEnd()
	{
		Interactive::started = false;
		Interactive::objectTables.Clear();
	}

	void	ProcessTriggers(float timeDelta)
	{
		for (index_t i = 0; i < Interactive::API::activeTriggers.Count(); i++)
		{
			auto&t = Interactive::API::activeTriggers[i];
			t.remainingSeconds -= timeDelta;
			if (t.remainingSeconds < 0)
			{
				t.trigger();
				Interactive::API::activeTriggers.Erase(i);
				i--;
			}
		}
	}
}



#endif
