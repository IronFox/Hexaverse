
#include <global_root.h>
#include "tutorial.h"
#include "dialog.h"
#include <io/xml.h>
#include <container/hashtable.h>
#include "../../version.h"
#include "../clientRuntimeConfig.h"
#include <objects/resource.h>
#include <luaGameAPI.h>
#include "../clientEvents.h"
#include "../clientRuntimeConfig.h"
#include <engine/interface/mouse.h>
#include "../network.h"
#include "objectSelection.h"
#include "../network.h"
#include "ui.h"
#include "../serverProcess.h"
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

		int					progress = 1;

		StringFile			memory;

		StringTable<Message,Strategy::Swap>	messages;
		GUI::PWindow		taskWindow;
		bool				tutorialGameShown=false;


		void				HideGUI()
		{
			GUI::CloseAllModal();
			if (taskWindow)
				GUI::Hide(taskWindow);
		}

		bool				RegisterMemory(bool&shown, const String&name)
		{
			if (shown)
				return false;
			shown = true;
			memory << name << nl;
			memory.Flush();
			return true;
		}

		PathString	GetMemoryFilename()
		{
			return RuntimeConfig::ConfigFile("tutorialMemory.txt");
		}

		void	InitMemoryFile()
		{
			const PathString filename = GetMemoryFilename();
			try
			{
				memory.Open(filename);
				String line;
				while (memory >> line)
				{
					Message*msg = messages.QueryPointer(line);
					if (msg)
						msg->shown = true;
					elif (line == "tutorialGame")
						tutorialGameShown = true;
				}
		
				memory.Close();
				memory.Append(filename);
			}
			catch (...)
			{
				memory.Create(filename);
			}

		}

		void	FlushMemoryFile()
		{
			messages.VisitAllValues([](Message&m)
			{
				m.shown = false;
			});
			memory.Close();
			memory.Create(GetMemoryFilename());
			if (tutorialGameShown)
				memory << "tutorialGame" << nl;
			memory.Flush();
		}

	}

	namespace Interactive
	{
		class CameraEvent
		{
		public:
			Timer::Time		lastEvent;
			bool			happend=false;
			Lua::Function<void()>	trigger;

			void			Process()
			{
				if (happend && timer.GetSecondsSince(lastEvent) > 0.5f)
				{
					happend = false;
					trigger();
				}
			}
			void			Signal()
			{
				lastEvent = timer.Now();
				happend = true;
			}
		};

		Lua::PProgram		tutorialProgram;
		Lua::Table			api,myUnits, myResearch;
		bool				started = false;

		CameraEvent			drag,rotate,zoom;


		namespace Trigger
		{
			Lua::Function<void()>	checkTaskConditions,onMessageConfirm;
			Lua::Function<void(int)>	begin;
		}

		namespace API
		{


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

			void ReplaceTags(String&msg)
			{
				for (index_t i = 0; i < Objects::Resource::N; i++)
					msg.FindAndReplace('%'+String(Objects::Resource::Reinterpret(i).ToString())+'%',Objects::ResourceTag(i));
				msg.FindAndReplace("%Time%",Objects::TimeResourceTag());
			}

			void	DisplayMessage(String msg, const String&img)
			{
				Detail::HideGUI();
				
				ReplaceTags(msg);
				using namespace Detail;
				auto messageWindow = Dialog::CreateTutorialMessage("", msg,img);
				messageWindow->check = []()->Dialog::CheckResult{ return Network::client.IsConnected() ? Dialog::CheckResult::OkayContinue : Dialog::CheckResult::Abort; };
				messageWindow->onConfirm = 
				[]()
				{
					Trigger::onMessageConfirm();
				};
				messageWindow->canBeImplicitlyClosed = false;
				messageWindow->isTutorial = false;	//prevent tutorial message from opening the next message automatically
				GUI::Show(messageWindow);
			}

			void	SignalTaskStart(int taskAt)
			{
				if (taskAt == Detail::progress)
					return;
				Detail::progress = taskAt;
				Network::Channels::tutorialAt.SendTo(Network::client,taskAt);
			}

			void	DescribeTask(String msg)
			{
				Detail::HideGUI();
				using namespace Detail;

				ReplaceTags(msg);
				if (taskWindow)
				{
					To<GUI::Label>::Cast(taskWindow->GetComponents().First())->SetText(msg);
				}
				else
				{
					taskWindow = GUI::Window::Create();
					taskWindow->canBeImplicitlyClosed = false;

					taskWindow->AddNewComponent(new GUI::Label(msg))->AnchorTop(0).FillHorizontally(0);
					taskWindow->SetAutomaticHeight(true);
					taskWindow->SetAutoCenter(false);
					{
						auto mod = taskWindow->ModifyAnchorConfig();
						mod.Left().Unset();
						mod.Right().Set(0.1f);
						mod.Bottom().Set(0.1f);
					}
					taskWindow->SetSize(float2(0.25));
					taskWindow->isModal = false;
				}
				GUI::Show(taskWindow);
				Trigger::checkTaskConditions();
			}

			void	CallDelayed(float delay, Lua::Function<void()> f)
			{
				activeTriggers.Append().remainingSeconds = delay;
				activeTriggers.Last().trigger = f;
			}

			void	FocusCameraAt(int stackID)
			{
				if (stackID < 0 || stackID >= Grid::LowSetup::GridType::CellCount)
					return;
				Camera::TranslateTo(Grid::low.cells[stackID].centerVertex.xyz);
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


		void RebuildResearchState()
		{
			if (myResearch.IsEmpty())
			{
				myResearch = tutorialProgram->CreateTable();
				api.SetField("myResearch",myResearch);
			}
			else
				myResearch.ClearTable();

			Lua::Reference current;
			const auto&st = Objects::Faction::mine->researchState.researchState;
			//const index_t at = Objects::Faction::mine->researchState.GetActiveTopicIndex();
			Array<UINT32> q;
			Objects::Faction::mine->researchState.ExportResearchQueue(q);
			for (index_t i = 0; i < Objects::ResearchState::globalState.allTopics.Count(); i++)
			{
				auto t = Objects::ResearchState::GetTopic(i);
				if (t->IsGroup())
					continue;
				String key = t->GetFullName();

				Lua::Table outT = tutorialProgram->CreateTable();
				myResearch.SetField(key,outT);

				outT.SetField("fullName",key);
				outT.SetField("name",t->name);
				outT.SetField("max",t->maxProgress);
				outT.SetField("isEndless",t->maxProgress <= 0);
				
				const int prog = t->getProgress(st);
				outT.SetField("progress",prog);
				outT.SetField("isCompleted",t->ResearchCompleted(st));

				bool isActive = q.IsNotEmpty() && q.First() == i;
				outT.SetField("isActive",isActive);
				outT.SetField("isQueued",q.Contains((UINT32)i));

				if (isActive)
					current = outT;
			}


			api.SetField("myCurrentResearch",current);
		}

		void	ProcessTriggers()
		{
			if (!RuntimeConfig::tutorialMode)
				return;
			const float timeDelta = Engine::timing.delta;
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

			Interactive::rotate.Process();
			Interactive::drag.Process();
			Interactive::zoom.Process();

		}

		void	SignalCameraDrag()
		{
			if (RuntimeConfig::tutorialMode)
				Interactive::drag.Signal();
		}

		void	SignalCameraRotation()
		{
			if (RuntimeConfig::tutorialMode)
				Interactive::rotate.Signal();
		}

		void	SignalZoom()
		{
			if (RuntimeConfig::tutorialMode)
				Interactive::zoom.Signal();
		}

		void	SignalEntitySelectionChange()
		{
			if (!Interactive::started)
				return;
			try
			{
				Interactive::RebuildFactionUnits();
				Interactive::RebuildSelected();

				Interactive::Trigger::checkTaskConditions();
			}
			catch (const Lua::Exception&ex)
			{
				LogEvent(EventClass::Preset::configurationError,ex.what());
			}
		}




		void Create()
		{
			try
			{
				objectTables.Clear();
				tutorialProgram = Lua::Program::Create("Tutorial");
				api = Lua::CreateGeneralAPI(tutorialProgram);
				Lua::DefineLowGridCells(api);
				myUnits = tutorialProgram->CreateTable();
				api.SetField("myUnits",myUnits);

				api.SetFunction("DisplayMessage",API::DisplayMessage);
				api.SetFunction("DescribeTask",API::DescribeTask);
				api.SetFunction("CallDelayed",API::CallDelayed);
				api.SetFunction("FocusCameraAt",API::FocusCameraAt);
				api.SetFunction("SignalTaskStart",API::SignalTaskStart);


				tutorialProgram->ExecuteFile("../lua/tutorial.lua");

				Trigger::begin = tutorialProgram->RequireGlobal("OnBegin");
				Trigger::checkTaskConditions = tutorialProgram->RequireGlobal("CheckTaskConditions");
				Trigger::onMessageConfirm = tutorialProgram->RequireGlobal("OnMessageConfirm");
				drag.trigger = tutorialProgram->RequireGlobal("OnCameraDrag");
				rotate.trigger = tutorialProgram->RequireGlobal("OnCameraRotate");
				zoom.trigger = tutorialProgram->RequireGlobal("OnCameraZoom");


				Camera::onZoom << SignalZoom;
				Camera::onDrag << SignalCameraDrag;
				Camera::onRotate << SignalCameraRotation;
				GlobalEvents::frameBegin << ProcessTriggers;

				GlobalEvents::invalidateObjectCache << SignalEntitySelectionChange;	//something happend to the selected unit
				Objects::onSelectionChanged << SignalEntitySelectionChange;
			}
			catch (const std::exception&ex)
			{
				LogEvent(EventClass::Preset::configurationError,ex.what());
			}
		}
	}



	void	ShowMessage(const String&name, bool forceOpenNow/*=false*/, bool overrideMemory/*=false*/)
	{
		if (RuntimeConfig::aiMode)
			return;
		Detail::Message*msg = Detail::messages.QueryPointer(name);
		if (!msg)
			return;
		if (!Detail::RegisterMemory(msg->shown,name) && !overrideMemory)
			return;
		Dialog::EnqueueTutorial(name,msg->lines,msg->image,forceOpenNow);
		if (forceOpenNow)
			Dialog::ShowNextTutorial(!overrideMemory);
	}

	void	HideMessage(const String&name)
	{
		Dialog::RemoveTutorial(name);
	}

	void	FlushMemory()
	{
		Detail::FlushMemoryFile();
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
					line.TrimThis();
					line.FindAndReplace("%PRODUCT_NAME%",PRODUCT_NAME);
					line.FindAndReplace("%FUEL%",Objects::ResourceTag(Objects::Resource::Fuel));
					line.FindAndReplace("%MINERALS%",Objects::ResourceTag(Objects::Resource::Minerals));
					line.FindAndReplace("%URANIUM%",Objects::ResourceTag(Objects::Resource::Uranium));
					line.FindAndReplace("%ENERGY%",Objects::ResourceTag(Objects::Resource::Energy));
					
				}
			}
			else
				FATAL__("Found unexpected entry in tutorial text file");
		
		}

		FileSystem::CreateDirectory("config");

		Detail::InitMemoryFile();
	

		GlobalEvents::frameBegin << []()
		{
			if (!Engine::mouse.buttons.pressed)		
			{
				if (!Tutorial::movementTutorialIssued && Objects::selectedEntity && Objects::selectedEntity->isOwnedByMe && Objects::selectedEntity->IsMobile() && timer.GetSecondsSince(Tutorial::lastDelayingEvent) > 3.f)
				{
					Tutorial::movementTutorialIssued = true;
					Tutorial::ShowMessage("Move To");
					Tutorial::lastDelayingEvent = timer.Now();
				}
				if (!Tutorial::endRoundTutorialIssued && Network::client.IsConnected() && RuntimeConfig::phase == RuntimeConfig::Phase::CommandPhase && timer.GetSecondsSince(Tutorial::lastDelayingEvent) > 10.f)
				{
					Tutorial::endRoundTutorialIssued = true;
					Tutorial::ShowMessage("End Round");
					Tutorial::lastDelayingEvent = timer.Now();
				}
			}
		};

		if (!RuntimeConfig::aiMode)
		{
			Interactive::Create();
			if (Detail::RegisterMemory(Detail::tutorialGameShown,"tutorialGame"))
			{
				Dialog::Confirm("Welcome to Hexaverse. Would you like to start a tutorial game server to learn the game's basics?",[]()
				{
					ServerProcess::StartTutorialServer();
				},
				[](){return Dialog::CheckResult::OkayContinue;});
			}
		}

	}

	void	SignalStart()
	{
		try
		{
			Interactive::started = true;
			Interactive::RebuildFactionUnits();
			Interactive::RebuildSelected();
			Interactive::RebuildResearchState();
			Interactive::Trigger::begin(Detail::progress);
		}
		catch (const Lua::Exception&ex)
		{
			LogEvent(EventClass::Preset::configurationError,ex.what());
		}
	}

	void	SignalRoundStart()
	{
		if (!Interactive::started)
			return;
		try
		{
			Interactive::RebuildFactionUnits();
			Interactive::Trigger::checkTaskConditions();
		}
		catch (const Lua::Exception&ex)
		{
			LogEvent(EventClass::Preset::configurationError,ex.what());
		}
		
	}




	void	SignalResearchChange()
	{
		if (!Interactive::started || !Objects::Faction::mine)
			return;
		try
		{
			Interactive::RebuildResearchState();
			Interactive::Trigger::checkTaskConditions();
		}
		catch (const Lua::Exception&ex)
		{
			LogEvent(EventClass::Preset::configurationError,ex.what());
		}
	}

	
	void	SignalEnd()
	{
		Interactive::started = false;
		Interactive::objectTables.Clear();
		Detail::HideGUI();
		Detail::progress = 1;
	}


	void	SetProgress(int at)
	{
		if (at == Detail::progress)
			return;
		Detail::progress = at;
		try
		{
			if (Interactive::started)
				Interactive::Trigger::begin(Detail::progress);
		}
		catch (const Lua::Exception&ex)
		{
			LogEvent(EventClass::Preset::configurationError,ex.what());
		}
	}

}


