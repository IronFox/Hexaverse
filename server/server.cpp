// server.cpp : Defines the entry point for the console application.
//

#include "./components/stdafx.h"

#include "../shared/grid.h"
#include "../shared/objects.h"
#include "../shared/sphereGrid.h"

#include "../client/version.h"
#include "../shared/protocol.h"
#include "../shared/runtimeConfig.h"

#include <remote/bz2/bz2.h>
#include <io/string_file.h>
#include <io/file_system.h>
#include <io/riff_handler.h>
#include <io/config.h>
#include "components/tests.h"
#include "components/aiProcess.h"
#include "components/remoteControl.h"
#include "components/serverLogEvent.h"
#include "components/udpBroadcast.h"
#include "components/state.h"
#include "components/networkChannels.h"
#include "components/journalSaveGame.h"
#include "components/reflection.h"
#include "components/luaInterface.h"
#include "components/configurationMode.h"

UINT32 globalVersionOfCurrentFile = 0;	//file version being loaded right now



Random		random;


#ifdef _DEBUG
	bool	debugCheckProgress = false;
#endif


void	SignalChainReactionExplosion(const Grid::LCellIdI&center, float radius, int damageLevel, float atTime)
{
	Game::evolver->HandleChainReactionExplosion(Evolver::TExplosion( center, radius, damageLevel, atTime ));
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

	if (_v->IsVehicle() && To<Objects::Vehicle>::Cast(_v)->turret->detectsResources)
	{
		Game::Network::UpdateResourceCoverage(_v->owner, _v->location,_v->GetScanningRange(),timestamp);
	}

	if (Game::Config::basesRequired && v.GetOperationSpeed(Objects::Component::Construct) && !v.owner->hasTerraformer)
	{
		LogEvent(EventSignificance::Debug,_v.get(),"Recognized as terraformer. Reevaluating survivial conditions");
		v.owner->hasTerraformer = true;
		String whyNot;
		bool canLive = v.owner->CanLive(whyNot);
		
		if (canLive)	//conditions changed
		{
			if (v.owner->deathInRounds > 0)
			{
				v.owner->deathInRounds = 0;
				Network::PClient client;
				if (Network::playerMap.query(v.owner,client))
					client->SendProgressUpdate(false,whyNot);
			}
		}
		else
			LogEvent(EventSignificance::Debug,_v.get(),"Nope: "+whyNot);
	}
}

void	SignalNewStructure(const Objects::PStructure&s, float atTime)
{
	const Objects::Structure&v = *s;

	//State::PClient*cl = State::playerMap.QueryPointer(v.owner);
	//if (!cl)
	//	return;

	LogEvent(EventSignificance::Common,s.get(),"Signalling new structure to owner at t="+String(atTime));
	Protocol::StructureConstruction	w;
	Protocol::Timestamp timestamp(RuntimeConfig::IsInAnyCommandPhase(),atTime);
	Game::Network::Describe(w,*s,true,timestamp);
	Network::SendToOwner(Channels::createStructure,v,w);


	//State::Describe(w2.Ref(),s,true,&parent);
	//w.Seal();
	//w2.Seal();

	//Channels::createStructure.SendTo((*cl)->peer.lock(),w);

	//SendToOwner(Channels::createStructure,v,w);

	//foreach (State::clients,cl)
	//{
	//	TCP::PPeer peer = (*cl)->peer.lock();
	//	if (peer && Receiver::CanSee(*cl,s))
	//		Channels::createStructure.SendTo(peer,Receiver::HasAccess(*cl,s) ? w2 : w);
	//}
}


void	SignalOwnershipChange(const Objects::PWorldEntity&e,const Objects::OwnerParameter&target, float t)
{
	for (const auto&p : Network::activePlayersAndSpectators)
	{
		if (p->CanSee(e,t) || p->faction == target)
		{
			TCP::PPeer peer = p->peer.lock();
			DBG_ASSERT1__(peer,p->ToString());
			if (peer)
			{
				LogEvent(EventSignificance::Common,*peer,e,"Reported seeing ownership change (-> '"+target->GetName()+"')");
				Channels::changeEntityOwner.SendTo(peer,Protocol::ChangeEntityOwner(e->id,target->id));
			}
		}
	}
}


void	SignalAppearanceToHostileFaction(const Objects::OwnerParameter&target, const Objects::PWorldEntity&e, float atTime)
{
	if (!e)
	{
		LogEvent(EventClass::Preset::error,nullptr, "Trying to show null");
		return;
	}
	Network::PClient client;
	if (!Network::playerMap.query(target,client))
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
	LogEvent(EventSignificance::Common,*client->peer.lock(),e,"Signalling appearance "+(timestamp.rightNow ? "(now)" : " at time="+String(atTime)));
	
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
				LogEvent(EventSignificance::Debug,(*cl)->ToString()+" reported NOT seeing removal of "+e->GetName()+" (t="+String(t.ToString())+", access="+String(access)+", sendToSelf="+String(sendToSelf)+") at time "+String(atTime) );
				continue;
			}
			const Protocol::DestroyEntity&send = access ? self : others;
			LogEvent(EventSignificance::Common,(*cl)->ToString()+" reported seeing removal of "+e->GetName()+" at time "+String(atTime) );


			//(*cl)->Hide(e);
			Channels::destroyEntity.SendTo(peer,send);
		}
		else
			LogEvent(EventSignificance::Debug,(*cl)->ToString()+" reported NOT seeing removal of "+e->GetName()+" (t="+String(t.ToString())+", access="+String(access)+", sendToSelf="+String(sendToSelf)+") at time "+String(atTime) +" - hasPeer="+String((bool)peer));
	}

	
}

void	SignalDisappearanceToHostileFaction(const Objects::OwnerParameter&target, const Objects::PWorldEntity&e, float atTime)
{
	if (!e)
	{
		LogEvent(EventClass::Preset::error,nullptr, "Trying to hide null");
		return;
	}
	Network::PClient client;
	if (!Network::playerMap.query(target,client))
		return;
	Protocol::DestroyEntity de;
	de.id = e->id;
	de.type = Objects::RemovalType::Vanished;
	de.timestamp.atTime = atTime;
	de.timestamp.rightNow = RuntimeConfig::IsInAnyCommandPhase();

	//LogEvent(EventSignificance::Common,*client->peer.lock(),e,"Signalling disappearance");
	LogEvent(EventSignificance::Common,*client->peer.lock(),e,"Signalling disappearance "+(de.timestamp.rightNow ? "(now)" : " at time="+String(atTime)));

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

	if (v->IsVehicle() && To<Objects::Vehicle>::Cast(v)->turret->detectsResources)
	{
		Game::Network::UpdateResourceCoverage(v->owner,v->location,v->GetScanningRange(),motion.timestamp);
	}


	foreach (Network::activePlayersAndSpectators,cl)
	{
		TCP::PPeer peer = (*cl)->peer.lock();
		if (peer && Network::CanSee(*cl,v,t0))
		{
			LogEvent(EventSignificance::Common,(*cl)->ToString()+" reported seeing motion of "+v->GetName()+" at time "+String(t0)+"+"+String(timeNeeded) );

			Channels::mobileEntityMotion.SendTo(peer,motion);
		}
	}
}

void	SignalTurretRotation(const Objects::PWorldEntity&e, const TVec3<>&toWorld, float rotationTime)
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
	rot.timestamp.atTime = e->motionTime;

	foreach (Network::activePlayersAndSpectators,cl)
	{
		TCP::PPeer peer = (*cl)->peer.lock();
		if (peer && Network::CanSee(*cl,e,rotationTime))
			Channels::entityTurretRotation.SendTo(peer,rot);
	}
}



void	SignalNexusControlChange(const Objects::PFaction&f)
{
	if (RuntimeConfig::phase == RuntimeConfig::Phase::EndOfGame)
		return;
	if (RuntimeConfig::gameGoal == RuntimeConfig::GameGoal::ConquerNexus)
	{
		if (f->nexusControl >= Game::Config::GetTargetNexusCount())
		{
			Game::Network::ChangePhase(RuntimeConfig::Phase::EndOfGame);
			Game::winner = f;

			foreach (Network::activePlayersAndSpectators,p)
			{
				(*p)->SendProgressUpdate(true,"<null>");
			}
		}
		else
		{
			Network::PClient client;
			if (Network::playerMap.Query(f,client))
			{
				String why;
				if (Game::Config::basesRequired && f->deathInRounds > 0)
					f->CanLive(why);
				client->SendProgressUpdate(false,why);
			}
			return;
		}
	}
}


/*
	if (!e)
	{
		LogEvent(EventClass::Preset::commandRejected,nullptr, "NULL attempting attack");
		return;
	}
	LogEvent(EventSignificance::Common,e.get(),"Attacking "+traceTo.ToString()+" in "+String(flightTime)+" seconds");
	Protocol::EntityAttack roughAttack,preciseAttack;
	float zOffset;
	Objects::PTurretBase base = e->GetTurretBase(zOffset);
	ASSERT__(base);
	preciseAttack.turretType = base->turret->constName;
	preciseAttack.traceTo = traceTo;

	preciseAttack.timestamp.rightNow = false;
	const float3 traceToWorld = Grid::ToWorld(traceTo);

	preciseAttack.sourcePosition = e->system.w.xyz + e->system.z.xyz * zOffset;
	//attack.timestamp.atTime = v->timeAt;
	
	{
		Objects::PVehicle v = To<Objects::Vehicle>::DynamicCast(e);
		if (v)
			preciseAttack.timestamp.atTime = v->timeAt;
		else
		{
			Objects::PTower t = To<Objects::Tower>::DynamicCast(e);
			if (t)
				preciseAttack.timestamp.atTime = t->timeAt;
			else
			{
				LogEvent(EventClass::Preset::commandRejected,e.get(), "Incompatible entity attempting attack");
				return;	
			}
		}
	}

	preciseAttack.flightTime = flightTime;
	roughAttack = preciseAttack;

	const float range = base->GetAttackRange();
	float radius = range * 0.2f;
	float3 mod = float3(random.getFloat(-1,1),random.getFloat(-1,1),random.getFloat(-1,1)) * radius;
	count_t numIt = 0;
	while (Vec::quadraticDistance(mod + roughAttack.sourcePosition.value, traceToWorld) > sqr(range) && ++numIt < 1000)
		mod = float3(random.getFloat(-1,1),random.getFloat(-1,1),random.getFloat(-1,1)) * radius;
	ASSERT__(numIt < 1000);
	roughAttack.sourcePosition.value += mod;

	for (auto cl : State::clients)
	{
		TCP::PPeer peer = (cl)->peer.lock();
		if (!peer)
			continue;
		if (Receiver::CanSee(cl,e,preciseAttack.timestamp.atTime))
			Channels::entityAttack.SendTo(peer,preciseAttack);
		else
			if (cl->faction
	}
	*/


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
	preciseAttack.timestamp.atTime = attack.attackFrom->motionTime;
	
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


void	SignalExplosion(const TAttack&attack, const TVec3<>&center,float explosionRadius,bool isEMP, bool isDoomsday, float atTime)
{
	Protocol::Explosion explosion;
	explosion.center = center;
	explosion.radius = explosionRadius;
	explosion.isEMP = isEMP;
	explosion.timestamp.atTime = atTime;
	explosion.timestamp.rightNow = false;

	//static GenericHashSet<Objects::OwnerParameter,Objects::OwnerHash>	map;
	static Buffer0<Objects::OwnerParameter>	canSee;
	static Buffer0<Network::PClient>	clientsCanSee;

	bool allSee = isDoomsday && RuntimeConfig::doomsdayWeapons == RuntimeConfig::DoomsdayWeapons::Expose;

	canSee.Clear();
	if (allSee)
	{
		//just set all. I'm lazy, and these are rare
		foreach (Objects::Faction::all, f)
			canSee.Append(f->lock());
		canSee.Append(Objects::Faction::nexus);
	}
	else
	{
		static Grid::HighSetup::TVolumeDescriptor volume;
		Sphere<> explosionSphere(center,explosionRadius);
		Grid::high.GenerateVolume(explosionSphere,volume);

		Objects::ForeachFaction([explosionSphere](const Objects::PFaction&f)
		{
			if (f->GridCanSee(volume) || f->CountOffGridCoverage(explosionSphere))
			{
				canSee.Append(f);
			};
		});
	}

	clientsCanSee.Clear();

	for (const Objects::OwnerParameter&p : canSee)
	{
		Network::PClient client;
		if (Network::playerMap.Query(p,client))
			clientsCanSee << client;
	}
	clientsCanSee.Append(Network::spectators);

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

		const TVec3<>& traceToWorld = attack.traceTo;

		roughAttack.sourcePosition = attack.attackFrom->GetWorldPosition() + Grid::low.cells[attack.attackFrom->location.stackID].centerVertex.xyz * zOffset;
		//attack.timestamp.atTime = v->timeAt;
		


		const float range = roughAttack.turretRange;//base->GetFlatAttackRange();
		float radius = range * 0.2f;
		float3 mod = float3(random.getFloat(-1,1),random.getFloat(-1,1),random.getFloat(-1,1)) * radius;
		count_t numIt = 0;
		while (!attack.attackFrom->CanHit(mod + roughAttack.sourcePosition.value,roughAttack.attackAngle.value)
				//Vec::quadraticDistance(mod + roughAttack.sourcePosition.value, traceToWorld) > sqr(range) 
			&& ++numIt < 1000)
			mod = float3(random.getFloat(-1,1),random.getFloat(-1,1),random.getFloat(-1,1)) * radius;
		ASSERT__(numIt < 1000);
		roughAttack.sourcePosition.value += mod;
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






namespace InformationProvider
{
	TCP::Server		server;
	bool			created = false;

	void	OnPing(Protocol::Ping&ping, TCP::Peer&peer);

	TCP::ObjectSender<Protocol::ServerInfo,Protocol::InfoChannel::ServerInfo>	infoSender;
	TCP::Channel<Protocol::Ping,0,Protocol::InfoChannel::Ping>	pingChannel(OnPing);

	void	OnPing(Protocol::Ping&ping, TCP::Peer&peer)
	{
		pingChannel.SendTo(peer,ping);
	}



	class Updater : public ThreadObject
	{
	public:
		volatile bool	doEnd = false;

		void	ThreadMain() override
		{
			Protocol::ServerInfo info;
			info.hostInfo = UDPBroadCast::packet;

			while (!application_shutting_down && !doEnd)
			{
				Sleep(1000);
				Game::Network::AssembleGameInfo(info.gameInfo);

				infoSender.SendTo(server,info);
			}
			server.EndService();
		}
	}		updater;


	void		Start(UINT16 basePort)
	{
		if (!created)
		{
			created = true;
			server.InstallChannel(pingChannel);
		}

		UINT16 port = basePort + PortOffset::Info;

		if (!server.StartService(port))
			FATAL__("Unable to register information server. Chances are, a server is already running on port "+String(port)+".")
		else
		{
			LogEvent(EventSignificance::Uncommon,"Info server started on port "+String(port));
			updater.doEnd = false;
			updater.start();
		}



	}

	void		Stop()
	{
		updater.doEnd = true;
		updater.awaitCompletion();
	}

	void		Destroy()
	{
		//should end on its own
	
	}


}

void MyFatal(const FatalDescriptor&descriptor)
{
	String message = descriptor.ToString();
	LogFile file("fatal.log",false);
	
	file.Log(message+"\n",true);
	file.Close();

	Logging::WriteEvent(EventClass::Preset::fatal,message,nullptr,nullptr);

	application_shutting_down = true;	//prevent certain operations in Close()
	DisplayMessage(PRODUCT_NAME" Server Error",message,false);
	//ErrMessage("Hexaverse Server: "+message);
	std::cerr << message << std::endl;
	#ifdef _DEBUG
		char confirm[0x100];
		std::cin.getline(confirm,sizeof(confirm));
	#endif


	//NetworkConnection::Close();

}


template <typename EnumerationType>
EnumerationType	GetType(Config::Context*context, const String&path)
{
	String value = context->GetString(path).trim();
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

//template <typename T>
//T	GetPrimitive(Config::Context*context, const String&path)
//{
//	const Config::Context::Attribute*attrib = context->GetAttrib(path);
//	if (!attrib)
//	{
//		FATAL__("Trying to extract primitive from '"+context->name+"/"+path+"' but item is not defined");
//	}
//	
//	String raw = attrib->value;
//	if (raw.endsWith(';'))
//		raw = raw.subStringRef(0,raw.length()-1);
//	T rs;
//	if (!convert(raw.c_str(),rs))
//	{
//		FATAL__("Trying to extract primitive from '"+context->name+"/"+path+"' but '"+raw+"' cannot be converted to designated type");
//	}
//	return rs;
//}

//void GetConfig(Grid::TPlanetConfig::TLayer&layer, Config::Context*context, const String&path)
//{
//	layer.baseHeight = GetPrimitive<int>(context,path+"/baseHeight");
//	layer.heightScale = GetPrimitive<float>(context,path+"/noiseHeightScale");
//	layer.frequency = GetPrimitive<float>(context,path+"/frequency");
//	layer.dampening = GetPrimitive<int>(context,path+"/dampening");
//	ASSERT_IS_CONSTRAINED__(layer.dampening, 0, 10);
//
//
//}


void EstimatePresence(Objects::WorldEntity&e, const Grid::LCellIdI&location, float*hostileNearNexus, float*constructorNearNexus, float*friendlyNearNexus)
{
	const TVec3 < >&loc = Grid::low.cells[location.stackID].centerVertex.xyz;
	float impact = e.GetConstructionCost();
	if (impact > 0)
		for (index_t i = 0; i < 12; i++)
			if (BaseGrid::IsIsolatedCorner(i))
			{
				float opacity = std::max(0.f,Vec::dot(Grid::low.cells[i].centerVertex.xyz, loc));
				//opacity = Grid::powi(opacity,2);
				if (opacity > 0)
				{
					if (e.owner != Objects::Faction::nexus)
					{
						hostileNearNexus[i] += opacity * impact;

						Objects::PTurretBase b = e.GetTurretBase();
						if (b && b->turret->CanDo(Objects::Component::Construct))
						{
							constructorNearNexus[i] += Grid::powi(opacity,64);
						}
					}
					else
					{
						if (e.IsMobile())
							friendlyNearNexus[i] += opacity * impact;
					}
				}
			}
}



static Objects::PNexusPort ClosestPort(const Objects::PWorldEntity&e, const Objects::PNexusPort cornerPorts[12])
{
	float d2 = std::numeric_limits<float>::max();
	Objects::PNexusPort rs;
	for (int i = 0; i < 12; i++)
	{
		if (!cornerPorts[i])
			continue;
		float nd2 = Vec::quadraticDistance(e->GetWorldPosition(),cornerPorts[i]->GetWorldPosition());
		if (nd2 < d2)
		{
			d2 = nd2;
			rs = cornerPorts[i];
		}
	}
	return rs;
}


void ProcessAICoverage()
{
	if (Game::Config::Nexus::enableAI
		//#ifdef _DEBUG
		//	|| State::Config::Nexus::haveBases
		//#endif

	)
	{
		//LogEvent(EventType::SessionRelated,"Processing AI coverage");
		float	hostileNearNexus[12] = { 0 }, // will assume nexus-ports are where they are supposed to be and use corner locations
			friendlyNearNexus[12] = { 0 },
			constructorNearNexus[12] = {0};

		if (Game::Config::startBases)
		{
			

		//			Objects::PMine mine = To<Objects::Mine>::Cast(
		//			Objects::Mine::globalPrototype->CreateNewInstance(client->faction,location,0));
		//		mine->CorrectTerrain(false,0,true);
		//		Objects::InsertConstructed(mine,0);
		//		
		//		Objects::PGenerator generator = To<Objects::Generator>::Cast(
		//			Objects::Generator::globalPrototype->CreateNewInstance(client->faction,at1,0));
		//		generator->CorrectTerrain(false,0,true);
		//		generator->SetEnabledOverride(true);
		//		Objects::InsertConstructed(generator,0);
		//		
		//		std::shared_ptr<Objects::MineralContainer> container = To<Objects::MineralContainer>::Cast(
		//			Objects::MineralContainer::globalPrototype->CreateNewInstance(client->faction,at2,0));
		//		container->CorrectTerrain(false,0,true);
		//		Objects::InsertConstructed(container,0);




		//		Objects::SpawnVehicle(client->faction,at3,Objects::Component::hoverbase, Objects::Component::terraformer,true,0);
		//		Objects::SpawnVehicle(client->faction,at4,Objects::Component::hoverbase, Objects::Component::resourceScanner,true,0);


		}


		for (int i = 0; i < 12; i++)
			if (BaseGrid::IsIsolatedCorner(i))
				friendlyNearNexus[i] = Game::factionStartMineralValue /*- State::Config::nexusStartMineralValue*/;
		Objects::PNexusPort ports[12];
		index_t portAt = 0;
		foreach(Objects::structures, s)
		{
			//bool nexus = (*s)->owner == Objects::Faction::nexus;
			Objects::PNexusPort p = To<Objects::NexusPort>::DynamicCast(*s);
			if (p && BaseGrid::IsIsolatedCorner((*s)->location.stackID))
			{
				ASSERT_LESS__(portAt, BaseGrid::IsolatedCornerCount);
				portAt++;
				index_t portIn = (*s)->location.stackID;
				ASSERT_LESS__(portIn, 12);
				ASSERT__(!ports[portIn]);
				ports[portIn] = p;
			}
		}
		ASSERT_EQUAL__(portAt, BaseGrid::IsolatedCornerCount);

		foreach(Objects::vehicles, v)
		{
			EstimatePresence(**v,(*v)->location,hostileNearNexus,constructorNearNexus,friendlyNearNexus);
		}

		foreach(Objects::structures, s)
		{
			EstimatePresence(**s,(*s)->location,hostileNearNexus,constructorNearNexus,friendlyNearNexus);
		}
		static Buffer0<UINT32> neighborField;
		for (int i = 0; i < 12; i++)
			if (BaseGrid::IsIsolatedCorner(i) && (Grid::empZonesUnlocked & (1<<i)))
			{
				Objects::PNexusPort p = ports[i];
				if (RuntimeConfig::roundNumber - p->lastSpawnRound < 5 || p->owner != Objects::Faction::nexus)
					continue;	//not allowed to spawn anything for the moment

				float threatLevel = sqr(hostileNearNexus[i] )*Game::Config::Nexus::threatEstimationFactor
						*1e-3f;
				LogEvent(EventSignificance::Debug, "Threat level for " + ports[i]->GetName() + " considered " + String(threatLevel));
				if (Game::Config::Nexus::haveBases)
				{
					Objects::RefreshCompoundCoverageIfNeeded(0);

					Objects::Compound&n = p->GetCompound();
					count_t producing = 0,
							existing = 0;
					for (Objects::WWorkingStructure _ws : n.workingStructures)
					{
						Objects::PWorkingStructure	ws = _ws.lock();
						if (!ws)
							continue;
						#ifndef NO_HIBERNATION
							if (ws->ToHibernating()->IsHibernating())
								continue;
						#endif
						if (Grid::AffectedByEMP(ws->location) || ClosestPort(ws,ports) != p)
							continue;
						if (ws->health < ws->GetMaxHealth())
							friendlyNearNexus[i] -= ws->GetConstructionCost();
						Objects::PMobileEntityProducer plant = To<Objects::MobileEntityProducer>::DynamicCast(ws);
						if (plant)
						{
							existing++;
							if (plant->AwaitsUserInteraction())
							{
								float t = 0;
								Objects::PMobileEntity v = plant->SpawnFinishedProductionAnywhere(t);
								if (!v)
								{
									producing++;	//dont count
									continue;
								}
								Journal::currentRound.RegisterChoiceIntersection(false,ws,v->location,Objects::CursorMode::Idle,v);
								EstimatePresence(*v,v->location,hostileNearNexus,constructorNearNexus,friendlyNearNexus);
							}
							if (plant->queue.IsNotEmpty())
							{
								producing++;
								ASSERT__(plant->queue.first().producing->owner == Objects::Faction::nexus);
								EstimatePresence(*plant->queue.first().producing,ws->location,hostileNearNexus,constructorNearNexus,friendlyNearNexus);
							}
						}
					}

	//				if (producing < existing)
					{
						while (producing < existing && (friendlyNearNexus[i] < threatLevel || (constructorNearNexus[i] > 0.2f && friendlyNearNexus[i] < Game::factionStartMineralValue + 20.f)))
						{
							for (Objects::WWorkingStructure _ws : n.workingStructures)
							{
								Objects::PWorkingStructure ws = _ws.lock();
								if (!ws)
									continue;
								Objects::PMobileEntityProducer plant = To<Objects::MobileEntityProducer>::DynamicCast(ws);
		//						Objects::MobileEntityProducer*plant = ws->ToProducer();
								Objects::PVehiclePlant vplant = To<Objects::VehiclePlant>::DynamicCast(ws);
								if (plant && vplant && plant->queue.IsEmpty())
								{
									index_t choice = vplant->entityRandom.getIndex(Game::Config::Nexus::vehicleTurrets.Count() - 1);
									//random.getIndex(State::Config::Nexus::vehicleTurrets.Count() - 1);
									ASSERT_LESS__(choice,Game::Config::Nexus::vehicleTurrets.Count());
									//Objects::Component::Turret* turret = State::Config::Nexus::vehicleTurrets[choice];
									//Objects::MobileEntityProducer::WorkItem item = vplant->EnqueueProductionOfVehicle(&Objects::Component::hoverbase,turret);
									//Objects::PVehicle v = To<Objects::Vehicle>::Cast(item.producing);
									ConstName	parameters[2] = {ConstName::VehicleBase_HoverBase, Game::Config::Nexus::vehicleTurrets[choice]->constName};
									LogEvent(EventSignificance::Common,vplant.get(),"Enqueueing vehicle production job due to high threat level");
									Journal::currentRound.RegisterProductionJobStart(true,vplant,plant,parameters,2);
									
									EstimatePresence(*vplant->queue.last().producing,vplant->location,hostileNearNexus,constructorNearNexus,friendlyNearNexus);
									producing++;
									break;
								}
							}
						}
					}

			
					#ifdef ONE_COMPOUND_PER_FACTION
						break;	//only one compound for nexus faction. all done
					#endif

				}
				else
				while (friendlyNearNexus[i] < threatLevel || (constructorNearNexus[i] > 0.2f && friendlyNearNexus[i] < Game::factionStartMineralValue + 20.f))
				{
					LogEvent(EventSignificance::Debug, "Spawning nexus unit near " + p->GetName());
					p->lastSpawnRound = RuntimeConfig::roundNumber;


					Grid::LCellIdI location = Grid::LCellIdI::Invalid;
					Grid::low.GetExtendedNeighborhood(i, 3, neighborField);
					foreach(neighborField, n)
					{
						Grid::LCellIdI candidate(*n, ports[i]->location.height);
						Grid::AssertIsValid(candidate);
						candidate.height = Objects::FindApplicableGround(candidate, true);

						if (candidate.height != Objects::badHeight && !Objects::GetVehicle(candidate))
						{
							location = candidate;
							break;
						}

					}

					if (location != Grid::LCellIdI::Invalid)
					{
						Objects::Component::Turret* turret = Game::Config::Nexus::vehicleTurrets[random.getIndex(Game::Config::Nexus::vehicleTurrets.Count() - 1)];
						float t = 0;
						Objects::PVehicle v = Objects::SpawnVehicle(Objects::Faction::nexus, location, &Objects::Component::hoverbase, turret, true, t);
						Journal::currentRound.LogEntityCreation(v);
						EstimatePresence(*v,v->location, hostileNearNexus,constructorNearNexus, friendlyNearNexus);
					//	Objects::VerifyVisibilityNetwork();
					}
					else
					{
						LogEvent(EventSignificance::Debug, "Unable to spawn nexus vehicle. Not enough space near " + ports[i]->GetName());
						break;
					}
				}

			}
	}
}


Semaphore mainThreadDone;


void TerminateNow(bool isMain)
{
	static Mutex mutex;
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
			application_shutting_down = true;
			mainThreadDone.enter();
		}


		Logging::WriteEvent(EventSignificance::Uncommon,"Closing down",nullptr,nullptr);
		Network::server.Shutdown();

		if (Game::Config::Nexus::enableAI)
			AIProcess::End();


		InformationProvider::Destroy();
		Objects::Destroy();
		Grid::Destroy();
		UDPBroadCast::Terminate();

		RemoteControl::Destroy();


		Logging::WriteEvent(EventSignificance::Uncommon,"Terminating process",nullptr,nullptr);
		Logging::End();

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
			LogEvent(EventSignificance::Uncommon,"Received close event. Terminating");
			TerminateNow(false);
		return TRUE;
	
	}
	return FALSE;
}

void LogConfig(const String&msg)
{
	Logging::WriteEvent(EventClass::Preset::configurationPrint,msg,nullptr,nullptr);
}



bool ApplyCoverageChange(const Objects::PFaction&faction)
{
	return World::Network::CheckCoverageSendTerrain(faction,TCP::PPeer());
}


namespace ParameterConfig
{
	return LuaRun(argc,argv);
	UINT16 basePort = DEFAULT_SERVER_BASE_PORT;
	
	enum mode_t
	{
		Idle,
		ExpectPort,
		ExpectWorldID,
		ExpectFilename,
		ExpectAutoSaveNumber,

	}		mode = Idle;

	PathString	loadFile;
	bool	startOffline = false,
			startInConfig = false;
	bool	customWorldID = false;
	bool	enableRemoteControl = true;
	const char*	programName="";
	TCP::Server::Protocol controlProtocol = TCP::Server::Protocol::IPv6;

	void	SetProgramName(const char*base)
	{
		const char*slashAt;
		while (slashAt = Template::strchr(base,FOLDER_SLASH))
		{
			base = slashAt+1;
		}
		programName = base;
	}

	void	PrintHelp()
	{
		std::cout << std::endl << "usage: "<<programName<<" ..."<<std::endl;
		std::cout << "  --load/-l [filename]\tload savegame"<<std::endl;
		std::cout << "  --loadAuto/-a [number]\tload auto-savegame. number=0 => most recent"<<std::endl;
		std::cout << "  --offline/-o \tstart offline"<<std::endl;
		std::cout << "  --config/-c \tstart in configuration mode"<<std::endl;
		std::cout << "  --port/-p [basePort]\tuse a non-default base port ("<<DEFAULT_SERVER_BASE_PORT<<" is default)"<<std::endl;
		std::cout << "  --noRemote/-r \tdisable remote control service"<<std::endl;
		std::cout << "  --rv4\tStart remote control service using IPv4"<<std::endl;
		std::cout << "  --test/-t \trun self-tests, then quit"<<std::endl;
		std::cout << "  --world/-w [worldID] \tuse custom world ID, and start empty"<<std::endl;
	}



	void	Process(const char*arg)
	{
		LogEvent(EventSignificance::Debug,"ParameterConfig::Process: '"+String(arg)+"'");
		switch (mode)
		{
			case ExpectWorldID:
				Bin2Str::Convert(World::worldID.bytes,World::worldID.NumBytes,arg,strlen(arg));
				LogEvent(EventClass::Preset::configuration,"World ID set to "+World::worldID.ToFullString());
				mode = Idle;
				customWorldID = true;
			return;
			case ExpectFilename:
				loadFile = arg;
				mode = Idle;
			break;
			case ExpectAutoSaveNumber:
			{
				mode = Idle;
				index_t index;
				if (!convert(arg,index))
				{
					LogEvent(EventClass::Preset::configurationError,"Unable to convert '"+String(arg)+"' to a valid index");
					return;
				}
				loadFile = FileIO::GetAutoSavePath(index);
			}
			break;
			case Idle:
				if (!strcmp(arg,"--test") || !strcmp(arg,"-t"))
					inTestMode = true;
				elif (!strcmp(arg,"--rv4"))
					controlProtocol = TCP::Server::Protocol::IPv4;
				elif (!strcmp(arg,"--port") || !strcmp(arg,"-p"))
					mode = ExpectPort;
				elif (!strcmp(arg,"--offline") || !strcmp(arg,"-o"))
					startOffline = true;
				elif (!strcmp(arg,"--config") || !strcmp(arg,"-c"))
					startInConfig = true;
				elif (!strcmp(arg,"--loadAuto") || !strcmp(arg,"-a"))
					mode = ExpectAutoSaveNumber;
				elif (!strcmp(arg,"--help"))
				{
					PrintHelp();
					exit(0);
				}
				elif (!strcmp(arg,"--world") || !strcmp(arg,"-w"))
					mode = ExpectWorldID;
				elif (!strcmp(arg,"-r") || !strcmp(arg,"--noRemote"))
					enableRemoteControl = false;
				elif (!strcmp(arg,"--load") || !strcmp(arg,"-l"))
					mode = ExpectFilename;
				else
				{
					PathString path = arg;
					if (path.endsWithCaseIgnore(FileIO::savFileExtension) && FileSystem::IsFile(arg))
					{
						mode = ExpectFilename;
						Process(arg);
						return;
					}
					LogEvent(EventClass::Preset::configurationError, "Unsupported runtime parameter '"+String(arg)+"'");
					PrintHelp();
				}
			return;
			case ExpectPort:
				if (!convert(arg,basePort))
				{
					LogEvent(EventClass::Preset::configurationError,"Unable to convert '"+String(arg)+"' to a valid port number");
				}
				else
				{
					Network::Config::sessionPort = basePort + PortOffset::Session;
					LogEvent(EventClass::Preset::configuration,"Base port redefined as "+String(basePort)+", session port set to "+String(Network::Config::sessionPort));
				}
				mode = Idle;
			return;
		}
	}


}


void	StartUp()
{
	UDPBroadCast::Start();
	Network::Start();
	InformationProvider::Start(ParameterConfig::basePort);
}

void	ShutDown()
{
	UDPBroadCast::Terminate();
	Network::server.StopService();
	InformationProvider::Stop();
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

	String					ToString() const override
	{
		return checkIsOnline();
	}


	bool					Set(const String&value) override
	{
		bool v;
		if (!convert(value.c_str(),value.length(),v))
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
	LogEvent(EventSignificance::Debug,"Checking server target directory candidate '"+String(dir)+"'=>'"+String(test)+"'. Result="+String(rs));


	return rs;
}


int main( int argc, const char* argv[])
{
	

	onFatal = MyFatal;


	try
	{

		RuntimeConfig::InitConfigDir("server");
		ASSERT__(FileSystem::CreateDirectory(RuntimeConfig::ConfigFile("logs")));
		Logging::Begin(RuntimeConfig::ConfigFile(PathString("logs/server_"+Logging::TimeStamp(true)+".log")));
		LogEvent(EventSignificance::Uncommon,nullptr,"Starting server v" PRODUCT_VERSION);


		TCP::onIgnoredException = [](const TCodeLocation&loc, const std::exception&ex)
		{
			LogEvent(EventSignificance::Debug,"TCP exception: "+String(loc.ToString())+": "+ex.what());
		};
		TCP::onIgnoredArbitraryException = [](const TCodeLocation&loc)
		{
			LogEvent(EventSignificance::Debug,"TCP exception (arbitrary): "+String(loc.ToString()));
		};

		SetConsoleCtrlHandler(MyHandlerRoutine,TRUE);

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

		Evolver::Create();
		Game::evolver = Evolver::GetDefault();

		Journal::Create();

		World::worldID.Generate();
		ParameterConfig::SetProgramName(argc > 0 ? argv[0] : "program");
		for (int i = 1; i < argc; i++)
			ParameterConfig::Process(argv[i]);

		Network::serverID.Generate();
		UDPBroadCast::Create(ParameterConfig::basePort,Network::serverID);

		if (ParameterConfig::enableRemoteControl)
			RemoteControl::Start(ParameterConfig::basePort,ParameterConfig::controlProtocol);



		#ifdef _DEBUG
			//inTestMode = true;
		#endif



		Grid::Create0();
		Objects::Create();
		Grid::Create1();


		Reflection::Create();
		Reflection::state.PushFocus();
		Reflection::state.EnterNewFolder("/Online");
		Reflection::state.SetVariable(CLI::PVariable(new ServiceVar("udpBroadcast",UDPBroadCast::IsStarted,UDPBroadCast::Terminate,[](){UDPBroadCast::Start();})));
		Reflection::state.SetVariable(CLI::PVariable(new ServiceVar("info",[]()->bool{return InformationProvider::server.IsOnline();},InformationProvider::Stop,[](){InformationProvider::Start(ParameterConfig::basePort);})));
		Reflection::state.SetVariable(CLI::PVariable(new ServiceVar("session",[]()->bool{return Network::server.IsOnline();},[](){Network::SuspendSession();},Network::RestartSession)));
		Reflection::state.DefineCommand("closeSession",(CLI::Command::function0_t)[](){Network::CloseSession();},CLI::NoCompletion);	//why...?

		Reflection::state.PopFocus();

		Reflection::state.DefineCommand("/Server/shutdown",(CLI::Command::function0_t)[]()
		{
			application_shutting_down = true;
		})->SetUniqueHelp("Shuts down the service and terminates the application");

		Reflection::state.PushFocus();
			Reflection::state.EnterNewFolder("/IO");
			Reflection::state.DefineCommand("autoSaveNow",FileIO::AutoSave,CLI::NoCompletion);
			Reflection::state.DefineCommand("saveTo fileName",(CLI::Command::function1_t)[](const String&path)
			{
				FileIO::Save(PathString(path));
			},CLI::FileCompletion);

			Reflection::state.DefineCommand("loadFrom fileName",(CLI::Command::function1_t)[](const String&path)
			{
				FileIO::Load(PathString(path));
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
		Reflection::state.PopFocus();


		Reflection::state.DefineCommand("/World/generate [planetType] [discovery]",(CLI::Command::function2_t)[](const String&planet, const String&discovery)
		{
			if (planet.IsNotEmpty())
			{
				if (!FileSystem::IsFile(RuntimeConfig::ConfigFile(PathString("config/planets/"+planet+".ini"))))
				{
					LogEvent(EventClass::Preset::remoteControlError,"'"+planet+"' is not a valid planet type");
					return;
				}
			}

			if (planet.IsNotEmpty())
			{
				Reflection::Variable::planetType->Set(planet.trim());
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
			FileSystem::Folder folder = RuntimeConfig::ConfigFile("config/planets");
			if (!folder.IsValidLocation())
			{
				LogEvent(EventClass::Preset::remoteControlError,"Internal error: Unable to locate planets-directory");
				return;
			}
			FileSystem::File file;
			folder.Rewind();
			CLI::println("-");
			while (folder.NextFile("ini",file))
			{
				CLI::println(String(file.GetInnerName()));
			}
			CLI::println("-");
		},CLI::NoCompletion);

		


		if (!inTestMode)
			Reflection::Load(false);

		Objects::Faction::RecreateNexus();


		if (inTestMode)
		{
			std::cout << "Running tests..." << std::endl;

			World::BuildConfiguredWorld(random);


			{
				Test::Run(Test::ServerClient());
				Test::Run(Test::HighGridFamilies());
				Test::Run(Test::AutoTargeting());
				Test::Run(Test::Bin2Str());
				Test::Run(Test::Serialization());
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



		//Reflection::CreatePrototypeDeclaration();
		if (!ParameterConfig::customWorldID)
		{
			World::BuildConfiguredWorld(random);
		}


		//{
		//	State::Config::Nexus::vehicleTurrets.SetSize(State::Config::nexusVehicleTurretNames.Count());
		//	for (index_t i = 0; i < State::Config::nexusVehicleTurretNames.Count(); i++)
		//	{
		//		State::Config::Nexus::vehicleTurrets[i] = Objects::Component::GetTurretByName(State::Config::nexusVehicleTurretNames[i]);
		//		if (!State::Config::Nexus::vehicleTurrets[i])
		//		{
		//			FATAL__("Nexus AI: '" + State::Config::nexusVehicleTurretNames[i] + "' is not a valid turret name");
		//		}
		//	}
		//	//State::Config::nexusVehicleTurretNames.Clear();
		//}











		//State::currentState = Grid::cellStack;	//reference for deltas

		//Array<BYTE>	out(Grid::cellStack.GetContentSize());

		//for (BYTE k = 0; k < BaseGrid::NumFamilies; k++)
		//{
		//	const index_t offset = Grid::high.grid.GetFamilyOffset(k);
		//	const index_t range = Grid::high.grid.CountFamilyMembers(k) * Grid::CellStack::Range;
		//	Timer::Time t0 = timer.Now();
		//	size_t size = BZ2::compress(Grid::cellStack.pointer()+offset,sizeof(Grid::CellStack::Cell)*range,out.pointer(),out.GetContentSize());
		//	std::cout << (int)k<< " "<<timer.GetSecondsSince(t0)<<std::endl;;;
		//}
		//std::flush(std::cout);
		//ShowMessage(String(size)+"/"+String(Grid::cellStack.GetContentSize()));





		if (!ParameterConfig::startOffline)
		{
			StartUp();
			if (ParameterConfig::startInConfig)
			{
				ConfigurationMode::Begin();
			}
		}



		if (false)
		{
			Logging::WriteEvent(EventSignificance::Uncommon,"Testing server.",nullptr,nullptr);

			FileIO::Save(PathString("test0."+FileIO::savFileExtension));
			FileIO::Load(PathString("test0."+FileIO::savFileExtension));
			FileIO::Save(PathString("test1."+FileIO::savFileExtension));

			Riff::File a,b;
			ASSERT__(a.Open(PathString("test0."+FileIO::savFileExtension).c_str()));
			ASSERT__(b.Open(PathString("test1."+FileIO::savFileExtension).c_str()));
			Buffer<Riff::TID>	stack;
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
				Logging::WriteEvent(EventClass::Preset::fileError,"Specified file ("+String(filename)+") does not exist. Skipping loading operation.",nullptr,nullptr);
		}

		//Journal::globalWriter.InitNew(RuntimeConfig::ConfigFile("saves/log"));
		//Journal::globalWriter.ContinueExisting(RuntimeConfig::ConfigFile("saves/log"));

		Logging::WriteEvent(EventSignificance::Uncommon, "Preparation of world '"+World::worldID.ToNiceString()+"'/" + World::worldID.ToString() + " completed.", nullptr, nullptr);
		Logging::WriteEvent(EventSignificance::Important,"Server "+Network::serverID.ToString()+"/"+UDPBroadCast::packet.version+" is online.",nullptr,nullptr);
		FileIO::RefreshWindowTitle();

	
		LogConfig("AI is configured as follows:");
		if (Game::Config::Nexus::enableAI)
		{
			LogConfig(" -Nexus-AI enabled");
			LogConfig(" -ThreatEstimationFactor=" + String(Game::Config::Nexus::threatEstimationFactor));
			//LogConfig(" -NexusTurrets=" + implode(',', State::Config::Nexus::vehicleTurretNames));
		}
		else
			LogConfig(" -Disabled");

		LogConfig("This game is configured as follows:");
		#ifndef NO_HIBERNATION
			LogConfig(" -Uplink traffic per faction is limited to "+String(State::Config::uplinkBandwidth));
		#endif

		LogConfig(" -Factions: "+Game::Config::FactionSupportFlagString());

		if (World::Config::simulateLiquids)
			LogConfig(" -Liquids: Expanding.");
		else
			LogConfig(" -Liquids: Static.");
		switch (RuntimeConfig::gameGoal)
		{
			case RuntimeConfig::GameGoal::None:
				LogConfig(" -Goal: None.");
			break;
			case RuntimeConfig::GameGoal::ConquerNexus:
				static_assert(BaseGrid::IsolatedCornerCount==3,"Unexpected count");
				LogConfig(" -Goal: Connect and enable 2+ Nexus Ports.");
			break;
		}
		if (Game::Config::closedGame)
			LogConfig(" -Closed: Joining will be restricted to first round and access tokens. Game can be won.");
		else
			LogConfig(" -Open: Players can join any time. Game is endless.");
		if (Game::Config::Nexus::haveBases)
			LogConfig(" -Nexus-Bases: Nexus bases will be randomly created during world creation");
		else
			LogConfig(" -No Nexus-Bases: Nexus bases will not be created");

		switch (RuntimeConfig::doomsdayWeapons)
		{
			case RuntimeConfig::DoomsdayWeapons::Conceal:
				LogConfig(" -Doomsday weapons: Treated like normal weapons (must be in sensor range to see)");
			break;
			case RuntimeConfig::DoomsdayWeapons::Disallow:
				LogConfig(" -Doomsday weapons: Forbidden to fire (can still be built, however)");
			break;
			case RuntimeConfig::DoomsdayWeapons::Expose:
				LogConfig(" -Doomsday weapons: Doomsday explosions are made visible to all factions");
			break;
			default:
				LogConfig(" -Doomsday weapons: "+String(RuntimeConfig::doomsdayWeapons.ToString()));
			break;
		}

		if (Game::Config::roundTimer > 0)
			LogConfig(" -Round Timer: Rounds beyond the first may last at most "+String(Game::Config::roundTimer)+" seconds.");
		else
			LogConfig(" -No Round Timer: Rounds end when all players requested so.");
		if (Game::Config::basesRequired)
			LogConfig(" -Bases Required: Factions that do not meet the station requirements die after five rounds.");
		else
			LogConfig(" -No Survival Conditions: Factions live forever (although their vehicles/structures might not).");
		if (Game::Config::startBases)
			LogConfig(" -Bases: New factions are given minimal bases.");
		else
			LogConfig(" -Flexibility: New factions are given vehicles, but no structures.");
		if (Objects::newVehiclesHaveFullAmmo)
			LogConfig(" -Ammo: New vehicles using standard ammo are given full ammo.");
		else
			LogConfig(" -No Ammo: New vehicles have zero ammo.");
		if (Game::Config::exposeActionLeftToHostileFactions)
			LogConfig(" -Expose Vehicle Initiative: Vehicle initiative is exposed to hostile factions.");
		else
			LogConfig(" -Private Vehicle Action: Vehicle movement is not exposed to hostile factions.");
		if (RuntimeConfig::empZones)
			LogConfig(" -EMP Zones: Vehicles enterting residual EMP fields will suffer extensive EMP damage.");
		else
			LogConfig(" -No EMP Zones: The entire planet is freely accessible.");

		if (FileIO::Config::saveEveryNRounds > 0)
			LogConfig(" -Auto-saves will be created every "+String(FileIO::Config::saveEveryNRounds)+" round(s).");
		if (FileIO::Config::saveWhenAllQuit)
			LogConfig(" -Auto-saves will be created when all quit.");


		//FATAL__("test");
		if (Game::Config::Nexus::enableAI)
		{
			AIProcess::Begin(Objects::Faction::nexus->accessToken);
		}

		Game::ResetFactionStartMineralValue();


		while (!application_shutting_down)
		{
			if (RuntimeConfig::IsInAnyCommandPhase() || RuntimeConfig::phase == RuntimeConfig::Phase::AnimationPhase)
				Sleep(1);
			Network::server.Resolve();
			RemoteControl::Resolve();
			if (Game::Config::Nexus::enableAI)
				AIProcess::Check();


			RuntimeConfig::Phase p0 = RuntimeConfig::phase;

			if (RuntimeConfig::IsInAnyCommandPhase())
			{
				bool newRoundNow = false;

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
						if ((*p)->isSpectator)	//doesn't count towards new round
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
					Objects::ForeachWorldEntityIncludingStored([](const Objects::PWorldEntity&e)
					{
						e->ReSeedRandom();
					});

					ProcessAICoverage();

					Journal::currentRound.roundNumber = RuntimeConfig::roundNumber;
					Journal::currentRound.evolver = Game::evolver;
					Reflection::CheckAllSaveGameVariables();
					Journal::currentRound.AppendEntityVerification();

					//Journal::globalWriter.CommitAndClear(Journal::currentRound);
					Journal::currentRound.Clear();

					foreach (Network::activePlayersAndSpectators,p)
						(*p)->wantNewRound = false;

					Game::evolver->BeginRoundProcessing();
					Game::ProcessChanges(true);
				}

			}

			if (RuntimeConfig::phase == RuntimeConfig::Phase::AnimationPhase)
			{
				if (timer.GetSecondsSince(Game::lastPhaseChange) >= Game::effectiveAnimationLength+0.5f)
				{
					if (Network::AnyHumanPlayersActive())
						Game::roundTimerPaused = false;
					Game::evolver->FinishRoundProcessing();
				}
			}
			else if (!RuntimeConfig::IsInAnyCommandPhase())
			{
				if (!Game::evolver->ContinueRoundProcessing())
				{
					float animationLength = Game::evolver->animationLength;
					Game::effectiveAnimationLength = std::min(10.f,animationLength);
					Game::Network::ChangePhase(RuntimeConfig::Phase::AnimationPhase,std::max(1.f,animationLength / 10.f));
				}
				Game::ProcessChanges(true);
			}



			Game::ProcessChanges(false);





			//#ifndef _DEBUG
			{
				static Buffer0<TCP::PPeer>	disconnect;
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


				static Buffer0<Network::PClient>	clientDump;
				for (const auto&p : Network::activePlayersAndSpectators)
				{
					if (p->peer.expired())
					{
						clientDump << p;
					}
				}

				for (const auto&p : clientDump)
				{
					LogEvent( EventClass( EventSignificance::ClientFatal, EventFlavor::ServerPrivate ),nullptr,"Peer lost: "+p->ToString());
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

