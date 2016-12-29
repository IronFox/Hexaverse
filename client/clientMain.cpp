

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

#ifdef HAS_WINDOW
	#include "components/explosions.h"
#endif

#include <geometry/bvh.h>

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
#include "components/configurationMode.h"

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







namespace LocalApplicationInterface
{
	#ifdef HAS_WINDOW
		void						QueryNickname();
		void						QueryNicknameIfEmpty();
		void						RefreshSessionView();
		//void						OnSelectionChange(bool stateChangeOnly);
	#else

	#endif

	void							SignalMarkerDeletion(TUniqueID markerID);	//must be copy to prevent weird shit
	/**
	Triggered when entering command mode and animations have finished (whichever of those events happens later)
	*/
	void	OnPlayerControlBegin()
	{
	//	LogEvent(EventSignificance::Debug,"OnPlayerControlBegin("+String(RuntimeConfig::roundNumber)+","+String(RuntimeConfig::techniteSubRoundNumber)+")");
		LogEvent(EventSignificance::Debug,"OnPlayerControlBegin("+RuntimeConfig::roundNumber.ToString()+")");

		#ifdef HAS_WINDOW
			ApplicationInterface::OnSelectionChange(true);
			UI::RefreshButtonState();
		#endif
		#ifndef NO_SATELLITES
			Objects::RefreshSkyHubConnections();
		#endif
		Animation::UpdateAutomation(CLOCATION);
		Objects::CheckMobileEntitySanity();
		if (RuntimeConfig::autoNewRound && ApplicationInterface::ShouldSignalRoundEnd())
			ApplicationInterface::SignalRoundEnd();
	}
}

#ifdef HAS_WINDOW

	void	OnTutorialsChanged()
	{
		UI::HexGrid::BottomRightPanel::Refresh();
	}

#endif




//void LogEvent(const EventClass&type,  const Objects::WorldEntity*e, const String&_event);



bool CanEnableAtAll()
{
	return Network::client.IsConnected();

}

static void SendDirectTo(const Objects::PWorldEntity&v, const ArrayRef<Grid::LCellIdI>&targets, UINT8 flags, Objects::Vehicle::mode_t mode)
{
	Protocol::EntityDirectCommand	cmd;
	cmd.id = v->id;
	cmd.mode = (UINT16)mode;
	cmd.path = targets;
	cmd.directFlags = flags;
	Network::Channels::entityDirectCommand.SendTo(Network::client,cmd);
}


bool	MoveVehicleTo(const Objects::PMobileEntity&v, const Grid::LCellIdI&prev, Grid::LCellIdI target, UINT8 flags)
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
			#ifndef NO_SATELLITES
				if (v->IsSatellite())
					foundPath = Objects::AppendShortestPathI((const Objects::Satellite*)v.get(),prev,target.stackID,path,false,&tooLong);
				else
			#endif
				FATAL__("Routing not implemented for "+v->GetTypeName());
		}
		if (!foundPath)
		{
			if (!Grid::actualWorld.LowCellIsClearIgnoreStructure(target) && target.height+1 <= Grid::CellStack::TopHeight)
			{
				target++;
				return MoveVehicleTo(v,prev,target,flags);
			}
			LogEvent(EventClass::Preset::commandRejected,v.get(),"Unable to find route to designated destination");
			//SendDirectTo(v,target,false,false,Objects::CursorMode::Idle);	//maybe some error in the client
			return false;
		}
		if (!tooLong)
			SendDirectTo(v,target,flags,Objects::CursorMode::Idle);	//all good
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
			SendDirectTo(v,waypoints.ToRef(),flags,Objects::CursorMode::Idle);
		}
	}
	return true;
}


namespace LocalApplicationInterface
{
	bool	DirectTo(const Objects::PWorldEntity&v, const ArrayRef<Grid::LCellIdI>&targets, UINT8 flags, Objects::Vehicle::mode_t mode, bool interactive)
	{
		if (!v)
			return false;
		DBG_ASSERT__(!RuntimeConfig::aiMode || !Network::newRoundRequested);
		const bool moveTo = mode == Objects::CursorMode::Idle && !(flags & Protocol::DirectFlags::LeftClick);

		if (v->IsMobile() && moveTo)
		{
			Objects::PMobileEntity me = To<Objects::MobileEntity>::Cast(v);

			#ifdef HAS_WINDOW
			#ifndef NO_SATELLITES
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
			#endif

			if (!(flags & Protocol::DirectFlags::Detailed))
			{
				Grid::LCellIdI start = me->GetLastPointOfOrigin();
				foreach (targets,t)
				{
					if (!MoveVehicleTo(me,start,*t,flags))
						return false;
					start = *t;
				}
			}
			else
				SendDirectTo(v,targets,flags,Objects::CursorMode::Idle);
		}
		else
		{
			SendDirectTo(v,targets,flags,mode);
		}

		#ifdef HAS_WINDOW
			Objects::DisableModes(Network::ControlEnabled());
			if (moveTo)
				Tutorial::ShowMessage("Move To");
		#endif
		return true;
	}


	UINT8	AssembleMotionFlags(bool detailedPath, bool leftClick, bool clearSafeTasks)
	{
		UINT8 flags = 0;
		if (detailedPath)
			flags |= Protocol::DirectFlags::Detailed;
		if (leftClick)
			flags |= Protocol::DirectFlags::LeftClick;
		if (clearSafeTasks)
			flags |= Protocol::DirectFlags::ClearSafeTasks;
		return flags;
	}

	bool	DirectTo(const Objects::PWorldEntity&v, const ArrayRef<Grid::LCellIdI>&targets, UINT8 flags, Objects::Vehicle::mode_t mode)
	{
		return DirectTo(v, targets, flags, mode, true);
	}
}

namespace ApplicationInterface
{
	bool	DirectTo(const Objects::PWorldEntity&v, const ArrayRef<Grid::LCellIdI>&targets, bool detailedPath, bool leftClick)
	{
		return
		#ifdef HAS_WINDOW
			LocalApplicationInterface::DirectTo(v,targets, LocalApplicationInterface::AssembleMotionFlags(detailedPath,leftClick,false),Objects::activeCursorMode);
		#else
			LocalApplicationInterface::DirectTo(v,targets, LocalApplicationInterface::AssembleMotionFlags(detailedPath,leftClick,false),Objects::CursorMode::Idle);
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
		LogEvent(EventSignificance::Common,"Signalling choice intersection with "+ch.target.ToString());
		Network::Channels::choiceIntersection.SendTo(Network::client,ch);

		#ifdef HAS_WINDOW
		
			UI::HexGrid::Configure(Objects::selectedEntity);
		#endif
	}


	void	SignalTargetLessAttack(const Objects::PWorldEntity&e)
	{
	//	Grid::LCellIdI loc = e->location;
		LocalApplicationInterface::DirectTo(e,Grid::LCellIdI(e->location),  LocalApplicationInterface::AssembleMotionFlags(false,true,false),Objects::CursorMode::AttackMode);
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
			cb.parameters.First() = baseName;
			cb.parameters.last() = turretName;
		}
		else
		{
			cb.parameters.SetSize(1);
			cb.parameters.First() = baseName;
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
		cb.parameters.First() = newTurret->constName;
		Network::Channels::constructionBegin.SendTo(Network::client,cb);
	}

	bool	HasSignalledRoundEnd()
	{
		return Network::newRoundRequested;
	}

	bool	CanChangeRoundState()
	{
		return Network::client.IsConnected() && RuntimeConfig::phase == RuntimeConfig::Phase::CommandPhase && !Network::amSpectator;
	}


	void	SignalRoundEnd()
	{
		if (Network::ControlEnabled())
		{
			Network::client.SendSignal(Protocol::ChannelID::RequestRoundEnd);
			Network::newRoundRequested = true;
			#ifdef HAS_WINDOW
				Objects::UpdateChoices(Network::ControlEnabled());
			#endif
			LogEvent(EventClass::Preset::common,nullptr,"Requested new round");
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
	ApplicationInterface::SignalRoundEnd();

}

void	InnerUndoTask(const Objects::PWorldEntity&e, bool force);

namespace ApplicationInterface
{

	void	SignalResearch(const Objects::PResearchTopic&topic)
	{
		#ifdef HAS_WINDOW
			Network::Channels::selectResearch.SendTo(Network::client,Protocol::SelectResearch((UINT32)topic->serialIndex));
			if (Objects::selectedEntity && Objects::selectedEntity->IsWorkingStructure() && !To<Objects::WorkingStructure>::Cast( Objects::selectedEntity)->IsEnabled())
			{
				Network::Channels::enableStructure.SendTo(Network::client,Protocol::EnableStructure(Objects::selectedEntity->id,true));
			}
		#endif
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


}





//float							eyeHeight = 1.7f;
		



								







/**
Retrieves the low resolution cell id of the cell under the current cursor location
*/
bool							GetLowCellUnderCursor(Grid::LCellIdI&result, bool includeWater, bool includeEntities);







#ifdef HAS_WINDOW

	bool IsValidFileChar(char c)
	{
		if (Template::isalnum(c))
			return true;
		switch (c)
		{
		case ' ':
		case '-':
			return true;
		}
		return false;
	}

	PathString MakeValidFilename(String base)
	{
		base.eraseCharacters(IsValidFileChar,false);
		return PathString(base);
	}

	void WriteLabel(StringFile&file, const Objects::BaseCommand::Label&label, const String&myTypeName)
	{
		foreach(label.rawLines, line)
		{
			if (line->first.length() == 0)
				continue;
			if (line->second)
				file << "*";
			String linkedLine = line->first;
			linkedLine.replace("Hrz. Attack range","[[concepts/attack range|Hrz. Attack range]]");
			linkedLine.replace("Base horizontal attack range","[[concepts/attack range|Base horizontal attack range]]");
			linkedLine.replace("Initiative","[[concepts/initiative|Initiative]]");
			linkedLine.replace("Weight","[[concepts/vehicle|Weight]]");
			linkedLine.replace("Carrying capacity","[[concepts/vehicle|Carrying capacity]]");
			linkedLine.replace("Base scanning range","[[concepts/sensory|Base scanning range]]");
			linkedLine.replace("Scanning range","[[concepts/sensory|Scanning range]]");
			linkedLine.replace("compound","[[concepts/compound]]");
			linkedLine.replace("phase","[[concepts/round]]");
			linkedLine.replace("scanning range","[[concepts/scanning range]]");

			
			
			foreach (Objects::structurePrototypes,s)
			{
				String name = (*s)->GetTypeName().copyToLowerCase();
				if (name.equalsIgnoreCase(myTypeName))
					continue;

				linkedLine.replace(name,"[[units/structures/"+name+"]]");
			}



			foreach(linkedLine, c)
			{
				if (Objects::IsTag(*c))
				{
					file << "%"<<Objects::GetTagName(*c)<<"%";
					continue;
				}
				file << *c;
			}
			file << nl;
		}
	}

	void WriteDocumentation(const FileSystem::Folder&rootFolder, const FileSystem::Folder&outFolder, StringFile&listFile, const String&typeName, const Objects::BaseCommand::Label&label, const String&pretext = "")
	{
		LogEvent(EventSignificance::Uncommon, "Documenting "+typeName);

		PathString name = MakeValidFilename( typeName );
		PathString outName = (name+".prefix").convertToLowerCase();
		StringFile file;
		if (!outFolder.IsValidLocation())
			FileSystem::CreateDirectory(outFolder.GetLocation());

		String relative = String(FileSystem::GetRelativePath(rootFolder.GetLocation(),outFolder.GetLocation()));
		relative.replace('\\','/');
		PathString outPath = outFolder.FindFile(outName,false)->GetLocation();
		listFile << "* [[units/"<<relative<<'/'<<typeName<<'|'<<typeName <<"]]"<< nl;
		file.Create(outPath);

		file << "{{ img:units/"<< String(name) <<".png|"<<typeName<<"}}"<<nl;
		file << "="<<typeName<<"="<<nl;
		file << pretext;
		WriteLabel(file,label,typeName);
	}

	void	FinishRendering(const FileSystem::Folder&folder, const DX11::Texture&texture, const String&typeName)
	{
		static Image image;
		display.Copy(texture.GetHandle2D(),image);
					
		image.ScaleHalf();
		image.ScaleHalf();
		image.TruncateToOpaque();
		PathString outName = MakeValidFilename(typeName)+".png";

		png.SaveToFile(image, folder.FindFile(outName,false)->GetLocation().c_str());
	}


	bool saveScreenshot = false;
	bool saveTransparentScreenshot = false;

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
		ProductionPipe<std::pair<Image,bool> >	workPipe;

		StorageThread()
		{}

		void ThreadMain() override
		{
			Array<std::shared_ptr<std::pair<Image,bool> > >	images;
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

					
				for (index_t i = 0; i < numImages; i++)
				{
					Image&image = images[i]->first;

					if (images[i]->second)
					{
						image.TruncateToOpaque();
	
						for (UINT32 x = 0; x < image.GetWidth(); x++)
							for (UINT32 y = 0; y < image.GetHeight(); y++)
							{
								BYTE*pixel = image.GetPixel(x,y);
								if (pixel[3] > 0 && pixel[3] < 255)
								{
									//#ifdef WHITE_STYLE
									//	TVec3<> intermediate;
									//	Vec::mult(Vec::ref3(pixel),1.f / 255.f,intermediate);
									//	float a = float(pixel[3])/255.f;
									//	Vec::addVal(intermediate,a - 1.f);
									//	Vec::div(intermediate,a);
									//	Vec::clamp(intermediate,0,1);
									//	Vec::mult(intermediate,255.f,Vec::ref3(pixel));
	
									//#else
										float scale = 1.f / (float(pixel[3])/255.f);
										Vec::mult(Vec::ref3(pixel),scale);
			//						#endif
								}
							}
					}
					else
						image.SetChannels(3);

					if (!TimeLapse::enabled)
					{
						PathString screenshotsAt = RuntimeConfig::ConfigFile("screenshots");
						FileSystem::CreateFolder(screenshotsAt);
						index_t counter = 0;
						PathString filename = screenshotsAt + FOLDER_SLASH_STR + PathString("screenshot"+String(counter)+".png");
						while (FileSystem::IsFile(filename))
						{
							counter++;
							filename = screenshotsAt + FOLDER_SLASH_STR + PathString("screenshot"+String(counter)+".png");
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
		void	Run(const std::shared_ptr<std::pair<Image, bool> >&image)
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


namespace LocalApplicationInterface
{
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
}


namespace ApplicationInterface
{
	void	OnVanish(const Objects::PWorldEntity&e, Objects::RemovalType t)
	{
		if (Network::amSpectator)
			return;
		if (t == Objects::RemovalType::UncontrolledExplosion && e->isOwnedByMe)
		{
			Protocol::Marker marker;
			static_assert(Objects::Marker::Type::EntityDeath == 0, "Please update marker logic");
			marker.id.value.Generate();
			FillMarker(marker, e, marker.data.Set0());
			LocalApplicationInterface::SignalMarkerCreation(marker);
			LogEvent(EventSignificance::Debug,e.get(),"Created friendly loss marker for "+e->GetName());
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
					LocalApplicationInterface::SignalMarkerCreation(marker);

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
			LocalApplicationInterface::SignalMarkerDeletion(m->id);
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
						LocalApplicationInterface::SignalMarkerDeletion(m.id);
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
		if (stateChangeOnly)
		{
			if (RuntimeConfig::phase == RuntimeConfig::Phase::AnimationPhase || !Animation::AllDone())
				return;
		}
		else
			Objects::CommandGroup::CollapseAll();
		Objects::Renderer::ClearSmokeTrailHighlighting();

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
				if (Objects::selectedMobileEntity && Objects::selectedMobileEntity->IsAboveGrid())
				{
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
					sum += t->job->resourceCostLeft;
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
				if (f->compound.allStructures.IsEmpty())
				{
					bool any = false;
					for (const auto& s : Objects::structures)
						if (s->owner == f)
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

				#ifndef NO_SATELLITES
				for (const auto&sat : Objects::satellites)
				{
					if (sat->owner == f && sat->RequiresFuel())
					{
						change.level[Objects::Resource::Fuel] -= sat->GetFuelConsumptionPerRound() + sat->GetExtraFuelDemand();
					}
				}
				#endif

				Objects::PWorkingStructure ws = To<Objects::WorkingStructure>::DynamicCast(Objects::selectedEntity);
				if (ws)
				{
					Objects::ResourceVector _change, storage, _max,zero;
					ws->GetResourceVector(_change, storage, _max);
					UI::Update(
						#ifndef NO_HIBERNATION
							Objects::selectedEntity->GetUplinkTraffic(), 
						#endif
						current, max, change,storage);
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
	#endif
}


#ifdef HAS_WINDOW
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


	Animation::Update();




	#ifdef HAS_WINDOW
		bool alwaysDay = RuntimeConfig::FromFile::alwaysDay && !Camera::autoRotation;

		if (!alwaysDay)
			Grid::PerFrameUpdateSunDirection();


		Camera::ProgressRotation();



		//Resolution windowSize = display.clientSize();
	
		//bool rebuildCamera = false;

		if (!GUI::IsModal() && !Camera::locked)
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


		if (alwaysDay)
			Grid::UpdateSunDirection(sunDirection);


		Objects::Renderer::AdvanceTurretAnimations();

		World::RecompileIfNecessary();
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


			Objects::Renderer::RenderTranslucent(false);

			if (Objects::resourceView)
				World::RenderResources(frustum);

			#ifndef NO_SATELLITES
			if (Camera::SkyGridIsVisible())
				Grid::visibleOrbitGrid.Render(frustum);
			#endif

			Objects::Renderer::RenderTranslucent(true);

			Objects::Renderer::RenderTranslucentBeams();

			World::RenderOverlay(frustum);

			World::RenderBuildBlocks();

			Explosions::Render(Camera::scene);






			Objects::Renderer::RenderProgressBars();

			if (Objects::showArrow && !GUI::IsModal())
			{
	//			display.SetDepthTest(false, false);
				bool includeWater = false;
				if (Objects::activeCursorMode == Objects::CursorMode::AttackMode)
					includeWater = true;
				else
					if (Objects::selectedVehicle /*&& Objects::selectedVehicle->base->moveOnWater*/)
						includeWater = true;
				Grid::LCellIdI loc;
				Objects::hasArrowCellTarget = GetLowCellUnderCursor(loc,includeWater, Objects::activeCursorMode == Objects::CursorMode::AttackMode);
				if (Objects::hasArrowCellTarget && loc != Objects::arrowCellTarget.cellLocation)
					Objects::arrowCellTarget = Objects::TrajectoryEndPoint(loc);
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
				Grid::UpdatePlanetRenderingParameters(Grid::ToRadius(choice->location.height + choice->visualVerticalOffset));
				Grid::low.RenderTransparentCell(choice->location.stackID);
			}
			if (choice != InvalidIndex)
			{
				const Objects::Choice&ch = Objects::movementChoices[choice];
				Grid::UpdatePlanetRenderingParameters(Grid::ToRadiusf(float(ch.location.height + ch.visualVerticalOffset) + 0.2f));
				Grid::low.RenderCell(ch.location.stackID);
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
				if (!Dialog::serverChoice || timer.CheckIfOutdatedAndUpdate(lastCheck,1.f))
					UpdateServerChoice();
			}


			if (UI::showControlsAndContacts && !saveScreenshot)
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
					std::shared_ptr<std::pair<Image,bool> > capture(new std::pair<Image,bool>());
					display.Capture(capture->first);
					capture->second = saveTransparentScreenshot;

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

			if (UI::showControlsAndContacts)
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
					textout << "Round Number: "<<RuntimeConfig::roundNumber.main<<nl;
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
				if (Camera::locked && !TimeLapse::enabled)
					textout << "Camera locked. Press F4 to unlock" << nl;
				#ifdef _DEBUG
					textout << "last remote message: "<<timer.GetSecondsSince(Network::client.GetLastPackageTime());
				#endif

			}
	

			if (ConfigurationMode::IsActive())
			{
				static const String line = "Interactive Configuration Mode";

				textout.pushState();
				textout.scale(2.0f,2.0f);
				textout.locate(0.95f - textout.scaledWidth(line),0.95f);
				textout << line;
				textout.popState();
			}


			DebugCheckVector(scene.location,-1000,1000);
		
		
			UI::Render();
			display.pick(Camera::hud);
			GUI::Render();

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
		GUI::OnWindowResize();
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

			#ifndef NO_SATELLITES
			if (Camera::SkyGridIsVisible())
			{
				index_t stackID = Grid::low.TestGridIntersection(b, d, Grid::visibleOrbitGrid.orbit, distance);
				if (stackID != InvalidIndex && Vec::dot(Grid::low.cells[stackID].centerVertex.xyz,d) < 0.f)
				{
					result = Grid::LCellIdI((UINT32)stackID,Grid::FromRadius(Grid::visibleOrbitGrid.orbit));
					return true;
				}
			}
			#endif

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

		if (GUI::HandleMouseDown() || UI::HandleClick())
			return;
		if (Rect<>(0.f,0.97f,0.1f,1.f).Contains(Engine::mouse.location.windowRelative))
		{
			LocalApplicationInterface::QueryNickname();
			return;
		}

		//int oldVehicleHeight = Grid::SeeThrough::GetFocusPoint().height;
		//if (Objects::selectedEntity)
		//	oldVehicleHeight = Objects::selectedEntity->location.height;

	


		if (Objects::selectedMobileEntity && Objects::CursorModeExpectsLeftClick())
		{
			Grid::LCellIdI	target;
			bool includeWater = true;

			if (GetLowCellUnderCursor(target,includeWater, Objects::activeCursorMode == Objects::CursorMode::AttackMode))
			{
				int h;
				if (!Objects::selectedEntity->CanSpawnInGrid() && Objects::selectedEntity->CanSpawnAboveGrid(h))
					target.height = h;

				if (Objects::activeCursorMode == Objects::CursorMode::PlaceStructureMode)
				{
					
					if (Objects::selectedMobileEntity->ConstructingHere(target))
					{
						LogEvent(EventClass::Preset::commandRejected,Objects::selectedMobileEntity.get(),"Can't build here, already trying to build something here");
					}
					else
						if (LocalApplicationInterface::DirectTo(Objects::selectedMobileEntity,target,0,Objects::CursorMode::Idle,false))
						{
							ApplicationInterface::SignalNewJob(Objects::selectedMobileEntity,ConstName::Job_Construct,Objects::activeStructureToPlace->GetCTypeName(),target);
							Objects::arrowFromCell = target;
						}
				}
				else
					ApplicationInterface::DirectTo(Objects::selectedMobileEntity,target,false,true);
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
						ApplicationInterface::DirectTo(Objects::selectedEntity,target,false,true);
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
					if (Objects::CommonCmd::IsModeActive(Objects::CursorMode::BuildBlockMode) || Objects::CommonCmd::IsModeActive(Objects::CursorMode::RemoveBlockMode))
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
								if (Objects::CommonCmd::IsModeActive(Objects::CursorMode::BuildBlockMode))
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
								if (Objects::CommonCmd::IsModeActive(Objects::CursorMode::RemoveBlockMode))
									activity = "remove";
								Grid::LCellIdI origin = Objects::selectedVehicle->GetLastPointOfOrigin();
								if (h < Grid::CellStack::BottomHeight)
								{
									LogEvent(EventSignificance::NonFatalError, Objects::selectedEntity.get(), "Cannot "+activity+" here: Cannot alter bottom-layer");
								}
								elif (abs(h - origin.height) > 1)
								{
									LogEvent(EventSignificance::NonFatalError, Objects::selectedEntity.get(), "Cannot "+activity+" here: Height difference too big: Selected="+String(h)+", vehicle at: "+String(origin.height));
								}
								elif(Objects::vehicleDistanceField.distance[stackID] > 3)
									LogEvent(EventSignificance::NonFatalError, Objects::selectedEntity.get(), "Cannot " + activity + " here: Too far away from vehicle");
								elif(Objects::CommonCmd::IsModeActive(Objects::CursorMode::BuildBlockMode) && h == origin.height && Objects::vehicleDistanceField.distance[stackID] <= 1)
									LogEvent(EventSignificance::NonFatalError, Objects::selectedEntity.get(), "Cannot " + activity + " here: Too close to vehicle");
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
		saveTransparentScreenshot = Engine::input.pressed[Key::Shift];
	}

	void ToggleCameraLock()
	{
		if (TimeLapse::enabled)
		{
			LogEvent(EventClass::Preset::error,"Cannot alter camera lock while time-lapse is active");
			return;
		}
		if (!Camera::locked)
		{
			Dialog::Confirm("Enabling camera lock will disallow all changes to the active perspective until unlocked."
							"Press F4 again to unlock. Commence camera lock?",[](){Camera::locked=true;},
							[]()->Dialog::CheckResult {if (!Network::client.IsConnected()) return Dialog::CheckResult::Abort; return Dialog::CheckResult::OkayContinue;});
		}
		else
			Camera::locked = false;
	}


	void ToggleTimeLapse()
	{
		if (TimeLapse::enabled)
		{
			TimeLapse::enabled = false;
			Camera::locked = false;
			GUI::SignalSituationChange();
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
					Camera::locked = true;
					TimeLapse::basePath = outDirName;
					FileSystem::CreateDirectory(outDirName);
					LogEvent(EventClass::Preset::uncommon,"Time lapse enabled. Writing to "+String(outDirName));
					Dialog::PermanentMessage("Time lapse recording is enabled. Camera is locked. Press F2 to end time lapse recording",[](){return TimeLapse::enabled ? Dialog::CheckResult::OkayContinue : Dialog::CheckResult::Abort;});
				},[](){return Dialog::CheckResult::OkayContinue;});
			}
		}
	}



	void ToggleSeeThrough()
	{
		bool enable = !Grid::SeeThrough::IsEnabled();
		Grid::SeeThrough::Update(enable, Grid::SeeThrough::LocateCamera());
		if (enable)
			Tutorial::ShowMessage("Clip Terrain");
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




void	InnerUndoTask(const Objects::PWorldEntity&e, bool force)
{
	if (!e)
		return;
	if (e->IsMobile())
	{
		#ifdef HAS_WINDOW
			if (Objects::CommonCmd::IsModeActive(Objects::CursorMode::BuildBlockMode) || Objects::CommonCmd::IsModeActive(Objects::CursorMode::RemoveBlockMode))
			{
				Objects::cellSelection.EraseLast();
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
		OnTutorialsChanged();
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
						if (possibleTarget && !possibleTarget->isOwnedByMe && Objects::selectedEntity->CanHit( Objects::TrajectoryEndPoint( possibleTarget->location )))
						{
							LocalApplicationInterface::DirectTo(Objects::selectedEntity,possibleTarget->location,
								LocalApplicationInterface::AssembleMotionFlags(false,true,!Engine::input.pressed[Key::Shift]),
								Objects::CursorMode::AttackMode);
							return;
						}
					}
					if (Objects::selectedEntity->IsTower())
					{
						if (GetLowCellUnderCursor(target,true/*Objects::selectedVehicle && Objects::selectedVehicle->base->moveOnWater*/,true))
						{
							LocalApplicationInterface::DirectTo(Objects::selectedEntity,target,
								LocalApplicationInterface::AssembleMotionFlags(false,true,!Engine::input.pressed[Key::Shift]),
								Objects::CursorMode::AttackMode);
							return;
						}
					}
				}
				else
				{
					if (Objects::selectedEntity->CastToProducer() && !Objects::selectedMobileEntity)
					{
						if (GetLowCellUnderCursor(target,true/*Objects::selectedVehicle && Objects::selectedVehicle->base->moveOnWater*/,false))
						{
							ApplicationInterface::DirectTo(Objects::selectedEntity,target,false,false);
							return;
						}
					}
				}
				
		
			}

			if (Objects::selectedMobileEntity 
				&& Objects::VehicleCanBeDirected()	//TODO: rename/relocate for mobile entities
			)
			{

				if (GetLowCellUnderCursor(target,true/*Objects::selectedVehicle && Objects::selectedVehicle->base->moveOnWater*/,true))
				{
					//Grid::LCellIdI	target((UINT32)cellID,Grid::FromRadius(height));
					int h;
					if (!Objects::selectedMobileEntity->CanSpawnInGrid() && Objects::selectedMobileEntity->CanSpawnAboveGrid(h))
						target.height = h;
					LocalApplicationInterface::DirectTo(Objects::selectedMobileEntity,target, LocalApplicationInterface::AssembleMotionFlags(false,false,!Engine::input.pressed[Key::Shift]),Objects::activeCursorMode);
				}
			}
		}

	}

	void	UndoTask()
	{
		if (Objects::DisableModes(Network::ControlEnabled()))
		{
			OnTutorialsChanged();
			return;
		}

		InnerUndoTask(Objects::selectedEntity,false);
	}

	void	QueryAndSendChatMessage()
	{
		if (GUI::IsModal())
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

	namespace LocalApplicationInterface
	{

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
	}

	void	AbortMode()
	{
		if (GUI::CloseAllModal())
		{
			LocalApplicationInterface::QueryNicknameIfEmpty();
			return;
		}
		if (Objects::DisableModes(Network::ControlEnabled()))
			return;

		if (!GUI::IsVisible(mainMenu))
		{
			GUI::Show(mainMenu);
		}
		else
			GUI::Hide(mainMenu);
		GUI::SignalSituationChange();
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

#endif


bool	CanHit(const Objects::PMobileEntity&m, const Grid::LCellIdI&loc)
{
	return m->CanHit(Objects::TrajectoryEndPoint( loc ));
//			if (source.ExtCanHit(source.owner, Objects::TrajectoryEndPoint( source.location,source.base->height),Objects::TrajectoryEndPoint( candidate.location,0 ),target))

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
						if (!other->isOwnedByMe && CanHit(v,other->location))
							return true;
				for (auto other : Objects::structures)
					if (!emp || !other->empDamageLevel)
						if (!other->isOwnedByMe && CanHit(v,other->location))
							return true;
			}
			else
				#ifndef NO_SATELLITES
				if (v->IsSatellite())
				{
					for (auto other : Objects::satellites)
						if (!other->isOwnedByMe && CanHit(v,other->location))
							if (!emp || !other->empDamageLevel)
								return true;
				}
				else
				#endif
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

		
	//Objects::PVehiclePlant vp = To<Objects::VehiclePlant>::DynamicCast(s);
	if (s->CastToProducer())
		return (s->AwaitsUserInteraction() || /*vp->queue.IsEmpty() || */
		(!ws->IsEnabled()
		&&
		ws->CouldEnable(true)
		)
		);
	Objects::PMine mine = To<Objects::Mine>::DynamicCast(s);
	if (mine)
		return !mine->IsEnabled() && mine->CouldEnable();

	auto lab = To<Objects::Laboratory>::DynamicCast(s);
	if (lab)
	{
		if ((lab->IsEnabled() || lab->CouldEnable()) && lab->owner->researchState.IsIdle())
			return true;
		return !lab->IsEnabled() && lab->CouldEnable();
	}

	#ifndef NO_SATELLITES
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
				if ((p->queue.IsEmpty() || !p->IsEnabled()) && p->CouldEnable(true))
					return true;
			}
		}
	#endif
	return false;

}

bool	IsIdle(const Objects::PWorldEntity&e)
{
	if (e->IsStructure())
		return IsIdle(To<Objects::Structure>::Cast(e));
	if (e->IsMobile())
		return IsIdle(To<Objects::MobileEntity>::Cast(e));
	return false;
}


namespace ApplicationInterface
{
	bool	ShouldSignalRoundEnd()
	{
		if (!CanChangeRoundState())
			return false;

		if (!Objects::Faction::mine || Objects::Faction::mine->requestedNewRound || Network::amSpectator)
			return false;
		#if 0
			count_t total=0,requested=0;
			Objects::ForeachFaction([&total,&requested](const Objects::PFaction&f)
			{
				total++;
				if ((f->requestedNewRound || !f->online) && f != Objects::Faction::mine)
					requested++;
			});

			if (requested+1 != total)
				return false;
		#endif /*0*/

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
}

#ifdef HAS_WINDOW
	template <typename T>
	void GetIterateableEntity(Objects::PWorldEntity&rs, index_t&i, const BasicBuffer<T>&list)
	{
		if (rs)
			return;
		if (i < list.Count())
		{
			rs = list[i];
			return;
		}
		i -= list.Count();
	}

	

	template <typename T>
	void FindIterateableEntity(index_t&offset,index_t&rs, const Objects::PWorldEntity&e, const BasicBuffer<T>&list)
	{
		if (rs != InvalidIndex)
			return;
		rs = list.GetIndexOf(e);
		if (rs == InvalidIndex)
			offset += list.Count();
		else
			rs += offset;
	}


	count_t		CountIterateableEntities()
	{
		return
			Objects::vehicles.Count() 
			#ifndef NO_SATELLITES
				+ Objects::satellites.Count() 
			#endif
			+ Objects::structures.Count();
	}

	Objects::PWorldEntity	GetIterateableEntity(index_t i)
	{
		Objects::PWorldEntity rs;
		GetIterateableEntity(rs,i,Objects::vehicles);
		#ifndef NO_SATELLITES
			GetIterateableEntity(rs,i,Objects::satellites);
		#endif
		GetIterateableEntity(rs,i,Objects::structures);
		return rs;
	}
	
	index_t	FindIterateableEntity(const Objects::PWorldEntity&e)
	{
		index_t rs = InvalidIndex, offset=0;
		FindIterateableEntity(offset,rs,e,Objects::vehicles);
		#ifndef NO_SATELLITES
			FindIterateableEntity(offset,rs,e,Objects::satellites);
		#endif
		FindIterateableEntity(offset,rs,e,Objects::structures);
		return rs;

	}

	void	SelectAndFocusNextIdle()
	{
		if (Dialog::ConfirmCurrent(true))
			return;

		const count_t total = CountIterateableEntities();
		if ((total == 0) || !Network::haveID)
			return;
		index_t from = FindIterateableEntity(Objects::selectedEntity);
		if (from == InvalidIndex)
			from = 0;

		index_t to = from;
		do
		{
			to ++;
			if (to >= total)
				to = 0;

			Objects::PWorldEntity v = GetIterateableEntity(to);
			if (v->isOwnedByMe && IsIdle(v))
			{
				Objects::Select(v,Network::ControlEnabled());
				Camera::TranslateTo(Objects::selectedEntity->system.w.xyz);
				return;
			}
		}
		while (to != from);

		UI::RefreshButtonState();	//just in case
	}




	
	void	SelectAndFocusPrev()
	{
		const count_t total = CountIterateableEntities();
		if ((total == 0) || !Network::haveID)
			return;
		index_t from = FindIterateableEntity(Objects::selectedEntity);
		if (from == InvalidIndex)
			from = 0;
			
		index_t to = from;
		do
		{
			if (to > 0)
			{
				to --;
			}
			else
				to = total-1;

			Objects::PWorldEntity v = GetIterateableEntity(to);

			if (v->owner == Objects::Faction::mine)
			{
				Objects::Select(v,Network::ControlEnabled());
				Camera::TranslateTo(Objects::selectedEntity->system.w.xyz);
				return;
			}
		}
		while (to != from);


	}

	void	SelectAndFocusNext()
	{
		const count_t total = CountIterateableEntities();
		if ((total == 0) || !Network::haveID)
			return;
		index_t from = FindIterateableEntity(Objects::selectedEntity);
		if (from == InvalidIndex)
			from = 0;

		index_t to = from;
		do
		{
			to ++;
			if (to >= total)
				to = 0;

			Objects::PWorldEntity v = GetIterateableEntity(to);

			if (v->owner == Objects::Faction::mine)
			{
				Objects::Select(v,Network::ControlEnabled());
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
					GUI::Hide(Dialog::loginMenu);
				},[supportFactionFlag]()
				{
					if (!Network::amOnline || Network::haveFaction || !(supportFactionFlag & Network::gameFlags))
						return Dialog::CheckResult::Abort;
					if (RuntimeConfig::phase != RuntimeConfig::Phase::CommandPhase)
						return Dialog::CheckResult::OkayNotRightNow;
					return Dialog::CheckResult::OkayContinue;
				});
			}
			else
			{
				Network::AttemptFactionLogin(true,typeFlags);
				GUI::Hide(Dialog::loginMenu);
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

	void	OnWheel(short delta)
	{
		if (GUI::HandleMouseWheel(delta) == GUI::WheelRs::NotContained)
			Camera::Event::WheelAction(delta);

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
				throw Except::Program::ParameterFault(CLOCATION, "Unable to parse udpListen='" + value + "' to bool");
		}
		elif(key == "serverURL")
		{
			RuntimeConfig::serverAddress = Network::ServerAddress(value);
			LogEvent(EventClass::Preset::configuration, nullptr, "Server address set to "+RuntimeConfig::serverAddress.ToString());
		}
		elif(key == "documentHere")
		{
			FileSystem::Folder().FindFolder(PathString(value),RuntimeConfig::documentationRootFolder,false);
			if (!RuntimeConfig::documentationRootFolder.DoesExist())
				FileSystem::CreateDirectory(RuntimeConfig::documentationRootFolder);
		}
		else
			throw Except::Program::ParameterFault(CLOCATION, "`"+key+"` is not a recognized runtime parameter");
	}
	catch (const std::exception&e)
	{
		throw Except::Program::ParameterFault(CLOCATION, "While parsing `" + key + "`='" + value + "': "+String(e.what()));
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
			throw Except::Program::ParameterFault(CLOCATION, "Configuration syntax violation: Unable to parse parameter line '"+cmd+"' (missing closing quotes perhaps?)");
		}

		String parameterName;
		bool expectValue = false;
		Tokenizer::dequote(tokenList, cfg);
		foreach(tokenList, token)
		{
			if (*token == '=')
			{
				if (parameterName.IsEmpty())
					throw Except::Program::ParameterFault(CLOCATION, "Configuration syntax violation: Missing parameter name before '='");
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
					throw Except::Program::ParameterFault(CLOCATION, "Configuration syntax violation: Found multiple tokens ('" + parameterName + "' and '" + *token + "') without intermediate '='");
				parameterName = *token;
				//expectValue = true;
			}
		}
		if (expectValue)
		{
			throw Except::Program::ParameterFault(CLOCATION, "Configuration syntax violation: Missing parameter value for '" + parameterName + "'");
		}
		else
			if (parameterName.IsNotEmpty())
				throw Except::Program::ParameterFault(CLOCATION, "Configuration syntax violation: Missing '=' and parameter value for '" + parameterName + "'");
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


	Except::onFatal = MyFatal;
	RuntimeConfig::ClientInit();

	ASSERT__(FileSystem::CreateDirectory(RuntimeConfig::ConfigFile("logs")));
	Logging::Begin(RuntimeConfig::ConfigFile(PathString("logs/client "+Logging::TimeStamp(true)+".log")));

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

namespace StartConfig
{
	Configuration	sourceConfig,baseConfig,destConfig;
	bool			changed = false;
	PathString		configPath;

	void			Load(const PathString&userConfigPath)
	{
		configPath = userConfigPath;
		baseConfig.LoadFromFile("game.ini");
		if (!sourceConfig.LoadFromFile(userConfigPath))
		{
			LogEvent(EventClass::Preset::configuration,nullptr,"Unable to load user configuration from '"+String(userConfigPath)+"': '"+sourceConfig.Report()+"'. Attempting to load original config");
			sourceConfig = baseConfig;
		}
		destConfig = sourceConfig;
	}


	void Fix(const String&path, const String&fallback)
	{


		StringRef comment,def = fallback.ref();
		const Configuration::Attribute*attrib = baseConfig.GetAttrib(path);
		if (attrib)
		{
			comment = attrib->comment.ref();
			def = attrib->value.ref();
			LogEvent(EventClass::Preset::configurationError,"Config error: '"+path+"' not found. Defaulting to '"+(def)+"'");
		}
		else
			LogEvent(EventClass::Preset::configurationError,"Config error: '"+path+"' not found in actual or base configuration. Defaulting to '"+(def)+"'");
		destConfig.Define(path,def).comment = comment;
		changed = true;
	}

	const String&	GetString(const String&path, const String&fallback)
	{
		Configuration::Attribute*attrib = sourceConfig.GetAttrib(path);
		if (attrib)
			return attrib->value;
		Fix(path,fallback);
		return fallback;
	}

	
	bool	GetBool(const String&path, bool fallback = false)
	{
		Configuration::Attribute*attrib = sourceConfig.GetAttrib(path);
		bool rs;
		if (attrib && convert(attrib->value.c_str(),rs))
			return rs;
		Fix(path,fallback);
		return fallback;
	}
	
	int	GetInt(const String&path, int fallback = 0)
	{
		Configuration::Attribute*attrib = sourceConfig.GetAttrib(path);
		int rs;
		if (attrib && convert(attrib->value.c_str(),rs))
			return rs;
		Fix(path,fallback);
		return fallback;
	}
	
	float	GetFloat(const String&path, float fallback = 0)
	{
		Configuration::Attribute*attrib = sourceConfig.GetAttrib(path);
		float rs;
		if (attrib && convert(attrib->value.c_str(),rs))
			return rs;
		Fix(path,fallback);
		return fallback;
	}

	void	Finish()
	{
		if (changed)
		{
			LogEvent(EventClass::Preset::configuration,"Rewriting configuration ("+String(configPath)+")");
			destConfig.SaveToFile(configPath,PRODUCT_NAME " Client");
			changed = false;
		}
	}


}


int main( int argc, const char* argv[])
{
	Except::onFatal = MyFatal;
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

			PathString userConfigPath = RuntimeConfig::ConfigFile("game.ini");
			if (!FileSystem::IsFile(userConfigPath))
				FileSystem::CopyFile(PathString("./game.ini"),userConfigPath,false);
			StartConfig::Load(userConfigPath);

			#ifdef HAS_WINDOW
				String val = StartConfig::GetString("runtime/capFPS","-1");
				if (!val.equalsIgnoreCase("off"))
					convert(val.c_str(),RuntimeConfig::FromFile::capFPS);
				RuntimeConfig::FromFile::alwaysDay = StartConfig::GetBool("runtime/alwaysDay",true);
				display.config.vertical_sync = StartConfig::GetInt("runtime/vsync");
				display.config.vertical_sync = display.config.vertical_sync > 0 ? 1 : 0;
				display.config.fsaa_samples = StartConfig::GetInt("runtime/fsaa",4);

				UI::resourcesAsList = StartConfig::GetBool("runtime/resourcesAsList",false);

				TimeLapse::customBasePath = PathString(StartConfig::GetString("timelapse/outDirectory",""));
				TimeLapse::frameDelaySeconds = StartConfig::GetFloat("timelapse/frameDelaySeconds",0.2f);
				TimeLapse::maxCompressionPNG = StartConfig::GetBool("timelapse/maxCompressionPNG");

				if (!RuntimeConfig::aiMode && StartConfig::GetBool("runtime/soundEnabled",true))
				{
					AudioInterface::Create();
					AudioInterface::SetMasterVolume(StartConfig::GetFloat("runtime/soundVolume",1.f));
				}

			#endif

			StartConfig::Finish();
		}
		LoadHosts();


		applicationStart = timer.Now();




		Engine::GlobalAspectConfiguration::worldZIsUp = true;
		
		
		//display.create(Engine::DisplayConfig("TerraLab");

		#ifdef HAS_WINDOW
			storageThread.start();

			Engine::CreateDisplay(display,
				RuntimeConfig::ConfigFile("window"),
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

			//Engine::input.Bind(Key::Up, Camera::Event::ZoomIn);
			//Engine::input.Bind(Key::Down, Camera::Event::ZoomOut);

			Engine::input.Bind(Key::Left, SelectAndFocusPrev);
			Engine::input.Bind(Key::Right, SelectAndFocusNext);
			Engine::input.Bind(Key::Space, SelectAndFocusNextIdle);

			Engine::input.Bind(Key::G, Camera::ToggleOrbit);

			Engine::input.Bind(Key::PageDown, ViewDown);
			Engine::input.Bind(Key::PageUp, ViewUp);

			Engine::input.Bind(Key::Shift, ResumeBatchConstruction, AbortBatchConstruction);
			

		//	Engine::input.Bind(Key::PageDown, DeclineCamera);
			//Engine::input.Bind(Key::PageUp, InclineCamera);

			Engine::input.Bind(Key::Backspace, UndoTask);
			Engine::input.Bind(Key::Escape, AbortMode);
			Engine::input.Bind(Key::Enter, SignalRoundEndOrMaximize);
			Engine::input.Bind(Key::Return, SignalRoundEndOrMaximize);
			Engine::input.Bind(Key::Tab, UI::ShowSession);
			Engine::input.Bind(Key::T,[](){}, QueryAndSendChatMessage);
			Engine::input.Bind(Key::R,Camera::ToggleAutoRotation);
//			Engine::input.Bind(Key::R,ToggleResourceView);
			Engine::input.Bind(Key::Ctrl,ToggleSeeThrough);

			Engine::input.Bind(Key::H,ToggleControlGUI);

			Engine::input.BindCtrl(Key::C,ConfigurationMode::OnCopy);
			Engine::input.BindCtrl(Key::V,ConfigurationMode::OnPaste);

			Engine::input.Bind(Key::MouseButton0, Select, GUI::HandleMouseUp);
			Engine::input.BindCtrl(Key::MouseButton0, Select);
			Engine::input.Bind(Key::RightMouseButton, BeginCameraDrag,EndCameraDrag<true>);
			Engine::input.Bind(Key::MiddleMouseButton, BeginCameraDrag,EndCameraDrag<false>);

			Engine::input.Bind(Key::F4, ToggleCameraLock);
			Engine::input.Bind(Key::F3, SaveScreenshot);
			Engine::input.Bind(Key::F2,ToggleTimeLapse);

			#ifdef RAYTRACE
				Engine::input.Bind(Key::Up,ZoomIn);
				Engine::input.Bind(Key::Down,ZoomOut);
			#endif




			Engine::mouse.BindWheel(OnWheel);

			textout.setScale(1.f/display.clientWidth(),1.f/display.clientHeight());


			display.SetBackbufferClearColor(0,0,0,0);

			ProgressBar::Create();
			Dialog::Create();

		#endif


		
		Grid::Create0();
		Objects::Create();
		Grid::Create1();


		#ifdef HAS_WINDOW
			Camera::Setup(display.pixelAspect());	//creates a menu item, for which Objects::Create() must have been called


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
			mainMenu->AddCommand("Change Nickname",LocalApplicationInterface::QueryNickname);
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
				&& RuntimeConfig::phase == RuntimeConfig::Phase::CommandPhase;
			}
			);
			AddFactionCreationChoice("Spectate",Protocol::FactionType::SpectatorFaction, Protocol::GameFlag::SupportSpectatorFactions);
			AddFactionCreationChoice("Create new faction",Protocol::FactionType::RegularFaction, Protocol::GameFlag::SupportRegularFactions);

			


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
					LocalApplicationInterface::QueryNickname();
				}
			}


			if (RuntimeConfig::documentationRootFolder.DoesExist())
			{
				{
					FileSystem::Folder imageTarget = FileSystem::Folder(RuntimeConfig::documentationRootFolder).FindFolder("images/units",false);
					if (imageTarget.IsValidLocation())
					{
						LogEvent(EventSignificance::Uncommon, "Rendering type images to "+String(imageTarget.GetLocation())+ "...");
						Objects::GeometryImage::resultResolution = 1024;
						Objects::EntityCommand container;
						Image image;
						FileSystem::Folder folder(imageTarget);
						foreach (Objects::structurePrototypes,s)
						{
							LogEvent(EventSignificance::Uncommon, "Capturing "+(*s)->GetTypeName());
							Objects::EntityCommand::UpdateTexture(container,*s,true);
							FinishRendering(folder, container.texture, (*s)->GetTypeName());
						}

						foreach (Objects::Component::allTurretComponents,c)
						{
							LogEvent(EventSignificance::Uncommon, "Capturing "+(*c)->typeName);
							Objects::EntityConstructionCommand::UpdateTexture(container,nullptr,*c);
							FinishRendering(folder, container.texture, (*c)->typeName);
						}

						foreach (Objects::Component::baseComponents,c)
						{
							LogEvent(EventSignificance::Uncommon, "Capturing "+(*c)->typeName);
							Objects::EntityConstructionCommand::UpdateTexture(container,*c,nullptr);
							FinishRendering(folder, container.texture, (*c)->typeName);
						}
					}
				}
				{
					FileSystem::Folder documentationTarget = FileSystem::Folder(RuntimeConfig::documentationRootFolder).FindFolder("documentation",false);
					if (documentationTarget.IsValidLocation())
					{
						FileSystem::Folder rootFolder(documentationTarget.FindFolder("units",false));
						FileSystem::Folder structureFolder(rootFolder.FindFolder("structures",false));
						FileSystem::Folder turretFolder(rootFolder.FindFolder("turrets",false));
						FileSystem::Folder vbaseFolder(rootFolder.FindFolder("vbases",false));

						StringFile unitList;
						unitList.Create(rootFolder.FindFile("list.prefix",false)->GetLocation());

						unitList << "=Unit Types and Components="<<nl;

						unitList << "==Structures=="<<nl;
						foreach (Objects::structurePrototypes,s)
						{
							Objects::BaseCommand::Label label;
							(*s)->DescribeType(label,0xFFFFFFFF & ~Objects::WorldEntity::DescIncludeName & ~Objects::WorldEntity::DescIncludeCurrentHealth);
							String pretext;
							auto tb = (*s)->GetTurretBase();
							if (tb)
							{
								label.rawLines.Insert(0).first = "Based on [[units/turrets/"+String(MakeValidFilename(tb->turret->typeName))+"|"+tb->turret->typeName+"]] turret";
								label.rawLines.First().second = true;
							}
							WriteDocumentation(rootFolder, structureFolder,unitList, (*s)->GetTypeName(), label,pretext);
						}

						unitList << "==Turrets=="<<nl;
						foreach (Objects::Component::allTurretComponents,c)
						{
							Objects::BaseCommand::Label label;
							(*c)->DescribeType(label,false,false,"",Objects::OwnerParameter());
							WriteDocumentation(rootFolder, turretFolder,unitList, (*c)->typeName, label);
						}

						unitList << "==Vehicle Bases=="<<nl;
						foreach (Objects::Component::baseComponents,c)
						{
							Objects::BaseCommand::Label label;
							(*c)->DescribeType(label,false,false,Objects::OwnerParameter());
							WriteDocumentation(rootFolder, vbaseFolder,unitList, (*c)->typeName, label);
						}
					}
				}

			}
			else
			{
				display.assign(loop);
				display.execute();
			}
		#else
			while (!application_shutting_down && loop())
			{};
				

		#endif




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
	catch (const Except::Exception&exception)
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




