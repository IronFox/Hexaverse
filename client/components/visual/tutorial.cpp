
#include <global_root.h>
#include "tutorial.h"
#include "hud/dialog.h"
#include <io/xml.h>
#include <container/hashtable.h>
#include "../../version.h"
#include "../clientRuntimeConfig.h"
#include <objects/resource.h>
#include <lua/luaGameAPI.h>
#include "../clientEvents.h"
#include "../clientRuntimeConfig.h"
#include <engine/interface/mouse.h>
#include "../network.h"
#include "objectSelection.h"
#include "../network.h"
#include "hud/hexGrid.h"
#include <serverProcess.h>
#include <lua.hpp>
#include "../configurationMode.h"
#include "../cameraControl.h"

namespace Tutorial
{
	Timer::Time	lastDelayingEvent = timer.Now();
	bool			movementTutorialIssued = false,
					endRoundTutorialIssued = false;


	namespace Detail
	{
		using namespace HUD::Component;

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
		PWindow				taskWindow;
		HUD::Dialog::PDialog	messageWindow;
		bool				tutorialGameShown=false;

		HUD::Dialog::WDialog	holdOn;
		index_t				holdOnID = InvalidIndex;
		Timer::Time			holdOnCreated;


		bool				WaitingForUserOkay()
		{
			return !!messageWindow /*&& HUD::WindowStack::IsVisible(messageWindow)*/;
		}

		void				HideGUI()
		{
			HUD::WindowStack::CloseAll();
			if (messageWindow)
			{
				HUD::WindowStack::Hide(messageWindow);
				messageWindow= nullptr;
			}
			if (taskWindow)
			{
				HUD::WindowStack::Hide(taskWindow);
				taskWindow= nullptr;
			}
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
			return RuntimeConfig::myConfig.Lookup("tutorialMemory.txt");
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

		bool				started = false, expectStart = false;

		CameraEvent			drag,rotate,zoom;


		namespace Trigger
		{
			Lua::Function<void()>	checkTaskConditions,onMessageConfirm;
			Lua::Function<void(int)>	begin;
		}

		Lua::API scriptAPI;


		void	CheckTaskConditions()
		{
			ASSERT__(scriptAPI.myEntityList.GetLength() > 0);
			Trigger::checkTaskConditions();
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
			//		To<Label>::Cast(taskWindow->GetReferences().First())->SetText("Well done!");
			//}

			void ReplaceTags(String&msg)
			{
				for (index_t i = 0; i < Objects::Resource::N; i++)
					msg.FindAndReplace('%'+String(Objects::Resource::Reinterpret(i).ToString())+'%',Objects::ResourceTag(i));
				msg.FindAndReplace("%Time%",Objects::TimeResourceTag());
			}

			void	DisplayMessage(String msg, const String&img)
			{
				DBG_ASSERT__(Interactive::API::activeTriggers.IsEmpty());
				Detail::HideGUI();
				DBG_ASSERT_EQUAL__(HUD::WindowStack::CountVisibleWindows(),0);
				ReplaceTags(msg);
				using namespace Detail;
				messageWindow = HUD::Dialog::CreateTutorialMessage("", msg,img);
				messageWindow->check = []()->HUD::Dialog::CheckResult{ return Network::client.IsConnected() ? HUD::Dialog::CheckResult::OkayContinue : HUD::Dialog::CheckResult::Abort; };
				messageWindow->onConfirm = 
				[]()
				{
					HUD::WindowStack::Hide(messageWindow);
					messageWindow = nullptr;
					Trigger::onMessageConfirm();
					if (!Detail::WaitingForUserOkay())
						Interactive::CheckTaskConditions();
				};
				messageWindow->canBeImplicitlyClosed = false;
				messageWindow->isTutorial = false;	//prevent tutorial message from opening the next message automatically
				HUD::WindowStack::Show(messageWindow);
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
				DBG_ASSERT__(!Detail::WaitingForUserOkay());
				Detail::HideGUI();
				using namespace Detail;

				ReplaceTags(msg);
				if (taskWindow)
				{
					To<Label>::Cast(taskWindow->GetReferences().First())->SetText(msg);
				}
				else
				{
					taskWindow = Window::Create();
					taskWindow->canBeImplicitlyClosed = false;

					taskWindow->AddNewComponent(new Label(msg),CLOCATION)->AnchorTop(0).FillHorizontally(0);
					taskWindow->SetAutomaticHeight(true);
					taskWindow->Anchor().Bottom(0.15f,0.25f).Right(0.05f,0.25f);

					taskWindow->isModal = false;
				}
				HUD::WindowStack::Show(taskWindow);
				CheckTaskConditions();
			}

			void	ClearDelayed()
			{
				activeTriggers.Clear();
			}

			void	CallDelayed(float delay, Lua::Function<void()> f)
			{
				DBG_ASSERT__(!Detail::WaitingForUserOkay());
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


		void RebuildFactionUnits()
		{
			scriptAPI.FlushAllObjects();
			scriptAPI.AddFactionEntities(Objects::Faction::mine);
			scriptAPI.SetSelectedUnit(Objects::selectedEntity,false);
			scriptAPI.UpdateMyResearch();
		}


		void RebuildResearchState()
		{
			scriptAPI.UpdateMyResearch();
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
					auto copy = t;
					Interactive::API::activeTriggers.Erase(i);	//change order to assure we don't trigger verification issues in debug mode
					i--;
					copy.trigger();
				}
			}

			if (!Detail::WaitingForUserOkay())
			{
				Interactive::rotate.Process();
				Interactive::drag.Process();
				Interactive::zoom.Process();
			}

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
				if (!Detail::WaitingForUserOkay())
					Interactive::CheckTaskConditions();
			}
			catch (const Lua::Exception&ex)
			{
				LogEvent(EventClass::Preset::configurationError,ex.what());
			}
		}

		void	SignalStart()
		{
			if (!Interactive::expectStart)
				return;
			try
			{
				Interactive::started = true;
				Interactive::RebuildFactionUnits();
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
				if (!Detail::WaitingForUserOkay())
					Interactive::CheckTaskConditions();
			}
			catch (const Lua::Exception&ex)
			{
				LogEvent(EventClass::Preset::configurationError,ex.what());
			}
		
		}
		


	
		void	SignalEnd()
		{
			using namespace Interactive;
			using namespace Detail;

			expectStart = false;
			started = false;
			scriptAPI.FlushAllObjects(true);

			if (holdOnID != InvalidIndex)
			{
				GlobalEvents::frameBegin.Unregister(holdOnID);
				holdOnID = InvalidIndex;
			}

			Detail::HideGUI();
			Detail::progress = 1;
		}


		void Create()
		{
			try
			{
				scriptAPI.Create("Tutorial");
				scriptAPI.AddGrid();
				scriptAPI.gameTable.SetFunction("DisplayMessage",API::DisplayMessage);
				scriptAPI.gameTable.SetFunction("DescribeTask",API::DescribeTask);
				scriptAPI.gameTable.SetFunction("CallDelayed",API::CallDelayed);
				scriptAPI.gameTable.SetFunction("ClearAllDelayed",API::ClearDelayed);
				scriptAPI.gameTable.SetFunction("FocusCameraAt",API::FocusCameraAt);
				scriptAPI.gameTable.SetFunction("SignalTaskStart",API::SignalTaskStart);
				scriptAPI.program->ExecuteFile("../lua/tutorial.lua");

				Trigger::begin = scriptAPI.program->RequireGlobal("OnBegin");
				Trigger::checkTaskConditions = scriptAPI.program->RequireGlobal("CheckTaskConditions");
				Trigger::onMessageConfirm = scriptAPI.program->RequireGlobal("OnMessageConfirm");
				drag.trigger = scriptAPI.program->RequireGlobal("OnCameraDrag");
				rotate.trigger = scriptAPI.program->RequireGlobal("OnCameraRotate");
				zoom.trigger = scriptAPI.program->RequireGlobal("OnCameraZoom");


				Camera::onZoom << SignalZoom;
				Camera::onDrag << SignalCameraDrag;
				Camera::onRotate << SignalCameraRotation;
				GlobalEvents::frameBegin << ProcessTriggers;

				GlobalEvents::invalidateObjectCache << SignalEntitySelectionChange;	//something happend to the selected unit
				Objects::onSelectionChanged << SignalEntitySelectionChange;


				GlobalEvents::serverConnectionLost << SignalEnd;
				GlobalEvents::serverConnectionFailed << SignalEnd;
				GlobalEvents::playerControlBegin << SignalRoundStart;
				GlobalEvents::beginGame << SignalStart;
			}
			catch (const std::exception&ex)
			{
				LogEvent(EventClass::Preset::configurationError,ex.what());
			}
		}
	}



	void	SignalResearchChange()
	{
		if (!Interactive::started || !Objects::Faction::mine)
			return;
		try
		{
			Interactive::RebuildResearchState();
			if (!Detail::WaitingForUserOkay())
				Interactive::CheckTaskConditions();
		}
		catch (const Lua::Exception&ex)
		{
			LogEvent(EventClass::Preset::configurationError,ex.what());
		}
	}

	void	ShowMessage(const String&name, bool forceOpenNow/*=false*/, bool overrideMemory/*=false*/)
	{
		if (RuntimeConfig::aiMode || RuntimeConfig::tutorialMode || ConfigurationMode::IsActive())
			return;
		Detail::Message*msg = Detail::messages.QueryPointer(name);
		if (!msg)
			return;
		if (!Detail::RegisterMemory(msg->shown,name) && !overrideMemory)
			return;
		HUD::Dialog::EnqueueTutorial(name,msg->lines,msg->image,forceOpenNow);
		if (forceOpenNow)
			HUD::Dialog::ShowNextTutorial(!overrideMemory);
	}

	void	HideMessage(const String&name)
	{
		HUD::Dialog::RemoveTutorial(name);
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
				HUD::Dialog::Confirm("Welcome to Hexaverse. Would you like to start a tutorial game server to learn the game's basics?",[]()
				{
					Tutorial::StartTutorialServer();
				},
				[](){return HUD::Dialog::CheckResult::OkayContinue;});
			}
		}

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


	void	StartTutorialServer()
	{
		using namespace Detail;

		if (ServerProcess::IsRunning())
		{
			HUD::Dialog::Confirm("In order to start a tutorial server, the local server must be shut down. Continue?",
			[]()
			{
				ServerProcess::End();
				StartTutorialServer();
			},
			[]()->HUD::Dialog::CheckResult
			{
				if (!ServerProcess::IsRunning())
					return HUD::Dialog::CheckResult::Confirm;
				return HUD::Dialog::CheckResult::OkayContinue;
			});
			return;
		}

		if (HUD::WindowStack::IsVisible(holdOn.lock()))
			return;

		Interactive::expectStart = true;


		LogEvent(EventClass::Preset::uncommon,"Starting tutorial server...");
		RuntimeConfig::serverAddress = Network::ServerAddress("localhost");
		holdOn = HUD::Dialog::PermanentMessage("Hold on. Getting tutorial server ready.",[]()->HUD::Dialog::CheckResult
		{
			if (Network::haveFaction)
			{
				//HUD::Dialog::ShowMessage("Welcome to the Hexaverse Tutorial");
				return HUD::Dialog::CheckResult::Abort;
			}
			return HUD::Dialog::CheckResult::OkayContinue;
		});
		holdOnCreated = timer.Now();

		if (holdOnID == InvalidIndex)
			holdOnID = GlobalEvents::frameBegin.Register([]()
			{
				HUD::Dialog::PDialog d = holdOn.lock();
				if (!d || !HUD::WindowStack::IsVisible(d))
				{
					GlobalEvents::frameBegin.Unregister(holdOnID);
					holdOnID = InvalidIndex;
					return;
				}
				if (timer.GetSecondsSince(holdOnCreated) > 30)
				{
					ServerProcess::End();
					HUD::WindowStack::Hide(d);
					GlobalEvents::frameBegin.Unregister(holdOnID);
					holdOnID = InvalidIndex;
					RuntimeConfig::serverAddress = Network::ServerAddress();
					HUD::Dialog::ShowMessage("Tutorial server took too long to start. Something went wrong. Please check any available logs in\n"
							+String(FileSystem::Folder( RuntimeConfig::myConfig.GetRoot()).ParentLocation()->GetLocation())
						+"\nSorry about this :S");
				}

			});
			
		ServerProcess::Begin(true,true,"");
	}

}


