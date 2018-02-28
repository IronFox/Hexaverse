// server.cpp : Defines the entry point for the console application.
//

#include "./components/stdafx.h"

#include <boot.h>
#include <grid/worldData.h>
#include <objects.h>

#include "../client/version.h"
#include <protocol.h>
#include <runtimeConfig.h>

//#include <third_party/bz2/bz2.h>
#include <io/string_file.h>
#include <io/file_system.h>
#include <io/riff_handler.h>
#include <io/config.h>
#include <globalEvents.h>
#include "components/tests.h"
#include "components/aiProcess.h"
#include "components/remoteControl.h"
#include "components/serverLogEvent.h"
#include "components/udpBroadcast.h"
#include "components/state.h"
#include "components/networkChannels.h"
#include "components/journalSaveGame.h"
#include "components/reflection.h"
#include <luaInterface.h>
#include "components/nexusThreatHandling.h"

UINT32 ApplicationInterface::globalVersionOfCurrentFile = 0;	//file version being loaded right now

volatile bool quitMainLoop = false;

TwisterRandom		random;


#ifdef _DEBUG
	bool	debugCheckProgress = false;
#endif


namespace ApplicationInterface
{

	void	SignalOperationLacksResources(const Objects::WorldEntity&e, const Ctr::ArrayRef<Objects::Resource>&resources, float atTime)
	{
		Network::PClient*cl = Network::playerMap.QueryPointer(e.owner);
		if (!cl && Network::spectators.IsEmpty())
			return;

		Protocol::OperationLacksResources outRes;
		outRes.resourceIndices.SetSize(resources.Count());
		for (index_t i = 0; i < resources.Count(); i++)
			outRes.resourceIndices[i] = (BYTE)resources[i];
		outRes.timestamp.atTime = atTime;
		outRes.timestamp.rightNow = RuntimeConfig::IsInAnyCommandPhase();
		outRes.entityID = e.id;

		Channels::operationLacksResources.SendTo((*cl)->peer.lock(),outRes);
		foreach (Network::spectators,s)
			Channels::operationLacksResources.SendTo((*s)->peer.lock(),outRes);
	}


	void	SignalChainReactionExplosion(const Objects::PWorldEntity&causedBy, const Grid::LCellIdI&center, float radius, int damageLevel, float atTime)
	{
		Game::GetEvolver()->HandleChainReactionExplosion(Evolver::TExplosion(causedBy, center, radius, damageLevel, atTime ));
	}


	void	SignalNewMobileEntity(const Objects::PMobileEntity&_v, bool restoredFromStorage, float atTime)
	{
		const Objects::MobileEntity&v = *_v;



		Protocol::Timestamp timestamp(RuntimeConfig::IsInAnyCommandPhase(),atTime);
		//if (client)
		{
			if (!restoredFromStorage)
			{
				Protocol::NewMobileEntity	w;
				Game::Network::Describe(w,v,true,timestamp,0xFFFFFFFF);
				Network::SendToOwner(Channels::newMobileEntity,v,w);
	//			Channels::newMobileEntity.SendTo((client)->peer.lock(),w);
			}
			else
			{
				Protocol::MobileState state;
				Game::Network::Describe(state,(const Objects::MobileEntity&)v,true,timestamp,0xFFFFFFFF);
				Network::SendToOwner(Channels::setMobileState,v,state);
				//Channels::setMobileState.SendTo(client->peer.lock(),state);
			}
		}

		if (_v->IsGroundBoundVehicle() && (To<Objects::Vehicle>::Cast(_v)->turret->detectsResources))
		{
			Game::Network::UpdateResourceCoverage(_v->owner, _v->location,_v->GetScanningRange(),timestamp);
		}

		if (v.GetOperationSpeed(Objects::Component::Construct))
			Game::RegisterClassGain(_v, Objects::Faction::UnitClass::Terraformer, "terraformer");
	}

	void	SignalNewStructure(const Objects::PStructure&s, float atTime)
	{
		const Objects::Structure&v = *s;


		LogEvent(EventSignificance::Debug,s.get(),"Signaling new structure to owner at t="+String(atTime));
		Protocol::StructureConstruction	w;
		Protocol::Timestamp timestamp(RuntimeConfig::IsInAnyCommandPhase(),atTime);
		Game::Network::Describe(w,*s,true,timestamp);
		Network::SendToOwner(Channels::createStructure,v,w);

		if (s->IsTower() && To<Objects::Tower>::Cast(s)->turret->detectsResources)
		{
			Game::Network::UpdateResourceCoverage(s->owner, s->location,s->GetScanningRange(),timestamp);
		}
	}


	void	SignalOwnershipChange(const Objects::PWorldEntity&e,const Objects::PFaction&target, float t)
	{
		for (const auto&p : Network::activePlayersAndSpectators)
		{
			if (p->CanSee(e,t) || p->faction == target)
			{
				TCP::PPeer peer = p->peer.lock();
				DBG_ASSERT1__(peer,p->ConvertToString());
				if (peer)
				{
					LogEvent(EventSignificance::Common,*peer,e,"Reported seeing ownership change (-> '"+target->GetName()+"')");
					Channels::changeEntityOwner.SendTo(peer,Protocol::ChangeEntityOwner(e->id,target->id));
				}
			}
		}
	}


	void	SignalAppearanceToHostileFaction(const Objects::PFaction&target, const Objects::PWorldEntity&e, float atTime)
	{
		if (!e)
		{
			LogEvent(EventClass::Preset::error,nullptr, "Trying to show null");
			return;
		}
		Network::PClient client;
		if (!Network::playerMap.Query(target,client))
			return;
		//if (!client->ShowNew(e))
			//return;	//already knows this
		Protocol::Timestamp timestamp(RuntimeConfig::IsInAnyCommandPhase(),atTime);

		if (e->IsMobile())
		{
			Objects::PMobileEntity v = To<Objects::MobileEntity>::Cast(e);
		//	TCP::Serial<Protocol::VehicleConstruction>	w;
			TCP::Serial<Protocol::NewMobileEntity>	w;
			Game::Network::Describe(w.Ref(),*v,false,timestamp,0xFFFFFFFF);
			w.Seal();
			Channels::newMobileEntity.SendTo(client->peer.lock(),w);
		}
		else
		{
			Objects::PStructure s = To<Objects::Structure>::DynamicCast(e);
			ASSERT1__(s,e->GetName());
			TCP::Serial<Protocol::StructureConstruction>	w;
			Game::Network::Describe(w.Ref(),*s,false,timestamp);
			w.Seal();
			Channels::createStructure.SendTo(client->peer.lock(),w);
		}
		LogEvent(EventSignificance::Common,*client->peer.lock(),e,"Signaling appearance "+(timestamp.rightNow ? "(now)" : " at time="+String(atTime)));
	
	}


	/*
	Note: anyone that can see this entity must be told it is being removed. There will be no other notifications
	*/
	void	SignalRemoval(const Objects::PWorldEntity&e, Objects::RemovalType t, float atTime)
	{
		if (!e)
		{
			LogEvent(EventClass::Preset::error,nullptr, "Trying to remove null");
			return;
		}
		Protocol::DestroyEntity self,others;
		self.id = e->id;
		self.type = t;
		self.timestamp.atTime = atTime;
		self.timestamp.rightNow = RuntimeConfig::IsInAnyCommandPhase();
		others = self;
		bool sendToSelf = true;
		switch (t)
		{
			case Objects::RemovalType::DestroyedInStorage:
				others.type = Objects::RemovalType::Vanished;
			break;
			case Objects::RemovalType::EnteredStorage:
				sendToSelf = false;
				//bleed into:
			case Objects::RemovalType::BeingReplaced:
			case Objects::RemovalType::Vanished:
			case Objects::RemovalType::ChangingOwnership:
				others.type = Objects::RemovalType::Vanished;
			break;
			case Objects::RemovalType::ControlledExplosion:
			case Objects::RemovalType::UncontrolledExplosion:
				others.type = Objects::RemovalType::UncontrolledExplosion;
			break;
			default:
				FATAL__("Unrecognized removal type "+String(t.ToString()));
		}

		foreach (Network::activePlayersAndSpectators,cl)
		{
			TCP::PPeer peer = (*cl)->peer.lock();
			if (peer && Network::CanSee(*cl,e,atTime))
			{
				bool access = Network::CanSeeAsOwner(*cl,e);
				if (
					(access && !sendToSelf)
					)
				{
					LogEvent(EventSignificance::Debug,(*cl)->ConvertToString()+" reported NOT seeing removal of "+e->GetName()+" (t="+String(t.ToString())+", access="+String(access)+", sendToSelf="+String(sendToSelf)+") at time "+String(atTime) );
					continue;
				}
				const Protocol::DestroyEntity&send = access ? self : others;
				LogEvent(EventSignificance::Common,(*cl)->ConvertToString()+" reported seeing removal of "+e->GetName()+" at time "+String(atTime) );


				//(*cl)->Hide(e);
				Channels::destroyEntity.SendTo(peer,send);
			}
			else
				LogEvent(EventSignificance::Debug,(*cl)->ConvertToString()+" reported NOT seeing removal of "+e->GetName()+" (t="+String(t.ToString())+", access="+String(access)+", sendToSelf="+String(sendToSelf)+") at time "+String(atTime) +" - hasPeer="+String((bool)peer));
		}

	
	}

	void	SignalDisappearanceToHostileFaction(const Objects::PFaction&target, const Objects::PWorldEntity&e, float atTime)
	{
		if (!e)
		{
			LogEvent(EventClass::Preset::error,nullptr, "Trying to hide null");
			return;
		}
		Network::PClient client;
		if (!Network::playerMap.Query(target,client))
			return;
		Protocol::DestroyEntity de;
		de.id = e->id;
		de.type = Objects::RemovalType::Vanished;
		de.timestamp.atTime = atTime;
		de.timestamp.rightNow = RuntimeConfig::IsInAnyCommandPhase();

		//LogEvent(EventSignificance::Common,*client->peer.lock(),e,"Signaling disappearance");
		LogEvent(EventSignificance::Common,*client->peer.lock(),e,"Signaling disappearance "+(de.timestamp.rightNow ? "(now)" : " at time="+String(atTime)));

		Channels::destroyEntity.SendTo(client->peer.lock(),de);
	}




	void	SignalMotion(const Objects::PMobileEntity&v, const Grid::LCellIdI&from,const Grid::LCellIdI&to, float t0, float timeNeeded)
	{
		if (!v)
		{
			LogEvent(EventClass::Preset::commandRejected,nullptr, "Trying to move null");
			return;
		}
		Protocol::MobileEntityMotion motion;
		motion.id = v->id;
		motion.fromLocation = from;
		motion.toLocation = to;
		motion.timeNeeded = timeNeeded;
		motion.timestamp.rightNow = false;
		motion.timestamp.atTime = t0;

		if (v->IsGroundBoundVehicle() && To<Objects::Vehicle>::Cast(v)->turret->detectsResources)
		{
			Game::Network::UpdateResourceCoverage(v->owner,v->location,v->GetScanningRange(),motion.timestamp);
		}


		foreach (Network::activePlayersAndSpectators,cl)
		{
			TCP::PPeer peer = (*cl)->peer.lock();
			if (peer && Network::CanSee(*cl,v,t0))
			{
				LogEvent(EventSignificance::Common,(*cl)->ConvertToString()+" reported seeing motion of "+v->GetName()+" at time "+String(t0)+"+"+String(timeNeeded) );

				Channels::mobileEntityMotion.SendTo(peer,motion);
			}
		}
	}

	void	SignalTurretRotation(const Objects::PWorldEntity&e, const M::TVec3<>&toWorld, float rotationTime)
	{
		if (!e)
		{
			LogEvent(EventClass::Preset::commandRejected,nullptr, "Trying to let null attack");
			return;
		}
		Protocol::EntityTurretRotation rot;
		rot.id = e->id;
		rot.toWorld = toWorld;
		rot.rotationTime = rotationTime;
		rot.timestamp.rightNow = false;
		rot.timestamp.atTime = e->entityTime;

		foreach (Network::activePlayersAndSpectators,cl)
		{
			TCP::PPeer peer = (*cl)->peer.lock();
			if (peer && Network::CanSee(*cl,e,rotationTime))
				Channels::entityTurretRotation.SendTo(peer,rot);
		}
	}


	void	SignalNexusControlChange( const Objects::PAlliance&a )
	{
		if (RuntimeConfig::phase == RuntimeConfig::Phase::EndOfGame)
			return;
		{
			auto stats  = a->GetNexusStats();

			if (RuntimeConfig::gameGoal == RuntimeConfig::GameGoal::ConquerNexus && stats.nexusControl >= Game::Config::GetTargetNexusCount())
				Game::DeclareWinner(a);
			else
			{
				a->ForeachMember([&stats](const Objects::PFaction&f)
				{
					Network::PClient client;
					if (Network::playerMap.Query(f,client))
					{
						String why;
						if (Game::Config::basesRequired && f->deathInRounds > 0)
							f->CanLive(why);
						client->SendProgressUpdate(why,stats);
					}
				},false);
			}
		}
	}

	void	SignalFactionDied(const Objects::PFaction&f)
	{
		Channels::KillFaction.SendTo(Network::server,f->id,Channels::Level::Faction);
		Network::tokenMap.Unset(f->accessToken);
		Network::PClient client;
		if (Network::playerMap.Query(f,client))
		{
			auto peer = client->peer.lock();
			if (peer)
			{
				/*
				There are numerous issues with keeping dead factions alive, if member of an alliance:
				* Allied terrain sensor updates are sent only if terraforming operations have been performed
				* Allied units are still shown, and animated/updated, but dead client colors all terrain as fog of war
				*/
				LogEvent(EventSignificance::ClientFatal,peer,"Faction died. Disconnecting client");
				peer->DisconnectPeer();
			}
		}
	}


	void	SignalAttack(const TAttack&attack)
	{
		if (!attack.attackFrom)
		{
			LogEvent(EventClass::Preset::commandRejected,nullptr, "NULL attempting attack");
			return;
		}
		LogEvent(EventSignificance::Common,attack.attackFrom.get(),"Attacking "+attack.traceTo.ToString()+" in "+String(attack.flightTime)+" seconds");
		Protocol::PreciseAttackTrajectory preciseAttack;
		preciseAttack.attacker = attack.attackFrom->id;
		preciseAttack.traceTo = attack.traceTo;
		preciseAttack.isAutoAttack = attack.isAutoAttack;
		preciseAttack.attackAngle = attack.attackAngle;
		preciseAttack.timestamp.rightNow = false;
		preciseAttack.timestamp.atTime = attack.attackFrom->entityTime;
	
		LogEvent(EventSignificance::Debug,attack.attackFrom.get(),"Commencing attack at t="+String(preciseAttack.timestamp.atTime));


		preciseAttack.flightTime = attack.flightTime;

		for (auto cl : Network::activePlayersAndSpectators)
		{
			TCP::PPeer peer = (cl)->peer.lock();
			if (!peer)
				continue;
			if (Network::CanSee(cl,attack.attackFrom,preciseAttack.timestamp.atTime))
				Channels::preciseAttackTrajectory.SendTo(peer,preciseAttack);
		}
	}


	void	SignalExplosion(const TAttack&attack, const M::TVec3<>&center,float explosionRadius,bool isEMP, bool isDoomsday, float atTime)
	{
		Protocol::Explosion explosion;
		explosion.center = center;
		explosion.radius = explosionRadius;
		explosion.isEMP = isEMP;
		explosion.timestamp.atTime = atTime;
		explosion.timestamp.rightNow = false;

		//static GenericHashSet<Objects::OwnerParameter,Objects::OwnerHash>	map;
		static Ctr::Buffer0<Objects::OwnerParameter>	canSee;
		static Ctr::Buffer0<Network::PClient>	clientsCanSee;

		bool allSee = isDoomsday && RuntimeConfig::doomsdayWeapons == RuntimeConfig::DoomsdayWeapons::Expose;

		canSee.Clear();
		if (allSee)
		{
			//just set all. I'm lazy, and these are rare
			foreach (Objects::Faction::all, f)
				canSee.Append(*f);
			canSee.Append(Objects::Faction::nexus);
		}
		else
		{
			static Grid::HighSetup::TVolumeDescriptor volume;
			static Grid::HighSetup::TVolumeDescriptor::Operator op;
			M::Sphere<> explosionSphere(center,explosionRadius);
			Grid::high.GenerateVolume(explosionSphere,op,volume);

			Objects::ForeachFaction([explosionSphere,&atTime](const Objects::PFaction&f)
			{
				if (f->GridCanSee(volume,atTime) || f->CountOffGridCoverage(explosionSphere))
				{
					canSee.Append(f);
				};
			});
		}

		clientsCanSee.Clear();

		for (const Objects::PFaction&p : canSee)
		{
			Network::PClient client;
			if (Network::playerMap.Query(p,client))
				clientsCanSee << client;
		}
		clientsCanSee.AppendAll(Network::spectators);

		if (clientsCanSee.IsEmpty())
			return;
	


		Protocol::VagueAttackTrajectory	roughAttack;
		bool hasTrajectory = false;
		if (attack.attackFrom)
		{
			hasTrajectory = true;
			LogEvent(EventSignificance::Common,attack.attackFrom.get(),"Tracing rough trajectory to "+Vec::toString(center)+" in "+String(attack.flightTime)+" seconds");
			float zOffset;
			Objects::PTurretBase base = attack.attackFrom->GetTurretBase(zOffset);
			ASSERT__(base);
			roughAttack.turretType = base->turret->constName;
			roughAttack.traceTo = attack.traceTo;

			roughAttack.timestamp.rightNow = false;
			roughAttack.timestamp.atTime = atTime - attack.flightTime;
			roughAttack.flightTime = attack.flightTime;
			roughAttack.attackAngle = attack.attackAngle;
			roughAttack.turretRange = base->GetFlatAttackRange();
			roughAttack.explosionRadius = base->turret->GetExplosionRadius(attack.attackFrom->owner);

			const M::TVec3<>& traceToWorld = attack.traceTo;

			roughAttack.sourcePosition = attack.attackFrom->GetWorldPosition() + Grid::low.cells[attack.attackFrom->location.stackID].centerVertex.xyz * zOffset;
			//attack.timestamp.atTime = v->timeAt;
		


			const float range = roughAttack.turretRange;//base->GetFlatAttackRange();
			if (range > 0)
			{
				float radius = range * 0.2f;
				float3 mod = float3(random.NextFloat(-1,1),random.NextFloat(-1,1),random.NextFloat(-1,1)) * radius;
				count_t numIt = 0;
				while (!attack.attackFrom->CanHit(mod + roughAttack.sourcePosition,roughAttack.attackAngle)
						//Vec::quadraticDistance(mod + roughAttack.sourcePosition.value, traceToWorld) > sqr(range) 
					&& ++numIt < 1000)
					mod = float3(random.NextFloat(-1,1),random.NextFloat(-1,1),random.NextFloat(-1,1)) * radius;
				ASSERT1__(numIt < 1000,range);
				roughAttack.sourcePosition += mod;
			}
		}

		for (const Network::PClient&client : clientsCanSee)
		{
			TCP::PPeer peer = client->peer.lock();
			if (peer)
			{
				if (!client->CanSee(attack.attackFrom,roughAttack.timestamp.atTime) && hasTrajectory)
					Channels::vagueAttackTrajectory.SendTo(peer,roughAttack);
				Channels::explosion.SendTo(peer,explosion);
			}
		}
	}

}



void TerminateNow(bool isMain);


void MyFatal(const FatalDescriptor&descriptor)
{
	String message = descriptor.ToString();
	LogFile file("fatal.log",false);
	
	file.Log(message+"\n",true);
	file.Close();

	Logging::WriteEvent(EventClass::Preset::fatal,message,nullptr,nullptr, nullptr);

	Logging::CallBugReporter();			//must call before application_shutting_down, since is threaded ATM
	application_shutting_down = true;	//prevent certain operations in Close()
										
	//DisplayMessage(PRODUCT_NAME" Server Error",message,false);
	//ErrMessage("Hexaverse Server: "+message);
	std::cerr << message << std::endl;

	try
	{
		PathString out = RuntimeConfig::sharedConfig.Lookup("saves/crash.hsf");
		Logging::WriteEvent(EventClass::Preset::uncommon,"Trying to save game to "+String(out),nullptr,nullptr,nullptr);
		FileIO::Save(out,RuntimeConfig::Phase::CommandPhase);
		Logging::WriteEvent(EventClass::Preset::uncommon,"Success",nullptr,nullptr,nullptr);
	}
	catch (...)
	{
		Logging::WriteEvent(EventClass::Preset::error,"Failed to save to file",nullptr,nullptr,nullptr);
	}


	TerminateNow(true);

	//NetworkConnection::Close();

}


template <typename EnumerationType>
EnumerationType	GetType(Config::Context*context, const String&path)
{
	String value = context->GetString(path).Trim();
	if (value.IsEmpty())
	{
		FATAL__("Trying to extract grid content type from '"+context->name+"/"+path+"' but item is not defined");
	}

	EnumerationType rs;
	if (!rs.Decode(value.c_str()))
	{
		FATAL__("Trying to extract grid content type from '"+context->name+"/"+path+"' but '"+value+"' is not a valid type name");
	}
	return rs;
}



Sys::Semaphore mainThreadDone;


void TerminateNow(bool isMain)
{
	static Sys::Mutex mutex;
	mutex.lock();
		static volatile bool terminated = false;
		if (terminated)
		{
			mutex.release();
			return;
		}
		terminated = true;
		if (!isMain)
		{
			quitMainLoop = true;
			mainThreadDone.enter();	//wait for main loop to end
		}


		Logging::WriteEvent(EventSignificance::Uncommon,"Closing down",nullptr,nullptr,nullptr);

		GlobalEvents::applicationTerminate();

		Logging::WriteEvent(EventSignificance::Uncommon,"Terminating process",nullptr,nullptr,nullptr);
		Logging::End();

		application_shutting_down = true;

	mutex.release();
	TerminateProcess(GetCurrentProcess(),0);

	

}


BOOL WINAPI MyHandlerRoutine(_In_  DWORD dwCtrlType)
{
	switch (dwCtrlType)
	{
		case CTRL_C_EVENT:
		case CTRL_CLOSE_EVENT:
		case CTRL_SHUTDOWN_EVENT:
			LogEvent(EventSignificance::Uncommon,"Received close event #"+String(dwCtrlType)+". Terminating");
			TerminateNow(false);
		return TRUE;
	
	}
	return FALSE;
}



bool ApplyCoverageChange(const Objects::PFaction&faction)
{
	return World::Network::CheckCoverageSendTerrain(faction,TCP::PPeer());
}


namespace ParameterConfig
{
	UINT16 basePort = DEFAULT_SERVER_BASE_PORT;
	

	typedef std::function<void(const char*)> FOnParam;

	FOnParam	onParam;

	PathString	loadFile,customConfigPath,outConfigPath;
	bool	startOffline = false,
			startInTutorial = false,
			heartBeat = false;
	bool	customWorldID = false;
	bool	enableRemoteControl = true;
	const char*	programName="";
	TCP::Server::Protocol controlProtocol = TCP::Server::Protocol::IPv6;

	void	SetProgramName(const char*base)
	{
		const char*slashAt;
		while (slashAt = CharFunctions::strchr(base,FOLDER_SLASH))
		{
			base = slashAt+1;
		}
		programName = base;
	}

	void	PrintHelp()
	{
		std::cout << std::endl << "Usage: "<<programName<<" [options]"<<std::endl;
		std::cout << "Supported options:"<<std::endl;
		std::cout << "  --load/-l <filename>\tLoad savegame"<<std::endl;
		std::cout << "  --loadAuto/-a <0-9>\tLoad auto-savegame. number=0 => most recent"<<std::endl;
		std::cout << "  --offline/-o \tStart offline"<<std::endl;
		std::cout << "  --heartbeat/-b \tRequire localhost client heartbeat every 3 seconds or quit"<<std::endl;
		std::cout << "  --tutorial/-t \tStart in tutorial mode"<<std::endl;
		std::cout << "  --port/-p <1025-65533>\tUse a non-default base port ("<<DEFAULT_SERVER_BASE_PORT<<" is default). Three successive ports are used"<<std::endl;
		std::cout << "  --noRemote/-r \tDisable remote control service"<<std::endl;
		std::cout << "  --rv4\tStart remote control service using IPv4"<<std::endl;
		std::cout << "  --test \tRun self-tests, then quit"<<std::endl;
		std::cout << "  --config/-c <config path>\tLoad specified path instead of user configuration"<<std::endl;
		std::cout << "  --generate/-g <config path>\tGenerate specified configuration file, then quit. Executed once config is loaded"<<std::endl;
		std::cout << "  --version/-v \tShow version, then quit"<<std::endl;
		//std::cout << "  --world/-w [worldID] \tuse custom world ID, and start empty"<<std::endl;
	}



	void	Process( const char*arg )
	{
		LogEvent(EventSignificance::Debug,"ParameterConfig::Process: '"+String(arg)+"'");
		if (onParam)
		{
			onParam(arg);
			onParam = nullptr;
			return;
		}
		if (!strcmp(arg,"--test"))
			inTestMode = true;
		elif (!strcmp(arg,"--rv4"))
			controlProtocol = TCP::Server::Protocol::IPv4;
		elif (!strcmp(arg,"--version") || !strcmp(arg,"-v"))
		{
			std::cout << SERVER_NAME<<std::endl;
			exit(0);
		}
		elif (!strcmp(arg,"--port") || !strcmp(arg,"-p"))
			onParam = [](const char*arg)
			{
				if (!convert(arg,basePort))
				{
					LogEvent(EventClass::Preset::configurationError,"Unable to convert '"+String(arg)+"' to a valid port number");
				}
				else
				{
					Network::Config::sessionPort = basePort + PortOffset::Session;
					LogEvent(EventClass::Preset::configuration,"Base port redefined as "+String(basePort)+", session port set to "+String(Network::Config::sessionPort));
				}

			};
		elif (!strcmp(arg,"--offline") || !strcmp(arg,"-o"))
			startOffline = true;
		elif (!strcmp(arg,"--heartbeat") || !strcmp(arg,"-b"))
			heartBeat = true;
		elif (!strcmp(arg,"--tutorial") || !strcmp(arg,"-t"))
		{
			loadFile = "config/tutorial.hsf";
			startInTutorial = true;
		}
		elif (!strcmp(arg,"--loadAuto") || !strcmp(arg,"-a"))
			onParam = [](const char*arg)
			{
				index_t index;
				if (!convert(arg,index))
				{
					LogEvent(EventClass::Preset::configurationError,"Unable to convert '"+String(arg)+"' to a valid index");
					return;
				}
				loadFile = FileIO::GetAutoSavePath(index);
			};
		elif (!strcmp(arg,"--help") || !strcmp(arg,"-h"))
		{
			PrintHelp();
			exit(0);
		}
		elif (!strcmp(arg,"--world") || !strcmp(arg,"-w"))
			onParam = [](const char*arg)
			{
				Bin2Str::Convert(World::worldID.bytes,World::worldID.NumBytes,arg,strlen(arg));
				LogEvent(EventClass::Preset::configuration,"World ID set to "+World::worldID.ToFullString());
				customWorldID = true;
			};
		elif (!strcmp(arg,"-r") || !strcmp(arg,"--noRemote"))
			enableRemoteControl = false;
		elif (!strcmp(arg,"-c") || !strcmp(arg,"--config"))
		{
			onParam = [](const char*arg)
			{
				customConfigPath = arg;
				LogEvent(EventClass::Preset::configuration,"Loading configuration from custom path "+String(arg));
			};
		}
		elif (!strcmp(arg,"-g") || !strcmp(arg,"--generate"))
			onParam = [](const char*arg){ outConfigPath = arg;};
		elif (!strcmp(arg,"--load") || !strcmp(arg,"-l"))
			onParam = [](const char*arg)
			{
				loadFile = arg;
				LogEvent(EventClass::Preset::configuration,"Loading save game from "+String(arg));
			};
		else
		{
			PathString path = arg;
			if (path.EndsWithIgnoreCase(RuntimeConfig::savFileExtension) && FileSystem::IsFile(arg))
			{
				loadFile = arg;
				return;
			}
			LogEvent(EventClass::Preset::configurationError, "Unsupported runtime parameter '"+String(arg)+"'");
			PrintHelp();
		}
	}
}


void	StartUp()
{
	UDPBroadCast::Start();
	Network::Start(ParameterConfig::basePort);
}



class ServiceVar : public Reflection::Variable::Base
{
	typedef Reflection::Variable::Base	Super;
public:
	typedef std::function<bool()>	FCheckIsOnline;
	typedef std::function<void()>	FSet;

	FCheckIsOnline			checkIsOnline;
	FSet					setOnline,setOffline;

	/**/					ServiceVar(const String&name):Super(name,"ServiceOnline",Reflection::Variable::Persistence::Temporary,1,CLI::EraseProtection)
							{}
	/**/					ServiceVar(const String&name, const FCheckIsOnline&c, const FSet&offline, const FSet&online)
								:Super(name,"ServiceOnline",Reflection::Variable::Persistence::Temporary,1,CLI::EraseProtection),
								checkIsOnline(c),setOffline(offline),setOnline(online)
							{}

	String					ConvertToString() const override
	{
		return checkIsOnline();
	}


	bool					Set(const String&value) override
	{
		bool v;
		if (!convert(value.c_str(),value.GetLength(),v))
			return false;
		if (v == checkIsOnline())
			return true;
		if (v)
			setOnline();
		else
			setOffline();
		return true;
	}

};

bool	IsTargetWorkingDirectory(const PathString&dir)
{
	PathString test = dir + "/config/declaration.xml";
	bool rs = FileSystem::IsFile(test);
	LogEvent(EventSignificance::Debug,"Checking server target directory candidate '"+String(dir)+"' => IsFile('"+String(test)+"'). Result="+String(rs));


	return rs;
}


int main( int argc, const char* argv[])
{
	Except::onFatal = MyFatal;
	try
	{

		RuntimeConfig::myConfig.InitConfigDir("server");
		RuntimeConfig::sharedConfig.InitConfigDir("shared");
		ASSERT__(FileSystem::CreateDirectory(RuntimeConfig::myConfig.Lookup("logs")));


		TCP::onIgnoredException = [](const TCodeLocation&loc, const std::exception&ex)
		{
			LogEvent(EventSignificance::Debug,"TCP exception: "+String(loc)+": "+ex.what());
		};
		TCP::onIgnoredArbitraryException = [](const TCodeLocation&loc)
		{
			LogEvent(EventSignificance::Debug,"TCP exception (arbitrary): "+String(loc));
		};

		SetConsoleCtrlHandler(MyHandlerRoutine,TRUE);
		Sys::DisableConsoleQuickEdit();


		{
			wchar_t path[MAX_PATH];
			ASSERT__(GetModuleFileNameW(NULL,path,MAX_PATH));
			StringW dir = FileSystem::ExtractFileDir(StringW(path));
			while (dir.GetLength() > 1 && !IsTargetWorkingDirectory(dir))
			{
				dir = FileSystem::ExtractFileDir(dir);
				LogEvent(EventSignificance::Debug,"Walking up to "+String(dir.c_str()));
			}
			if (IsTargetWorkingDirectory(dir))
				FileSystem::SetWorkingDirectory(dir);
			else
				FATAL__("Something is wrong. Unable to locate server executable, or config/declaration.xml is missing.");
		}

		ParameterConfig::SetProgramName(argc > 0 ? argv[0] : "program");
		for (int i = 1; i < argc; i++)
			ParameterConfig::Process(argv[i]);

		Logging::Begin(RuntimeConfig::myConfig.Lookup(PathString("logs/server "+Logging::TimeStamp(true)+".log")));
		LogEvent(EventSignificance::Uncommon,nullptr,"Starting server v" PRODUCT_VERSION);


		Boot();

		Evolver::Create();
		Game::SetDefaultEvolver();
		Journal::Create();

		World::worldID.Generate();


		Network::serverID.Generate();
		UDPBroadCast::Create(ParameterConfig::basePort,Network::serverID);





		Reflection::Create();
		Reflection::state.PushFocus();
		Reflection::state.EnterNewFolder("/Server/Online");
		Reflection::state.SetVariable(CLI::PVariable(new ServiceVar("udpBroadcast",UDPBroadCast::IsStarted,UDPBroadCast::Stop,[](){UDPBroadCast::Start();})));
		Reflection::state.SetVariable(CLI::PVariable(new ServiceVar("info",[]()->bool{return Network::InformationProvider::IsOnline();},Network::InformationProvider::Stop,[](){Network::InformationProvider::Start(ParameterConfig::basePort);})));
		Reflection::state.SetVariable(CLI::PVariable(new ServiceVar("session",
			[]()->bool{return Network::SessionIsStarted();},
			[](){Network::SuspendSession();},
			[](){
				Network::RestartSession(false);
			}))
		);
		//Reflection::state.DefineCommand("closeSession",(CLI::Command::function0_t)[](){Network::CloseSession();},CLI::NoCompletion);	//why...?

		Reflection::state.PopFocus();

		Reflection::state.DefineCommand("/Server/describeStatus",(CLI::Command::function0_t)[]()
		{
			RemoteControl::RelayEvent(Network::DescribeStatus());
			Network::DescribeConfiguration([](const String&line){RemoteControl::RelayEvent(line);});
		},CLI::NoCompletion);

		Reflection::state.DefineCommand("/Server/shutdown",(CLI::Command::function0_t)[]()
		{
			quitMainLoop = true;
		})->SetUniqueHelp("Shuts down the service and terminates the application");



		Reflection::state.PushFocus();
			Reflection::state.EnterNewFolder("/IO");
			Reflection::state.DefineCommand("autoSaveNow",FileIO::AutoSave,CLI::NoCompletion);
			Reflection::state.DefineCommand("saveTo fileName",(CLI::Command::function1_t)[](const String&path0)
			{
				PathString saveDirPath = RuntimeConfig::sharedConfig.Lookup("saves");
				FileSystem::ForceCreateDirectory( saveDirPath );
				FileSystem::Directory saveDir = saveDirPath;
				FileSystem::File file;
				PathString path = PathString(path0);
				if (!path.EndsWithIgnoreCase(RuntimeConfig::savFileExtension))
					path += RuntimeConfig::savFileExtension;
				if (!saveDir.FindFile(path,file,false))
				{
					LogEvent(EventClass::Preset::remoteControlError,"Unable to save to '"+String(path)+"'. Invalid path");
					return;
				}
				FileIO::Save(file.GetLocation(),RuntimeConfig::phase);
			},CLI::FileCompletion);

			Reflection::state.DefineCommand("loadFrom fileName",(CLI::Command::function1_t)[](const String&path0)
			{
				PathString saveDirPath = RuntimeConfig::sharedConfig.Lookup("saves");
				FileSystem::Directory saveDir = saveDirPath;
				
				FileSystem::File file;
				PathString path = PathString(path0);
				if (!path.EndsWithIgnoreCase(RuntimeConfig::savFileExtension))
					path += RuntimeConfig::savFileExtension;
				if (!saveDir.FindFile(path,file,true))
				{
					LogEvent(EventClass::Preset::remoteControlError,"Unable to load '"+String(path)+"'. File does not exist");
					return;
				}
				FileIO::Load(file.GetLocation());
			},CLI::FileCompletion);

			Reflection::state.DefineCommand("loadAutoSave [index]",(CLI::Command::function1_t)[](const String&sindex)
			{
				index_t index;
				if (sindex.IsEmpty())
					index = 0;
				else
					if (!convert(sindex.c_str(),index))
					{
						LogEvent(EventClass::Preset::remoteControlError,"Unable to parse '"+sindex+"' to index");
						return;
					}
				FileIO::Load(FileIO::GetAutoSavePath(index));
			},CLI::FileCompletion);


			Reflection::state.DefineCommand("reloadResearchProgram",(CLI::Command::function0_t)[]()
			{
				Objects::ResearchState::LoadDefaultProgram();
				Protocol::GameConfig localhost,other;
				Game::Network::AssembleGameConfig(other,false);
				Game::Network::AssembleGameConfig(localhost,true);

				Network::server.VisitAllPeers([localhost,other](const TCP::PPeer&p)
				{
					if (p->IsLocalhost())
						Channels::GameConfig.SendTo(p,localhost);
					else
						Channels::GameConfig.SendTo(p,other);
				});


			})->SetUniqueHelp("Reloads research program from file and redeploys to clients");

		Reflection::state.PopFocus();


		Reflection::state.DefineCommand("/World/generate [planetType] [discovery]",(CLI::Command::function2_t)[](const String&planet, const String&discovery)
		{
			if (planet.IsNotEmpty())
			{
				if (!World::IsPlanet(planet))
				{
					LogEvent(EventClass::Preset::remoteControlError,"'"+planet+"' is not a valid planet type");
					return;
				}
			}

			if (planet.IsNotEmpty())
			{
				Reflection::Variable::planetType->Set(planet.Trim());
				Reflection::ReloadPlanet();
			}
			if (discovery.IsNotEmpty())
			{
				if (!World::Config::worldDiscovery.Decode(discovery.c_str()))
				{
					LogEvent(EventClass::Preset::remoteControlError,"'"+discovery+"' is not a valid planet discovery type. Check World/worldDiscovery for details");
				}
			}
			Network::BuildConfiguredWorld();

		},CLI::NoCompletion);

		Reflection::state.DefineCommand("/World/showPlanetTypes",(CLI::Command::function0_t)[]()
		{
			Ctr::Array<World::TPlanetIniFile>	planetIniFiles;
			World::GetAvailablePlanetIniFiles(planetIniFiles);

			CLI::println("-");
			foreach (planetIniFiles,f)
				CLI::println(f->name+(f->userDefined.GetLocation().IsNotEmpty()? ( f->factoryDefault.GetLocation().IsNotEmpty() ? " (user override)" :" (user)"): ""));
			CLI::println("-");
		},CLI::NoCompletion);

		
		#ifdef _DEBUG
		//	inTestMode = true;
		#endif

		if (!inTestMode)
		{
			Reflection::Load(ParameterConfig::customConfigPath);
			if (ParameterConfig::outConfigPath.IsNotEmpty())
			{
				Reflection::SaveRootConfigTo(ParameterConfig::outConfigPath);
				mainThreadDone.release();
				Journal::Destroy();
				TerminateNow(true);
				return 0;
			}
		}

		Objects::Faction::RecreateNexus();


		if (inTestMode)
		{
			std::cout << "Building initial test world..." << std::endl;

			World::BuildConfiguredWorld(random);


			std::cout << "Running tests..." << std::endl;
			{
				Test::Run(Test::AutoTargeting());
				Test::Run(Test::StringMethods());
				Test::Run(Test::Serialization());
				Test::Run(Test::CharacterEncoding());
				Test::Run(Test::ServerClient());
				Test::Run(Test::TerrainUpdates());
				Test::Run(Test::SharedContainer());
				Test::Run(Test::HighGridFamilies());
				Test::Run(Test::Bin2Str());
				Test::Run(Test::VehiclePlant());
				IF_SATELLITES(Test::Run(Test::SkyPort()));
				Test::Run(Test::StorageUnits());
			}
			std::cout << "Tests completed. Press enter to quit" << std::endl;
			char c;
			std::cin.getline(&c, 1);
			std::cout << "bye"<<std::endl;
			//hmm, program hangs here. let's self-terminate instead
			TerminateNow(true);

			return 0;
		}




		if (!ParameterConfig::customWorldID)
		{
			World::BuildConfiguredWorld(random);
		}





		if (false)
		{
			Logging::WriteEvent(EventSignificance::Uncommon,"Testing server.",nullptr,nullptr,nullptr);

			FileIO::Save(PathString("test0"+RuntimeConfig::savFileExtension),RuntimeConfig::phase);
			FileIO::Load(PathString("test0"+RuntimeConfig::savFileExtension));
			FileIO::Save(PathString("test1"+RuntimeConfig::savFileExtension),RuntimeConfig::phase);

			Riff::File a,b;
			ASSERT__(a.Open(PathString("test0"+RuntimeConfig::savFileExtension).c_str()));
			ASSERT__(b.Open(PathString("test1"+RuntimeConfig::savFileExtension).c_str()));
			Ctr::Buffer<Riff::TID>	stack;
			FileIO::Compare(a,b,stack);

		}


		if (ParameterConfig::loadFile.IsNotEmpty())
		{
			PathString filename = ParameterConfig::loadFile;
			if (FileSystem::IsFile(filename))
			{
				FileIO::Load(filename);
			}
			else
				LogEvent(EventClass::Preset::fileError,"Specified file ("+String(filename)+") does not exist. Skipping loading operation.");
		}
		RuntimeConfig::tutorialMode = ParameterConfig::startInTutorial;


		if (ParameterConfig::heartBeat)
			Network::StartHeartbeat(ParameterConfig::basePort);

		if (!ParameterConfig::startOffline)
			StartUp();


		//Journal::globalWriter.InitNew(RuntimeConfig::Lookup("saves/log"));
		//Journal::globalWriter.ContinueExisting(RuntimeConfig::Lookup("saves/log"));

		LogEvent(EventSignificance::Uncommon, "Preparation of world '"+World::worldID.ToNiceString()+"'/" + ToString(World::worldID) + " completed.");
		
		Network::LogServerState();



		//FATAL__("test");
		if (Game::Config::Nexus::enableAI && Network::SessionIsStarted())
		{
			AIProcess::Begin(Objects::Faction::nexus->accessToken);
		}

		Game::ResetFactionStartMineralValue();


		if (ParameterConfig::enableRemoteControl)
			RemoteControl::Start(ParameterConfig::basePort,ParameterConfig::controlProtocol);


		while (!application_shutting_down && !quitMainLoop)
		{
			if (RuntimeConfig::IsInAnyCommandPhase() || RuntimeConfig::phase == RuntimeConfig::Phase::AnimationPhase)
				Sleep(1);
			Network::Tick();
			RemoteControl::Resolve();
			if (RuntimeConfig::IsInAnyCommandPhase())
				Game::Vote::CheckAllTimeouts();


			RuntimeConfig::Phase p0 = RuntimeConfig::phase;

			if (RuntimeConfig::IsInAnyCommandPhase())
			{
				bool newRoundNow = Game::endRoundNow;

				if (Game::Config::roundTimer > 0 && !Game::roundTimerPaused)
				{
					if (timer.GetSecondsSince(Game::lastPhaseChange) - 0.5f > Game::Config::roundTimer)//tolerance of a half second
					{
						newRoundNow = true;
						LogEvent(EventClass( EventSignificance::Uncommon, EventFlavor::Loss ),"Round timeout reached. Forcing new round.");
					}
				}





				if (!newRoundNow)
				{
					count_t numFactions=0, wantNewRound = 0;
					bool hasNexus = false,nexusWantsNewRound = false;
					foreach(Network::activePlayersAndSpectators, p)
					{
						if (!(*p)->HasLivingFaction()) //don't count towards new round
							continue;
						if ((*p)->faction == Objects::Faction::nexus)
						{
							ASSERT__(!hasNexus);
							hasNexus = true;
							if ((*p)->wantNewRound)
								nexusWantsNewRound = true;
							continue;
						}
						numFactions++;
						if ((*p)->wantNewRound)
							wantNewRound++;
					}

						newRoundNow = wantNewRound >= numFactions
									&& wantNewRound > 0
									&& (!hasNexus || nexusWantsNewRound);
					if (newRoundNow)
					{
						LogEvent(EventSignificance::Common,"All players have requested a new round. Commencing...");
					}
				}

				if (newRoundNow)
				{
					Game::endRoundNow = false;
					Objects::ForeachWorldEntityIncludingStored([](const Objects::PWorldEntity&e)
					{
						e->ReSeedRandom();
					},false);

					Nexus::ProcessCoverage();

					Journal::currentRound.roundNumber = RuntimeConfig::roundNumber;
					Journal::currentRound.evolver = Game::GetEvolver();
					Reflection::CheckAllSaveGameVariables();
					Journal::currentRound.AppendEntityVerification();

					//Journal::globalWriter.CommitAndClear(Journal::currentRound);
					Journal::currentRound.Clear();

					foreach (Network::activePlayersAndSpectators,p)
						(*p)->wantNewRound = false;

					Game::BeginRoundProcessing();
					Game::ProcessChanges(true);
				}

			}

			if (RuntimeConfig::phase == RuntimeConfig::Phase::AnimationPhase)
			{
				if (timer.GetSecondsSince(Game::lastPhaseChange) >= Game::effectiveAnimationLength+0.5f)
				{
					if (Network::AnyHumanPlayersActive())
						Game::roundTimerPaused = false;
					Game::FinishRoundProcessing();
					Objects::PAlliance a = Game::postEvolutionWinner.lock();
					if (a)
						Game::DeclareWinner(a);
					if (FileIO::postEvolutionAutoSave)
						FileIO::AutoSave();
					else
						FileIO::CheckAutoSave(false);	//round number is formally incremented at the beginning of the command phase, but animation phase produces a delay anyways, so better save here
				}
			}
			else if (!RuntimeConfig::IsInAnyCommandPhase() && !RuntimeConfig::GameHasEnded())
			{
				if (!Game::ContinueRoundProcessing())
				{
					float animationLength = Game::GetEvolver()->animationLength;
					Game::effectiveAnimationLength = std::min(10.f,animationLength);
					Game::Network::ChangePhase(RuntimeConfig::Phase::AnimationPhase,std::max(1.f,animationLength / 10.f));
				}
				Game::ProcessChanges(true);
			}



			Game::ProcessChanges(false);





			//#ifndef _DEBUG
			{
				static Ctr::Buffer0<TCP::PPeer>	disconnect;
				Network::server.VisitAllPeers([](const TCP::PPeer&peer)
				{
					if (timer.GetSecondsSince(peer->GetLastPackageTime()) > 30.f)
					{
						LogEvent( EventClass( EventSignificance::ClientFatal, EventFlavor::ServerPrivate ),*peer,"Client ping timeout (30 sec) exceeded. Disconnecting client");
						disconnect << peer;
					}
				});
				foreach (disconnect,d)
					(*d)->DisconnectPeer();
				disconnect.Clear();


				static Ctr::Buffer0<Network::PClient>	clientDump;
				for (const auto&p : Network::activePlayersAndSpectators)
				{
					if (p->peer.expired())
					{
						clientDump << p;
					}
				}

				for (const auto&p : clientDump)
				{
					LogEvent( EventClass( EventSignificance::ClientFatal, EventFlavor::ServerPrivate ),nullptr,"Peer lost: "+p->ConvertToString());
					Network::HandleConnectionLoss(p);
				}
				clientDump.Clear();
			}
			//#endif

			UDPBroadCast::Update();
			//break;
		}
		mainThreadDone.release();

		Journal::Destroy();

		TerminateNow(true);

	}
	catch (const std::exception&ex)
	{
		FATAL__(ex.what());
	}

	return 0;
}

