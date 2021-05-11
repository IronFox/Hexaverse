
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
	Time::Stamp		lastDelayingEvent;
	bool			movementTutorialIssued = false,
					endRoundTutorialIssued = false;

	using namespace Time::Literals;


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
		Time::Stamp			holdOnCreated;


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
				try
				{
					memory.Create(filename);
				}
				catch (const std::exception&ex)
				{
					LogEvent(EventClass::Preset::fileError,ex.what());
				}

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
			Time::Stamp		lastEvent;
			bool			happend=false;
			Lua::Function<void()>	trigger;

			void			Process()
			{
				if (happend && lastEvent.Elapsed() > 0.5_s)
				{
					happend = false;
					trigger();
				}
			}
			void			Signal()
			{
				lastEvent.Update();
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

			void	EnableWorkingStructure(const TUniqueID&entityID, bool doEnable)
			{
				Protocol::EnableStructure cmd;
				cmd.enable = doEnable;
				cmd.id = entityID;
				Network::Channels::enableStructure.SendTo(Network::client, cmd);
			}


			//void	NextTask()
			//{
			//	if (taskWindow)
			//		To<Label>::Cast(taskWindow->GetReferences().First())->SetText("Well done!");
			//}

			void ReplaceTags(String&msg)
			{
				for (index_t i = 0; i < Objects::Resource::table.GetSize(); i++)
					msg.FindAndReplaceIgnoreCase('%'+String(Objects::Resource::ID(i).ToString())+'%',Objects::Resource::ID(i).GetTagStr());
				char c = Objects::TimeResourceTag();
				msg.FindAndReplace("%Time%",StringRef(&c,1));
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
			scriptAPI.UpdateResearch();
			//ASSERT__(scriptAPI.myFactionIsSet);	can't verify this: if disconnecting, order cannot be asserted, might trigger this while flushing objects, before closing down tutorial mode
		}


		void RebuildResearchState()
		{
			scriptAPI.UpdateResearch();
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
			catch (const std::exception&ex)
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
			catch (const std::exception&ex)
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
			catch (const std::exception&ex)
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
				scriptAPI.onEnableWorkingStructure = API::EnableWorkingStructure;

				scriptAPI.Create("Tutorial");
				scriptAPI.AddGrid();
				scriptAPI.gameTable.SetFunction("DisplayMessage",API::DisplayMessage);
				scriptAPI.gameTable.SetFunction("DescribeTask",API::DescribeTask);
				scriptAPI.gameTable.SetFunction("CallDelayed",API::CallDelayed);
				scriptAPI.gameTable.SetFunction("ClearAllDelayed",API::ClearDelayed);
				scriptAPI.gameTable.SetFunction("FocusCameraAt",API::FocusCameraAt);
				scriptAPI.gameTable.SetFunction("SignalTaskStart",API::SignalTaskStart);
				scriptAPI.program->ExecuteFile(L"../lua/tutorial.lua",nullptr);

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
				GlobalEvents::clientControlBegin << SignalRoundStart;
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
		catch (const std::exception&ex)
		{
			LogEvent(EventClass::Preset::configurationError,ex.what());
		}
	}

	void	ShowMessage(const String&name, bool forceOpenNow/*=false*/, bool overrideMemory/*=false*/)
	{
		if (RuntimeConfig::aiMode || ConfigurationMode::IsActive())
			return;
		Detail::Message*msg = Detail::messages.QueryPointer(name);
		if (!msg)
			return;
		if (!Detail::RegisterMemory(msg->shown,name) && !overrideMemory)
			return;
		if (RuntimeConfig::tutorialMode)
			return;	//don't enqueue anything. assume known
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
		xtutorials.LoadFromFile(L"resources/tutorial/messages.xml");
		for (const auto&xmessage : xtutorials.root_node.children)
		{
			if (xmessage.name == "message")
			{
				String name;
				ASSERT__(xmessage.Query("name",name));
				Detail::Message*msg = Detail::messages.SetNew(name);
				xmessage.Query("image",msg->image);	//optional
				Explode('\n',xmessage.inner_content,msg->lines);
				for (auto&line : msg->lines)
				{
					line.TrimThis();
					line.FindAndReplace("%PRODUCT_NAME%",PRODUCT_NAME);
					Interactive::API::ReplaceTags(line);
				}
			}
			else
				FATAL__("Found unexpected entry in tutorial text file");
		
		}

		FileSystem::CreateDirectory(L"config");

		Detail::InitMemoryFile();
	

		GlobalEvents::frameBegin << []()
		{
			if (!Engine::mouse.buttons.pressed)		
			{
				if (!Tutorial::movementTutorialIssued && Objects::selectedEntity && Objects::selectedEntity->isOwnedByMe && Objects::selectedEntity->IsMobile() && Tutorial::lastDelayingEvent.Elapsed() > 3_s)
				{
					Tutorial::movementTutorialIssued = true;
					Tutorial::ShowMessage("Move To");
					Tutorial::lastDelayingEvent.Update();
				}
				if (!Tutorial::endRoundTutorialIssued && Network::client.IsConnected() && RuntimeConfig::phase == RuntimeConfig::Phase::CommandPhase && Tutorial::lastDelayingEvent.Elapsed() > 10_s)
				{
					Tutorial::endRoundTutorialIssued = true;
					Tutorial::ShowMessage("End Round");
					Tutorial::lastDelayingEvent.Update();
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
		catch (const std::exception&ex)
		{
			LogEvent(EventClass::Preset::configurationError,ex.what());
		}
	}


	void	SignalConnectedToTutorialServer()
	{
		Interactive::expectStart = true;
	}


	static Random random;

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

		const UINT16 port = TUTORIAL_BASE_PORTS_FROM + random.Next((65535 - TUTORIAL_BASE_PORTS_FROM)/3) * 3;

		LogEvent(EventClass::Preset::uncommon,"Starting tutorial server ...");
		LogEvent(EventClass::Preset::debug,"Starting tutorial server on port "+String(port));
		RuntimeConfig::serverAddress = Network::ServerAddress("localhost:"+String(port));
		holdOn = HUD::Dialog::PermanentMessage("Hold on. Getting tutorial server ready.",[]()->HUD::Dialog::CheckResult
		{
			if (Network::haveFaction)
			{
				//HUD::Dialog::ShowMessage("Welcome to the Hexaverse Tutorial");
				return HUD::Dialog::CheckResult::Abort;
			}
			return HUD::Dialog::CheckResult::OkayContinue;
		});
		holdOnCreated.Update();

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
				if (holdOnCreated.Elapsed() > 30_s)
				{
					ServerProcess::End();
					HUD::WindowStack::Hide(d);
					GlobalEvents::frameBegin.Unregister(holdOnID);
					holdOnID = InvalidIndex;
					RuntimeConfig::serverAddress = Network::ServerAddress();
					HUD::Dialog::ShowMessage("Tutorial server took too long to start. Something went wrong. Please check any available logs in\n"
							+FileSystem::PathToString(FileSystem::Folder( RuntimeConfig::myConfig.GetRoot()).ParentLocation()->GetLocation(),false)
						+"\nSorry about this :S");
				}

			});
			
		ServerProcess::Begin(true,true,L"",port);
	}

}


