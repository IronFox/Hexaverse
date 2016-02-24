

#ifdef HAS_WINDOW
	#include "./ExtRenderer/DirectX11.h"
	#include <engine/display.h>
	#include <engine/auto_display_configure.h>
	#include <engine/aspect.h>
#endif

#include <math/vclasses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <general/thread_system.h>
#include <string/string_converter.h>
#include <general/random.h>
#include <image/converter/magic.h>
#include <structure/mapping.h>
#ifdef HAS_WINDOW
	#include "./DX11/texture_font.h"
#endif
#include <io/config.h>
#include <container/string_list.h>
#include <io/file_system.h>

#include "../shared/sphereGrid.h"
#include "../shared/objects.h"
#include "TechniteLogic/techniteProtocol.h"
#include "components/techniteLogicLink.h"

#ifdef HAS_WINDOW
	#include "components/explosions.h"
#endif

#include <geometry/bvh.h>
#include "../shared/noise.h"

#include "version.h"
#include "../shared/protocol.h"
#include "../shared/objects/faction.h"
#include "../shared/objects/structure.h"
#include "../shared/objects/gridAndChoices.h"
#include "../shared/objects/visualSelection.h"
#include "../shared/objects/visualChoices.h"
#include "../shared/objects/prototypes.h"
#include "../shared/objects/rendering.h"

#include "components/network.h"
#include "components/ui.h"
#include "components/animation.h"
#include "components/cameraControl.h"
#include "components/progressBar.h"
#include "components/dialog.h"
#include "components/tutorial.h"
#include "components/lineRenderer.h"
#include "components/surfaceRenderer.h"
#include "components/ai.h"
#include "components/clientRuntimeConfig.h"
#include "components/world.h"
#include "components/atmosphere.h"
#include "components/guiHostList.h"
#include "components/serverProcess.h"
#include "components/audioInterface.h"

#include "rc/Resource.h"

/**
The following DWORD causes Nvidia Optimus to select the high performance GPU.
Don't change the name, value, or dllexport-declaration ("C", so it shows up as that name in the export and not some mangled thing)
*/
extern "C"
{
	_declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;
}

#ifdef HAS_WINDOW
	namespace DX11 = Engine::DX11; //!< Shortcut

	Engine::Display<Engine::DirectX11>	display;
	Engine::Textout<DX11::TextureFont>	textout;


	Dialog::PMenu		mainMenu;

#endif


Timer::Time applicationStart;


//static Mutex mutex;
SpinLock	eventLock;
#ifdef HAS_WINDOW

	struct VisualEvent
	{
		String		message;
		float3		color = float3(1);
		float		age=0.f,scale=1.f;
		static const float MaxAge;

		/**/		VisualEvent():color(1.f),age(0)
		{}

		static float3 ToAverage(const float3&source, float level)
		{
			float3 avg = (float3)1.f;
			//float3((source.r + source.g + source.b)/3.f);
			Vec::interpolate(source,avg,level,avg);
			return avg;
		}

		/**/		VisualEvent(const String&message, EventClass type):message(message)
		{
			color = float3(1.f);
			switch (type.flavor)
			{
				case EventFlavor::Gain:
					color = float3(0,1,0);
				break;
				case EventFlavor::Loss:
					color = float3(1,0.35,0.3);
				break;
				case EventFlavor::SystemError:
				case EventFlavor::UserError:
					color = float3(1,0.3,0.4);
				break;
				case EventFlavor::Neutral:
					if (type.significance == EventSignificance::Important)
						color = float3(0.5,0.5,1);
				break;
			}


			switch (type.significance)
			{
				case EventSignificance::ClientFatal:
				case EventSignificance::FactionFatal:
				case EventSignificance::SystemFatal:
				case EventSignificance::Important:
				case EventSignificance::NonFatalError:
					age = -MaxAge;
				break;
				case EventSignificance::Uncommon:
					color = ToAverage(color,0.65f);
					color *= 0.85f;
					age = -MaxAge/2.f;
				break;
				default:
					color = ToAverage(color,0.75f);
					color *= 0.7f;
					age = -MaxAge/4.f;
				break;
			}
		}

		void		adoptData(VisualEvent&other)
		{
			message.adoptData(other.message);
			color = other.color;
			age = other.age;
			scale = other.scale;
		}


	};

	/*static */const float VisualEvent::MaxAge = 30.f;


	Buffer<VisualEvent,0,Adopt>	events;
#endif

//LogFile	eventLog;

#ifdef _DEBUG
namespace Debug
{
	float3			lastClickPoint;
	Grid::LCellIdI	lastClickCell;
}
#endif


Timer::Time rightMouseDragBegun;
bool rightMouseDragged = false;
int2 rightMouseDragFrom;



#ifdef HAS_WINDOW
	void						QueryNickname();
	void						QueryNicknameIfEmpty();
	void						RefreshSessionView();
	void						OnSelectionChange(bool stateChangeOnly);
#else

#endif



void							SignalMarkerDeletion(TUniqueID markerID);	//must be copy to prevent weird shit


/**
Triggered when entering command mode and animations have finished (whichever of those events happens later)
*/
void	OnPlayerControlBegin()
{
	#ifdef HAS_WINDOW
		UI::RefreshButtonState();
	#endif
	Objects::RefreshSkyHubConnections();
	Animation::UpdateAutomation(CLOCATION);
}

void	OnTutorialsChanged()
{
	UI::HexGrid::RefreshTutorials();
}




//void LogEvent(const EventClass&type,  const Objects::WorldEntity*e, const String&_event);



bool CanEnableAtAll()
{
	return Network::client.IsConnected();

}

static void SendDirectTo(const Objects::PWorldEntity&v, const ArrayRef<Grid::LCellIdI>&targets, bool detailedPath, bool leftClick, Objects::Vehicle::mode_t mode)
{
	Protocol::EntityDirectCommand	cmd;
	cmd.id = v->id;
	cmd.mode = (UINT16)mode;
	cmd.path = targets;
	cmd.directFlags = 0;
	if (detailedPath)
		cmd.directFlags |= Protocol::DirectFlag::Detailed;
	if (leftClick)
		cmd.directFlags |= Protocol::DirectFlag::LeftClick;
	Network::Channels::entityDirectCommand.SendTo(Network::client,cmd);
}


bool	MoveVehicleTo(const Objects::PMobileEntity&v, const Grid::LCellIdI&prev, Grid::LCellIdI target)
{
	if (prev == target)
		return true;
	Buffer0<Objects::PathNode>		path;
	{
		bool tooLong;
		bool foundPath;

		if (v->IsVehicle())
		{
			foundPath = Objects::AppendShortestPathI((const Objects::Vehicle*)v.get(),prev,target,path,false,&tooLong);
		}
		else
		{
			if (v->IsSatellite())
				foundPath = Objects::AppendShortestPathI((const Objects::Satellite*)v.get(),prev,target.stackID,path,false,&tooLong);
			else
				FATAL__("Routing not implemented for "+v->GetTypeName());
		}
		if (!foundPath)
		{
			if (!Grid::actualWorld.LowCellIsClearIgnoreStructure(target) && target.height+1 <= Grid::CellStack::TopHeight)
			{
				target++;
				return MoveVehicleTo(v,prev,target);
			}
			LogEvent(EventClass::Preset::commandRejected,v.get(),"Unable to find route to designated destination");
			//SendDirectTo(v,target,false,false,Objects::CursorMode::Idle);	//maybe some error in the client
			return false;
		}
		if (!tooLong)
			SendDirectTo(v,target,false,false,Objects::CursorMode::Idle);	//all good
		else
		{
			LogEvent(EventSignificance::Common,v.get(),"Splitting long path into parts");
			static Buffer0<Grid::LCellIdI>	waypoints;
			waypoints.Clear();
			bool open = true;
			for (index_t i = 0; i < path.length(); i++)
			{
				if (!((i+1)%5))
				{
					waypoints << path[i].cell;
					//SendDirectTo(v,path[i].cell,false,Objects::CursorMode::Idle);
					open = false;
				}
				else
					open = true;
			}

			if (open)
				waypoints << path.last().cell;
				//SendDirectTo(v,path.last().cell,false,Objects::CursorMode::Idle);
			SendDirectTo(v,waypoints.ToRef(),false,false,Objects::CursorMode::Idle);
		}
	}
	return true;
}


bool	DirectTo(const Objects::PWorldEntity&v, const ArrayRef<Grid::LCellIdI>&targets, bool detailedPath, bool leftClick, Objects::Vehicle::mode_t mode, bool interactive)
{
	if (!v)
		return false;
	DBG_ASSERT__(!RuntimeConfig::aiMode || !Network::newRoundRequested);
	const bool moveTo = mode == Objects::CursorMode::Idle && !leftClick;
	if (moveTo && v->IsSatellite())
		foreach (targets,t)
			ASSERT_EQUAL__(t->height,v->location.height);


	if (v->IsMobile() && moveTo)
	{
		Objects::PMobileEntity me = To<Objects::MobileEntity>::Cast(v);

		#ifdef HAS_WINDOW
			if (interactive && me->IsSatellite() && !me->HasMotionTasks())
			{
				Objects::PSatellite sat = To<Objects::Satellite>::Cast(me);
				if (sat->IsSkyHub())
				{
					Objects::PSkyHub hub = To<Objects::SkyHub>::Cast(me);
					if (!hub->connectedTo.expired())
					{
						TUniqueID vid = v->id;
						Array<Grid::LCellIdI> pathCopy = targets;
						Grid::LCellIdI sourceLoc = me->GetLastPointOfOrigin();
						Grid::LCellIdI originLoc = me->location;
						Dialog::Confirm(me->GetNiceName()+" is currently tethered to "+hub->connectedTo.lock()->CastToStructure()->GetNiceName()+". The link keeps it afloat for free, and supplies fuel to distribute to other nearby satellites."
										" Moving the satellite will sever this link, and require it to be supplied by another hub satellite. Please confirm.",[vid,pathCopy,detailedPath,leftClick,mode]()
										{
											DirectTo(Objects::GetEntity(vid),pathCopy,detailedPath,leftClick,mode,false);
										},[vid,sourceLoc,originLoc]()->Dialog::CheckResult
										{
											Objects::PWorldEntity e = Objects::GetEntity(vid);
											if (!e || !e->IsMobile())
												return Dialog::CheckResult::Abort;
											Objects::PMobileEntity me = To<Objects::MobileEntity>::Cast(e);
											if (e->location != originLoc || me->GetLastPointOfOrigin() != sourceLoc)
												return Dialog::CheckResult::Abort;
											if (!RuntimeConfig::IsInAnyCommandPhase() || Objects::Faction::mine->requestedNewRound)
												return Dialog::CheckResult::OkayNotRightNow;
											return Dialog::CheckResult::OkayContinue;
										});
						return false;
					}
				}
			}
		#endif

		if (!detailedPath)
		{
			Grid::LCellIdI start = me->GetLastPointOfOrigin();
			foreach (targets,t)
			{
				if (!MoveVehicleTo(me,start,*t))
					return false;
				start = *t;
			}
		}
		else
			SendDirectTo(v,targets,detailedPath,false,Objects::CursorMode::Idle);
	}
	else
	{
		SendDirectTo(v,targets,detailedPath,leftClick,mode);
	}

	#ifdef HAS_WINDOW
		Objects::DisableModes(Network::ControlEnabled());
		if (moveTo)
			Tutorial::ShowMessage("Move To");
	#endif
	return true;
}

bool	DirectTo(const Objects::PWorldEntity&v, const ArrayRef<Grid::LCellIdI>&targets, bool detailedPath, bool leftClick, Objects::Vehicle::mode_t mode)
{
	return DirectTo(v, targets, detailedPath, leftClick, mode, true);
}

bool	DirectTo(const Objects::PWorldEntity&v, const ArrayRef<Grid::LCellIdI>&targets, bool detailedPath, bool leftClick)
{
	return
	#ifdef HAS_WINDOW
		DirectTo(v,targets, detailedPath,leftClick,Objects::activeCursorMode);
	#else
		DirectTo(v,targets, detailedPath,leftClick,Objects::CursorMode::Idle);
	#endif
}


void	SignalChoiceIntersection(const Objects::PWorldEntity&e,const Grid::LCellIdI&c, Objects::Vehicle::mode_t mode, const Objects::PMobileEntity&deployingWhat)
{
	Protocol::ChoiceIntersection	ch;
	ch.id = e->id;
	ch.mode = (UINT16)mode;
	ch.target = c;
	if (deployingWhat)
		ch.deployingWhat.Set0().value = deployingWhat->id;
	Network::Channels::choiceIntersection.SendTo(Network::client,ch);

	#ifdef HAS_WINDOW
		
		UI::HexGrid::Configure(Objects::selectedEntity);
	#endif
}


void	SignalTargetLessAttack(const Objects::PWorldEntity&e)
{
//	Grid::LCellIdI loc = e->location;
	DirectTo(e,Grid::LCellIdI(e->location),false,true,Objects::CursorMode::AttackMode);
}


void	SignalConstructionAbort(const TUniqueID&plantID,const TUniqueID&taskId)
{
	Protocol::ConstructionAbort		ca;
	ca.plantID = plantID;
	ca.taskId = taskId;
	Network::Channels::constructionAbort.SendTo(Network::client,ca);
}

void	SignalConstructionBegin(const TUniqueID&plantID,ConstName baseName, ConstName turretName)
{
	Protocol::ConstructionBegin	cb;
	cb.plantID = plantID;
	if (turretName != ConstName::Empty)
	{
		cb.parameters.SetSize(2);
		cb.parameters.first() = baseName;
		cb.parameters.last() = turretName;
	}
	else
	{
		cb.parameters.SetSize(1);
		cb.parameters.first() = baseName;
	}
	Network::Channels::constructionBegin.SendTo(Network::client,cb);
}

void	SignalRestoreCoreLink(const TUniqueID&mineId)
{
	Network::Channels::restoreCoreLink.SendTo(Network::client,mineId);}

void	SignalAutomation(const Objects::PWorldEntity&e, bool doAutomate)
{
	Network::Channels::automate.SendTo(Network::client,Protocol::EntityStateUpdate(e->id,doAutomate));
}

void	SignalRemoval(const Objects::PWorldEntity&entity)
{
	TUniqueID id = entity->id;
	#ifdef HAS_WINDOW
		Dialog::Confirm("Confirm destruction of "+entity->GetNiceName()+". This operation cannot be undone.",[id]()
		{
			Network::Channels::removeEntity.SendTo(Network::client,id);
	
		},
		[id]()->Dialog::CheckResult
		{
			if (!Network::client.IsConnected())
				return Dialog::CheckResult::Abort;
			Objects::PWorldEntity e = Objects::GetEntity(id);
			if (!e || !e->isOwnedByMe)
				return Dialog::CheckResult::Abort;
			if (RuntimeConfig::phase != RuntimeConfig::Phase::CommandPhase)
				return Dialog::CheckResult::OkayNotRightNow;
			return Dialog::CheckResult::OkayContinue;
		}
		);
	#else

		Network::Channels::removeEntity.SendTo(Network::client,entity->id);
	#endif
}

void	SignalConstructTowerTurret(const TUniqueID&towerID,const Objects::Component::Turret*&newTurret)
{
	Protocol::ConstructionBegin	cb;
	cb.plantID = towerID;
	cb.parameters.SetSize(1);
	cb.parameters.first() = newTurret->constName;
	Network::Channels::constructionBegin.SendTo(Network::client,cb);
}

#ifdef HAS_WINDOW

#endif


bool	HasSignalledRoundEnd()
{
	return Network::newRoundRequested;
}

bool	CanChangeRoundState()
{
	return Network::client.IsConnected() && RuntimeConfig::phase == RuntimeConfig::Phase::CommandPhase && !Network::amSpectator;
}



void	TransmitRoundEndRequest()
{
	Network::client.SendSignal(Protocol::ChannelID::RequestRoundEnd);
	LogEvent(EventSignificance::Common,"Requested new round");
}

void	SignalRoundEnd()
{
	if (Network::ControlEnabled())
	{
		Network::newRoundRequested = true;
		#ifdef HAS_WINDOW
			Objects::UpdateChoices(Network::ControlEnabled());
		#endif
		if (!Network::amTechniteFaction || TechniteLogicLink::LogicIsDone())
			TransmitRoundEndRequest();
	}
}

void	SignalUndoRoundEnd()
{
	if (Network::client.IsConnected() && RuntimeConfig::phase == RuntimeConfig::Phase::CommandPhase && Network::newRoundRequested)
	{
		Network::client.SendSignal(Protocol::ChannelID::RequestContinueRound);
		Network::newRoundRequested = false;
		LogEvent(EventClass::Preset::uncommon,nullptr,"Requested to continue new round");
	}

}

#ifdef HAS_WINDOW

	void VisualLogEvent( EventClass type,  const Objects::WorldEntity*e, const String&_event)
	{
		String event = _event, outEvent = _event;
		if (e)
		{
			event = e->owner->GetName()+","+ e->GetNiceName()+": "+event;
			outEvent = e->owner->GetLogName()+","+ e->GetName()+": "+event;
		}
		eventLock.lock();
		if (type.significance > EventSignificance::Common)
		{
			#ifdef HAS_WINDOW
				if (type.flavor == EventFlavor::ChatMessage)
					events << VisualEvent("",EventSignificance::Debug);
				events << VisualEvent(event,type);
			#else
				std::cout << event << std::endl;
			#endif
		}
		eventLock.unlock();
	}


	bool	IsFullScreen()
	{
		Resolution screen = display.getScreenSize();
		return display.clientSize() == screen;
	}


	void	SetFullScreen(bool isFull)
	{
		static RECT stored;
		static bool hasAnyStored = false;
		static bool storedIsMaximized = false;
		if (isFull == IsFullScreen())
			return;
		Resolution screen = display.getScreenSize();
		if (!isFull)
		{
			//is fullscreen => restore

			display.resizeWindow(screen.width,screen.height,Engine::DisplayConfig::ResizableBorder);
			if (hasAnyStored)
				display.locateWindow(stored);
			if (storedIsMaximized)
				display.Maximize();

			::SetWindowPos(Engine::context.getWindow(),       // handle to window
				HWND_NOTOPMOST,  // placement-order handle
				0,     // horizontal position
				0,      // vertical position
				0,  // width
				0, // height
				SWP_NOMOVE | SWP_NOSIZE // window-positioning options);
				);

		}
		else
		{
			//is not fullscreen => enter fullscreen
			if (!display.IsMaximized())
			{
				stored = display.windowLocation();
				hasAnyStored = true;
				storedIsMaximized = false;
			}
			else
				storedIsMaximized = true;//leave stored as is
			display.locateWindow(0,0,screen.width,screen.height);
			display.resizeWindow(screen.width,screen.height,Engine::DisplayConfig::NoBorder);
			display.locateWindow(0,0,screen.width,screen.height);


			::SetWindowPos(Engine::context.getWindow(),       // handle to window
				HWND_TOPMOST,  // placement-order handle
				0,     // horizontal position
				0,      // vertical position
				0,  // width
				0, // height
				SWP_NOMOVE | SWP_NOSIZE // window-positioning options);
				);

		}
	}

#endif



void	SignalRoundEndOrMaximize()
{
	#ifdef HAS_WINDOW
		if (Engine::input.pressed[Key::Alt])
		{
			SetFullScreen(!IsFullScreen());
			return;
		}

		if (Dialog::ConfirmCurrent(false))
			return;
	#endif
	SignalRoundEnd();

}


void	SignalNewJob(const Objects::PMobileEntity&v, ConstName jobName, ConstName parameterName, const Grid::LCellIdI&overrideLocation)
{
	const Grid::LCellIdI targetLocation = overrideLocation != Grid::LCellIdI::Invalid ? overrideLocation : v->GetLastPointOfOrigin();
	#ifdef HAS_WINDOW
		if (jobName == ConstName::Job_Construct && parameterName != ConstName::Type_Connector)
		{
			Objects::NaturalResourceVector	res;
			if (res.SampleWorld(targetLocation.stackID))
			{
				const auto& proto = Objects::GetPrototypeByName(parameterName);
				ASSERT1__(proto,(int)parameterName);
				String name = proto->GetTypeName();
				TUniqueID id = v->id;
				if (parameterName != ConstName::Type_Mine && res.IsNotEmpty())
				{
					Dialog::Confirm(v->GetNiceName()+" will construct a "+name+" on a location with known natural resources, which could be harvested by a Mine. Do you wish to continue building the "+name+"?",
						[id,jobName,parameterName,targetLocation]()
						{
							Protocol::StandardJob	job;
							job.ownerID = id;
							job.jobName = jobName;
							job.parameterName = parameterName;	
							Network::Channels::signalNewJob.SendTo(Network::client,job);
						},
						[id,jobName,parameterName]()->Dialog::CheckResult
						{
							if (!Objects::GetEntity(id) || !Network::client.IsConnected())
								return Dialog::CheckResult::Abort;
							if (RuntimeConfig::phase != RuntimeConfig::Phase::CommandPhase)
								return Dialog::CheckResult::OkayNotRightNow;
							return Dialog::CheckResult::OkayContinue;
						}
					);
					return;
				}
				if (parameterName == ConstName::Type_Mine && res.IsEmpty())
				{
					Dialog::Confirm(v->GetNiceName()+" will construct a Mine on a location that has no natural resources. Building a mine here seems like a waste of resources. Do you wish to continue building the Mine?",
						[id,jobName,parameterName,targetLocation]()
						{
							Protocol::StandardJob	job;
							job.ownerID = id;
							job.jobName = jobName;
							job.parameterName = parameterName;
							Network::Channels::signalNewJob.SendTo(Network::client,job);
						},
						[id,jobName,parameterName]()->Dialog::CheckResult
						{
							if (!Objects::GetEntity(id) || !Network::client.IsConnected())
								return Dialog::CheckResult::Abort;
							if (RuntimeConfig::phase != RuntimeConfig::Phase::CommandPhase)
								return Dialog::CheckResult::OkayNotRightNow;
							return Dialog::CheckResult::OkayContinue;
						}
					);
					return;
				}
			}

		}
	#endif
	Protocol::StandardJob	job;
	job.ownerID = v->id;
	job.jobName = jobName;
	job.parameterName = parameterName;	
	Network::Channels::signalNewJob.SendTo(Network::client,job);

}

void	SignalNewJob(const Objects::PMobileEntity&v, ConstName jobName, ConstName parameterName)
{
	SignalNewJob(v,jobName,parameterName,Grid::LCellIdI::Invalid);
}


void	SignalTerrainModification(const Objects::PVehicle&v,Objects::Vehicle::mode_t activeCursorMode,const Array<Grid::HCellIdI>&selection)
{
	Protocol::TerrainMod	job;
	job.id = v->id;
	job.mode = (UINT16)activeCursorMode;
	job.cells = selection;
	Network::Channels::signalTerrainMod.SendTo(Network::client,job);
}

void	SignalSleep(const Objects::PWorldEntity&e, bool doSleep)
{
	Protocol::EntityStateUpdate update;
	update.entityID = e->id;
	update.doSet = doSleep;
	Network::Channels::sleep.SendTo(Network::client,update);
}

void	InnerUndoTask(const Objects::PWorldEntity&e, bool force);

void	SignalDropLastTask(const Objects::PWorldEntity&e)
{
	InnerUndoTask(e,false);
}

void	SignalDropAllTasks(const Objects::PWorldEntity&e)
{
	#ifdef HAS_WINDOW
	if (e->IsMobile())
	{
		Objects::PMobileEntity v = To<Objects::MobileEntity>::Cast(e);
		if (v->constructionRoundCounter > 0 
		&& v->myTasks.IsNotEmpty() 
		&& v->myTasks.Peek().IsJob() 
		&& v->myTasks.Peek().job->resourceCostLeft > 0)
		{
			TUniqueID id = e->id;
			Dialog::Confirm("Are you sure you wish to clear all tasks? The resources invested in the leading production job will be lost.",[id]()
			{
				Network::Channels::dropAllTasks.SendTo(Network::client,id);
			},
			[id]()->Dialog::CheckResult
			{
				if (!Network::client.IsConnected() || !Objects::GetEntity(id))
					return Dialog::CheckResult::Abort;
				if (RuntimeConfig::phase != RuntimeConfig::Phase::CommandPhase || Network::newRoundRequested)
					return Dialog::CheckResult::OkayNotRightNow;
				return Dialog::CheckResult::OkayContinue;
			});
			return;
		}
		else
		{
			
		}
		
	}
	#endif
	Network::Channels::dropAllTasks.SendTo(Network::client,e->id);
}
void SignalEnableStructure(Objects::Structure&s, bool enabled)
{
	Protocol::EnableStructure es;
	es.id = s.id;
	es.enable = enabled;
	Network::Channels::enableStructure.SendTo(Network::client,es);
}








//float							eyeHeight = 1.7f;
		



								







/**
Retrieves the low resolution cell id of the cell under the current cursor location
*/
bool							GetLowCellUnderCursor(Grid::LCellIdI&result, bool includeWater, bool includeEntities);







#ifdef HAS_WINDOW
	bool saveScreenshot = false;

	namespace TimeLapse
	{
		index_t runningNumber = 0;
		bool enabled = false;
		float		frameDelaySeconds=0.2f;
		PathString	basePath;
		PathString	customBasePath;
		bool		maxCompressionPNG=false;
	}


	class StorageThread : public ThreadObject
	{
	public:
		//Image		image;
		//Semaphore	semaphore;
		//volatile bool	active;
		Timer::Time	lastSubmission = timer.Now();
		ProductionPipe<Image>	workPipe;

		StorageThread()
		{}

		void ThreadMain() override
		{
			Array<std::shared_ptr<Image> >	images;
			while (!application_shutting_down)
			{
				workPipe.WaitForContent();
				if (application_shutting_down)
					return;
				workPipe.SignalRead();

				const count_t numImages = workPipe.Count();
				if (numImages > images.Count())
					images.SetSize(numImages);
				for (index_t i = 0; i < numImages; i++)
					images[i].swap(workPipe[i]);
				workPipe.ExitRead();
				if (application_shutting_down)
					return;
	//
	//			image.truncateToOpaque();
	//
	//			for (UINT32 x = 0; x < image.getWidth(); x++)
	//				for (UINT32 y = 0; y < image.getHeight(); y++)
	//				{
	//					BYTE*pixel = image.getPixel(x,y);
	//					if (pixel[3] > 0 && pixel[3] < 255)
	//					{
	//						//#ifdef WHITE_STYLE
	//						//	TVec3<> intermediate;
	//						//	Vec::mult(Vec::ref3(pixel),1.f / 255.f,intermediate);
	//						//	float a = float(pixel[3])/255.f;
	//						//	Vec::addVal(intermediate,a - 1.f);
	//						//	Vec::div(intermediate,a);
	//						//	Vec::clamp(intermediate,0,1);
	//						//	Vec::mult(intermediate,255.f,Vec::ref3(pixel));
	//
	//						//#else
	//							float scale = 1.f / (float(pixel[3])/255.f);
	//							Vec::mult(Vec::ref3(pixel),scale);
	////						#endif
	//					}
	//				}
				for (index_t i = 0; i < numImages; i++)
				{
					Image&image = *images[i];
					image.setChannels(3);

					if (!TimeLapse::enabled)
					{
						FileSystem::CreateFolder("screenshots");
						index_t counter = 0;
						PathString filename = PathString("screenshots/screenshot"+String(counter)+".png");
						while (FileSystem::IsFile(filename))
						{
							counter++;
							filename = PathString("screenshots/screenshot"+String(counter)+".png");
						}
						png.SaveToFile(image,filename);
						LogEvent(EventClass::Preset::uncommon, nullptr, "Screenshot written to "+String(filename));
					}
					else
					{
						String runningNumber = String(TimeLapse::runningNumber++);
						while (runningNumber.length() < 4)
							runningNumber = '0'+runningNumber;
						String core = "capture"+runningNumber+".png";
						PathString filename = TimeLapse::basePath + FOLDER_SLASH + core;

						if (TimeLapse::maxCompressionPNG)
							png.SaveToFile(image,filename);
						else
							png.SaveToFileQ(image,filename);
						count_t remaining = numImages - i - 1 + workPipe.Count();
						LogEvent(EventClass::Preset::uncommon, nullptr, "Time lapse frame written to "+String(core)+". "+String(remaining)+" remaining.");
					}
				}
			}
		}

		bool	CanRun() const {return timer.GetSecondsSince(lastSubmission) > TimeLapse::frameDelaySeconds;}
		void	Run(const std::shared_ptr<Image>&image)
		{
			if (!CanRun())
				return;

	//		active = true;
		//	semaphore.release();
			workPipe << image;
			lastSubmission = timer.Now();
		}
		void	Terminate()
		{
			ASSERT__(application_shutting_down);
			workPipe.OverrideSignalNow();
		}

	}		storageThread;




#endif













void FillMarker(Protocol::Marker&marker, const Objects::PWorldEntity&e, Protocol::MarkerReferenceEntity&outE)
{
	marker.location = e->location;
	outE.id = e->id;
	outE.owner = e->owner->id;
	Objects::PVehicle v = To<Objects::Vehicle>::DynamicCast(e);
	if (!v)
	{
		outE.type0 = e->GetCTypeName();
		outE.type1 = ConstName::Empty;
	}
	else
	{
		outE.type0 = v->base->constName;
		outE.type1 = v->turret->constName;
	}
}


void							SignalMarkerDeletion(TUniqueID markerID)	//must be copy to prevent weird shit
{
	Network::Channels::destroyMarker.SendTo(Network::client, markerID);
	Objects::Marker::Destroy(markerID,Network::ControlEnabled());
}

void	SignalMarkerCreation(Protocol::Marker&marker)
{
	marker.createdInRound = RuntimeConfig::roundNumber;
	Network::Channels::createMarker.SendTo(Network::client, marker);
	Network::Receiver::CreateMarker(marker);	//one-way
}



void	SignalMarkerTouch(Objects::Marker&marker)
{
	marker.createdInRound = RuntimeConfig::roundNumber;
	marker.UpdateAge(RuntimeConfig::roundNumber);
	Protocol::TouchedMarker m;
	m.id = marker.id;
	m.newRoundNumber = RuntimeConfig::roundNumber;
	Network::Channels::touchMarker.SendTo(Network::client,m);
}




void	OnVanish(const Objects::PWorldEntity&e, Objects::RemovalType t)
{
	if (t == Objects::RemovalType::UncontrolledExplosion && e->isOwnedByMe)
	{
		Protocol::Marker marker;
		static_assert(Objects::Marker::Type::EntityDeath == 0, "Please update marker logic");
		marker.id.value.Generate();
		FillMarker(marker, e, marker.data.Set0());
		SignalMarkerCreation(marker);
	}
	else
		if (t == Objects::RemovalType::Vanished && !e->isOwnedByMe)
		{
			//this event can be high-frequency, happen several times per round for the same entity,
			//and happen, in fact, so rapidly, the creation event hasn't been confirmed yet


			if (!Objects::Marker::FindByReference(e->id))//apparently, this can happen
			{
				
				Protocol::Marker marker;
				marker.id.value.Generate();
				static_assert(Objects::Marker::Type::HostileContact == 1, "Please update marker logic");
				FillMarker(marker, e, marker.data.Set1());
				SignalMarkerCreation(marker);

				LogEvent(EventSignificance::Debug,e.get(),"Created hostile contact marker for "+e->GetName()+" since original disappeared");
			}
		}
}

void	OnAppear(const Objects::PWorldEntity&e)
{
	//fix: doesn't matter if e->isOwnedByMe is true or not. Must ALWAYS remove existing markers if we see the original again.
	//reason for this problem: entity ownership change causes vanishing, and thus ghost creation, which was not removed here, because e was suddenly mine.
	Objects::Marker*m = Objects::Marker::FindByReference(e->id);
	if (m)
	{
		LogEvent(EventSignificance::Debug,e.get(),"Deleting marker "+m->GetName()+" since original reappeared");
		SignalMarkerDeletion(m->id);
	}


	if (e->isOwnedByMe)
	{
		if (To<Objects::Structure>::DynamicCast(e))
		{
			for (index_t i = 0; i < Objects::Marker::all.Count(); i++)
			{
				Objects::Marker&m = Objects::Marker::all[i];
				if (m.type == Objects::Marker::Type::EntityDeath && m.location == e->location)
				{
					LogEvent(EventSignificance::Debug,e.get(),"Deleting marker "+m.GetName()+" since new structure constructed in its place");
					SignalMarkerDeletion(m.id);
					i--;
				}
			}
		}
	}
}






#ifdef HAS_WINDOW



	void	SignalUserMessage(const String&message)
	{
		Dialog::ShowMessage(message);

	}


	void OnSelectionChange(bool stateChangeOnly)
	{
		Objects::Renderer::HighlightSmokeTrail(InvalidIndex);

		Tutorial::lastDelayingEvent = timer.Now();
		//bool strip = true;
		if (Objects::selectedEntity)
		{
			Objects::PTurretBase base = Objects::selectedEntity->GetTurretBase();
			if (base)
			{
				if (base->turret->CanAttack())
					Tutorial::ShowMessage("Show Ranges");
				Tutorial::ShowMessage(base->turret->typeName);
			}
			else
				Tutorial::ShowMessage(Objects::selectedEntity->GetTypeName());
			if (Objects::selectedEntity->health <= 0)
			{
				if (Objects::selectedEntity->isOwnedByMe)
					Tutorial::ShowMessage("Friendly Ghosts");
				else
					Tutorial::ShowMessage("Hostile Ghosts");
			}

			if (!stateChangeOnly)
			{
				if (Objects::selectedMobileEntity && Objects::selectedMobileEntity->IsSatellite())
				{
	//				Grid::visibleOrbitGrid.Color(Objects::selectedMobileEntity->location, Objects::selectedMobileEntity->turret->fuelSupplyRadius);
//					strip = false;
					Camera::TargetSkyOrbit();
				}
				else
					Camera::TargetGroundOrbit();

			}
		}
		//if (strip)
			//Grid::visibleOrbitGrid.StripColor();

		#ifndef ONE_COMPOUND_PER_FACTION
		if (Objects::selectedVehicle)
		{
			//if (Objects::selectedVehicle->tasks.IsNotEmpty() && Objects::selectedVehicle->tasks.Peek().job && Objects::selectedVehicle->tasks.Peek().job->mineralCostLeft)
			Objects::ResourceVector change;
			const Objects::ResourceVector&current = Objects::selectedVehicle->currentStorage;
			const Objects::ResourceVector max = Objects::selectedVehicle->GetMaxStorageCapacity();
			int sum = 0;
			foreach (Objects::selectedVehicle->myTasks,t)
				if (t->job)
					sum += t->job->mineralCostLeft;
			if (sum)
			{
				change.level[Objects::Resource::Minerals] -= sum;//-Objects::selectedVehicle->tasks.Peek().job->mineralCostLeft;
				UI::Update(
						#ifndef NO_HIBERNATION
							Objects::selectedVehicle->GetUplinkTraffic(),	
						#endif
							current,max,change);
			}
			else
				UI::Update(
						#ifndef NO_HIBERNATION
							Objects::selectedVehicle->GetUplinkTraffic(),
						#endif
						current,max);
		}
		else if (Objects::selectedEntity && Objects::selectedEntity->IsMobile())
		{
			FATAL__("not implemented yet");
		}
		else if (Objects::selectedEntity && Objects::selectedEntity->IsStructure())
		{
			if (Objects::selectedEntity->health > 0)	//ghosts have no health and no network
			{
				const Objects::Compound&n = Objects::compounds[To<Objects::Structure>::Cast(Objects::selectedEntity)->compoundID];
				Objects::PWorkingStructure ws = To<Objects::WorkingStructure>::DynamicCast(Objects::selectedEntity);
				if (ws)
				{
					Objects::ResourceVector change, storage, max;
					ws->GetResourceVector(change, storage, max);
					UI::Update(
						#ifndef NO_HIBERNATION
							Objects::selectedEntity->GetUplinkTraffic(), 
						#endif
						n.total.storage, n.total.storageMax, n.total.supply - n.total.demand, storage/*,change, ws->IsEnabled()*/);
				}
				else
					UI::Update(
						#ifndef NO_HIBERNATION
							Objects::selectedEntity->GetUplinkTraffic(),
						#endif
						n.total.storage, n.total.storageMax, n.total.supply - n.total.demand);
			}
			else
				UI::HideExtraGauges();


		}
		else
			UI::HideExtraGauges();

		#else
			
			Objects::PFaction f;
			if (Objects::selectedEntity)
				f = Objects::selectedEntity->owner;
			else
				f = Objects::Faction::mine;
			if (f)
			{
				if (f->compound.containedStructures.IsEmpty())
				{
					bool any = false;
					for (const auto& s : Objects::structures)
						if (s->IsWorkingStructure() && s->owner == f)
							any = true;
					if (any)	//prevent stack overflow
						Objects::RefreshCompoundCoverage(0);
				}
			}
			if (f)
			{
				const Objects::Compound&compound = f->compound;
				const Objects::ResourceVector&current = compound.total.storage;
				const Objects::ResourceVector&max = compound.total.storageMax;
				Objects::ResourceVector change = compound.total.supply - compound.total.demand;
				Objects::ForeachMobileWorldEntity([f,&change](const Objects::PMobileEntity&v)
				{
					if (v->owner != f)
						return;
					if (v->myTasks.IsEmpty() || !v->myTasks.Peek().job)
						return;
					int opSpeed = v->GetOperationSpeed(v->myTasks.Peek().job->GetOperationAffinity());
					if (!opSpeed)
						return;
					change.level[Objects::Resource::Minerals] -= std::min(opSpeed, v->myTasks.Peek().job->resourceCostLeft);

				});

				for (const auto&sat : Objects::satellites)
				{
					if (sat->owner == f && sat->RequiresFuel())
					{
						change.level[Objects::Resource::Fuel] -= sat->GetFuelConsumptionPerRound() + sat->GetExtraFuelDemand();
					}
				}

				Objects::PWorkingStructure ws = To<Objects::WorkingStructure>::DynamicCast(Objects::selectedEntity);
				if (ws)
				{
					Objects::ResourceVector _change, storage, _max;
					ws->GetResourceVector(_change, storage, _max);
					UI::Update(
						#ifndef NO_HIBERNATION
							Objects::selectedEntity->GetUplinkTraffic(), 
						#endif
						current, max, change, storage/*,change, ws->IsEnabled()*/);
				}
				else
					UI::Update(
						#ifndef NO_HIBERNATION
							Objects::selectedEntity ? Objects::selectedEntity->GetUplinkTraffic() : 0, 
						#endif
						current, max, change);


			}
			else
				UI::HideExtraGauges();


		#endif



		UI::HexGrid::Configure(Objects::selectedEntity);

	}


	void RenderShadows(float shadowRadius, DX11::FBO&shadowMap, Engine::OrthographicAspect<>&shadowCamera, DX11::ObjectBuffer<TMatrix4<> >&shadowMatrix
	#ifdef CASCADING_SHADOW
		,index_t cascade
	#endif

	)
	{
		float3 center = Camera::scene.GetAbsoluteLocation() - Camera::scene.GetViewingDirection() * shadowRadius;
		Vec::setLen(center,Grid::ToRadius(0));

		const float3&up = Grid::GetSunDirection();
		Mat::makeAxisSystem(center,-up,2,shadowCamera.viewInvert);
		Mat::invertSystem(shadowCamera.viewInvert,shadowCamera.view);
		shadowCamera.UpdateProjection2(-shadowRadius,-shadowRadius,shadowRadius,shadowRadius,100.f,-100.f);
		shadowCamera.depthTest = Engine::NormalDepthTest;


		//Objects::shadowCamera.UpdateInvert();


		//float3 t;
		//float4 c;
		//shadowCamera.Project(center,t);
		//Mat::mult(Objects::shadowCamera.projection,float4(t,1),c);

		//display.SetDepthTest(false,false);	//disables depth output
		Objects::Renderer::RenderShadows(
			#ifdef CASCADING_SHADOW
				cascade
			#endif
		);
		for (BYTE k = 0; k < BaseGrid::NumFamilies; k++)
		{
			Grid::TFamily::TData&dat = Grid::high.family[k].normal;
			if (dat.terrain.shadowInstruction.shader.IsSet())
				display.Render(dat.terrain.shadowInstruction);
			if (dat.overlay.shadowInstruction.shader.IsSet())
				display.Render(dat.overlay.shadowInstruction);
			if (dat.technites.shadowInstruction.shader.IsSet())
				display.Render(dat.technites.shadowInstruction);
		}
	}

	#ifdef CASCADING_SHADOW
	void RenderShadows(float shadowRadius, index_t cascade)
	{
		RenderShadows(shadowRadius,Objects::shadowMap[cascade],Objects::shadowCamera[cascade],Objects::shadowMatrix[cascade],cascade);

	}
	#endif
#endif





static Timer::Time	capTime = timer.Now();

bool loop()
{



	#ifndef HAS_WINDOW
		Sleep(50);
		AI::Process(0.5f);
	#else
		DebugCheckCamera();
		Explosions::Update(Engine::timing.delta);


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

		#if 1
		{
			if (RuntimeConfig::FromFile::capFPS > 0)
			{

				Timer::Time passed = timer.now() - capTime;
				static const Timer::Time minTime = timer.toTicks(1.f / float(RuntimeConfig::FromFile:: capFPS));
				while (passed < minTime)
				{
					Sleep(0);
					passed = timer.now() - capTime;
				}
				capTime = timer.Now();
			}
		}
		#endif
		if (RuntimeConfig::aiMode && RuntimeConfig::phase == RuntimeConfig::Phase::CommandPhase)
		{
			if (Engine::context.isMinimized())
				AI::Process(0.5f);
			else
				AI::Process(0.05f);
		}
	#endif


	Network::Update();
	TechniteLogicLink::ProcessEvents();


	Animation::Update();




	#ifdef HAS_WINDOW
		Camera::ProgressRotation();



		//Resolution windowSize = display.clientSize();
	
		//bool rebuildCamera = false;

		if (!Dialog::IsModal() && !TimeLapse::enabled)
		{
			{
				if (Engine::mouse.buttons.right_down || Engine::mouse.buttons.middle_down)
				{
					Camera::AbortTargetedTranslation();
					if (!rightMouseDragged)
					{
						float2 delta = Engine::mouse.location.absolute - rightMouseDragFrom;
						if (delta.Length() > 20.f)
						{
							rightMouseDragged = true;
						}
					}
					if (rightMouseDragged)
					{
						float2 delta = (Engine::mouse.location.windowRelative - Engine::mouse.previous_location.windowRelative);
						rightMouseDragged = true;
						delta.x *= Engine::mouse.speed.x * -100.f;
						delta.y *= Engine::mouse.speed.y * -100.f;
						delta *= 0.5f;
						Camera::Translate(delta * -Camera::scene.retraction.z / Camera::GetCurrentOrbit());
					}
				}
			}

			float2 change;
			if (Engine::input.pressed[Key::A] || Engine::input.pressed[Key::Q])
			{
				change.x --;
			}
			if (Engine::input.pressed[Key::D] || Engine::input.pressed[Key::E])
			{
				change.x++;
			}
			if (Engine::input.pressed[Key::W])
			{
				change.y--;
			}
			if (Engine::input.pressed[Key::S])
			{
				change.y++;
			}
			if (!Vec::zero(change))
			{
				//Vec::setLen(change, Engine::timing.delta*100.f);
				Vec::mult(change,Engine::timing.delta*100.f);
				Camera::Rotate(change);
			}
			if (Engine::input.pressed[Key::Up])
			{
				Camera::Event::Zoom(Engine::timing.delta);
			}
			if (Engine::input.pressed[Key::Down])
			{
				Camera::Event::Zoom(-Engine::timing.delta);
			}
		}


		if (RuntimeConfig::FromFile::alwaysDay)
			Grid::UpdateSunDirection(sunDirection);
		else
			Grid::PerFrameUpdateSunDirection();


		Objects::Renderer::AdvanceTurretAnimations();

		{

	//		static const float shadowRadius = 30.f;
		
			//center = float3();
			//center.Normalized0();
			#ifdef CASCADING_SHADOW
				for (index_t i = 0; i < Objects::NumCascades; i++)
				{
					RenderShadows(-20.f + 30.f * (1 << i) ,i);
				}
			#else
				RenderShadows(30.f, Objects::shadowMap, Objects::shadowCamera, Objects::shadowMatrix);
			#endif

			Objects::Renderer::EndShadowRendering();
		}

		AudioInterface::UpdateListener(Camera::scene);
		display.pick(Camera::scene);
		static Engine::Frustum<> frustum;
		Camera::scene.GetFrustum(frustum);


		//rendere scene here

		display.SetRasterizer(true, true);


		{
			//Atmosphere::Render(Grid::ToRadius(-Grid::CellStack::HalfRange),Grid::ToRadius(Grid::CellStack::HalfRange-1), Camera::scene.GetAbsoluteLocation());
			Atmosphere::Render(Camera::scene);
			World::RecompileIfNecessary();
			if (Grid::SeeThrough::IsEnabled() /*&& !Objects::selectedEntity*/)
				Grid::SeeThrough::Update(Grid::SeeThrough::IsEnabled(), Grid::SeeThrough::LocateCamera());

	//		const Grid::Resource&resource = Grid::Resource::all[Objects::activeResource];

			#if 0
				for (int i = 0; i < 20; i++)
					for (BYTE k = 0; k < BaseGrid::NumFamilies; k++)
					{
						Grid::TFamily::TData&dat = Grid::high.family[k].normal;
						dat.outdated = false;
						familiesRebuilt++;
						if (Grid::SeeThrough::IsEnabled())
						{
							Grid::high.BakeBlockInstruction(Grid::cellStack, k, Grid::SeeThrough::ClipHeight);
						}
						else
						{
							Grid::high.BakeBlockInstruction(Grid::cellStack, k, Grid::Clip::NoClip);
						}
					}


			#else
				World::Render(frustum);


			#endif
		//display.SetDepthTest(false,false);



			if (Objects::selectedEntity)
			{
				display.SetDepthTest(true,true);
				display.SetBlendMode(Engine::BlendMode::None);
				Grid::UpdatePlanetRenderingParameters(Grid::ToRadiusf(float(Objects::selectedEntity->location.height)+0.1f));
				Grid::low.RenderCell(Objects::selectedEntity->location.stackID);

				//Objects::PVehicle v = To<Objects::Vehicle>::DynamicCast(Objects::selectedEntity);
				if (Objects::selectedMobileEntity)
				{
					if (Objects::selectedMobileEntity->myTasks.IsNotEmpty())
					{
						foreach (Objects::selectedMobileEntity->myTasks.Peek().foci,f)
						{
							Grid::UpdatePlanetRenderingParameters(Grid::ToRadiusf(float(f->target.height)+0.1f));
							Grid::low.RenderCell(f->target.stackID);
						}
					}
				}
				display.SetDepthTest(true,true);
			}


			Objects::Renderer::RefreshFactionColors(!saveScreenshot);
			Objects::Renderer::RenderScene();



			if (Animation::projectileActive)
			{
				display.SetDepthTest(false,false);
				display.SetRasterizer(true,false);
				display.SetBlendMode(Engine::BlendMode::FlareBlend);
				display.Render(Animation::trajectoryVisualization);
				display.SetBlendMode(Engine::BlendMode::AlphaBlend);
			}


			display.SetDepthTest(true, true);
			display.SetBlendMode(Engine::BlendMode::AlphaBlend);
			display.SetRasterizer(true,true);



			World::RenderWater(frustum);


			if (Camera::SkyGridIsVisible())
			{
				//render sky grid
				//Grid::UpdatePlanetRenderingParameters();
				Grid::visibleOrbitGrid.Render(frustum);
				//Grid::low.RenderGrid(Grid::ToRadius(Objects::Satellite::BaseResidenceHeight));

			}

			Objects::Renderer::RenderTranslucent();

			if (Objects::resourceView)
				World::RenderResources(frustum);

			Objects::Renderer::RenderTranslucentBeams();

			World::RenderOverlay(frustum);

			World::RenderBuildBlocks();

			Explosions::Render(Camera::scene);






			Objects::Renderer::RenderProgressBars();

			if (Objects::showArrow)
			{
	//			display.SetDepthTest(false, false);
				bool includeWater = false;
				if (Objects::activeCursorMode == Objects::CursorMode::AttackMode)
					includeWater = true;
				else
					if (Objects::selectedVehicle /*&& Objects::selectedVehicle->base->moveOnWater*/)
						includeWater = true;
				Objects::hasArrowCellTarget = GetLowCellUnderCursor(Objects::arrowCellTarget,includeWater, Objects::activeCursorMode == Objects::CursorMode::AttackMode);
				if (Objects::hasArrowCellTarget)
				{
				
					if (Objects::activeCursorMode == Objects::CursorMode::AttackMode
					 && 
						(
							(Objects::selectedMobileEntity && !Objects::selectedMobileEntity->CanHitFrom(Objects::selectedMobileEntity->GetLastPointOfOrigin(),Objects::arrowCellTarget))
							||
							(Objects::selectedEntity && !Objects::selectedEntity->CanHit(Objects::arrowCellTarget))
						)
					 //!Objects::Turret::CanHit(Objects::selectedVehicle->GetFlatAttackRange(),Objects::selectedVehicle->GetLastPointOfOrigin(),Objects::arrowCellTarget)
					 )
						Objects::hasArrowCellTarget = false;
				}

			}
		}
			



		display.ClearDepth();

		{
		
			float3 b, d;
			Camera::scene.ScreenToVector(Engine::mouse.location.windowRelative * 2.f - 1.f, b, d);
			index_t choice = InvalidIndex;
			if (!Vec::zero(d))
			{
				Vec::normalize(d);

				float distance = std::numeric_limits<float>::max();
				choice = Objects::DetectChoiceIntersection(b, d, distance);
			}




			foreach(Objects::movementChoices, choice)
			{
				Grid::UpdatePlanetRenderingParameters(Grid::ToRadius(choice->height));
				//if (choice->height >= Grid::CellStack::TopHeight)
					Grid::low.RenderTransparentCell(choice->stackID);
				//else
			}
			if (choice != InvalidIndex)
			{
				Grid::UpdatePlanetRenderingParameters(Grid::ToRadiusf(float(Objects::movementChoices[choice].height) + 0.2f));
				Grid::low.RenderCell(Objects::movementChoices[choice].stackID);
			}
		}



		LineRenderer::Flush();

		//#ifdef _DEBUG
		//if (!saveScreenshot)
		//{
		//	LineRenderer::SetColor(float4(1,0,0,1));
		//	display.SetDepthTest(false,false);
		//	LineRenderer::Rasterize(Debug::lastClickPoint - float3(1,0,0),Debug::lastClickPoint + float3(1,0,0));
		//	LineRenderer::Rasterize(Debug::lastClickPoint - float3(0,1,0),Debug::lastClickPoint + float3(0,1,0));
		//	LineRenderer::Rasterize(Debug::lastClickPoint - float3(0,0,1),Debug::lastClickPoint + float3(0,0,1));
		//	LineRenderer::Flush();
		//}
		//#endif

		display.pick(Camera::hud);
			display.SetRasterizer(true,false);
			display.SetDepthTest(false,false);
			display.SetBlendMode(Engine::BlendMode::AlphaBlend);

			textout.color(1,1,1);


			if (!Network::client.IsConnected() /*&& false*/)
			{
				static Timer::Time	lastCheck = timer.Now();

				Timer::Time now = timer.Now();
				if (!Dialog::serverChoice || timer.ToSeconds(now - lastCheck) > 1.f)
				{
					lastCheck = now;
					UpdateServerChoice();
				}
			}


			{
				static const String line = "(C) Stefan Elsen; v" PRODUCT_VERSION "; Compiled "+String(__TIMESTAMP__);
				//textout.locate(0.f,1.f - textout.getScaledHeight());
				textout.pushState();
				textout.scale(0.8f,0.8f);
				textout.locate(1.f - textout.scaledWidth(line),textout.getScaledHeight());
				textout << line;
				textout.popState();

			}

			if (saveScreenshot)
			{
				if (!TimeLapse::enabled 
					|| 
					(
						//RuntimeConfig::phase != RuntimeConfig::Phase::AnimationPhase 
						Animation::AllDone() 
						&& Explosions::NoneActive()
					)
					)
				{
					saveScreenshot = false;
				}
				//display.SetBackbufferClearColor(0,0,0,0);

				//Image image;
				if (storageThread.CanRun())
				{
					if (!TimeLapse::enabled)
						LogEvent(EventClass::Preset::uncommon,"Writing screenshot to disk...");
					//else
					//	LogEvent(EventClass::Preset::uncommon,"screenshot "+String(RuntimeConfig::phase.ToString())+", "+String(Animation::AllDone())+", "+String(Explosions::NoneActive()));
					std::shared_ptr<Image> capture(new Image());
					display.Capture(*capture);

					storageThread.Run(capture);
				}

			}

	
			{
	//			CommandPanel::Print();;

				if (Objects::selectedEntity)
					StatePanel::Render(*Objects::selectedEntity);

				//if (Objects::Network::Transfer::isActive)
				//	TransferPanel::Render(Objects::Network::Transfer::fromVehicle, Objects::Network::Transfer::toNetwork);
				//else
				//	TransferPanel::Invalidate();
			}

			{
				textout.pushState();
				textout.scale(0.9f, 0.9f);
				eventLock.lock();

				for (index_t i = 0; i < events.Count(); i++)
				{
					events[i].age += Engine::timing.delta;
					if (events[i].age > VisualEvent::MaxAge)
					{
						events.Erase(i);
						i--;
					}
				}
				count_t rows = std::min(events.Count(), (count_t)floor(0.7f / textout.getScaledHeight()));


				textout.locate(0.0f, 0.3f + float(rows) * textout.getScaledHeight());
				for (index_t i = events.Count() - rows; i < events.Count(); i++)
					//if (events[i].stackID < 10)
					{
						const auto&ev = events[i];
						//if (ev.age < 30.f)
						{
							textout.scale(ev.scale,ev.scale);
							textout.color(ev.color, (VisualEvent::MaxAge - ev.age)/VisualEvent::MaxAge);
							textout << ev.message;
							textout.scale(1.f/ev.scale,1.f/ev.scale);
							textout <<  nl;
						}
					}
				textout.popState();
				eventLock.unlock();
			}

			textout.color(1,1,1);
			textout.locate(0,1);
			textout << (Network::amOnline? "online" : "offline")<<", "<< (Network::haveFaction ? "logged in" : "not logged in")<< nl;
			textout << "Hello "<<Network::chosenNickname<<nl;
			if (Network::amOnline)
			{
				textout << "Server ID: " << Network::serverIDString<<nl;
				textout << "World ID: " << Network::worldIDString<<nl;
				textout << "Game Goal: "<<RuntimeConfig::gameGoal.ToString() << nl;
				#ifdef _DEBUG
					textout << "Access ID: " << Network::activeTokenString<<nl;
				#endif
				textout << "Phase: "<<RuntimeConfig::phase.ToString()<<nl;
				textout << "Round Number: "<<RuntimeConfig::roundNumber<<"/"<<RuntimeConfig::techniteSubRoundNumber <<nl;
				if (!Network::gameProgress.gameOver)
				{
					if (Network::gameProgress.status.IsNotEmpty())
						textout << "Progress: "<<Network::gameProgress.status<<nl;
					if (Network::gameProgress.warning.IsNotEmpty())
					{
						textout.color(1,0,0);
						textout << "Warning: "<<Network::gameProgress.warning<<nl;
						textout.color(1,1,1);
					}
				}
				else
					if (Network::gameProgress.gameOver)
					{
						if (Network::gameProgress.gameWon)
						{
							textout.color(0.4,0.5,1);
							textout << "YOU WIN"<<nl;
						}
						else
						{
							textout.color(1,0.5,0.4);
							textout << "GAME OVER"<<nl;
						}
						textout.color(1,1,1);
					}
			}
			#ifdef _DEBUG
				if (Network::haveID)
					textout << "My ID: "<<Network::myIDString<<nl;
				textout << -Camera::scene.retraction.z/WorldScale << nl;
			#endif
			if (RuntimeConfig::phase == RuntimeConfig::Phase::CommandPhase && Network::roundTimer > 0)
			{
				textout.pushState();
				float remaining = std::max(Network::roundTimer - timer.GetSecondsSince(Network::phaseStarted),0.f);
				textout << "Remaining seconds: "<<nl;
				textout.scale(2.f,2.f);
				if (remaining <= 10.f && !Network::newRoundRequested)
				{
					textout.scale(1.2f,1.2f);
					bool blinkOn = ((timer.Now()/(timer.GetTicksPerSecond()/5))%2) != 0;
					textout.color(1,0.2,0.2,blinkOn ? 0.5f : 1.f);
				}
				textout<<Round(remaining)<<nl;
				textout.popState();
			}


			textout.locate(0.0f,0.05f);
			textout.color(1,1,1);
			textout << "FPS: "<<1.f / Engine::timing.delta << nl;
			#ifdef _DEBUG
				textout << "last remote message: "<<timer.GetSecondsSince(Network::client.GetLastPackageTime());
			#endif

	

			DebugCheckVector(scene.location,-1000,1000);
		
		
			UI::Render();
			display.pick(Camera::hud);
			Dialog::Render();

	#endif
		//textout << "direction: "<<scene.GetViewingDirection()<< nl;
		//textout << "position: " << scene.location << nl;
	

	return !application_shutting_down;// && !Engine::input.pressed[Key::Escape];
}
//void ShowFPS()
//{
//	ShowMessage(1.f/Engine::timing.delta);
//}

void MyFatal(const FatalDescriptor&descriptor)
{
	String message = descriptor.ToString();
	LogFile file(RuntimeConfig::ConfigFile("fatal.log"),false);
	
	file.Log(message+"\n",true);
	file.Close();
	application_shutting_down = true;	//prevent certain operations in Close()
	LogEvent(EventClass( EventSignificance::SystemFatal, EventFlavor::SystemError ), nullptr, message);
	ErrMessage(message);
	//NetworkConnection::Close();

}



#ifdef HAS_WINDOW

	void	onWindowResize(const Resolution&, UINT32)
	{
		Camera::scene.UpdateProjection2(display.pixelAspect(),0.1f,1000.f);
	//	textout.setScale(1.f/display.clientWidth(),1.f/display.clientHeight());

		textout.setScale(1.f / display.clientWidth(), 1.f / display.clientHeight());

		UI::OnWindowResize();
		GUIElements::OnWindowResize();
	}


	bool GetLowCellUnderCursor(Grid::LCellIdI&result, bool includeWater, bool includeEntities)
	{
		float3 b,d;
		Camera::scene.ScreenToVector(Engine::mouse.location.windowRelative * 2.f - 1.f,b,d);
		if (!Vec::zero(d))
		{
			Vec::normalize(d);

			float distance = std::numeric_limits<float>::max();


			bool rs = false;

			if (includeEntities)
			{
				Objects::ForeachWorldEntity([&result,&rs,b,d,&distance](const Objects::PWorldEntity&v)
				{
					if (v->TestIntersection(b,d,distance))
					{
						result = v->location;
						rs = true;
					}
				});
			}

			if (Camera::SkyGridIsVisible())
			{
				index_t stackID = Grid::low.TestGridIntersection(b, d, Grid::visibleOrbitGrid.orbit, distance);
				if (stackID != InvalidIndex && Vec::dot(Grid::low.cells[stackID].centerVertex.xyz,d) < 0.f)
				{
					result = Grid::LCellIdI((UINT32)stackID,Grid::FromRadius(Grid::visibleOrbitGrid.orbit));
					return true;
				}
			}

			{
				TVec3<> normal;
				if (Grid::high.TestHullIntersection(b, d, distance, normal, includeWater))
				{
					#ifdef _DEBUG
						Debug::lastClickPoint = b + d * distance;
					#endif
					//if (test)
						//return false;
					float3 p = b + d * distance;
					float height = p.Length();
					distance = std::numeric_limits<float>::max();
					index_t stackID = Grid::low.TestGridIntersection(b, d, height, distance);
					if (stackID != InvalidIndex)
					{
						result = Grid::LCellIdI((UINT32)stackID,Grid::FromRadius(height));

						#ifdef _DEBUG
							Debug::lastClickCell = result;
						#endif

						return true;
					}
				}
			}
			return rs;
		}
		return false;
	}


	/**
	Primary entry point used when clicking anywhere on the window
	*/
	void Select()
	{
		Tutorial::lastDelayingEvent = timer.Now();

		if (Dialog::HandleMouseDown() || UI::HandleClick())
			return;
		if (Rect<>(0.f,0.97f,0.1f,1.f).Contains(Engine::mouse.location.windowRelative))
		{
			QueryNickname();
			return;
		}

		int oldVehicleHeight = Grid::SeeThrough::GetFocusPoint().height;
		if (Objects::selectedEntity)
			oldVehicleHeight = Objects::selectedEntity->location.height;

	


		if (Objects::selectedMobileEntity && Objects::CursorModeExpectsLeftClick())
		{
			Grid::LCellIdI	target;
			bool includeWater = true;

			if (GetLowCellUnderCursor(target,includeWater, Objects::activeCursorMode == Objects::CursorMode::AttackMode))
			{
				if (Objects::selectedEntity->IsSatellite())
					target.height = Objects::selectedMobileEntity->location.height;

				if (Objects::activeCursorMode == Objects::CursorMode::PlaceStructureMode)
				{
					
					if (Objects::selectedMobileEntity->ConstructingHere(target))
					{
						LogEvent(EventClass::Preset::commandRejected,Objects::selectedMobileEntity.get(),"Can't build here, already trying to build something here");
					}
					else
						if (DirectTo(Objects::selectedMobileEntity,target,false,false,Objects::CursorMode::Idle,false))
						{
							SignalNewJob(Objects::selectedMobileEntity,ConstName::Job_Construct,Objects::activeStructureToPlace->GetCTypeName(),target);
							Objects::arrowFromCell = target;
						}
				}
				else
					DirectTo(Objects::selectedMobileEntity,target,false,true);
				return;
			}
		}
		else
			if (Objects::selectedEntity && Objects::activeCursorMode == Objects::CursorMode::AttackMode)
			{
				float h;
				Objects::PTurretBase base = Objects::selectedEntity->GetTurretBase(h);
				if (base)
				{
					Grid::LCellIdI	target;
					if (GetLowCellUnderCursor(target,true,true))
					{
						DirectTo(Objects::selectedEntity,target,false,true);
						return;
					}
				}
			}

		//selectedId = InvalidIndex;
		{
			float3 b,d;
			Camera::scene.ScreenToVector(Engine::mouse.location.windowRelative * 2.f - 1.f,b,d);
			if (!Vec::zero(d))
			{
				Vec::normalize(d);

				float distance = std::numeric_limits<float>::max();


				bool handledChoice = Objects::HandleChoiceIntersection(b, d, distance);
				if (!handledChoice)
				{
					TVec3<> normal;
					bool selectionDone = false;
					if (Objects::Component::Terraformer::Cmd::BuildBlockIsActive(Objects::selectedEntity) || Objects::Component::Terraformer::Cmd::RemoveBlockIsActive(Objects::selectedEntity))
					{
						if (Grid::high.TestHullIntersection(b, d, distance, normal,false))
						{
							if (Objects::selectedVehicle)
							{
								float3 p = b + d * distance;
								float r = p.length();
								float3 d = p / r;
								UINT32 stackID;
								int h;
								if (Objects::Component::Terraformer::Cmd::BuildBlockIsActive(Objects::selectedEntity))
								{
									if (Vec::dot(d, normal) > 0.5f)	//flat selection
									{
										stackID = Grid::high.Locate(d);
										h = Grid::FromRadius(r);
									}
									else
									{
										stackID = Grid::high.Locate(d + normal*0.001f);
										h = Grid::FloorFromRadius(r);
									}
								}
								else
									if (Vec::dot(d, normal) > 0.5f)	//flat selection
									{
										stackID = Grid::high.Locate(d);
										h = int(floor(Grid::FromRadiusf(r)-0.5f));
									}
									else
									{
										stackID = Grid::high.Locate(d - normal*0.01f);
										h = Grid::FloorFromRadius(r);
									}
								String activity = "build";
								if (Objects::Component::Terraformer::Cmd::RemoveBlockIsActive(Objects::selectedEntity))
									activity = "remove";
								Grid::LCellIdI origin = Objects::selectedVehicle->GetLastPointOfOrigin();
								if (h < -Grid::CellStack::HalfRange)
								{
									LogEvent(EventSignificance::Common, Objects::selectedEntity.get(), "Cannot "+activity+" here: Cannot alter bottom-layer");
								}
								elif (abs(h - origin.height) > 1)
								{
									LogEvent(EventSignificance::Common, Objects::selectedEntity.get(), "Cannot "+activity+" here: Height difference too big: Selected="+String(h)+", vehicle at: "+String(origin.height));
								}
								elif(Objects::vehicleDistanceField.distance[stackID] > 3)
									LogEvent(EventSignificance::Common, Objects::selectedEntity.get(), "Cannot " + activity + " here: Too far away from vehicle");
								elif(Objects::Component::Terraformer::Cmd::BuildBlockIsActive(Objects::selectedEntity) && h == origin.height && Objects::vehicleDistanceField.distance[stackID] <= 1)
									LogEvent(EventSignificance::Common, Objects::selectedEntity.get(), "Cannot " + activity + " here: Too close to vehicle");
								else
								{
									Grid::HCellIdI	cell = Grid::HCellIdI(stackID, h);
									index_t current = Objects::cellSelection.GetIndexOf(cell);
									if (current == InvalidIndex)
										Objects::cellSelection << cell;
									else
										Objects::cellSelection.Erase(current);
								}
							}
							else
								FATAL__("vehicle selection lost");
							selectionDone = true;
						}
					}
					if (!selectionDone)
						if (Objects::Select(b, d, distance, Network::ControlEnabled()))
						{
							selectionDone = true;
						}
						else
						{
							distance = std::numeric_limits<float>::max();

							if (Grid::high.TestHullIntersection(b, d, distance, normal,false))
							{
								float3 p = b + d * distance;
								float height = p.Length();

								distance = std::numeric_limits<float>::max();

								index_t stackID = Grid::low.TestGridIntersection(b, d, height, distance);
								if (stackID != InvalidIndex)
								{
									Grid::LCellIdI	target((UINT32)stackID,Grid::FromRadius(height));
									if (target.height < Grid::CellStack::HalfRange && target.height >= -Grid::CellStack::HalfRange)
									{
										Objects::PWorldEntity e = Objects::GetEntity(target);
										Objects::Select(e,Network::ControlEnabled());
										selectionDone = true;
										#ifdef _DEBUG
											if (Engine::input.pressed[Key::Shift])
												ShowMessage(target.ToString());
										#endif
									}
								}
							}
						}
					if (!selectionDone)
						Objects::Deselect(Network::ControlEnabled());
					//else
						//ShowMessage("nothing");
				}

			}
		}
	}

	void SaveScreenshot()
	{
		saveScreenshot =true;
	}



	void ToggleTimeLapse()
	{
		if (TimeLapse::enabled)
		{
			TimeLapse::enabled = false;
			Dialog::SignalSituationChange();
			LogEvent(EventClass::Preset::uncommon,"Time lapse disabled");
		}
		else
		{
			if (!Network::amSpectator || !Network::client.IsConnected())
			{
				LogEvent(EventClass(EventSignificance::Important,EventFlavor::UserError),"Time lapse recording cannot be enabled if the local client is not configured as spectator");
			}
			else
			{
				PathString outDirName = PathString(Logging::TimeStamp(true));
				if (TimeLapse::customBasePath.IsNotEmpty())
				{
					FileSystem::Folder f = FileSystem::CreateDirectory(TimeLapse::customBasePath);
					if (f.IsValidLocation())
						outDirName = f.FindFile(outDirName,false)->GetLocation();
					else
					{
						LogEvent( EventClass::Preset::commandRejected , "Unable to find folder '"+String(TimeLapse::customBasePath)+". Defaulting to default Documents folder");
						outDirName = RuntimeConfig::ConfigFile("timelapses/"+outDirName);
					}
				}
				else
					outDirName = RuntimeConfig::ConfigFile("timelapses/"+outDirName);

				int framesPerSecond = (int)ceil(1.f / std::max(0.001f,TimeLapse::frameDelaySeconds));
				Dialog::Confirm("Timelapse mode will capture roughly "+String(framesPerSecond)+" frame(s) per second during animation phase, but remain inactive during all other phases. The camera will be locked in place.\nProceed?",
				[outDirName]()
				{
					TimeLapse::runningNumber = 0;
					TimeLapse::enabled = true;
					TimeLapse::basePath = outDirName;
					FileSystem::CreateDirectory(outDirName);
					LogEvent(EventClass::Preset::uncommon,"Time lapse enabled. Writing to "+String(outDirName));
					Dialog::PermanentMessage("Time lapse recording is enabled. Camera is locked. Press F2 to end time lapse recording",[](){return TimeLapse::enabled ? Dialog::CheckResult::OkayContinue : Dialog::CheckResult::Abort;});
				},[](){return Dialog::CheckResult::OkayContinue;});
			}
		}
	}



	void ViewDown()
	{
		if (Grid::SeeThrough::GetFocusPoint().height > -Grid::CellStack::HalfRange)
		{
			Grid::SeeThrough::Update(true, Grid::SeeThrough::LocateCamera()-1);
			Objects::UpdateChoices(Network::ControlEnabled());
			Tutorial::ShowMessage("Clip Terrain");
		}
	}

	void ViewUp()
	{
		if (Grid::SeeThrough::GetFocusPoint().height < Grid::CellStack::HalfRange)
		{
			Grid::SeeThrough::Update(true, Grid::SeeThrough::LocateCamera() + 1);
			Objects::UpdateChoices(Network::ControlEnabled());
			Tutorial::ShowMessage("Clip Terrain");
		}
	}


#endif



const String&	Vocal(Random&random)
{
	static const String vocals[] = 
	{
		"a",
		"e",
		//"i",
		"o",
		"u",
		"y",
		"ou",
		//"ae",
		//"ei",
		"ea",
	};
	return vocals[random.getIndex(ARRAYSIZE(vocals)-1)];
}


const String&	Consonant(Random&random)
{
	static const String consonants[] =
	{
		"b",
		"c",
		"d",
		"g",
		"h",
		//"j",
		"l",
		"m",
		"n",
		"r",
		"s",
		"t",
		"v",
		"w",
		"x",
		"z",
		"th",
		"ch",
	};
	return consonants[random.getIndex(ARRAYSIZE(consonants)-1)];

}

const String&	PreConsonant(Random&random)
{
	static const String preOnly[] =
	{
		"dr",
		"bl",
		"fl",
		"kr",
		"tr",
		"cr",
		"f",
		"k",
		"p",
		"q",

	};


	return random.getFloat() < 0.9f ? Consonant(random) : preOnly[random.getIndex(ARRAYSIZE(preOnly)-1)];
}

const String&	PostConsonant(Random&random)
{
	static const String postOnly[] =
	{
		"ng",
		"ct",
		"st",
	};

	return random.getFloat() < 0.95f ? Consonant(random) : postOnly[random.getIndex(ARRAYSIZE(postOnly)-1)];
}


String GenerateOreName(Random&random)
{
	static const char c0[] =
	{
		'b',
		'c',
		'd',
		'g',
		'h',
		'j',
		'k',
		'm',
		'n',
		'p',
		'q',
		'r',
		's',
		't',
		'v',
		//'w',
		'x',
		'y',
		'z',
	};

	String rs0 = String(c0[random.getIndex(ARRAYSIZE(c0)-1)]);
	
	char c1 = c0[random.getIndex(ARRAYSIZE(c0)-1)];
	while (rs0.firstChar() == c1)
		c1 = c0[random.getIndex(ARRAYSIZE(c0)-1)];
	return rs0 + String(c1) + String(random.getIndex(99));





	String rs;
	
	if (random.getFloat() < 0.5f)
		rs += Vocal(random);
	
	rs +=  PreConsonant(random);

	rs += Vocal(random);// + PostConsonant(random);

	int len = 2;

	//for (int i = 1; i < len; i++)
	//{
	//	rs +=  PreConsonant(random) + Vocal(random);
	//};
	rs +=  PostConsonant(random);

	if (random.getFloat() < 0.8f)
		rs += "ite";
	else
		rs +=  Vocal(random) + PostConsonant(random);

	return rs;

}

void	InnerUndoTask(const Objects::PWorldEntity&e, bool force)
{
	if (!e)
		return;
	if (e->IsMobile())
	{
		#ifdef HAS_WINDOW
			if (Objects::Component::Terraformer::Cmd::BuildBlockIsActive(e))
			{
				Objects::buildCandidates.EraseLast();
				return;
			}
			if (Objects::Component::Terraformer::Cmd::RemoveBlockIsActive(e))
			{
				Objects::removeCandidates.EraseLast();
				return;
			}
		#endif
		Objects::PMobileEntity m = To<Objects::MobileEntity>::Cast(e);
		if (m->HasTasks())
		{
			#ifdef HAS_WINDOW
			if (!force)
			{
				Objects::PMobileEntity v = m;
				if (v->constructionRoundCounter > 0 
					&& v->myTasks.Count() == 1
					&& v->myTasks.GetNewest().IsJob() 
					&& v->myTasks.GetNewest().job->resourceCostLeft > 0)
				{
					TUniqueID id = v->id;
					Dialog::Confirm("Are you sure you wish to clear the current production task? The invested resources will be lost.",[id]()
					{
						InnerUndoTask(Objects::GetEntity(id),true);
					},
					[id]()->Dialog::CheckResult
					{
						if (!Network::client.IsConnected() || !Objects::GetEntity(id))
							return Dialog::CheckResult::Abort;
						if (RuntimeConfig::phase != RuntimeConfig::Phase::CommandPhase || Network::newRoundRequested)
							return Dialog::CheckResult::OkayNotRightNow;
						return Dialog::CheckResult::OkayContinue;
					});
					return;
				}
				else
				{
			
				}
		
			}
			#endif

			//SignalDropLastTask(v);
			//v->DropLastTask();
			bool isJob = m->myTasks.GetNewest().IsJob();
			Network::Channels::dropLastTask.SendTo(Network::client, m->id);
			if (isJob && m->myTasks.Count() > 1 
				&& m->myTasks[m->myTasks.Count() - 2].type == Objects::MobileEntity::Task::MoveTo)
			{
				Network::Channels::dropLastTask.SendTo(Network::client, e->id);
			}
		}
	}
	else
		Network::Channels::dropLastTask.SendTo(Network::client, e->id);
}


//void LogEvent( EventType type,  const Objects::WorldEntity*e, const String&_event)
//{
//	String event = _event, outEvent = _event;
//	if (e)
//	{
//		event = e->owner->GetName()+","+ e->GetNiceName()+": "+event;
//		outEvent = e->owner->GetLogName()+","+ e->GetName()+": "+event;
//	}
//	eventLock.lock();
//	if (type != EventSignificance::Debug && type != EventSignificance::PerformanceMeasurement && type != EventSignificance::Common)
//	{
//		#ifdef HAS_WINDOW
//			if (type == EventType::ChatMessage)
//				events << VisualEvent("",EventSignificance::Debug);
//			events << VisualEvent(event,type);
//		#else
//			std::cout << event << std::endl;
//		#endif
//	}
//	eventLog.Log(String(RuntimeConfig::roundNumber)+tab+ RuntimeConfig::phase.ToString()   + tab + outEvent,true);
//	eventLock.unlock();
//}


#ifdef HAS_WINDOW

	void ToggleControlGUI()
	{
		UI::showControlsAndContacts = !UI::showControlsAndContacts;
		UI::HexGrid::Configure(Objects::selectedEntity);
	}

	void BeginCameraDrag()
	{
		Tutorial::lastDelayingEvent = timer.Now();
		rightMouseDragged = false;
		rightMouseDragBegun = timer.Now();
		rightMouseDragFrom = Engine::mouse.location.absolute;
	}

	template <bool AlsoControlsUnits>
	void EndCameraDrag()
	{
		Tutorial::lastDelayingEvent = timer.Now();

		if (!AlsoControlsUnits)
			return;

		if (timer.GetSecondsSince(rightMouseDragBegun) < 0.2f && !rightMouseDragged)
		{
			Grid::LCellIdI	target;
			if (Objects::selectedEntity)
			{
				Objects::PTurretBase b = Objects::selectedEntity->GetTurretBase();
				if (b && b->turret->CanAttack() && b->turret->trajectoryType != Objects::TrajectoryType::VerticalBeam)
				{
					float3 b, d;
					Camera::scene.ScreenToVector(Engine::mouse.location.windowRelative * 2.f - 1.f, b, d);
					if (!Vec::zero(d))
					{
						Vec::normalize(d);

						float distance = std::numeric_limits<float>::max();

						Objects::PWorldEntity possibleTarget = Objects::GetRayIntersection(b,d,distance);
						if (possibleTarget && !possibleTarget->isOwnedByMe)
						{
							DirectTo(Objects::selectedEntity,possibleTarget->location,false,true, Objects::CursorMode::AttackMode);
							return;
						}
					}
					if (To<Objects::Tower>::DynamicCast(Objects::selectedEntity))
					{
						if (GetLowCellUnderCursor(target,true/*Objects::selectedVehicle && Objects::selectedVehicle->base->moveOnWater*/,false))
						{
							DirectTo(Objects::selectedEntity,target,false,true,Objects::CursorMode::AttackMode);
							return;
						}
					}
				}
				else
				{
					//if (Objects::selectedEntity->IsWorkingStructure())
					{
						//Objects::PWorkingStructure ws = To<Objects::WorkingStructure>::Cast(Objects::selectedEntity);
						//Objects::MobileEntityProducer*p = ws
						if (!Objects::selectedEntity->IsSatellite() && Objects::selectedEntity->CastToProducer())
						{
							if (GetLowCellUnderCursor(target,true/*Objects::selectedVehicle && Objects::selectedVehicle->base->moveOnWater*/,false))
							{
								DirectTo(Objects::selectedEntity,target,false,false);
								return;
							}
						}
					}
				}
				
		
			}

			if (Objects::selectedMobileEntity 
				&& Objects::VehicleCanBeDirected()	//TODO: rename/relocate for mobile entities
			)
			{

				if (GetLowCellUnderCursor(target,true/*Objects::selectedVehicle && Objects::selectedVehicle->base->moveOnWater*/,false))
				{
					//Grid::LCellIdI	target((UINT32)cellID,Grid::FromRadius(height));
					if (Objects::selectedMobileEntity->IsSatellite())
						target.height = Objects::selectedMobileEntity->location.height;
					DirectTo(Objects::selectedMobileEntity,target,false,false);
				}
			}
		}

	}

	void	UndoTask()
	{
		if (Objects::DisableModes(Network::ControlEnabled()))
			return;

		InnerUndoTask(Objects::selectedEntity,false);

	}

	void	QueryAndSendChatMessage()
	{
		if (Dialog::IsModal())
			return ;
		static const String allowed = "abcdefghijklmnopqrstuvwxyzäöüABCDEFGHIJKLMNOPQRSTUVWXYZÄÖÜ0123456789_- <>!'\"'$%^&*()=+.,/\\#@?";
		Dialog::Query("Please enter message","",allowed,[](const String&candidate)->bool
			{
				return candidate.length() <= 256;
			},
			[](const String&msg)
			{
				if (msg.trimRef().length() > 0)
				{
					if (Network::client.IsConnected())
						Network::Channels::chatMessage.SendTo(Network::client,msg);
				}
			}
		);


	}


	void	QueryNickname()
	{
		if (RuntimeConfig::aiMode)
			return;
		//if (Network::chosenNickname.IsEmpty())
		{
			static const String allowed = "abcdefghijklmnopqrstuvwxyzäöüABCDEFGHIJKLMNOPQRSTUVWXYZÄÖÜ0123456789_-";

			Dialog::Query("Please enter a valid nickname to continue (length 4-16)",Network::chosenNickname,
			allowed,
			[](const String&candidate)->bool
				{
					return Package::NetString<32>::IsValid(candidate) 
						&& candidate.length() > 3 && candidate.length() <= 16;
				},
				[](const String&name)
				{
					StringFile userName;
					FileSystem::CreateDirectory("config");
					userName.Create(RuntimeConfig::ConfigFile("userName.txt"));
					userName << name;
					Network::chosenNickname = name;
					if (Network::client.IsConnected())
					{
						Network::Channels::changeMyName.SendTo(Network::client,name);


					}
				}
			);
		}
	}
	void	QueryNicknameIfEmpty()
	{
		if (Network::chosenNickname.IsEmpty())
			QueryNickname();
	}

	void	AbortMode()
	{
		if (Dialog::CloseAllModal())
		{
			QueryNicknameIfEmpty();
			return;
		}
		if (Objects::DisableModes(Network::ControlEnabled()))
			return;

		if (!Dialog::IsVisible(mainMenu))
		{
			Dialog::Show(mainMenu);
		}
		else
			Dialog::Hide(mainMenu);
		Dialog::SignalSituationChange();
	}

	#ifdef HAS_WINDOW
		void	ReallyStartNewFaction()
		{
			if (Network::haveFaction)
				Network::AbandonCurrentFaction();

			//technite menu goes here

			//Network::StartNewFaction();
		}
		void	StartNewFaction()
		{
			if (Network::haveFaction)
			{
				Dialog::Confirm("You already are in control of a faction. If you start a new faction, your previous one will be abandoned. This process cannot be reversed. Continue?",
							ReallyStartNewFaction,
							[](){return Network::client.IsConnected() && Network::haveFaction ? Dialog::CheckResult::OkayContinue : Dialog::CheckResult::Abort;});
			}
			else
				ReallyStartNewFaction();

		}
	#endif

	void ToggleResourceView()
	{
		UI::resourcesAsList = !UI::resourcesAsList;

	}


	bool	IsIdle(const Objects::PMobileEntity&v)
	{
		//okay, clarification:
		//0: if dead: NOT IDLE (duh)
		if (v->health <= 0)
			return false;	//yes, this can happen if badly called
		//1: if hibernating: NOT IDLE, EVER
		if (v->IsHibernating())
			return false;
		//2: if automated, then some other logic takes care of everything. NOT IDLE
		if (v->isAutomated)
			return false;

		//3: if it already IS doing something, anything really, then it is NOT IDLE
		if (v->myTasks.IsNotEmpty())	//if it already is doing normal vehicle stuff, then it's not IDLE (duh)
			return false;
		const Objects::MobileEntityProducer*p =  v->CastToProducer();
		if (p && p->queue.IsNotEmpty())	//if it's a factory and has any task, then it's not IDLE
			return false;


		//4: if it's SLEEPING then it's not idle, UNLESS the local entity can attack, and has something to attack:
		if (v->IsSleeping())
		{

			if (v->CanAttack() && v->HasAmmoForShot() && v->CountPossibleAttacks())	//check that its turret can attack, there's ammo for the shot, and it has any shots left this round
			{
				bool emp = v->turret->dealsEMPDamage;
				if (v->IsVehicle())
				{
					for (auto other : Objects::vehicles)
						if (!emp || !other->empDamageLevel)
							if (!other->isOwnedByMe && v->CanHit(other->location))
								return true;
					for (auto other : Objects::structures)
						if (!emp || !other->empDamageLevel)
							if (!other->isOwnedByMe && v->CanHit(other->location))
								return true;
				}
				else
					if (v->IsSatellite())
					{
						for (auto other : Objects::satellites)
							if (!other->isOwnedByMe && v->CanHit(other->location))
								if (!emp || !other->empDamageLevel)
									return true;
					}
					else
					{
						FATAL__("Unknown mobile entity type "+v->GetTypeName());
					}
			}
			return false;	//cannot attack or has nothing to attack: NOT IDLE since it is SLEEPING
		}

		//not sleeping, no tasks, not producing, not automated, not hibernating:
		return true;	//yes, IDLE
	}


	bool	IsIdle(const Objects::PStructure&s)
	{
		if (s->health <= 0)
			return false;
		#ifndef NO_HIBERNATION
			if (s->ToHibernating()->IsHibernating())
				return false;
		#endif

		if (!s->IsWorkingStructure() || s->IsSleeping())
			return false;
		Objects::PWorkingStructure ws = To<Objects::WorkingStructure>::Cast(s);

		Objects::PVehiclePlant vp = To<Objects::VehiclePlant>::DynamicCast(s);
		if (vp)
			return (vp->AwaitsUserInteraction() || /*vp->queue.IsEmpty() || */
			(!vp->IsEnabled()
			&&
			vp->CouldEnable(true)
			)
			);
		Objects::PMine mine = To<Objects::Mine>::DynamicCast(s);
		if (mine)
			return !mine->IsEnabled() && mine->CouldEnable();
		if (s->IsSkyLinkStructure())
		{
			Objects::PSkyLinkStructure st = To<Objects::SkyLinkStructure>::DynamicCast(s);	//around a inheritance corner, prolly required to use dynamic cast here
			if (!st->connectedToHub.expired())
			{
				return !To<Objects::WorkingStructure>::Cast(s)->IsEnabled();
			}
			if (s->IsSkyPort())
			{
				Objects::PSkyPort p = To<Objects::SkyPort>::Cast(s);
				if (p->queue.IsEmpty() || (!p->IsEnabled() && p->CouldEnable()))
					return true;
			}
		}
		return false;

	}

	bool	ShouldSignalRoundEnd()
	{
		if (!Objects::Faction::mine || Objects::Faction::mine->requestedNewRound || Network::amSpectator)
			return false;
		count_t total=0,requested=0;
		Objects::ForeachFaction([&total,&requested](const Objects::PFaction&f)
		{
			total++;
			if ((f->requestedNewRound || !f->online) && f != Objects::Faction::mine)
				requested++;
		});

		if (requested+1 != total)
			return false;

		Objects::PWorldEntity any;
		Objects::ForeachWorldEntity([&any](const Objects::PWorldEntity&e)
		{
			if (any || !e->isOwnedByMe || e->health <= 0)
				return;
			if (e->IsMobile())
			{
				if (IsIdle(To<Objects::MobileEntity>::Cast(e)))
					any = e;
			}
			elif (e->IsStructure())
			{
				if (IsIdle(To<Objects::Structure>::Cast(e)))
					any = e;
			}
		});
		//#ifdef _DEBUG
		//if (any)
		//	LogEvent(EventSignificance::Uncommon,any.get(),"not idle");
		//#endif
		return !any;
	}
	

	void	SelectAndFocusNextIdle()
	{
		if (Dialog::ConfirmCurrent(true))
			return;

		const count_t total = Objects::vehicles.Count() + Objects::satellites.Count() + Objects::structures.Count();
		if ((total == 0) || !Network::haveID)
			return;
		index_t from = 0;
		if (Objects::selectedVehicle)
			from = Objects::vehicles.GetIndexOf(Objects::selectedVehicle);
		else
			if (Objects::selectedMobileEntity && Objects::selectedMobileEntity->IsSatellite())
			{
				from = Objects::satellites.GetIndexOf(Objects::selectedMobileEntity);
				if (from != InvalidIndex)
					from += Objects::vehicles.Count();
			}
			else
				if (Objects::selectedEntity && Objects::selectedEntity->IsStructure())
				{
					from = Objects::structures.GetIndexOf(Objects::selectedEntity);
					if (from != InvalidIndex)
						from += Objects::vehicles.Count() + Objects::satellites.Count();
				}
		if (from == InvalidIndex)
			from = 0;

		index_t to = from;
		do
		{
			to ++;
			if (to >= total)
				to = 0;
			if (to < Objects::vehicles.Count())
			{
				Objects::PVehicle v = Objects::vehicles[to];
				if (v->isOwnedByMe && IsIdle(v))
				{
					Objects::Select(v,Network::ControlEnabled());
					Camera::TranslateTo(Objects::selectedEntity->system.w.xyz);
					return;
				}
			}
			else
				if (to < Objects::vehicles.Count() + Objects::satellites.Count())
				{
					index_t at = to - Objects::vehicles.Count();
					Objects::PMobileEntity v = Objects::satellites[at];
					if (v->isOwnedByMe && IsIdle(v))
					{
						Objects::Select(v,Network::ControlEnabled());
						Camera::TranslateTo(Objects::selectedEntity->system.w.xyz);
						return;
					}
				}
				else
				{
					index_t at = to - Objects::vehicles.Count() - Objects::satellites.Count();
					Objects::PStructure s = Objects::structures[at];

					if (s->isOwnedByMe && IsIdle(s))
					{
						Objects::Select(s,Network::ControlEnabled());
						Camera::TranslateTo(Objects::selectedEntity->system.w.xyz);
						return;
					}
		
				}
		}
		while (to != from);

		UI::RefreshButtonState();	//just in case
	}




	
	void	SelectAndFocusPrev()
	{
		const count_t total = Objects::vehicles.Count() + Objects::satellites.Count();
		if ((total == 0) || !Network::haveID)
			return;
		index_t from = 0;
		if (Objects::selectedMobileEntity)
		{
			if (Objects::selectedMobileEntity->IsVehicle())
				from = Objects::vehicles.GetIndexOf(Objects::selectedMobileEntity);
			else
				if (Objects::selectedMobileEntity->IsSatellite())
				{
					from = Objects::satellites.GetIndexOf(Objects::selectedMobileEntity);
					if (from != InvalidIndex)
						from +=  Objects::vehicles.Count();
				}
			if (from == InvalidIndex)
				from = 0;

		}
		
		index_t to = from;
		do
		{
			if (to > 0)
			{
				to --;
			}
			else
				to = total-1;
			if (to < Objects::vehicles.Count())
			{
				if (Objects::vehicles[to]->owner == Objects::Faction::mine)
				{
					Objects::Select(Objects::vehicles[to],Network::ControlEnabled());
					Camera::TranslateTo(Objects::selectedEntity->system.w.xyz);
					return;
				}
			}
			else
				if (Objects::satellites[to - Objects::vehicles.Count()]->owner == Objects::Faction::mine)
				{
					Objects::Select(Objects::satellites[to - Objects::vehicles.Count()],Network::ControlEnabled());
					Camera::TranslateTo(Objects::selectedEntity->system.w.xyz);
					return;
				}
		}
		while (to != from);


	}

	void	SelectAndFocusNext()
	{
		const count_t total = Objects::vehicles.Count() + Objects::satellites.Count();
		if ((total == 0) || !Network::haveID)
			return;
		index_t from = 0;
		if (Objects::selectedMobileEntity)
		{
			if (Objects::selectedMobileEntity->IsVehicle())
				from = Objects::vehicles.GetIndexOf(Objects::selectedMobileEntity);
			else
				if (Objects::selectedMobileEntity->IsSatellite())
				{
					from = Objects::satellites.GetIndexOf(Objects::selectedMobileEntity);
					if (from != InvalidIndex)
						from +=  Objects::vehicles.Count();
				}
			if (from == InvalidIndex)
				from = 0;

		}
		index_t to = from;
		do
		{
			to ++;
			if (to >= total)
				to = 0;
			if (to < Objects::vehicles.Count())
			{
				if (Objects::vehicles[to]->owner == Objects::Faction::mine)
				{
					Objects::Select(Objects::vehicles[to],Network::ControlEnabled());
					Camera::TranslateTo(Objects::selectedEntity->system.w.xyz);
					return;
				}
			}
			else
				if (Objects::satellites[to - Objects::vehicles.Count()]->owner == Objects::Faction::mine)
				{
					Objects::Select(Objects::satellites[to - Objects::vehicles.Count()],Network::ControlEnabled());
					Camera::TranslateTo(Objects::selectedEntity->system.w.xyz);
					return;
				}
		}
		while (to != from);
	}

	void AddFactionCreationChoice(const String&line, UINT32 typeFlags, UINT32 supportFactionFlag)
	{
		Dialog::loginMenu->AddCommand(line,
		[typeFlags,supportFactionFlag]()
		{
			if (Network::HaveFactionKeyForCurrentWorld() && typeFlags != Protocol::FactionType::SpectatorFaction)
			{
				Dialog::Confirm("A faction key is known for this world ("+Network::worldIDString+"). If you create a new faction, this key will be lost, and control of your old faction will irreversible be abandoned. Please confirm.",
				[typeFlags](){
					Network::AttemptFactionLogin(true,typeFlags);
					Dialog::Hide(Dialog::loginMenu);
				},[supportFactionFlag]()
				{
					if (!Network::amOnline || Network::haveFaction || !(supportFactionFlag & Network::gameFlags))
						return Dialog::CheckResult::Abort;
					if (RuntimeConfig::phase != RuntimeConfig::Phase::CommandPhase && RuntimeConfig::phase != RuntimeConfig::Phase::TechniteCommandPhase)
						return Dialog::CheckResult::OkayNotRightNow;
					return Dialog::CheckResult::OkayContinue;
				});
			}
			else
			{
				Network::AttemptFactionLogin(true,typeFlags);
				Dialog::Hide(Dialog::loginMenu);
			}
		},[supportFactionFlag](){return Network::amOnline && !Network::haveFaction && (Network::gameFlags & supportFactionFlag) 
			&& (
					RuntimeConfig::IsInAnyCommandPhase() 
						|| RuntimeConfig::phase  == RuntimeConfig::Phase::AnimationPhase 
						|| RuntimeConfig::phase  == RuntimeConfig::Phase::EndOfGame
				)
			;},false);


	}

	void	ResumeBatchConstruction()
	{

		if (Objects::selectedMobileEntity && 
			Objects::selectedMobileEntity->turret->CanConstruct() &&
			Objects::activeStructureToPlace &&
			(Objects::activeCursorMode == Objects::CursorMode::Idle
			|| Objects::activeCursorMode == Objects::CursorMode::PlaceStructureMode
			)
		)
		{
			bool newly = Objects::activeCursorMode == Objects::CursorMode::Idle;
			Objects::arrowFromCell = Objects::selectedMobileEntity->GetLastPointOfOrigin();
			Objects::arrowFrom = Grid::low.cells[Objects::arrowFromCell.stackID].centerVertex.xyz * ( Grid::ToRadius(Objects::arrowFromCell.height));
			Objects::showArrow = true;
			Objects::activeCursorMode = Objects::CursorMode::PlaceStructureMode;
			if (newly)
				UI::RefreshButtonState();
		}

	}

	void	AbortBatchConstruction()
	{
		if (Objects::activeCursorMode == Objects::CursorMode::PlaceStructureMode)
		{
			Objects::activeCursorMode = Objects::CursorMode::Idle;
			Objects::showArrow = false;
			UI::RefreshButtonState();
		}

	}

#endif


void ParseKeyValue(const String&key, const String&value)
{
	try
	{
		LogEvent(EventClass::Preset::configuration, nullptr, "Setting `" + key + "` = '" + value + "'");
		if (key == "aiAccessToken")
		{
			RuntimeConfig::aiMode = true;
			RuntimeConfig::aiAccessToken.Decode(value);
		}
		elif (key == "udpListen")
		{
			if (!convert(value.c_str(), RuntimeConfig::udpListen))
				throw Program::ParameterFault("Unable to parse udpListen='" + value + "' to bool");
		}
		elif(key == "serverURL")
		{
			RuntimeConfig::serverAddress = Network::ServerAddress(value);
			LogEvent(EventClass::Preset::configuration, nullptr, "Server address set to "+RuntimeConfig::serverAddress.ToString());
		}
		else
			throw Program::ParameterFault("`"+key+"` is not a recognized runtime parameter");
	}
	catch (const std::exception&e)
	{
		throw Program::ParameterFault("While parsing `" + key + "`='" + value + "': "+String(e.what()));
	}

}


bool ConfigureClient(const wchar_t*commandLine)
{
	
	//ShowMessage(String(commandLine));
	try
	{
		String cmd = String(commandLine);
		//ShowMessage(cmd);

		StringList	tokenList;
		Tokenizer::Configuration cfg;
		cfg.convert_tabs = false;
		cfg.operators = "=";
		cfg.quotations = "\"'";
		cfg.quotation_escape_character = '\\';
		cfg.trim_characters = " \t";

		if (!Tokenizer::tokenize(cmd, cfg, tokenList))
		{
			throw Program::ParameterFault("Configuration syntax violation: Unable to parse parameter line '"+cmd+"' (missing closing quotes perhaps?)");
		}

		String parameterName;
		bool expectValue = false;
		Tokenizer::dequote(tokenList, cfg);
		foreach(tokenList, token)
		{
			if (*token == '=')
			{
				if (parameterName.IsEmpty())
					throw Program::ParameterFault("Configuration syntax violation: Missing parameter name before '='");
				expectValue = true;
				continue;
			}
			if (expectValue)
			{
				ParseKeyValue(parameterName, *token);
				expectValue = false;
				parameterName = "";
			}
			else
			{
				if (parameterName.IsNotEmpty())
					throw Program::ParameterFault("Configuration syntax violation: Found multiple tokens ('" + parameterName + "' and '" + *token + "') without intermediate '='");
				parameterName = *token;
				//expectValue = true;
			}
		}
		if (expectValue)
		{
			throw Program::ParameterFault("Configuration syntax violation: Missing parameter value for '" + parameterName + "'");
		}
		else
			if (parameterName.IsNotEmpty())
				throw Program::ParameterFault("Configuration syntax violation: Missing '=' and parameter value for '" + parameterName + "'");
	}
	catch (const std::exception&e)
	{
		MyFatal(FatalDescriptor(CLOCATION, e.what()));
		return false;
	}
	if (RuntimeConfig::serverAddress.host.IsNotEmpty())
	{
		Network::ServerDiscovery::RegisterNewHost(RuntimeConfig::serverAddress, false,true);
	}


	onFatal = MyFatal;
	RuntimeConfig::ClientInit();

	ASSERT__(FileSystem::CreateDirectory(RuntimeConfig::ConfigFile("logs")));
	Logging::Begin(RuntimeConfig::ConfigFile(PathString("logs/client_"+Logging::TimeStamp(true)+".log")));

	//(PRODUCT_NAME".log",true,true)
	return true;
}

bool	IsTargetWorkingDirectory(const PathString&dir)
{
	PathString test = dir + "/resources";
	bool rs = FileSystem::IsDirectory(test);
	LogEvent(EventSignificance::Debug,"Checking server target directory candidate '"+String(dir)+"'=>'"+String(test)+"'. Result="+String(rs));


	return rs;
}


int main( int argc, const char* argv[])
{
	onFatal = MyFatal;
	Vec::assertConsistency<float>();

	
	#ifndef HAS_WINDOW
		String cfg;
		for (int i = 1; i < argc; i++)
			cfg += " "+String(argv[i]);
		ConfigureClient(WString(cfg.c_str()).c_str());

	#endif


	try
	{
		LogEvent(EventClass::Preset::uncommon,nullptr,"Starting client v" PRODUCT_VERSION);

	//	FileSystem::CreateDirectory(RuntimeConfig::ConfigDir());
		
		{
			wchar_t path[MAX_PATH];
			ASSERT__(GetModuleFileNameW(NULL,path,MAX_PATH));
			StringW dir = FileSystem::ExtractFileDir(StringW(path));
			while (dir.length() > 1 && !IsTargetWorkingDirectory(dir))
			{
				dir = FileSystem::ExtractFileDir(dir);
				LogEvent(EventSignificance::Debug,"Walking up to "+String(dir.c_str()));
			}
			if (IsTargetWorkingDirectory(dir))
				FileSystem::SetWorkingDirectory(dir);
			else
				FATAL__("Something is wrong. Unable to locate server executable, or config/declaration.xml is missing.");
		}


		NiceName::Load();

		Network::LoadWorldAccessTokens();

		{
			Configuration	config;

			PathString userConfigPath = RuntimeConfig::ConfigFile("game.ini");
			if (!FileSystem::IsFile(userConfigPath))
				FileSystem::CopyFile(PathString("./game.ini"),userConfigPath,false);
			if (!config.LoadFromFile(userConfigPath))
			{
				LogEvent(EventClass::Preset::configuration,nullptr,"Unable to load user configuration from '"+String(userConfigPath)+"': '"+config.Report()+"'. Attempting to load original config");
				config.LoadFromFile("game.ini");
			}

			{
				TechniteLogicLink::autoStartClientPath = PathString(config.RequireString("technites/autoStartClientPath"));
				TechniteLogicLink::autoStartClient = config.RequireBool("technites/autoStartClient");
				bool fixedServerPort = config.RequireBool("technites/fixedServerPort");
				unsigned rawPortNumber = config.RequireUnsigned("technites/fixedServerPortNumber");
				TechniteLogicLink::initialServerPortNumber = (UINT16)(fixedServerPort ? rawPortNumber : 0);
			}




			#ifdef HAS_WINDOW
				Config::Context::Attribute * attrib = config.GetAttrib("runtime/capFPS");
				if (attrib && !attrib->value.equalsIgnoreCase("off"))
					convert(attrib->value.c_str(),RuntimeConfig::FromFile::capFPS);
				attrib = config.GetAttrib("runtime/alwaysDay");
				convert(attrib->value.c_str(),RuntimeConfig::FromFile::alwaysDay);
				display.config.vertical_sync = config.GetInt("runtime/vsync");
				display.config.vertical_sync = display.config.vertical_sync > 0 ? 1 : 0;
				display.config.fsaa_samples = config.GetInt("runtime/fsaa",4);

				UI::resourcesAsList = config.GetBool("runtime/resourcesAsList",false);

				TimeLapse::customBasePath = PathString(config.GetString("timelapse/outDirectory"));
				TimeLapse::frameDelaySeconds = config.GetFloat("timelapse/frameDelaySeconds",0.2f);
				TimeLapse::maxCompressionPNG = config.GetBool("timelapse/maxCompressionPNG");

				if (!RuntimeConfig::aiMode && config.GetBool("runtime/soundEnabled",true))
				{
					AudioInterface::Create();
					AudioInterface::SetMasterVolume(config.GetFloat("runtime/soundVolume",1.f));
				}

			#endif
		}
		LoadHosts();


		applicationStart = timer.Now();




		Engine::GlobalAspectConfiguration::worldZIsUp = true;
		
		
		//display.create(Engine::DisplayConfig("TerraLab");

		#ifdef HAS_WINDOW
			storageThread.start();

			Engine::CreateDisplay(display,
				RuntimeConfig::ConfigDir(),
				RuntimeConfig::aiMode ? "AI":
				PRODUCT_NAME
				#ifdef _DEBUG
					" (DEBUG)"
				#endif
				,Resolution(1280,800),onWindowResize
				,Engine::DisplayConfig::Icon(IDI_CLIENT)
				 );

			display.ScanShaderHeader("shaders");


			textout.getActiveFont()->LoadFromFile("Font/font.xml");


			LineRenderer::Create();
			SurfaceRenderer::Create();

			//Engine::input.bind(Key::Up, Camera::Event::ZoomIn);
			//Engine::input.bind(Key::Down, Camera::Event::ZoomOut);

			Engine::input.bind(Key::Left, SelectAndFocusPrev);
			Engine::input.bind(Key::Right, SelectAndFocusNext);
			Engine::input.bind(Key::Space, SelectAndFocusNextIdle);

			Engine::input.bind(Key::PageDown, ViewDown);
			Engine::input.bind(Key::PageUp, ViewUp);

			Engine::input.bind(Key::Shift, ResumeBatchConstruction, AbortBatchConstruction);
			

		//	Engine::input.bind(Key::PageDown, DeclineCamera);
			//Engine::input.bind(Key::PageUp, InclineCamera);

			Engine::input.bind(Key::Backspace, UndoTask);
			Engine::input.bind(Key::Escape, AbortMode);
			Engine::input.bind(Key::Enter, SignalRoundEndOrMaximize);
			Engine::input.bind(Key::Return, SignalRoundEndOrMaximize);
			Engine::input.bind(Key::Tab, UI::ShowSession);
			Engine::input.bind(Key::T,[](){}, QueryAndSendChatMessage);
			Engine::input.bind(Key::R,ToggleResourceView);

			Engine::input.bind(Key::H,ToggleControlGUI);

			Engine::input.bind(Key::MouseButton0, Select, Dialog::HandleMouseUp);
			Engine::input.bindCtrl(Key::MouseButton0, Select);
			Engine::input.bind(Key::RightMouseButton, BeginCameraDrag,EndCameraDrag<true>);
			Engine::input.bind(Key::MiddleMouseButton, BeginCameraDrag,EndCameraDrag<false>);

			Engine::input.bind(Key::F3, SaveScreenshot);
			Engine::input.bind(Key::F2,ToggleTimeLapse);

			#ifdef RAYTRACE
				Engine::input.bind(Key::Up,ZoomIn);
				Engine::input.bind(Key::Down,ZoomOut);
			#endif

			Camera::Setup(display.pixelAspect());



			Engine::mouse.bindWheel(Camera::Event::WheelAction);

			textout.setScale(1.f/display.clientWidth(),1.f/display.clientHeight());


			display.SetBackbufferClearColor(0,0,0,0);

			ProgressBar::Create();
			Dialog::Create();

		#endif


		
		Grid::Create0();
		Objects::Create();
		Grid::Create1();


		#ifdef HAS_WINDOW


			Explosions::Create();
		#endif
		Animation::Create();


		Network::Setup();

		#ifdef HAS_WINDOW
			UI::Create();
		#endif


		//#ifdef WHITE_STYLE
		//	display.SetBackbufferClearColor(1,1,1,0);
		//#endif


		#ifdef HAS_WINDOW

			mainMenu.reset(new Dialog::Menu(0.25f,"Main"));
			mainMenu->AddCommand("Change Nickname",QueryNickname);
			mainMenu->AddCommand("Abandon faction",Network::AbandonCurrentFaction,[](){return Network::amOnline && Network::haveFaction;});
			mainMenu->AddCommand("Disconnect",
				[]()
				{
					Network::client.Disconnect();
				},
				[](){return Network::client.IsConnected();});
			mainMenu->AddCommand("Quit to Desktop",[](){Dialog::Confirm("Really quit? Any active faction will be preserved server-sided for you to continue later on.",[](){application_shutting_down = true;},[](){return Dialog::CheckResult::OkayContinue;});});
			mainMenu->FullAutoLayout();

			Dialog::loginMenu->AddCommand("Continue existing faction",[](){Network::AttemptFactionLogin(false,Protocol::FactionType::ContinueOnly);},
			[]()
			{
				return Network::amOnline && !Network::haveFaction 
				#ifndef _DEBUG
				&& Network::HaveFactionKeyForCurrentWorld() 
				#endif
				&& (RuntimeConfig::phase == RuntimeConfig::Phase::CommandPhase || RuntimeConfig::phase == RuntimeConfig::Phase::TechniteCommandPhase);
			}
			);
			AddFactionCreationChoice("Spectate",Protocol::FactionType::SpectatorFaction, Protocol::GameFlag::SupportSpectatorFactions);
			AddFactionCreationChoice("Create new regular faction",Protocol::FactionType::RegularFaction, Protocol::GameFlag::SupportRegularFactions);
			AddFactionCreationChoice("Create new Technite faction",Protocol::FactionType::TechniteFaction, Protocol::GameFlag::SupportTechniteFactions);
			AddFactionCreationChoice("Create new hybrid regular/Technite faction",Protocol::FactionType::TechniteFaction | Protocol::FactionType::RegularFaction, Protocol::GameFlag::SupportHybridFactions);

			


			if (RuntimeConfig::FromFile::alwaysDay)
			{
				TMatrix3<> mat,rot;
				Mat::makeAxisSystem(float3::reinterpret(Camera::scene.location).Normalized(),2,mat);
				Mat::rotationMatrix(SunOffAngle, mat.x,rot);
				Mat::mult(rot,mat.z,sunDirection);
			}


			Atmosphere::Create();
			#if !defined HAS_WINDOW || !defined _DEBUG
				if (RuntimeConfig::aiMode)
					ShowWindow(Engine::context.getWindow(),SW_MINIMIZE);
			#endif
			textout.getActiveFont()->BeginTagMode(StatePanel::OnTag,StatePanel::TagWidth,
					Objects::TextoutResourceTagOffset,(char)(Objects::TextoutResourceTagOffset+(char)Objects::Resource::NumberOfPossibilities + 2));
			Tutorial::Create();


			Tutorial::ShowMessage("Welcome",true);
			Tutorial::ShowMessage("Tutorials");
			Tutorial::ShowMessage("Servers");


			{
				String name;
				{
					StringFile userName(RuntimeConfig::ConfigFile("userName.txt"));
					if (userName.IsActive())
						userName >> name;
				}
				if ( Package::NetString<32>::IsValid(name) 
						&& name.length() > 3 && name.length() <= 16)
				{
					Network::chosenNickname = name;
				}
				else
				{
					QueryNickname();
				}
			}


			display.assign(loop);
			display.execute();
		#else
			while (!application_shutting_down && loop())
			{};
				

		#endif


		TechniteLogicLink::Destroy();

		Network::Shutdown();

		#ifdef HAS_WINDOW
			ProgressBar::Destroy();
			Dialog::Destroy();
			Atmosphere::Destroy();
			UI::Destroy();
			Explosions::Destroy();
			LineRenderer::Destroy();
			SurfaceRenderer::Destroy();
			AudioInterface::Destroy();
		#endif
		Grid::Destroy();
		Objects::Destroy();
		Animation::Destroy();

		application_shutting_down = true;



		#ifdef HAS_WINDOW
			storageThread.Terminate();
			display.destroy();
		#endif

		ServerProcess::Destroy();
		Logging::End();

		TerminateProcess(GetCurrentProcess(),0);	//self-terminate

		return 0;
	}
	catch (const Exception&exception)
	{
		FATAL__("Caught unhandled exception of type "+String(exception.GetType())+":\n"+exception.what());
	}
	catch (const std::exception&exception)
	{
		FATAL__("Caught unhandled exception of type "+String(typeid(exception).name())+":\n"+String(exception.what()));
	}
	catch (...)
	{
		FATAL__("Caught something...");
	}
	return -1;
}

int APIENTRY _tWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPTSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
	return main(0,NULL);
}




