

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
		/**/		VisualEvent(const String&message, EventType type):message(message)
		{
			switch (type)
			{
				case EventType::CommandRejected:
				case EventType::InternalFault:
				case EventType::OperationAborted:
				case EventType::EntityProcessFailed:
				case EventType::GameProgressRelated:
					color = float3(1,0.5f,0.5f);
					age = -MaxAge;
				break;
				case EventType::ChatMessage:
					color = float3(0.9f,0.9f,1);
					scale = 1.5f;
					age = -MaxAge/2.f;
				break;
				case EventType::CompoundProcess:
					color = float3(0.5f,1,0.5f);
					age = -MaxAge/4.f;
				break;
				default:
					//color *= 0.8f;
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



void	OnTutorialsChanged()
{
	UI::HexGrid::RefreshTutorials();
}




void LogEvent(EventType type,  const Objects::WorldEntity*e, const String&_event);



bool CanEnableAtAll()
{
	return Network::client.IsConnected();

}



void	DirectTo(const Objects::PWorldEntity&v, const Grid::LCellIdI&target, bool leftClick, Objects::Vehicle::mode_t mode)
{
	const bool moveTo = mode == Objects::CursorMode::Idle && !leftClick;

	DBG_ASSERT__(!RuntimeConfig::aiMode || !Network::newRoundRequested);
	Protocol::EntityDirectCommand	cmd;
	cmd.id = v->id;
	cmd.mode = mode;
	cmd.target = target;
	if (moveTo && v->IsSatellite())
		cmd.target.height = v->location.height;
	cmd.leftClick = leftClick;
	Network::Channels::entityDirectCommand.SendTo(Network::client,cmd);

	#ifdef HAS_WINDOW
		Objects::DisableModes(Network::ControlEnabled());
		if (moveTo)
			Tutorial::ShowMessage("Move To");
	#endif

}

void	DirectTo(const Objects::PWorldEntity&v, const Grid::LCellIdI&target, bool leftClick)
{
	#ifdef HAS_WINDOW
		DirectTo(v,target,leftClick,Objects::activeCursorMode);
	#else
		DirectTo(v,target,leftClick,Objects::CursorMode::Idle);
	#endif
}


void	SignalChoiceIntersection(const Objects::PWorldEntity&e,const Grid::LCellIdI&c, Objects::Vehicle::mode_t mode, const Objects::PMobileEntity&deployingWhat)
{
	Protocol::ChoiceIntersection	ch;
	ch.id = e->id;
	ch.mode = mode;
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
	DirectTo(e,e->location,true,Objects::CursorMode::AttackMode);
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
	Network::Channels::automate.SendTo(Network::client,Protocol::Automate(e->id,doAutomate));
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
			if (!e || !e->isMine)
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
	return Network::client.IsConnected() && RuntimeConfig::phase == RuntimeConfig::Phase::CommandPhase;
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
		LogEvent(EventType::SessionRelated,nullptr,"Requested new round");
	}
}

void	SignalUndoRoundEnd()
{
	if (Network::client.IsConnected() && RuntimeConfig::phase == RuntimeConfig::Phase::CommandPhase && Network::newRoundRequested)
	{
		Network::client.SendSignal(Protocol::ChannelID::RequestContinueRound);
		Network::newRoundRequested = false;
		LogEvent(EventType::SessionRelated,nullptr,"Requested to continue new round");
	}

}

#ifdef HAS_WINDOW

	void VisualLogEvent( EventType type,  const Objects::WorldEntity*e, const String&_event)
	{
		String event = _event, outEvent = _event;
		if (e)
		{
			event = e->owner->GetName()+","+ e->GetNiceName()+": "+event;
			outEvent = e->owner->GetLogName()+","+ e->GetName()+": "+event;
		}
		eventLock.lock();
		if (type != EventType::Debug && type != EventType::PerformanceMeasurement && type != EventType::MinorEntityProcess)
		{
			#ifdef HAS_WINDOW
				if (type == EventType::ChatMessage)
					events << VisualEvent("",EventType::Debug);
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

void	SignalNewJob(const Objects::PMobileEntity&v, ConstName jobName, ConstName parameterName)
{
	#ifdef HAS_WINDOW
		if (jobName == ConstName::Job_Construct && parameterName != ConstName::Type_Connector)
		{
			Objects::NaturalResourceVector	res;
			if (res.SampleWorld(v->GetLastPointOfOrigin().stackID))
			{
				String name = Objects::GetPrototypeByName(parameterName)->GetTypeName();
				TUniqueID id = v->id;
				if (parameterName != ConstName::Type_Mine && res.IsNotEmpty())
				{
					Dialog::Confirm(v->GetNiceName()+" will construct a "+name+" on a location with known natural resources, which could be harvested by a Mine. Do you wish to continue building the "+name+"?",
						[id,jobName,parameterName]()
						{
							Protocol::StandardJob	job;
							job.id = id;
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
						[id,jobName,parameterName]()
						{
							Protocol::StandardJob	job;
							job.id = id;
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
	job.id = v->id;
	job.jobName = jobName;
	job.parameterName = parameterName;	
	Network::Channels::signalNewJob.SendTo(Network::client,job);

}

void	SignalTerrainModification(const Objects::PVehicle&v,Objects::Vehicle::mode_t activeCursorMode,const Array<Grid::HCellIdI>&selection)
{
	Protocol::TerrainMod	job;
	job.id = v->id;
	job.mode = activeCursorMode;
	job.cells = selection;
	Network::Channels::signalTerrainMod.SendTo(Network::client,job);
}

void	SignalDropLastTask(const Objects::PWorldEntity&e)
{
	#ifdef HAS_WINDOW
	if (e->IsMobile())
	{
		Objects::PMobileEntity v = To<Objects::MobileEntity>::Cast(e);
		if (v->constructionRoundCounter > 0 
			&& v->myTasks.Count() == 1
			&& v->myTasks.GetNewest().IsJob() 
			&& v->myTasks.GetNewest().job->mineralCostLeft > 0)
		{
			TUniqueID id = e->id;
			Dialog::Confirm("Are you sure you wish to clear the current production tasks? The invested resources will be lost.",[id]()
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
	Network::Channels::dropLastTask.SendTo(Network::client,e->id);
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
		&& v->myTasks.Peek().job->mineralCostLeft > 0)
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









#ifdef GAME_BLOB_SYSTEM


namespace Network
{
	bool blobSystemReady = false;
	static float metersPerLeafSector = 10;
	static const count_t rootSize = count_t(1) << 10;
	static const int maxSurfaceSize = (int)ceil(maxWorldSurfaceSize * 2 / metersPerLeafSector);

	Space SpaceOf(const int4&sid)
	{
		return Space((float3)sid.xyz * metersPerLeafSector, (float)sid.w * metersPerLeafSector);
	}

	int4 IDOf(const Space&space)
	{
		float4 raw(space.GetBase(), space.GetSize());
		raw /= metersPerLeafSector;
		return int4(
			(int)round(raw.x),
			(int)round(raw.y),
			(int)round(raw.z),
			(int)round(raw.w)
			);
	}


	bool GetHashValueTarget(const RevisionID&revisionID, Bit&result)
	{
		if (revisionID == activeRevision[renderSource])
		{
			result = renderSource;
			return true;
		}
		if (revisionID == activeRevision[constructionTarget])
		{
			result = constructionTarget;
			return true;
		}
		return false;
	}






	ZMQ::Socket		worldSocket;

	Buffer<zmq::pollitem_t>	pollItems;

	void Terminate()
	{
		worldSocket.close();
	}

	void	ReportException(const String&msg)
	{
		FATAL__(msg);
	}

	void	ReportFormatError(const String&context)
	{
		ReportException("Format error: "+context);
	}



	void	WriteCommand(instruction_t instruction, ZMQ::Socket&socket, bool more)
	{
		UINT32 p = (UINT32)instruction;
		socket.Write(p, more);
	}


	Timer::Time	lastRequest;

	void	Setup()
	{
		worldSocket.Init(ZMQ::context,zmq::socket_t::dealer, "WorldSocket");
		try
		{
			worldSocket.SetSendBufferSize(10000000);
			worldSocket.SetReceiveBufferSize(10000000);
		}
		catch (zmq::error_t&err)
		{
			FATAL__(err.what());
		}

		//cacheSocket.ForceConnect(ZMQ::TCPAddr("localhost", Port::CacheRequest));
//		worldDataSocket.ForceConnect(ZMQ::TCPAddr("localhost", Port::WorldRequest));
		worldSocket.ForceConnect(ZMQ::TCPAddr("localhost", Port::WorldLink));

		worldSocket.WriteSequence(__, (UINT32)Instruction::GetCurrentRevision);
		lastRequest = timer.Now();
	}

	//void	CacheRequest(const SurfaceRequest&req, instruction_t inst, float priority)
	//{
	//	ASSERT__(cacheSocket.WriteSequence(__, (UINT32)inst, req, priority));
	//}

	//void	UpdatePriority(const RevisionID&revisionID, const Space&space,const BlobHash&hash,float priority)
	//{
	//	SurfaceRequest req;
	//	req.space = space;
	//	req.blobDataHash = hash;
	//	req.revisionID = revisionID;
	//	CacheRequest(req,Instruction::UpdateConstructionPriority,priority);
	//}


	//void	RequestDataHash(const RevisionID&revID, const int4&sid)
	//{
	//	ASSERT__(located);
	//	ASSERT__(sid.w > 1);	//must be greater 1, because 1 ist the bottom level, and does not have data hashes - only data from parents
	//	//LogEvent("requesting ")

	//	ASSERT__(worldSocket.WriteSequence(__, (UINT32)Instruction::SectorDataHash, revID, SpaceOf(sid)));
	//}

	Timer::Time 				lastWorkResult = timer.Now();

	bool	CreateSurface(Sector&sector, Bit target)
	{
		DBG_ASSERT_GREATER__(sector.sectorID.w, 1);
		if (sector.state[target].CheckSaturation(sector.space.GetVolume()))
		{
			SurfaceData data;
			data.sourceDataHash = sector.state[target].GetBlobHash();
			sector.UpdateChildSurfaces(data, target);
			return false;
		}

		if (sector.state[!target].BlobMatch(sector.state[target].GetBlobHash()))
		{
			if (WorkerConnectionModule::WorkerExists(sector.state[!target].childDataRequested))
				return false;
		}
		ASSERT__(!sector.state[target].ChildrenHaveSurface());
		if (!WorkerConnectionModule::WorkerExists(sector.state[target].childDataRequested) || (timer.GetSecondsSince(lastWorkResult) > 2.f && timer.GetSecondsSince(sector.state[target].surfaceRequested) > 2.f))
		{
			sector.state[target].surfaceRequested = timer.Now();
			sector.state[target].childDataRequested =
				WorkerConnectionModule::SendDirectRequest(DirectRequest(sector.space, sector.state[target].GetBlobHash()),
				sector.state[target].containedBlobs, sector.GetChildPriority(target == renderSource));
			if (!Sector::childDataRequested.Contains(&sector))
				Sector::childDataRequested << &sector;
			return true;
		}
		return false;
	}

	//bool haltBlobRequests = false;

	Timer::Time		lastBlobCreation;
	bool			waitingForCreatedBlobs = false;

	bool	CreateBlobs(const WorldData::Blob*blob, count_t count)
	{
		if (constructionTarget != renderSource)
			return false;

		lastBlobCreation = timer.Now();
		waitingForCreatedBlobs = true;

		static Buffer<Sector*> affectedSectors;
		affectedSectors.Clear();
		constructionTarget = !constructionTarget;
		activeRevision[constructionTarget] = activeRevision[renderSource];

		rootSector.RecursivelyInsertNewBlobs(renderSource,constructionTarget, blob, count, affectedSectors);


		ASSERT__(located);
		worldSocket.WriteSequence(__, (UINT32)Instruction::CreateBlobs, activeRevision[constructionTarget], ZMQ::Reference(blob, count));
		LogEvent("Sent blob creation request for " + String(count) + " blob(s)");

		foreach(affectedSectors, sec)
			CreateSurface(**sec,constructionTarget);

		return true;
		//haltBlobRequests = true;
	}

	void	RemoveBlobs(const WorldData::Blob*blob, count_t count)
	{
		if (constructionTarget != renderSource)
			FATAL__("bad operation");

		lastBlobCreation = timer.Now();
		waitingForCreatedBlobs = true;

		static Buffer<Sector*> affectedSectors;
		affectedSectors.Clear();
		constructionTarget = !constructionTarget;
		activeRevision[constructionTarget] = activeRevision[renderSource];

		rootSector.RecursivelyRemoveBlobs(renderSource,constructionTarget, blob, count, affectedSectors);

		ASSERT__(located);
		worldSocket.WriteSequence(__, (UINT32)Instruction::RemoveBlobs, activeRevision[constructionTarget], ZMQ::Reference(blob, count));
		LogEvent("Sent blob removal request for " + String(count) + " blob(s)");

		foreach(affectedSectors, sec)
			CreateSurface(**sec,constructionTarget);
		//haltBlobRequests = true;
	}

	void	CreateNewWorld(const WorldData::Blob*blob, count_t count)
	{
		lastBlobCreation = timer.Now();
		waitingForCreatedBlobs = true;

		static Buffer<Sector*> affectedSectors;
		affectedSectors.Clear();
		constructionTarget = !constructionTarget;
		activeRevision[constructionTarget] = activeRevision[renderSource];

		rootSector.RecursivelyInsertNewBlobs(renderSource,constructionTarget, blob, count, affectedSectors);


		ASSERT__(located);
		worldSocket.WriteSequence(__, (UINT32)Instruction::CreateNewWorld, ZMQ::Reference(blob, count));
		LogEvent("Sent world creation request for " + String(count) + " blob(s)");

		foreach(affectedSectors, sec)
			CreateSurface(**sec,constructionTarget);
		//haltBlobRequests = true;
	}


	void	QueryDelta()
	{
		ASSERT__(located && blobSystemReady && !activeRevision[constructionTarget].IsEmpty());
		if (activeRevision[constructionTarget] != serverCurrenRevisionID)
			worldSocket.WriteSequence(__, (UINT32)Instruction::GetChangedSpace,
								activeRevision[constructionTarget],serverCurrenRevisionID);
	}


	void	_RequestData(const RevisionID&to, const int4&sectorID)
	{
		//ASSERT__(!haltBlobRequests);
		ASSERT__(!to.IsEmpty());
		try
		{
			worldSocket.WriteSequence(__, (UINT32)Instruction::SectorData, to, SpaceOf(sectorID));
		}
		catch (const zmq::error_t&err)
		{
			LogEvent(err.what());
			worldSocket.close();

			worldSocket.Init(ZMQ::context,zmq::socket_t::dealer, "WorldSocket2");
			try
			{
				worldSocket.SetSendBufferSize(10000000);
			}
			catch (zmq::error_t&err)
			{
				FATAL__(err.what());
			}

			//cacheSocket.ForceConnect(ZMQ::TCPAddr("localhost", Port::CacheRequest));
	//		worldDataSocket.ForceConnect(ZMQ::TCPAddr("localhost", Port::WorldRequest));
			worldSocket.ForceConnect(ZMQ::TCPAddr("localhost", Port::WorldLink));
			_RequestData(to,sectorID);
		}
	}

	void	RequestData(Sector&sector, Bit version)
	{
		if (sector.state[version].isShaded)
			return;
		_RequestData(activeRevision[version], sector.sectorID);
		sector.RecursivelyShadeChildren(version);
		sector.state[version].lastRequest = timer.Now();
		sector.state[version].hashRequested = true;
		//worldSocket.WriteSequence(__, (UINT32)Instruction::SectorDataHash, newRevisionID,sec->space);
		if (!Sector::childDataRequested.Contains(&sector))
			Sector::childDataRequested << &sector;
	}


	void	SignalNewServerRevision(bool wasDiverged)
	{
		if (serverCurrenRevisionID != activeRevision[constructionTarget] && !wasDiverged)
		{
			LogEvent("Received notification of new revision " + serverCurrenRevisionID.ToString(4));

			newRevisionReceived = timer.Now();
		}
	}

	void OnEvent(ZMQ::Socket&socket, instruction_t cmd)
	{
		//BlobRequest	blobRequest;
		SurfaceData data;
		Space space;
		BlobHash hash;

		switch (cmd)
		{
			case Instruction::DirectWorkResult:
				lastWorkResult = timer.Now();
				if (socket.ReadSequence(space,hash,data))
				//if (blobRequest.Read(socket, true) && data.Read(socket, false))
				{
					//#ifdef SURFACE_INCLUDES_SPACE
					//	DbgLog("received surface (" + data.ToString() + ") for sector " + data.space.ToString());
					//#else
					//	VerboseDbgLog("received surface (" + data.ToString() + ") as a response to " + blobRequest.ToString());
					//#endif
					int4 id = IDOf(space);
					Sector*sector = rootSector.Lookup(id);
					if (sector)
					{
						bool match[2] = { false, false };
						for (int i = 0; i < 2; i++)
							if (sector->state[i].BlobMatch(hash))
							{
								match[i] = true;
							}
						if (match[0] || match[1])
						{
							if (match[0])
								sector->UpdateChildSurfaces(data, 0);
							if (match[1])
								sector->UpdateChildSurfaces(data, 1);
						}
						else
							LogEvent("Bad surface data: Unknown source hash");
					}
					else
						ReportException("Unable to locate sector " + id.ToString() + "/" + space.ToString());
				}
				else
					ReportFormatError("WorkerSocket:WorkResult");
			break;
			default:
				ZMQ::DumpRemaining(socket);
				ReportFormatError("WorkerSocket:?");
		}	//end of switch
	}

	void	Process();
}


bool	SignificantPriorityChange(float from, float to)
{
	float change = (0.01f + from) / (0.01f + to);
	if (change < 1.f)
		change = 1.f / change;
	return change > 1.1f;
}


void UpdatePriority(Sector&sector, Bit target)
{
	Sector::State&state = sector.state[target];

	if (!state.ChildrenHaveSurface() && state.HasAnyBlobs())
	{
		float priority = sector.GetChildPriority(target == renderSource);
		if (!Network::CreateSurface(sector,target))
		{
			if (SignificantPriorityChange(state.lastRequestedPriority, priority))
			{
				float delta = fabs(priority - state.lastRequestedPriority);
				state.lastRequestedPriority = priority;
				WorkerConnectionModule::UpdateDirectPriority(state.childDataRequested, DirectRequest(sector.space, state.GetBlobHash()), priority);
				//Network::UpdatePriority(activeRevision[target], Network::SpaceOf(sector.sectorID), state.GetBlobHash(), priority);
			}
		}
	}
}

void CheckConstruction(Sector&sector, Bit target)
{
	Sector::State&state = sector.state[target];

	DBG_ASSERT__(state.HasBlobHash() || !state.ChildrenHaveSurface());











	if (!state.hashRequested)
	{
		Network::RequestData(sector,constructionTarget);
		state.hashRequested = true;
		//Network::RequestDataHash(activeRevision[target], sector.sectorID);
	}
	elif (state.HasAnyBlobs())
		UpdatePriority(sector, target);

}

#endif

bool saveScreenshot = false;


class StorageThread : public ThreadObject
{
public:
	Image		image;
	Semaphore	semaphore;
	volatile bool	active;

	StorageThread():active(false)
	{}

	void ThreadMain() override
	{
		while (!application_shutting_down)
		{
			semaphore.enter();
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
			image.setChannels(3);

			FileSystem::CreateFolder("screenshots");
			index_t counter = 0;
			PathString filename = PathString("screenshots/screenshot"+String(counter)+".png");
			while (FileSystem::IsFile(filename))
			{
				counter++;
				filename = PathString("screenshots/screenshot"+String(counter)+".png");
			}
			png.SaveToFile(image,filename);
			active = false;
			LogEvent(EventType::Notification, nullptr, "Screenshot written to "+String(filename));
		}
	}

	bool	CanRun() const {return !active;}
	void	Run()
	{
		if (active)
			return;
		active = true;
		semaphore.release();
	}
	void	Terminate()
	{
		ASSERT__(application_shutting_down);
		semaphore.release();
	}

}		storageThread;















#ifdef GAME_BLOB_SYSTEM


void	Network::Process()
{
	Timer::Time started = timer.Now();

	if (serverCurrenRevisionID != activeRevision[constructionTarget] && activeRevision[constructionTarget].IsNotEmpty())
	{
		if (constructionTarget == renderSource || timer.GetSecondsSince(newRevisionReceived) > 5.f)
		{
			newRevisionReceived = timer.Now();
			QueryDelta();
		}
		//newRevisionReceived = timer.Now();
	}



	bool hasEvents = true;
	while (hasEvents && timer.GetSecondsSince(started) < 0.1f)
	{
		if (WorkerConnectionModule::HasNewWorkers() || pollItems.IsEmpty())
		{
			pollItems << PollItem(worldSocket);
			WorkerConnectionModule::AppendNewWorkers(pollItems);
		}


		hasEvents = false;
		zmq_poll (pollItems.pointer(), (int)pollItems.Count(), 0);
		if (pollItems[0].revents & ZMQ_POLLIN)
		{
			//worldSocket
			hasEvents = true;
			UINT32 cmd;
			bool more;
			if (worldSocket.Read(ZMQ::Empty(), true) && worldSocket.Read(cmd,&more))
			{
				switch (cmd)
				{
					case Instruction::GetCurrentRevision:
					{
						//-> HelloWorld, RevisionID, int4, Space: world scope
						//Space		rootSpace;
						//int4		rootID;

						bool diverged = located && serverCurrenRevisionID != activeRevision[constructionTarget];
						if (more && 
							worldSocket.ReadSequence(serverCurrenRevisionID))
							//myCurrenRevisionID.Read(worldSocket,true) && ReadPOD(rootID,worldSocket,true) && rootSpace.Read(worldSocket,false))
						{
							if (!located)
							{

								//blob network:
								{
									//ASSERT__(worldSocket.WriteSequence(__, (UINT32)Instruction::SectorData, serverCurrenRevisionID, BlobNetwork::areaOfInterest));
								}


								located = true;




								//ShowMessage(myCurrenRevisionID.ToString());
								int4 rootID = int4(-(int)rootSize / 2, -(int)rootSize / 2, -(int)rootSize / 2, rootSize);
								Sector::childDataRequested.Clear();
								rootSector.Locate(rootID, SpaceOf(rootID));
								LogEvent("Located root to " + rootSector.sectorID.ToString() + " at revision " + serverCurrenRevisionID.ToString(4));

								
								CreateNewWorld(Grid::blobs.pointer(),Grid::blobs.Count());	//send blobs
								//activeRevision[constructionTarget] = serverCurrenRevisionID;
								//blobSystemReady = true;

							}
							else
								SignalNewServerRevision(diverged);
						}
						else
							ReportFormatError("WorldSocket:HelloWorld");
					}
					break;
					case Instruction::CreateNewWorld:
					case Instruction::CreateBlobs:
					case Instruction::RemoveBlobs:
					{
						bool diverged = serverCurrenRevisionID != activeRevision[constructionTarget];
						if (!more || !worldSocket.Read(serverCurrenRevisionID, false))
						{
							ReportFormatError("WorldSocket:HeardBeat");
						}
						else
						{
							SignalNewServerRevision(diverged);
						}

						blobSystemReady = true;
						activeRevision[constructionTarget] = serverCurrenRevisionID;
						//lastBeat = timer.Now();
					}
					break;
					case Instruction::SectorData:
					{
						Space space;
						Array <WorldData::Blob>	data;
						RevisionID	revisionID;
						//..., RevisionID, Space, THashValue
						if (more && worldSocket.ReadSequence(revisionID, space, data))
						{
							//if (!BlobNetwork::Init(revisionID,space,data))
							{
								std::sort(data.begin(), data.end());
								const int4 id = IDOf(space);
								Sector*sec = rootSector.Lookup(id);
								if (sec)
								{
									Bit target;
									if (GetHashValueTarget(revisionID, target))
									{
										static Buffer<Sector*>	affectedSectors;
										affectedSectors.Clear();
										sec->RecursivelyFillBlobs(target,data,affectedSectors);
										LogEvent("Filled "+String(data.Count())+" blob(s) into sector "+id.ToString()+". "+String(affectedSectors.Count())+ " sector(s) reported changes");
										foreach(affectedSectors, sec_)
										{
											Sector&sec = (**sec_);
											CreateSurface(sec, target);
										}
									}
									else
										ReportException("Bad revision id " + revisionID.ToString());
								}
								else
									ReportException("Unable to locate sector " + id.ToString() + "/" + space.ToString());
							}
						}
						else
							ReportFormatError("WorldSocket:SectorData");
					}
					break;
					//case Instruction::SectorDataHash:
					//{
					//	Space space;
					//	BlobHash	dataHash;
					//	RevisionID	revisionID;
					//	//..., RevisionID, Space, THashValue
					//	if (more && worldSocket.ReadSequence(revisionID,space, dataHash))
					//	{
					//		const int4 id = IDOf(space);
					//		Sector*sec = rootSector.Lookup(id);
					//		if (sec)
					//		{
					//			Bit target;
					//			if (GetHashValueTarget(revisionID, target))
					//			{
					//				bool requiresSurface = sec->SetHash(dataHash, target);
					//				if (requiresSurface)
					//				{
					//					SurfaceRequest req;
					//					req.blobDataHash = dataHash;
					//					req.revisionID = activeRevision[target];
					//					req.space = SpaceOf(id);
					//					if (id == int4(-64, -64, -64, 64))
					//						LogEvent("Requesting surface of sector " + id.ToString() + " using data hash " + dataHash.ToString(4));
					//					float p = sec->GetChildPriority(target == renderSource);
					//					CacheRequest(req, Instruction::InitConstruction, p);
					//					if (!Sector::childDataRequested.Contains(sec))
					//						Sector::childDataRequested << sec;

					//					sec->state[target].childDataRequested = true;
					//					sec->state[target].lastRequestedPriority = p;
					//				}
					//				//else
					//				//sec->UpdateChildren(SurfaceData(dataHash),constructionTarget);
					//			}
					//		}
					//		else
					//			ReportException("Unable to locate sector "+id.ToString()+"/"+space.ToString());
					//	}
					//	else
					//		ReportFormatError("WorldSocket:SectorDataHash");
					//}
					//break;
					case Instruction::GetChangedSpace:
					{
						//->GetChangedSpace, RevisionID from, RevisionID to, bool success, Box<> space

						M::Box<> changedSpace;
						bool visualsAreUpToDate = renderSource == constructionTarget;
						RevisionID	from,to;
						bool success;
						if (more && worldSocket.ReadSequence(from,to,success, changedSpace))
						{
							if (success && to == serverCurrenRevisionID)
							{
								if (from == activeRevision[constructionTarget] || activeRevision[constructionTarget].IsEmpty())
								{
									activeRevision[constructionTarget] = from;
									if (changedSpace.IsValid())
									{
										LogEvent("Moved to new revision " + to.ToString(4) + " with changed space=" + changedSpace.ToString());
										constructionTarget = !constructionTarget;
										activeRevision[constructionTarget] = to;


										static Buffer<SectorPriorityPair>	affectedSectors;
										affectedSectors.Clear();
										rootSector.RecursivelyDuplicateOrInvalidate(changedSpace, renderSource, constructionTarget, affectedSectors);
										rootSector.RecursivelyClearShadows(constructionTarget);

										std::sort(affectedSectors.begin(), affectedSectors.end());
										LogEvent(String(affectedSectors.Count()) + " sector(s) reported affected");

										foreach(affectedSectors, sec_)
										{
											Sector*sec = sec_->sector;// rootSector.Lookup(sectorID->vector);
											if (sec->sectorID.w < 2)
												continue;	//dont ask for anything on these tinies. they are updated from parents
											//Bit target = sec->SetHash(sectorID->GetDataHash());

											if (Sector::BuildSurfaceForThisSize(sec->space.GetSize() / 2) && !sec->state[constructionTarget].isShaded)
											{
												RequestData(*sec,constructionTarget);

											}
											//sec->lastRequestedPriority = sec->GetPriority();
										}

									}
									else
										LogEvent("Moved to new trivial revision " + to.ToString(4));

									//else leave it where it is
									activeRevision[constructionTarget] = to;
								}
								else
								{
									//outdated request
									QueryDelta();	//retry
								}
							}
							else
							{
								LogEvent("Error: Server does not recognize path to revision " + to.ToString(4) + ". Clearing all data.");

								int4 rootID = int4(-(int)rootSize / 2, -(int)rootSize / 2, -(int)rootSize / 2, rootSize);
								Sector::childDataRequested.Clear();
								rootSector.Locate(rootID, SpaceOf(rootID));
								LogEvent("Located root to " + rootSector.sectorID.ToString() + " at revision " + serverCurrenRevisionID.ToString(4));
								activeRevision[constructionTarget] = to;

								//located = false;	//clear current state
								//MoveToCurrentRevision();	//bullcrap
							}
						}
						else
							ReportException("Unable to read revision update");
					}
					break;
					default:
						if (more)
							DumpRemaining(worldSocket);
						ReportException("Invalid instruction code encountered: "+String(cmd));
					break;
				}
			}
			else
				ReportFormatError("WorldSocket");
		}

		WorkerConnectionModule::Process(pollItems, OnEvent);

		//if (pollItems [2].revents & ZMQ_POLLIN)
		//{
		//	//cacheSocket
		//	hasEvents = true;
		//	BlobRequest	blobRequest;
		//	SurfaceData	surfaceData;
		//	if (cacheSocket.ReadSequence(ZMQ::empty, blobRequest, surfaceData))
		//		//ReadEmpty(cacheSocket) && sectorID.Read(cacheSocket,true) && surfaceData.Read(cacheSocket,false))
		//	{
		//		const int4 id = IDOf(blobRequest.space);
		//		Sector*sec = rootSector.Lookup(id);
		//		if (sec)
		//		{
		//			Bit target;
		//			if (GetHashValueTarget(blobRequest.revisionID, target))
		//			{
		//				if (!sec->UpdateChildSurfaces(surfaceData, target))
		//				{
		//					LogEvent("Sector " + id.ToString() + " rejected data with blob data hash " + surfaceData.sourceDataHash.ToString(4)+". Resetting");
		//				
		//					if (sec->state[target].hashRequested && sec->state[target].HasHash())
		//					{
		//						LogEvent("Hash was set to " + sec->state[target].GeBlobHash().ToString(4));
		//						sec->state[target].hashRequested = false;
		//						sec->state[target].ClearHash();
		//						RequestDataHash(activeRevision[target],id);
		//					}
		//				}
		//				else
		//				{
		//					//LogEvent("Sector filled: " + id.ToString() + " from blob data hash " + surfaceData.sourceDataHash.ToString(4));
		//					LogEvent("Sector filled: " + id.ToString() + " at priority "+String(sec->GetChildPriority(target==renderSource)) );
		//					Sector::childDataRequested.FindAndErase(sec);
		//				}
		//			}
		//			else
		//				LogEvent("Target revision ID no longer exists");
		//		}
		//		else
		//			ReportException("Unable to resolve sector "+id.ToString()+"/"+blobRequest.space.ToString());
		//	}
		//	else
		//		ReportFormatError("CacheSocket");
		//}


	}

	//#ifndef _DEBUG
	//	if (timer.GetSecondsSince(lastBeat) > 5.f)
	//		FATAL__("World server does not reply");
	//#endif

	static float timeSinceLastCheck = 0;
	timeSinceLastCheck += Engine::timing.delta;
	if (timeSinceLastCheck > 1)
	{
		timeSinceLastCheck = 0;

		worldSocket.WriteSequence(__, (UINT32)Instruction::GetCurrentRevision);
//			WriteCommand(Instruction::HeartBeat,worldSocket,false);
	}
}




#endif



class Component
{
public:
	Rect<>					location;

	virtual bool			HandleClick()	{return false;};
	virtual void			Render() = 0;

};

typedef std::shared_ptr<Component>	PComponent;
//
//class Label : public Component
//{
//public:
//	enum orientation_t
//	{
//		ExpandLeft, ExpandRight, ExpandCenter
//
//	};
//
//
//	String					text;
//	float4					color;
//	float					fontScale;
//	//orientation_t			orientation;
//
//
//	/**/					Label(float2 topLeft, orientation_t orientation, const String&msg, float fontScale, float4 color=float4(1)):text(msg),color(color),fontScale(fontScale)//,orientation(orientation)
//	{
//		
//		#ifdef HAS_WINDOW
//			textout.pushState();
//
//			textout.scale(fontScale,fontScale);
//			switch (orientation)
//			{
//				case ExpandRight:
//					location.x.min = topLeft.x;
//					location.y.max = topLeft.y;
//					location.x.max = location.x.min + textout.getScaledWidth(text);
//					location.y.min = location.y.max - textout.getScaledHeight();
//				break;
//				case ExpandLeft:
//					location.x.min = topLeft.x - textout.getScaledWidth(text);
//					location.y.max = topLeft.y;
//					location.x.max = topLeft.x;
//					location.y.min = location.y.max - textout.getScaledHeight();
//				break;
//				case ExpandCenter:
//				{
//					float w = textout.getScaledWidth(text);
//					location.x.min = topLeft.x - w/2.f;
//					location.y.max = topLeft.y;
//					location.x.max = topLeft.x + w/2.f;
//					location.y.min = location.y.max - textout.getScaledHeight();
//				}
//				break;
//			}
//			textout.popState();
//		#endif
//	}
//
//	virtual void			Render()
//	{
//		#ifdef HAS_WINDOW
//			textout.pushState();
//			textout.color(color);
//			textout.locate(location.x.min,location.y.min);
//			textout.scale(fontScale,fontScale);
//			textout << text;
//			textout.popState();
//		#endif
//	}
//
//};
//
//
//class Slider : public Component
//{
//public:
//	const int				min,max;
//	int						current;
//
//	/**/					Slider(const Rect<>&location, int min, int max):min(min),max(max),current(0)
//	{
//		this->location = location;
//	}
//
//
//	virtual bool			HandleClick()	{return false;};
//	virtual void			Render() override
//	{
//		#ifdef HAS_WINDOW
//			TVec2<> edges[6];
//			static const float chamfer = 0.04f;
//			float aspect = display.pixelAspect();
//			edges[0] = float2(location.x.min,location.y.max);
//			edges[1] = edges[0];
//			edges[0].x += chamfer;
//			edges[1].y -= chamfer * aspect;
//
//			edges[2] = float2(location.x.min,location.y.min);
//
//			edges[3] = float2(location.x.max,location.y.min);
//			edges[4] = edges[3];
//			edges[3].x -= chamfer;
//			edges[4].y += chamfer * aspect;
//
//			edges[5] = float2(location.x.max,location.y.max);	
//	
//			SurfaceRenderer::SetColor(float4());
//			SurfaceRenderer::RasterizeConvexPolygon(edges,ARRAYSIZE(edges));
//		#endif
//	}
//};


void FillMarker(Protocol::Marker&marker, const Objects::PWorldEntity&e, Protocol::MarkerReferenceEntity&outE)
{
	marker.location = e->location;
	outE.id = e->id;
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
	if (t == Objects::RemovalType::UncontrolledExplosion && e->isMine)
	{
		Protocol::Marker marker;
		static_assert(Objects::Marker::Type::EntityDeath == 0, "Please update marker logic");
		marker.id.value.Generate();
		FillMarker(marker, e, marker.data.Set0());
		SignalMarkerCreation(marker);
	}
	else
		if (t == Objects::RemovalType::Vanished && !e->isMine)
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
			}
		}
}

void	OnAppear(const Objects::PWorldEntity&e)
{
	if (!e->isMine)
	{
		Objects::Marker*m = Objects::Marker::FindByReference(e->id);
		if (m)
		{
			LogEvent(EventType::Debug,e.get(),"Deleting marker "+m->GetName()+" since original reappeared");
			SignalMarkerDeletion(m->id);
		}
	}
	else
	{
		if (To<Objects::Structure>::DynamicCast(e))
		{
			for (index_t i = 0; i < Objects::Marker::all.Count(); i++)
			{
				Objects::Marker&m = Objects::Marker::all[i];
				if (m.type == Objects::Marker::Type::EntityDeath && m.location == e->location)
				{
					LogEvent(EventType::Debug,e.get(),"Deleting marker "+m.GetName()+" since new structure constructed in its place");
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
				if (Objects::selectedEntity->isMine)
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

		#ifndef CONNECTOR_LESS
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
				UI::Update(Objects::selectedVehicle->GetUplinkTraffic(),  current,max,change);
			}
			else
				UI::Update(Objects::selectedVehicle->GetUplinkTraffic(), current,max);
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
					UI::Update(Objects::selectedEntity->GetUplinkTraffic(), n.total.storage, n.total.storageMax, n.total.supply - n.total.demand, storage/*,change, ws->IsEnabled()*/);
				}
				else
					UI::Update(Objects::selectedEntity->GetUplinkTraffic(), n.total.storage, n.total.storageMax, n.total.supply - n.total.demand);
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
				f = Network::myFaction;
			if (f)
			{
				if (f->compound.containedStructures.IsEmpty())
					Objects::RefreshCompoundCoverage(0);
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
					if (!v->GetConstructionSpeed())
						return;
					if (v->myTasks.IsEmpty() || !v->myTasks.Peek().job)
						return;
					change.level[Objects::Resource::Minerals] -= std::min(v->GetConstructionSpeed(), v->myTasks.Peek().job->mineralCostLeft);

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
			if (!Tutorial::movementTutorialIssued && Objects::selectedEntity && Objects::selectedEntity->isMine && Objects::selectedEntity->IsMobile() && timer.GetSecondsSince(Tutorial::lastDelayingEvent) > 3.f)
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
		Camera::ProgressRotation();



		//Resolution windowSize = display.clientSize();
	
		//bool rebuildCamera = false;

		if (!Dialog::IsModal())
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
			if (Engine::input.pressed[Key::A])
			{
				change.x --;
			}
			if (Engine::input.pressed[Key::D])
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
				Vec::setLen(change, Engine::timing.delta*100.f);
				Camera::Rotate(change);
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
							Grid::UpdatePlanetRenderingParameters(Grid::ToRadiusf(float(f->height)+0.1f));
							Grid::low.RenderCell(f->stackID);
						}
					}
				}
				display.SetDepthTest(true,true);
			}



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
				static const String line = "(C) Stefan Elsen; Open Beta; v" PRODUCT_VERSION "; Compiled "+String(__TIMESTAMP__);
				//textout.locate(0.f,1.f - textout.getScaledHeight());
				textout.pushState();
				textout.scale(0.8f,0.8f);
				textout.locate(1.f - textout.scaledWidth(line),textout.getScaledHeight());
				textout << line;
				textout.popState();

			}

			if (saveScreenshot)
			{
				saveScreenshot = false;
				//display.SetBackbufferClearColor(0,0,0,0);

				//Image image;
				if (storageThread.CanRun())
				{
					LogEvent(EventType::Notification, nullptr, "Writing screenshot to disk...");
					display.Capture(storageThread.image);

					storageThread.Run();
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
				textout << "Round Number: "<<RuntimeConfig::roundNumber<<nl;
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
				if (Network::haveId)
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
	LogEvent(EventType::Fatal, nullptr, message);
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
					//if (test)
						//return false;
					float3 p = b + d * distance;
					float height = p.Length();
					distance = std::numeric_limits<float>::max();
					index_t stackID = Grid::low.TestGridIntersection(b, d, height, distance);
					if (stackID != InvalidIndex)
					{
						result = Grid::LCellIdI((UINT32)stackID,Grid::FromRadius(height));
						return true;
					}
				}
			}
			return rs;
		}
		return false;
	}

#endif



#ifdef HAS_WINDOW
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
//	if (Engine::input.pressed[Key::Control])
	//LogEvent(nullptr, "Select()");

	//if (Objects::Network::Transfer::isActive)
	//{
	//	TransferPanel::HandleClick();
	//	return;
	//}
	//if (!Network::client.IsConnected())
	//{
	//	if (Engine::mouse.location.windowRelative.x < 0.1f || Engine::mouse.location.windowRelative.x > 0.9f)
	//		return;
	//	if (Network::client.IsAttemptingToConnect())
	//		return;
	//	if (RuntimeConfig::serverURL.IsNotEmpty())
	//		return;
	//	foreach (Network::servers,srv)
	//	{
	//		if (srv->rightVersion && srv->bottom < Engine::mouse.location.windowRelative.y && srv->top > Engine::mouse.location.windowRelative.y)
	//		{
	//			index_t best = 0;
	//			float bestPing = srv->urls.first().second;
	//			for (index_t i = 1; i < srv->urls.Count(); i++)
	//				if (srv->urls[i].second < bestPing)
	//				{
	//					bestPing = srv->urls[i].second;
	//					best = i;
	//				}
	//			srv->selected = true;

	//			Network::serverID = srv->id;
	//			Network::serverIDString = Network::serverID.ToNiceString();
	//			Network::worldID = srv->info.worldID;
	//			Network::worldIDString = Network::worldID.ToNiceString();
	//			Network::client.ConnectAsync(srv->urls[best].first + ":"+String(SERVICE_PORT));
	//			return;
	//		}

	//	}


	//	return;
	//}

	

	int oldVehicleHeight = Grid::SeeThrough::GetFocusPoint().height;
	if (Objects::selectedEntity)
		oldVehicleHeight = Objects::selectedEntity->location.height;

	


	//{
	//	if (CommandPanel::HandleClick())
	//	{
	//		Objects::RefreshCommands(Network::ControlEnabled());
	//		return;
	//	}
	//}



	if (Objects::selectedMobileEntity 
		&& Objects::VehicleCanBeLeftClickDirected()	//also works for mobiles in general. TODO: rename/relocate
		)
	{
		if (Objects::selectedMobileEntity)
		{
			Grid::LCellIdI	target;
			bool includeWater = false;
			if (Objects::activeCursorMode == Objects::CursorMode::AttackMode)
				includeWater = true;
			else
				//if (Objects::selectedVehicle->base->moveOnWater)
					includeWater = true;

			if (GetLowCellUnderCursor(target,includeWater, Objects::activeCursorMode == Objects::CursorMode::AttackMode))
			{
				DirectTo(Objects::selectedMobileEntity,target,true);
				return;
			}
		}
		else
		{
			FATAL__("not implemented for non-mobiles yet" );
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
					DirectTo(Objects::selectedEntity,target,true);
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
								LogEvent(EventType::EntityProcess, Objects::selectedEntity.get(), "Cannot "+activity+" here: Cannot alter bottom-layer");
							}
							elif (abs(h - origin.height) > 1)
							{
								LogEvent(EventType::EntityProcess, Objects::selectedEntity.get(), "Cannot "+activity+" here: Height difference too big: Selected="+String(h)+", vehicle at: "+String(origin.height));
							}
							elif(Objects::vehicleDistanceField.distance[stackID] > 3)
								LogEvent(EventType::EntityProcess, Objects::selectedEntity.get(), "Cannot " + activity + " here: Too far away from vehicle");
							elif(Objects::Component::Terraformer::Cmd::BuildBlockIsActive(Objects::selectedEntity) && h == origin.height && Objects::vehicleDistanceField.distance[stackID] <= 1)
								LogEvent(EventType::EntityProcess, Objects::selectedEntity.get(), "Cannot " + activity + " here: Too close to vehicle");
							else
							{
								Grid::HCellIdI	cell = Grid::HCellIdI(stackID, h);
								index_t current = Objects::cellSelection.indexOf(cell);
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

#endif

void SaveScreenshot()
{
	saveScreenshot =true;
	//display.SetBackbufferClearColor(1,1,1,0);


}
//
//void SelectNext()
//{
//	selectedFamily++;
//	selectedFamily %= BaseGrid::NumFamilies;
//}
//void SelectPrev()
//{
//	if (selectedFamily > 0)
//		selectedFamily--;
//	else
//		selectedFamily = BaseGrid::NumFamilies - 1;
//}

#ifdef HAS_WINDOW

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

//void LogEvent( EventType type,  const Objects::WorldEntity*e, const String&_event)
//{
//	String event = _event, outEvent = _event;
//	if (e)
//	{
//		event = e->owner->GetName()+","+ e->GetNiceName()+": "+event;
//		outEvent = e->owner->GetLogName()+","+ e->GetName()+": "+event;
//	}
//	eventLock.lock();
//	if (type != EventType::Debug && type != EventType::PerformanceMeasurement && type != EventType::MinorEntityProcess)
//	{
//		#ifdef HAS_WINDOW
//			if (type == EventType::ChatMessage)
//				events << VisualEvent("",EventType::Debug);
//			events << VisualEvent(event,type);
//		#else
//			std::cout << event << std::endl;
//		#endif
//	}
//	eventLog.Log(String(RuntimeConfig::roundNumber)+tab+ RuntimeConfig::phase.ToString()   + tab + outEvent,true);
//	eventLock.unlock();
//}


#ifdef HAS_WINDOW

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
						if (possibleTarget && !possibleTarget->isMine)
						{
							DirectTo(Objects::selectedEntity,possibleTarget->location,true,Objects::CursorMode::AttackMode);
							return;
						}
					}
					if (To<Objects::Tower>::DynamicCast(Objects::selectedEntity))
					{
						if (GetLowCellUnderCursor(target,true/*Objects::selectedVehicle && Objects::selectedVehicle->base->moveOnWater*/,false))
						{
							DirectTo(Objects::selectedEntity,target,true,Objects::CursorMode::AttackMode);
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
						if (Objects::selectedEntity->CastToProducer())
						{
							if (GetLowCellUnderCursor(target,true/*Objects::selectedVehicle && Objects::selectedVehicle->base->moveOnWater*/,false))
							{
								DirectTo(Objects::selectedEntity,target,false);
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
					DirectTo(Objects::selectedMobileEntity,target,false);
				}
			}
		}

	}

	void	UndoTask()
	{
		if (Objects::selectedMobileEntity)
		{
			if (Objects::Component::Terraformer::Cmd::BuildBlockIsActive(Objects::selectedEntity))
			{
				Objects::buildCandidates.EraseLast();
				return;
			}
			if (Objects::Component::Terraformer::Cmd::RemoveBlockIsActive(Objects::selectedEntity))
			{
				Objects::removeCandidates.EraseLast();
				return;
			}
			if (Objects::selectedMobileEntity->HasTasks())
			{
				//SignalDropLastTask(v);
				//v->DropLastTask();
				Network::Channels::dropLastTask.SendTo(Network::client, Objects::selectedMobileEntity->id);
			}
		}

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
		if (v->IsHibernating() || v->IsSatellite())
			return false;
		if (v->myTasks.IsNotEmpty())
		{
			return false;
		}
		if (v->GetConstructionSpeed())
			return true;
		if (v->IsVehicle() && To<Objects::Vehicle>::Cast(v)->IsNonAutomatedResourceDetector())
			return true;
		if (v->CanAttack())
		{
			if (v->IsVehicle())
			{
				for (auto other : Objects::vehicles)
					if (!other->isMine && v->CanHit(other->location))
						return true;
			}
			else
				if (v->IsSatellite())
				{
					for (auto other : Objects::satellites)
						if (!other->isMine && v->CanHit(other->location))
							return true;
				}
				else
				{
					FATAL__("Unknown mobile entity type "+v->GetTypeName());
				}
		}
		return false;
	}

	bool	IsIdle(const Objects::PStructure&s)
	{
		#ifndef NO_HIBERNATION
			if (s->ToHibernating()->IsHibernating())
				return false;
		#endif
		Objects::PVehiclePlant vp = To<Objects::VehiclePlant>::DynamicCast(s);
		if (vp)
			return (vp->AwaitsUserInteraction() || /*vp->queue.IsEmpty() || */
			(!vp->IsEnabled()
			//&&
			//vp->CouldEnable()
			)
			);
		Objects::PMine mine = To<Objects::Mine>::DynamicCast(s);
		if (mine)
			return !mine->IsEnabled()/* && mine->CouldEnable()*/;

		return false;

	}
	

	void	SelectAndFocusNextIdle()
	{
		if (Dialog::ConfirmCurrent(true))
			return;

		const count_t total = Objects::vehicles.Count() + Objects::satellites.Count() + Objects::structures.Count();
		if ((total == 0) || !Network::haveId)
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
				if (v->isMine && IsIdle(v))
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
					if (v->isMine && IsIdle(v))
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

					if (s->isMine && IsIdle(s))
					{
						Objects::Select(s,Network::ControlEnabled());
						Camera::TranslateTo(Objects::selectedEntity->system.w.xyz);
						return;
					}
		
				}
		}
		while (to != from);
	}




	
	void	SelectAndFocusPrev()
	{
		const count_t total = Objects::vehicles.Count() + Objects::satellites.Count();
		if ((total == 0) || !Network::haveId)
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
				if (Objects::vehicles[to]->owner == Network::myFaction)
				{
					Objects::Select(Objects::vehicles[to],Network::ControlEnabled());
					Camera::TranslateTo(Objects::selectedEntity->system.w.xyz);
					return;
				}
			}
			else
				if (Objects::satellites[to - Objects::vehicles.Count()]->owner == Network::myFaction)
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
		if ((total == 0) || !Network::haveId)
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
				if (Objects::vehicles[to]->owner == Network::myFaction)
				{
					Objects::Select(Objects::vehicles[to],Network::ControlEnabled());
					Camera::TranslateTo(Objects::selectedEntity->system.w.xyz);
					return;
				}
			}
			else
				if (Objects::satellites[to - Objects::vehicles.Count()]->owner == Network::myFaction)
				{
					Objects::Select(Objects::satellites[to - Objects::vehicles.Count()],Network::ControlEnabled());
					Camera::TranslateTo(Objects::selectedEntity->system.w.xyz);
					return;
				}
		}
		while (to != from);
	}

#endif


#ifdef HAS_WINDOW
	void AddFactionCreationChoice(const String&line, UINT32 typeFlags, UINT32 supportFactionFlag)
	{
		Dialog::loginMenu->AddCommand(line,
		[typeFlags,supportFactionFlag]()
		{
			if (Network::HaveFactionKeyForCurrentWorld())
			{
				Dialog::Confirm("A faction key is known for this world ("+Network::worldIDString+"). If you create a new faction, this key will be lost, and control of your old faction will irreversible be abandoned. Please confirm.",
				[typeFlags](){
					Network::AttemptFactionLogin(true,typeFlags);
					Dialog::Hide(Dialog::loginMenu);
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
				Dialog::Hide(Dialog::loginMenu);
			}
		},[supportFactionFlag](){return Network::amOnline && !Network::haveFaction && (Network::gameFlags & supportFactionFlag) && RuntimeConfig::phase == RuntimeConfig::Phase::CommandPhase;},false);


	}

#endif


void ParseKeyValue(const String&key, const String&value)
{
	try
	{
		LogEvent(EventType::ConfigRelated, nullptr, "Setting `" + key + "` = '" + value + "'");
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
			RuntimeConfig::serverURL = value;
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
	if (RuntimeConfig::serverURL.IsNotEmpty())
	{
		Network::ServerDiscovery::RegisterNewHost(RuntimeConfig::serverURL, false,true);
	}


	onFatal = MyFatal;
	RuntimeConfig::ClientInit();

	ASSERT__(FileSystem::CreateDirectory(RuntimeConfig::ConfigFile("logs")));
	Logging::Begin(RuntimeConfig::ConfigFile(PathString("logs/client_"+Logging::TimeStamp(true)+".log")));

	//(PRODUCT_NAME".log",true,true)
	return true;
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
		LogEvent(EventType::SessionRelated,nullptr,"Starting client v" PRODUCT_VERSION);

	//	FileSystem::CreateDirectory(RuntimeConfig::ConfigDir());
		

		NiceName::Load();

		Network::LoadWorldAccessTokens();

		{
			Configuration	config;

			PathString userConfigPath = RuntimeConfig::ConfigFile("game.ini");
			if (!FileSystem::IsFile(userConfigPath))
				FileSystem::CopyFile(PathString("./game.ini"),userConfigPath,false);
			if (!config.LoadFromFile(userConfigPath))
			{
				LogEvent(EventType::ConfigRelated,nullptr,"Unable to load user configuration from '"+String(userConfigPath)+"': '"+config.Report()+"'. Attempting to load original config");
				config.LoadFromFile("game.ini");
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
			#endif
		}
		LoadHosts();


		applicationStart = timer.Now();


		//{
		//	StringFile	oreNames;
		//	oreNames.create("oreNames.txt");

		//	Random random;

		//	for (int i = 0; i < 100; i++)
		//		oreNames << GenerateOreName(random) << nl;

		//
		//}



#ifdef GAME_BLOB_SYSTEM

		WorkerConnectionModule::Start(ModuleLog);
		Network::Setup();
#endif /*0*/



		storageThread.start();

		Engine::GlobalAspectConfiguration::worldZIsUp = true;
		
		
		//display.create(Engine::DisplayConfig("TerraLab");

		#ifdef HAS_WINDOW
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
		#endif
	



	


		#ifdef HAS_WINDOW
			LineRenderer::Create();
			SurfaceRenderer::Create();

			Engine::input.bind(Key::Up, Camera::Event::ZoomIn);
			Engine::input.bind(Key::Down, Camera::Event::ZoomOut);

			Engine::input.bind(Key::Left, SelectAndFocusPrev);
			Engine::input.bind(Key::Right, SelectAndFocusNext);
			Engine::input.bind(Key::Space, SelectAndFocusNextIdle);

			Engine::input.bind(Key::PageDown, ViewDown);
			Engine::input.bind(Key::PageUp, ViewUp);
			

		//	Engine::input.bind(Key::PageDown, DeclineCamera);
			//Engine::input.bind(Key::PageUp, InclineCamera);

			Engine::input.bind(Key::Backspace, UndoTask);
			Engine::input.bind(Key::Escape, AbortMode);
			Engine::input.bind(Key::Enter, SignalRoundEndOrMaximize);
			Engine::input.bind(Key::Return, SignalRoundEndOrMaximize);
			Engine::input.bind(Key::Tab, UI::ShowSession);
			Engine::input.bind(Key::T,[](){}, QueryAndSendChatMessage);
			Engine::input.bind(Key::R,ToggleResourceView);

			Engine::input.bind(Key::MouseButton0, Select, Dialog::HandleMouseUp);
			Engine::input.bind(Key::RightMouseButton, BeginCameraDrag,EndCameraDrag<true>);
			Engine::input.bind(Key::MiddleMouseButton, BeginCameraDrag,EndCameraDrag<false>);

			Engine::input.bind(Key::F3, SaveScreenshot);

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


		
		Grid::Create0(Grid::TPlanetConfig(),Grid::WorldDiscovery());
		Objects::Create();
		Grid::Create1();

		Animation::Create();

		#ifdef HAS_WINDOW
			Explosions::Create();
		#endif


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
			mainMenu->AddCommand("Abandon faction",
				[]()
				{
					Dialog::Confirm("Your current faction will be abandoned. Its access key will be preserved until you create a new faction in this world. Please confirm.",
								Network::AbandonCurrentFaction,
								[](){return Network::client.IsConnected() && Network::haveFaction ? Dialog::CheckResult::OkayContinue : Dialog::CheckResult::Abort;});
				},
				[](){return Network::amOnline && Network::haveFaction;});
			mainMenu->AddCommand("Disconnect",
				[]()
				{
					Dialog::Confirm("You will be disconnected from the server and returned to the server selection screen.",
						[]()
						{ 
							Network::client.Disconnect();
						},
						[]()
						{
							return Network::client.IsConnected() ? Dialog::CheckResult::OkayContinue : Dialog::CheckResult::Abort;
						});
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
					QueryNickname();
				}
			}


			display.assign(loop);
			display.execute();
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
		#endif
		Grid::Destroy();
		Objects::Destroy();
		Animation::Destroy();

		application_shutting_down = true;


		storageThread.Terminate();

		#ifdef HAS_WINDOW
			display.destroy();
		#endif

		Logging::End();


		TerminateProcess(GetCurrentProcess(),0);	//self-terminate

		return 0;
	}
	catch (const Exception&exception)
	{
		FATAL__("Caught unhandled exception of type "+String(exception.getType())+":\n"+exception.what());
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




