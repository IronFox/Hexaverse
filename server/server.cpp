// server.cpp : Defines the entry point for the console application.
//

#include "./components/stdafx.h"

#include "../shared/grid.h"
#include "../shared/objects.h"
#include "../shared/sphereGrid.h"

#include "../client/version.h"
#include "../shared/protocol.h"

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


UINT32 globalVersionOfCurrentFile = 0;	//file version being loaded right now


TUniqueID	serverID;

Random		random;


#ifdef _DEBUG
	bool	debugCheckProgress = false;
#endif




void	SignalNewMobileEntity(const Objects::PMobileEntity&_v, bool restoredFromStorage, float atTime)
{
	State::PClient client;
	if (!State::playerMap.query(_v->owner,client))
		return;
	const Objects::MobileEntity&v = *_v;

	Protocol::Timestamp timestamp(State::phase == Protocol::Phase::CommandPhase,atTime);
	if (!restoredFromStorage)
	{
		TCP::Serial<Protocol::NewMobileEntity>	w;
		State::Describe(w.Ref(),v,true,timestamp,0xFFFFFFFF);
		w.Seal();

		Channels::newMobileEntity.SendTo((client)->peer.lock(),w);
	}
	else
	{
		Protocol::MobileState state;
		State::Describe(state,(const Objects::MobileEntity&)v,true,timestamp,0xFFFFFFFF);
		Channels::setMobileState.SendTo(client->peer.lock(),state);
	}
	if (_v->IsVehicle() && To<Objects::Vehicle>::Cast(_v)->turret->detectsResources)
	{
		client->UpdateResourceCoverage(_v->location,_v->GetScanningRange(),timestamp);
	}

	if (State::Config::basesRequired && v.GetConstructionSpeed() && !v.owner->hasTerraformer)
	{
		LogEvent(EventType::SessionRelated,_v.get(),"Recognized as terraformer. Reevaluating survivial conditions");
		v.owner->hasTerraformer = true;
		bool canLive = v.owner->CanLive();
		
		if (canLive)	//conditions changed
		{
			State::PClient client;
			if (State::playerMap.Query(v.owner,client))
			{
				if (v.owner->deathInRounds > 0)
				{
					v.owner->deathInRounds = 0;
					if (client)
						client->SendProgressUpdate(false);
				}
			}
		}

	}


	//foreach (State::clients,cl)
	//{
	//	TCP::PPeer peer = (*cl)->peer.lock();
	//	if (peer && Receiver::CanSee(*cl,_v))
	//		Channels::createVehicle.SendTo(peer,Receiver::HasAccess(*cl,_v) ? w2 : w);
	//}
}

void	SignalNewStructure(const Objects::PStructure&s, float atTime)
{
	const Objects::Structure&v = *s;

	State::PClient*cl = State::playerMap.QueryPointer(v.owner);
	if (!cl)
		return;


	TCP::Serial<Protocol::StructureConstruction>	w;
	Protocol::Timestamp timestamp(State::phase == Protocol::Phase::CommandPhase,atTime);
	State::Describe(w.Ref(),*s,true,timestamp);
	//State::Describe(w2.Ref(),s,true,&parent);
	w.Seal();
	//w2.Seal();

	Channels::createStructure.SendTo((*cl)->peer.lock(),w);

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
	for (const auto&p : State::activePlayers)
	{
		if (p->CanSee(e,t) || p->faction == target)
		{
			TCP::PPeer peer = p->peer.lock();
			DBG_ASSERT1__(peer,p->ToString());
			if (peer)
			{
				LogEvent(EventType::MinorEntityProcess,*peer,e,"Reported seeing ownership change (-> '"+target->GetName()+"')");
				Channels::changeEntityOwner.SendTo(peer,Protocol::ChangeEntityOwner(e->id,target->id));
			}
		}
	}
}


void	SignalAppearanceToHostileFaction(const Objects::OwnerParameter&target, const Objects::PWorldEntity&e, float atTime)
{
	if (!e)
	{
		LogEvent(EventType::InternalFault,nullptr, "Trying to show null");
		return;
	}
	State::PClient client;
	if (!State::playerMap.query(target,client))
		return;
	//if (!client->ShowNew(e))
		//return;	//already knows this
	Protocol::Timestamp timestamp(State::phase == Protocol::Phase::CommandPhase,atTime);

	Objects::PVehicle v = To<Objects::Vehicle>::DynamicCast(e);
	if (v)
	{
	//	TCP::Serial<Protocol::VehicleConstruction>	w;
		TCP::Serial<Protocol::NewMobileEntity>	w;
		State::Describe(w.Ref(),*v,false,timestamp,0xFFFFFFFF);
		w.Seal();
		Channels::newMobileEntity.SendTo(client->peer.lock(),w);
	}
	else
	{
		Objects::PStructure s = To<Objects::Structure>::DynamicCast(e);
		ASSERT__(s);
		TCP::Serial<Protocol::StructureConstruction>	w;
		State::Describe(w.Ref(),*s,false,timestamp);
		w.Seal();
		Channels::createStructure.SendTo(client->peer.lock(),w);
	}
	LogEvent(EventType::MinorEntityProcess,*client->peer.lock(),e,"Signalling appearance "+(timestamp.rightNow ? "(now)" : " at time="+String(atTime)));
	
}



void	SignalRemoval(const Objects::PWorldEntity&e, Objects::RemovalType t, float atTime)
{
	if (!e)
	{
		LogEvent(EventType::InternalFault,nullptr, "Trying to remove null");
		return;
	}
	Protocol::DestroyEntity self,others;
	self.id = e->id;
	self.type = t;
	self.timestamp.atTime = atTime;
	self.timestamp.rightNow = State::phase == Protocol::Phase::CommandPhase;
	others = self;
	bool sendToOthers = true,sendToSelf = true;
	switch (t)
	{
		case Objects::RemovalType::DestroyedInStorage:
			sendToOthers = false;
		break;
		case Objects::RemovalType::EnteredStorage:
			sendToSelf = false;
			//bleed into:
		case Objects::RemovalType::BeingReplaced:
		case Objects::RemovalType::Vanished:
			others.type = Objects::RemovalType::Vanished;
		break;
		case Objects::RemovalType::ControlledExplosion:
		case Objects::RemovalType::UncontrolledExplosion:
			others.type = Objects::RemovalType::UncontrolledExplosion;
		break;
		default:
			FATAL__("Unrecognized removal type "+String(t.ToString()));
	}

	foreach (State::clients,cl)
	{
		TCP::PPeer peer = (*cl)->peer.lock();
		if (peer && Receiver::CanSee(*cl,e,atTime))
		{
			if (
				((*cl)->faction != e->owner && !sendToOthers)
				||
				((*cl)->faction == e->owner && !sendToSelf)
				)
				continue;
			const Protocol::DestroyEntity&send = (*cl)->faction == e->owner ? self : others;
			LogEvent(EventType::MinorEntityProcess,(*cl)->ToString()+" reported seeing removal of "+e->GetName()+" at time "+String(atTime) );
			#ifdef _DEBUG
				if ((*cl)->faction != e->owner)
				{
					Objects::PWorldEntity seeingThis = e->seenBy.first();

					LogEvent(EventType::MinorEntityProcess," as seen by "+seeingThis->GetName() );
				}
			#endif


			//(*cl)->Hide(e);
			Channels::destroyEntity.SendTo(peer,send);
		}
	}

	
}

void	SignalDisappearanceToHostileFaction(const Objects::OwnerParameter&target, const Objects::PWorldEntity&e, float atTime)
{
	if (!e)
	{
		LogEvent(EventType::InternalFault,nullptr, "Trying to hide null");
		return;
	}
	State::PClient client;
	if (!State::playerMap.query(target,client))
		return;
	Protocol::DestroyEntity de;
	de.id = e->id;
	de.type = Objects::RemovalType::Vanished;
	de.timestamp.atTime = atTime;
	de.timestamp.rightNow = State::phase == Protocol::Phase::CommandPhase;

	//LogEvent(EventType::MinorEntityProcess,*client->peer.lock(),e,"Signalling disappearance");
	LogEvent(EventType::MinorEntityProcess,*client->peer.lock(),e,"Signalling disappearance "+(de.timestamp.rightNow ? "(now)" : " at time="+String(atTime)));

	Channels::destroyEntity.SendTo(client->peer.lock(),de);
}




void	SignalMotion(const Objects::PMobileEntity&v, const Grid::LCellIdI&from,const Grid::LCellIdI&to, float t0, float timeNeeded)
{
	if (!v)
	{
		LogEvent(EventType::CommandRejected,nullptr, "Trying to move null");
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
		State::PClient client;
		if (State::playerMap.query(v->owner,client))
		{
			client->UpdateResourceCoverage(v->location,v->GetScanningRange(),motion.timestamp);
		}
	}


	foreach (State::clients,cl)
	{
		TCP::PPeer peer = (*cl)->peer.lock();
		if (peer && Receiver::CanSee(*cl,v,t0))
		{
			LogEvent(EventType::MinorEntityProcess,(*cl)->ToString()+" reported seeing motion of "+v->GetName()+" at time "+String(t0)+"+"+String(timeNeeded) );
			#ifdef _DEBUG
				if ((*cl)->faction != v->owner)
				{
					Objects::PWorldEntity seeingThis = v->seenBy.first();

					LogEvent(EventType::MinorEntityProcess," as seen by "+seeingThis->GetName() );
				}
			#endif

			Channels::mobileEntityMotion.SendTo(peer,motion);
		}
	}
}

void	SignalTurretRotation(const Objects::PWorldEntity&e, const TVec3<>&toWorld, float rotationTime)
{
	if (!e)
	{
		LogEvent(EventType::CommandRejected,nullptr, "Trying to let null attack");
		return;
	}
	Protocol::EntityTurretRotation rot;
	rot.id = e->id;
	rot.toWorld = toWorld;
	rot.rotationTime = rotationTime;
	rot.timestamp.rightNow = false;

	{
		if (e->IsMobile())
		{
			Objects::PMobileEntity v = To<Objects::MobileEntity>::Cast(e);
			rot.timestamp.atTime = v->timeAt;
		}
		else
		{
			Objects::PTower t = To<Objects::Tower>::DynamicCast(e);
			if (t)
				rot.timestamp.atTime = t->timeAt;
			else
			{
				LogEvent(EventType::CommandRejected,e.get(), "Trying to let incompatible entity attack");
				return;	
			}
		}
	}

	foreach (State::clients,cl)
	{
		TCP::PPeer peer = (*cl)->peer.lock();
		if (peer && Receiver::CanSee(*cl,e,rotationTime))
			Channels::entityTurretRotation.SendTo(peer,rot);
	}
}



void	SignalNexusControlChange(const Objects::PFaction&f)
{
	if (State::phase == Protocol::Phase::EndOfGame)
		return;
	if (State::Config::goal == State::Config::Goal::ConquerNexus || State::Config::goal == State::Config::Goal::EMPConquerNexus)
	{
		if (f->nexusControl >= State::Config::GetTargetNexusCount())
		{
			State::ChangePhase(Protocol::Phase::EndOfGame);
			State::winner = f;

			foreach (State::activePlayers,p)
			{
				(*p)->SendProgressUpdate(true);
			}
		}
		else
		{
			State::PClient client;
			if (State::playerMap.Query(f,client))
			{
				client->SendProgressUpdate(false);
			}
			return;
		}
	}
}


/*
	if (!e)
	{
		LogEvent(EventType::CommandRejected,nullptr, "NULL attempting attack");
		return;
	}
	LogEvent(EventType::MinorEntityProcess,e.get(),"Attacking "+traceTo.ToString()+" in "+String(flightTime)+" seconds");
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
				LogEvent(EventType::CommandRejected,e.get(), "Incompatible entity attempting attack");
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


void	SignalAttack(const Objects::PWorldEntity&e, const Grid::LCellIdI&traceTo, float flightTime)
{
	if (!e)
	{
		LogEvent(EventType::CommandRejected,nullptr, "NULL attempting attack");
		return;
	}
	LogEvent(EventType::MinorEntityProcess,e.get(),"Attacking "+traceTo.ToString()+" in "+String(flightTime)+" seconds");
	Protocol::AttackTrajectory preciseAttack;
	float zOffset;
	Objects::PTurretBase base = e->GetTurretBase(zOffset);
	ASSERT__(base);
	preciseAttack.trajectoryType = base->turret->trajectoryType;
	preciseAttack.turretType = base->turret->constName;
	preciseAttack.traceTo = traceTo;
	preciseAttack.turretRange = base->GetFlatAttackRange();

	preciseAttack.timestamp.rightNow = false;
	const float3 traceToWorld = Grid::ToWorld(traceTo);

	const TVec3<>&d = Grid::low.cells[e->location.stackID].centerVertex.xyz;
	preciseAttack.sourcePosition = d * (zOffset + Grid::ToRadius(e->location.height));
	//float3 world = Grid::ToWorld(e->location);
	//attack.timestamp.atTime = v->timeAt;
	
	{
		if (e->IsMobile())
		{
			Objects::PMobileEntity v = To<Objects::MobileEntity>::Cast(e);
			preciseAttack.timestamp.atTime = v->timeAt;
		}
		else
		{
			Objects::PTower t = To<Objects::Tower>::DynamicCast(e);
			if (t)
				preciseAttack.timestamp.atTime = t->timeAt;
			else
			{
				LogEvent(EventType::CommandRejected,e.get(), "Incompatible entity attempting attack");
				return;	
			}
		}
	}

	preciseAttack.flightTime = flightTime;

	for (auto cl : State::clients)
	{
		TCP::PPeer peer = (cl)->peer.lock();
		if (!peer)
			continue;
		if (Receiver::CanSee(cl,e,preciseAttack.timestamp.atTime))
			Channels::attackTrajectory.SendTo(peer,preciseAttack);
	}
}


void	SignalExplosion(const Objects::PWorldEntity&attackFrom, const Grid::LCellIdI&traceTo, float flightTime, const TVec3<>&center,float explosionRadius,bool isEMP, bool isDoomsday, float atTime)
{
	Protocol::Explosion explosion;
	explosion.center = center;
	explosion.radius = explosionRadius;
	explosion.isEMP = isEMP;
	explosion.timestamp.atTime = atTime;
	explosion.timestamp.rightNow = false;

	static GenericHashSet<Objects::OwnerParameter,Objects::OwnerHash>	map;

	bool allSee = isDoomsday && Objects::RuntimeConfig::doomsdayWeapons == Objects::RuntimeConfig::DoomsdayWeapons::Expose;

	map.Clear();
	if (allSee)
	{
		//just set all. I'm lazy, and these are rare
		foreach (Objects::Faction::all, f)
			map.Set(f->lock());
		map.Set(Objects::Faction::nexus);
	}
	else
		Objects::ForeachWorldEntity([center,explosionRadius](const Objects::PWorldEntity&e)
		{
			if (!map.IsSet(e->owner) && e->CanSee(center,explosionRadius))
				map.Set(e->owner);
		});


	Protocol::AttackTrajectory	roughAttack;
	bool hasTrajectory = false;
	if (attackFrom)
	{
		hasTrajectory = true;
		LogEvent(EventType::MinorEntityProcess,attackFrom.get(),"Tracing rough trajectory to "+Vec::toString(center)+" in "+String(flightTime)+" seconds");
		float zOffset;
		Objects::PTurretBase base = attackFrom->GetTurretBase(zOffset);
		ASSERT__(base);
		roughAttack.turretType = base->turret->constName;
		roughAttack.traceTo = traceTo;

		roughAttack.timestamp.rightNow = false;
		roughAttack.timestamp.atTime = atTime - flightTime;
		roughAttack.flightTime = flightTime;
		roughAttack.turretRange = base->GetFlatAttackRange();

		const float3 traceToWorld = Grid::ToWorld(traceTo);

		roughAttack.sourcePosition = attackFrom->system.w.xyz + Grid::low.cells[attackFrom->location.stackID].centerVertex.xyz * zOffset;
		//attack.timestamp.atTime = v->timeAt;
		


		const float range = roughAttack.turretRange;//base->GetFlatAttackRange();
		float radius = range * 0.2f;
		float3 mod = float3(random.getFloat(-1,1),random.getFloat(-1,1),random.getFloat(-1,1)) * radius;
		count_t numIt = 0;
		while (!attackFrom->CanHit(mod + roughAttack.sourcePosition.value)
				//Vec::quadraticDistance(mod + roughAttack.sourcePosition.value, traceToWorld) > sqr(range) 
			&& ++numIt < 1000)
			mod = float3(random.getFloat(-1,1),random.getFloat(-1,1),random.getFloat(-1,1)) * radius;
		ASSERT__(numIt < 1000);
		roughAttack.sourcePosition.value += mod;
	}


	map.VisitAllKeys([explosion,attackFrom,roughAttack, hasTrajectory](const Objects::OwnerParameter&p)
	{
		State::PClient client;
		if (State::playerMap.Query(p,client))
		{
			TCP::PPeer peer = client->peer.lock();
			if (peer)
			{
				if (!client->faction->CanSee(attackFrom,roughAttack.timestamp.atTime) && hasTrajectory)
					Channels::attackTrajectory.SendTo(peer,roughAttack);


				Channels::explosion.SendTo(peer,explosion);




			}
		}
	});
}






namespace InformationProvider
{
	TCP::Server		server;


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
		void	ThreadMain() override
		{
			Protocol::ServerInfo info;
			info.hostInfo = UDPBroadCast::packet;

			while (!application_shutting_down)
			{
				Sleep(1000);
				info.activePhase = State::phase;
				info.numClients = (UINT16)State::clients.Count();
				info.numPlayers = (UINT16)State::activePlayers.Count();
				info.roundNumber = (UINT32)State::roundNumber;
				info.worldID = State::worldID;

				infoSender.SendTo(server,info);
			}
			server.EndService();
		}
	}		updater;


	void		Create()
	{
		server.InstallChannel(pingChannel);
		if (!server.StartService(INFO_PORT))
			FATAL__("Unable to register information server. Chances are, a server is already running.")
		else
			updater.start();


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

	Logging::WriteEvent(EventType::Fatal,message,nullptr,nullptr);

	application_shutting_down = true;	//prevent certain operations in Close()
	ErrMessage(message);
	std::cerr << message << std::endl;
	char confirm[0x100];
	std::cin.getline(confirm,sizeof(confirm));



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

template <typename T>
T	GetPrimitive(Config::Context*context, const String&path)
{
	const Config::Context::Attribute*attrib = context->GetAttrib(path);
	if (!attrib)
	{
		FATAL__("Trying to extract primitive from '"+context->name+"/"+path+"' but item is not defined");
	}
	
	String raw = attrib->value;
	if (raw.endsWith(';'))
		raw = raw.subStringRef(0,raw.length()-1);
	T rs;
	if (!convert(raw.c_str(),rs))
	{
		FATAL__("Trying to extract primitive from '"+context->name+"/"+path+"' but '"+raw+"' cannot be converted to designated type");
	}
	return rs;
}

void GetConfig(Grid::TPlanetConfig::TLayer&layer, Config::Context*context, const String&path)
{
	layer.baseHeight = GetPrimitive<int>(context,path+"/baseHeight");
	layer.heightScale = GetPrimitive<float>(context,path+"/noiseHeightScale");
	layer.frequency = GetPrimitive<float>(context,path+"/frequency");
	layer.dampening = GetPrimitive<int>(context,path+"/dampening");
	ASSERT_IS_CONSTRAINED__(layer.dampening, 0, 10);


}


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
						if (b && b->turret->constructionSpeed)
						{
							constructorNearNexus[i] += Grid::powi(opacity,64);
						}
					}
					else
					{
						if (e.IsVehicle())
							friendlyNearNexus[i] += opacity * impact;
					}
				}
			}
}


void ProcessAICoverage()
{
	if (State::Config::enableNexusAI 
		//#ifdef _DEBUG
		//	|| State::Config::NexusPorts::haveBases
		//#endif

	)
	{
		//LogEvent(EventType::SessionRelated,"Processing AI coverage");
		float	hostileNearNexus[12] = { 0 }, // will assume nexus-ports are where they are supposed to be and use corner locations
			friendlyNearNexus[12] = { 0 },
			constructorNearNexus[12] = {0};

		if (State::Config::startBases)
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
				friendlyNearNexus[i] = State::Config::factionStartMineralValue /*- State::Config::nexusStartMineralValue*/;
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
			if (BaseGrid::IsIsolatedCorner(i) && (State::unlockedEMPZones & (1<<i)))
			{
				Objects::PNexusPort p = ports[i];
				if (State::roundNumber - p->lastSpawnRound < 5 || p->owner != Objects::Faction::nexus)
					continue;	//not allowed to spawn anything for the moment

				float threatLevel = sqr(hostileNearNexus[i] )*State::Config::nexusThreatEstimationFactor
						*1e-3f;
				LogEvent(EventType::Debug, "Threat level for " + ports[i]->GetName() + " considered " + String(threatLevel));
				if (State::Config::NexusPorts::haveBases)
				{
					Objects::RefreshCompoundCoverageIfNeeded(0);

					Objects::Compound&n = p->GetCompound();
					count_t producing = 0,
							existing = 0;
					for (Objects::WWorkingStructure _ws : n.containedStructures)
					{
						Objects::PWorkingStructure	ws = _ws.lock();
						if (!ws)
							continue;
						#ifndef NO_HIBERNATION
							if (ws->ToHibernating()->IsHibernating())
								continue;
						#endif
						if (ws->health < ws->GetMaxHealth())
							friendlyNearNexus[i] -= ws->GetConstructionCost();
						Objects::PMobileEntityProducer plant = To<Objects::MobileEntityProducer>::DynamicCast(ws);
						if (plant)
						{
							existing++;
							if (plant->AwaitsUserInteraction())
							{
								Objects::PMobileEntity v = plant->SpawnFinishedProductionAnywhere();
								if (!v)
								{
									producing++;	//dont count
									continue;
								}
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
						while (producing < existing && (friendlyNearNexus[i] < threatLevel || (constructorNearNexus[i] > 0.2f && friendlyNearNexus[i] < State::Config::factionStartMineralValue + 20.f)))
						{
							for (Objects::WWorkingStructure _ws : n.containedStructures)
							{
								Objects::PWorkingStructure ws = _ws.lock();
								if (!ws)
									continue;
								Objects::PMobileEntityProducer plant = To<Objects::MobileEntityProducer>::DynamicCast(ws);
		//						Objects::MobileEntityProducer*plant = ws->ToProducer();
								Objects::PVehiclePlant vplant = To<Objects::VehiclePlant>::DynamicCast(ws);
								if (plant && vplant && plant->queue.IsEmpty())
								{
									Objects::Component::Turret* turret = State::Config::nexusVehicleTurrets[random.getIndex(State::Config::nexusVehicleTurrets.Count() - 1)];
									Objects::PVehicle v = vplant->Enqueue(&Objects::Component::hoverbase,turret);
									LogEvent(EventType::MinorEntityProcess,vplant.get(),"Enqueueing vehicle "+v->GetTypeName()+" production job due to high threat level");
									EstimatePresence(*v,vplant->location,hostileNearNexus,constructorNearNexus,friendlyNearNexus);
									producing++;
									break;
								}
							}
						}
					}

			
					#ifdef CONNECTOR_LESS
						break;	//only one compound for nexus faction. all done
					#endif

				}
				else
				while (friendlyNearNexus[i] < threatLevel || (constructorNearNexus[i] > 0.2f && friendlyNearNexus[i] < State::Config::factionStartMineralValue + 20.f))
				{
					LogEvent(EventType::Debug, "Spawning nexus unit near " + p->GetName());
					p->lastSpawnRound = State::roundNumber;


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
						Objects::Component::Turret* turret = State::Config::nexusVehicleTurrets[random.getIndex(State::Config::nexusVehicleTurrets.Count() - 1)];
						Objects::PVehicle v = Objects::SpawnVehicle(Objects::Faction::nexus, location, &Objects::Component::hoverbase, turret, true, 0);
						EstimatePresence(*v,v->location, hostileNearNexus,constructorNearNexus, friendlyNearNexus);
					//	Objects::VerifyVisibilityNetwork();
					}
					else
					{
						LogEvent(EventType::Debug, "Unable to spawn nexus vehicle. Not enough space near " + ports[i]->GetName());
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


		Logging::WriteEvent(EventType::SessionRelated,"Closing down",nullptr,nullptr);
		State::server.Shutdown();

		if (State::Config::enableNexusAI)
			AIProcess::End();


		InformationProvider::Destroy();
		Objects::Destroy();
		Grid::Destroy();
		UDPBroadCast::Terminate();

		RemoteControl::Destroy();


		Logging::WriteEvent(EventType::SessionRelated,"Terminating process",nullptr,nullptr);
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
			TerminateNow(false);
		return TRUE;
	
	}
	return FALSE;
}

void LogConfig(const String&msg)
{
	Logging::WriteEvent(EventType::ConfigRelated,msg,nullptr,nullptr);
}


int main( int argc, const char* argv[])
{

	//{
	//	float range = 7.f;

	//	float velocity = Objects::Turret::MaxRangeToVelocity(range);
	//	std::cout << "velocity at "<<range<<" = "<<velocity << std::endl;
	//	//float maxRange = Objects::Turret::VelocityToMaxRangeAtHeight(velocity,0);
	//	float maxHeight = Objects::Turret::VelocityToMaxVerticalHeight( velocity);
	//	std::cout << "max height = "<<maxHeight <<std::endl;


	//	for (float x = 0; x <= 10.f; x++)
	//	{
	//		float h = Objects::Turret::MaxHeightAtHDistance(range,x);
	//		std::cout << "height at "<<x<<" = "<<h<<std::endl;
	//	}

	//	for (float y = maxHeight-10; y <= maxHeight; y++)
	//	{
	//		float maxRange = Objects::Turret::VelocityToMaxRangeAtHeight(velocity,y);
	//		std::cout << "range at "<<y<<" = "<<maxRange<<std::endl;
	//		std::cout << "range2 at "<<y<<" = "<<Objects::Turret::MaxRangeAtHeight(range,y)<<std::endl;

	//		for (float x = fmod(maxRange,1.f); x <= maxRange+1.f; x++)
	//		{
	//			Objects::Turret::TAngles angles = Objects::Turret::GetAngleForDistance(velocity,x,y);
	//			float maxH = Objects::Turret::MaxHeightAtHDistance(range,x);
	//			std::cout << " "<<x;
	//			for (int i = 0; i < angles.numAngles; i++)
	//				std::cout << " "<<(angles.idealAngle[i] * 180.f / M_PIF);
	//			std::cout << " "<<(Objects::Turret::CanHit(range,x,y)?"hit": "no hit");
	//			std::cout << " /y="<<(y <= maxH ?"hit": "no hit");
	//			std::cout << std::endl;

	//		}

	//		

	//	
	//	
	//	}
	//	Buffer0<TVec4<> >	outSteps;
	//	Objects::Turret::TraceTrajectory(velocity,float3(0,0,100),float3(6,0,100),outSteps);
	//	for (auto step : outSteps)
	//	{
	//		std:: cout << "  "<<Vec::toString(step)<<std::endl;
	//	}



	//}

	onFatal = MyFatal;
	SetConsoleCtrlHandler(MyHandlerRoutine,TRUE);

	{
		wchar_t path[MAX_PATH];
		ASSERT__(GetModuleFileNameW(NULL,path,MAX_PATH));
		StringW dir = FileSystem::ExtractFileDir(StringW(path));
		while (dir.length() > 1 &&  !FileSystem::IsFile(dir + StringW(FOLDER_SLASH_STR)+"server.ini"))
		{
			dir = FileSystem::ExtractFileDir(dir);
			LogEvent(EventType::Debug,"Walking up to "+String(dir.c_str()));
		}
//		#ifdef _DEBUG
	//	#endif
		if (FileSystem::IsFile(dir + StringW(FOLDER_SLASH_STR)+"server.ini"))
			FileSystem::SetWorkingDirectory(dir);
		//ShowMessageW(dir);
	}


	NiceName::Load();




	inTestMode = argc > 1 && !strcmp(argv[1],"test");
	#ifdef _DEBUG
		//inTestMode = true;
	#endif


	if (argc > 1 && !strcmp(argv[1],"slaved"))
	{
		//std::cin.read
//		RemoteControl::Start(Protocol::ChannelID::RC_MakeWorld);


	}


	if (!inTestMode)
	{
		ASSERT__(FileSystem::CreateDirectory("logs"));
		Logging::Begin("logs/server_"+Logging::TimeStamp(true)+".log");
		LogEvent(EventType::SessionRelated,nullptr,"Starting server v" PRODUCT_VERSION);

	}



	Grid::TPlanetConfig	pconfig;
	Grid::WorldDiscovery	discovery;

	float atmosphereDensity = 0.5f;




	{
		
		Configuration	config;

		#ifdef _DEBUG
			const char*serverINI = "config/debug_server.ini";
		#else
			const char*serverINI = "config/server.ini";
		#endif
		if (config.LoadFromFile(serverINI))
		{
			State::Config::simulateLiquids = config.GetBool("NonPersistentGameSettings/simulateLiquids",false);

			State::Config::closedGame = config.GetBool("Game/closed",false);
			State::Config::roundTimer = config.GetInt("Game/roundTimer",-1);
			String goalStr = config.GetString("Game/goal","None");
			if (!State::Config::goal.Decode(goalStr.c_str()))
			{
				LogEvent(EventType::ConfigRelated,"Unsupported goal '"+goalStr+"'");
			}
			State::Config::startBases = config.GetBool("Game/startBases",false);
			State::Config::basesRequired = config.GetBool("Game/basesRequired",false);
			State::Config::exposeActionLeftToHostileFactions = config.GetBool("Game/exposeActionLeftToHostileFactions",true);
			State::Config::NexusPorts::haveBases = config.GetBool("Game/NexusPorts/haveBases");
			Objects::newVehiclesHaveFullAmmo = config.GetBool("Game/newVehiclesHaveFullAmmo",true);


			discovery = GetType<Grid::WorldDiscovery>(&config,"Game/worldDiscovery");

			int saveEveryNRounds = config.GetInt("AutoSaves/saveEveryNRounds",10);
			//State::Config::limitMinesTo = config.GetInt("NonPersistentGameSettings/limitMinesTo",10);
			State::Config::uplinkBandwidth = config.GetInt("NonPersistentGameSettings/uplinkBandwidth",500);
			State::Config::saveEveryNRounds  = saveEveryNRounds > 0 ? saveEveryNRounds : (UINT32)-1;
			State::Config::saveWhenAllQuit = config.GetBool("AutoSaves/saveWhenAllQuit", true);

			{
				if (!Objects::RuntimeConfig::doomsdayWeapons.Decode(config.GetString("Game/doomsdayWeapons","").c_str()))
				{
					String allowed = "'";
					for (index_t i = 0; i < Objects::RuntimeConfig::doomsdayWeapons.NumberOfPossibilities; i++)
					{
						if (i)
							allowed+="', '";
						allowed += Objects::RuntimeConfig::DoomsdayWeapons::Reinterpret(i).ToString();
					}
					allowed += "'";
					Objects::RuntimeConfig::doomsdayWeapons = Objects::RuntimeConfig::DoomsdayWeapons();
					LogEvent(EventType::ConfigRelated,"Doomsday configuration invalid. Defaulting to '"+String(Objects::RuntimeConfig::doomsdayWeapons.ToString())+"'. Allowed values are "+allowed);
				}
			}

			{
				Config::Context*ai = config.GetContext("AI");
				if (!ai)
					FATAL__("Configuration file lacks group 'AI'");

				State::Config::enableNexusAI = GetPrimitive<bool>(ai, "nexus");
				State::Config::nexusThreatEstimationFactor = GetPrimitive<float>(ai, "threatEstimationFactor");
				explode(',', ai->GetString("spawnTurrets"), State::Config::nexusVehicleTurretNames);
				if (State::Config::nexusVehicleTurretNames.IsEmpty())
				{
					FATAL__("'AI/spawnTurrets' not defined or empty");
				}
				if (State::Config::goal != State::Config::Goal::EMPConquerNexus && State::Config::goal != State::Config::Goal::ConquerNexus && State::Config::enableNexusAI)
				{
					LogEvent(EventType::ConfigRelated,"Cannot enable Nexus AI unless goal=ConquerNexus or goal=EMPConquerNexus. Disabling AI");
					State::Config::enableNexusAI = false;
				}
			}


			Config::Context*resources = config.GetContext("Game/World/Resources");
			if (!resources)
				FATAL__("Configuration file lacks group Game/World/Resources");
			if (resources)
				for (index_t i = 0; i < Objects::Resource::NumberOfPossibilities; i++)
				{
					Objects::Resource res = Objects::Resource::Reinterpret(i);
					Config::Context*thisResource = resources->GetContext(res.ToString());
					if (thisResource)
					{
						float p = thisResource->GetFloat("Probability",Grid::Resource::probability[i]);
						int max = thisResource->GetInt("Max",Grid::Resource::maximum[i]);
						//if (p != Grid::Resource::probability[i]  || max != Grid::Resource::maximum[i])
						LogEvent(EventType::ConfigRelated,"Imported resource probability for "+String(res.ToString())+": p="+String(p)+", max="+String(max));
						Grid::Resource::probability[i] = p;
						Grid::Resource::maximum[i] = max;
						ASSERT__(max >= 0);
						ASSERT__(p >= 0 && p <= 1);
					}
				}

			//Config::Context*types = config.GetContext("Game/World/Types");
			//if (!types)
			//{
			//	FATAL__("Unable to extract planet types from "+String(serverINI));
			//}

			String type;
			if (inTestMode)
				type = "LushGreen";
			else
			{
				type = config.GetString("Game/World/useType").trim();
				if (type.IsEmpty())
					FATAL__("Game/World/useType not set or empty");
			}
			{
				Configuration planetConfig;
				String path = "./config/planets/"+type+".ini";
				if (planetConfig.LoadFromFile(path))
				{
					atmosphereDensity = planetConfig.GetFloat("atmosphereDensity");
					pconfig.innerCoreType = GetType<Grid::Content>(&planetConfig,"innerCoreType");
					pconfig.extendedCoreType = GetType<Grid::Content>(&planetConfig,"ExtendedCore/type");
					GetConfig(pconfig.extendedCore,&planetConfig,"ExtendedCore");
					GetConfig(pconfig.mountains,&planetConfig,"Mountains");
					pconfig.topType = GetType<Grid::Content>(&planetConfig,"Mountains/top");
					pconfig.innerType = GetType<Grid::Content>(&planetConfig,"Mountains/type");
					pconfig.liquidCoverage = GetType<Grid::Content>(&planetConfig,"Liquid/type");
					pconfig.liquidHeight = GetPrimitive<int>(&planetConfig,"Liquid/level");
					pconfig.liquidProximity = GetType<Grid::Content>(&planetConfig,"Liquid/proximity");
					pconfig.snowHeight = GetPrimitive<int>(&planetConfig, "Mountains/snowHeight");
				}
				else
					FATAL__("Failed to load planet configuration: "+path+" ("+planetConfig.Report()+")");

			}
		}
		else
		{
			LogEvent(EventType::InternalFault,"Unable to parse "+String(serverINI)+":\n"+config.Report());
			LogEvent(EventType::SessionRelated,"Default configuration loaded");
		}






		pconfig.RandomizeSeeds(random);

		pconfig.GenerateAtmosphereColor(atmosphereDensity);
		pconfig.RandomizeSun(random);

		State::persistentPlanetInfo = pconfig;

		Grid::Create0(pconfig,discovery);
		Objects::Create();
		Grid::Create1();


		if (!config.IsEmpty())
		{
			Config::Context*unitLimits = config.GetContext("Game/UnitLimits");
			if (!unitLimits )
				FATAL__("Configuration file lacks group Game/UnitLimits");

			if (unitLimits)
			{
				Objects::RuntimeConfig::UnitLimits::Reset();
				{
					Config::Context*structures = unitLimits->GetContext("Structures");
					if (!structures)
						FATAL__("Configuration file lacks group Game/UnitLimits/Structures");
					if (structures)
					{
						for (const Objects::PStructure&p:Objects::structurePrototypes)
						{
							String key = p->GetTypeName();
							key.replace(' ','_');
							int count = structures->GetInt(key,-1);
							if (count == -1)
							{
								FATAL__("Configuration file lacks entry Game/UnitLimits/Structures/"+key);

								count = 0;
							}
							Objects::RuntimeConfig::UnitLimits::Set(p->GetCTypeName(),count);
						}
					}
				}
				{
					Config::Context*turrets = unitLimits->GetContext("MobileTurrets");
					if (!turrets)
						FATAL__("Configuration file lacks group Game/UnitLimits/MobileTurrets");
					if (turrets)
					{
						for (const auto&c:Objects::Component::allTurretComponents)
						{
							String key = c->typeName;
							key.replace(' ','_');
							int count = turrets->GetInt(key,-1);
							if (count == -1)
							{
								FATAL__("Configuration file lacks entry Game/UnitLimits/MobileTurrets/"+key);

								count = 0;
							}
							Objects::RuntimeConfig::UnitLimits::Set(c->constName,count);
						}

					}
				}
			}

		}
	}



	if (State::Config::enableNexusAI)
		Objects::Faction::nexus->currentState = Grid::initialWorld;

	{
		State::Config::nexusVehicleTurrets.SetSize(State::Config::nexusVehicleTurretNames.Count());
		for (index_t i = 0; i < State::Config::nexusVehicleTurretNames.Count(); i++)
		{
			State::Config::nexusVehicleTurrets[i] = Objects::Component::GetTurretByName(State::Config::nexusVehicleTurretNames[i]);
			if (!State::Config::nexusVehicleTurrets[i])
			{
				FATAL__("Nexus AI: '" + State::Config::nexusVehicleTurretNames[i] + "' is not a valid turret name");
			}
		}
		//State::Config::nexusVehicleTurretNames.Clear();
	}



	if (inTestMode)
	{
		std::cout << "Running tests..." << std::endl;

		{
			Test::Run(Test::HighGridFamilies());
			Test::Run(Test::AutoTargeting());
			Test::Run(Test::Bin2Str());
			Test::Run(Test::Serialization());
			Test::Run(Test::VehiclePlant());
			Test::Run(Test::SkyPort());
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





	if (State::Config::goal == State::Config::Goal::EMPConquerNexus)
	{
		LogEvent(EventType::SessionRelated, "Spawning Nexus Ports...");
		for (int pole = 0; pole < 12; pole++)
			State::SpawnNexusPortAtPole(pole);
		if (State::Config::NexusPorts::haveBases)
			for (int pole = 0; pole < 12; pole++)
				if (BaseGrid::IsIsolatedCorner(pole))
				{
					State::SpawnNexusBaseAtPoleWithPort(pole);
				}
	}


	if (State::Config::goal == State::Config::Goal::ConquerNexus)
	{
		LogEvent(EventType::SessionRelated, "Spawning Nexus Ports...");
		
		for (int pole = 0; pole < 12; pole++)
			if (BaseGrid::IsIsolatedCorner(pole))
			{
				State::SpawnNexusPortAtPole(pole);
				if (State::Config::NexusPorts::haveBases)
					State::SpawnNexusBaseAtPoleWithPort(pole);
			}
	}




	//State::currentState = Grid::cellStack;	//reference for deltas

	//Array<BYTE>	out(Grid::cellStack.contentSize());

	//for (BYTE k = 0; k < BaseGrid::NumFamilies; k++)
	//{
	//	const index_t offset = Grid::high.grid.GetFamilyOffset(k);
	//	const index_t range = Grid::high.grid.CountFamilyMembers(k) * Grid::CellStack::Range;
	//	Timer::Time t0 = timer.Now();
	//	size_t size = BZ2::compress(Grid::cellStack.pointer()+offset,sizeof(Grid::CellStack::Cell)*range,out.pointer(),out.contentSize());
	//	std::cout << (int)k<< " "<<timer.GetSecondsSince(t0)<<std::endl;;;
	//}
	//std::flush(std::cout);
	//ShowMessage(String(size)+"/"+String(Grid::cellStack.contentSize()));




	{
		UUID id;
		UuidCreate(&id);
		ASSERT_EQUAL__(sizeof(serverID),sizeof(TUniqueID));
		memcpy(serverID.bytes,&id,sizeof(UUID));
	}
	UDPBroadCast::Start();

	State::Start();

	InformationProvider::Create();

	if (false)
	{
		Logging::WriteEvent(EventType::SessionRelated,"Testing server.",nullptr,nullptr);

		State::Save("test0."+State::savFileExtension);
		State::Load("test0."+State::savFileExtension);
		State::Save("test1."+State::savFileExtension);

		Riff::File a,b;
		ASSERT__(a.Open(("test0."+State::savFileExtension).c_str()));
		ASSERT__(b.Open(("test1."+State::savFileExtension).c_str()));
		Buffer<Riff::TID>	stack;
		State::Compare(a,b,stack);

	}


	if (argc > 1)
	{
		String filename = argv[1];
		if (FileSystem::IsFile(filename))
		{
			State::Load(filename);
		}
		else
			Logging::WriteEvent(EventType::SessionRelated,"Specified file ("+filename+") does not exist. Skipping loading operation.",nullptr,nullptr);
	}

	Logging::WriteEvent(EventType::SessionRelated, "Preparation of world '" + State::worldID.ToString() + "' completed.", nullptr, nullptr);
	Logging::WriteEvent(EventType::SessionRelated,"Server "+serverID.ToString()+"/"+UDPBroadCast::packet.version+" is online.",nullptr,nullptr);
	State::RefreshWindowTitle();

	
	LogConfig("AI is configured as follows:");
	if (State::Config::enableNexusAI)
	{
		LogConfig(" -Nexus-AI enabled");
		LogConfig(" -ThreatEstimationFactor=" + String(State::Config::nexusThreatEstimationFactor));
		LogConfig(" -NexusTurrets=" + implode(',', State::Config::nexusVehicleTurretNames));
	}
	else
		LogConfig(" -Disabled");

	LogConfig("This game is configured as follows:");
	LogConfig(" -Uplink traffic per faction is limited to "+String(State::Config::uplinkBandwidth));
	if (State::Config::simulateLiquids)
		LogConfig(" -Liquids: Simulated.");
	else
		LogConfig(" -Liquids: Static.");
	switch (State::Config::goal)
	{
		case State::Config::Goal::EMPConquerNexus:
			LogConfig(" -Goal: Connect and enable 2+ Nexus Ports. Watch out for EMP fields.");
		break;
		case State::Config::Goal::None:
			LogConfig(" -Goal: None.");
		break;
		case State::Config::Goal::ConquerNexus:
			static_assert(BaseGrid::IsolatedCornerCount==3,"Unexpected count");
			LogConfig(" -Goal: Connect and enable 2 of 3 Nexus Ports.");
		break;
	}
	if (State::Config::closedGame)
		LogConfig(" -Closed: Joining will be restricted to first round and access tokens. Game can be won.");
	else
		LogConfig(" -Open: Players can join any time. Game is endless.");
	if (State::Config::NexusPorts::haveBases)
		LogConfig(" -Nexus-Bases: Nexus bases will be randomly created during world creation");
	else
		LogConfig(" -No Nexus-Bases: Nexus bases will not be created");

	switch (Objects::RuntimeConfig::doomsdayWeapons)
	{
		case Objects::RuntimeConfig::DoomsdayWeapons::Conceal:
			LogConfig(" -Doomsday weapons: Treated like normal weapons (must be in sensor range to see)");
		break;
		case Objects::RuntimeConfig::DoomsdayWeapons::Disallow:
			LogConfig(" -Doomsday weapons: Forbidden to fire (can still be built, however)");
		break;
		case Objects::RuntimeConfig::DoomsdayWeapons::Expose:
			LogConfig(" -Doomsday weapons: Doomsday explosions are made visible to all factions");
		break;
		default:
			LogConfig(" -Doomsday weapons: "+String(Objects::RuntimeConfig::doomsdayWeapons.ToString()));
		break;
	}

	if (State::Config::roundTimer > 0)
		LogConfig(" -Round Timer: Rounds beyond the first may last at most "+String(State::Config::roundTimer)+" seconds.");
	else
		LogConfig(" -No Round Timer: Rounds end when all players requested so.");
	if (State::Config::basesRequired)
		LogConfig(" -Bases Required: Factions that do not meet the station requirements die after five rounds.");
	else
		LogConfig(" -No Survival Conditions: Factions live forever (although their vehicles/structures might not).");
	if (State::Config::startBases)
		LogConfig(" -Bases: New factions are given minimal bases.");
	else
		LogConfig(" -Flexibility: New factions are given vehicles, but no structures.");
	if (Objects::newVehiclesHaveFullAmmo)
		LogConfig(" -Ammo: New vehicles using standard ammo are given full ammo.");
	else
		LogConfig(" -No Ammo: New vehicles have zero ammo.");
	if (State::Config::exposeActionLeftToHostileFactions)
		LogConfig(" -Expose Vehicle Initiative: Vehicle initiative is exposed to hostile factions.");
	else
		LogConfig(" -Private Vehicle Action: Vehicle movement is not exposed to hostile factions.");

	LogConfig(" -Auto-saves will be created every "+String(State::Config::saveEveryNRounds)+" round(s).");
	if (State::Config::saveWhenAllQuit)
		LogConfig(" -Auto-saves will be created when all quit.");


	//FATAL__("test");
	if (State::Config::enableNexusAI)
	{
		AIProcess::Begin(Objects::Faction::nexus->accessToken);
	}

	State::ResetFactionStartMineralValue();


	while (!application_shutting_down)
	{
		if (State::phase == Protocol::Phase::CommandPhase || State::phase == Protocol::Phase::AnimationPhase)
			Sleep(1);
		State::server.Resolve();
		if (State::Config::enableNexusAI)
			AIProcess::Check();


		Protocol::Phase p0 = State::phase;

		bool anythingChanged = false;

		bool newRoundNow = false;

		if (State::Config::roundTimer > 0 && !State::roundTimerPaused)
		{
			if (timer.GetSecondsSince(State::lastPhaseChange) - 0.5f > State::Config::roundTimer)//tolerance of a half second
			{
				newRoundNow = true;
				LogEvent(EventType::SessionRelated,"Round timeout reached. Forcing new round.");
			}
		}





		if (!newRoundNow)
		{
			count_t wantNewRound = 0;
			bool hasNexus = false;
			foreach(State::activePlayers, p)
			{
				//#ifndef _DEBUG
				if ((*p)->faction == Objects::Faction::nexus)
				{
					ASSERT__(!hasNexus);
					hasNexus = true;
					//wantNewRound++;
					//continue;
				}
				//#endif
				if ((*p)->wantNewRound)
					wantNewRound++;
			}
			newRoundNow = wantNewRound == State::activePlayers.Count() && wantNewRound > (hasNexus ? 1U : 0U);
			if (newRoundNow)
			{
				LogEvent(EventType::SessionRelated,"All players have requested a new round. Commencing...");
			}
		}

		if (newRoundNow)
		{
			anythingChanged = true;
			Objects::ProcessStructures0();
			foreach (State::activePlayers,p)
				(*p)->wantNewRound = false;


			//#ifdef _DEBUG
			foreach (Objects::Faction::all,a)
				a->lock()->DropFrameData();
			Objects::Faction::nexus->DropFrameData();

			Objects::ProcessEMPState(0,[](const Objects::PWorldEntity&e)->bool
			{
				return (Grid::low.cells[e->location.stackID].cornerFlags & State::unlockedEMPZones) != 0;
			});

			ProcessAICoverage();


			State::SimulateLiquidsIfEnabled();
			//#endif

			State::ChangePhase(Protocol::Phase::AttackPhase);
		}

		switch (State::phase)
		{
			case Protocol::Phase::AttackPhase:
				if (!Objects::ProcessNextVehicleAttack())
					State::ChangePhase(Protocol::Phase::MovementPhase);
				anythingChanged = true;
			break;
			case Protocol::Phase::MovementPhase:
				if (!Objects::ProcessVehicleMotion())
				{
					#ifndef NO_HIBERNATION
						State::UpdateHibernation();
					#endif

					float animationLength = 0.f;
					Objects::ForeachMobileEntityIncludingStored([&animationLength](const Objects::PMobileEntity&m)
					{
						animationLength = std::max(m->timeAt,animationLength);
					});
					State::PerformNewRoundLogic(animationLength);
					Objects::ResetTowerAttackAction(animationLength);
					Objects::CheckSatelliteSupplyAndSensory(animationLength,State::terrainChanged);
					State::terrainChanged = false;
					Objects::ProcessStructures1(animationLength);//clears overproduction: must be called after CheckSatelliteSupply()

					State::effectiveAnimationLength = std::min(10.f,animationLength);
					State::ChangePhase(Protocol::Phase::AnimationPhase,std::max(1.f,animationLength / 10.f));
				}
				anythingChanged = true;
			break;
			case Protocol::Phase::AnimationPhase:
				if (timer.GetSecondsSince(State::lastPhaseChange) >= State::effectiveAnimationLength+0.5f)
				{
					State::roundNumber++;
					if (State::activePlayers.IsNotEmpty())
						State::roundTimerPaused = false;


					State::ChangePhase(Protocol::Phase::CommandPhase,State::Config::roundTimer > 0 && !State::roundTimerPaused ? State::Config::roundTimer : -1);



				}
				anythingChanged = true;
			break;
			case Protocol::Phase::CommandPhase:
			break;
		}







	
		BYTE changed = 0,counter=0;
		for (BYTE k = 0; k < BaseGrid::NumFamilies; k++)
		{
			if (Grid::high.family[k].normal.outdated)
				changed ++;
		}
		for (BYTE k = 0; k < BaseGrid::NumFamilies; k++)
		{
			if (Grid::high.family[k].normal.outdated)
				State::SignalFamilyChange(k,++counter == changed,false);
		}
		if (changed)
		{
			State::terrainChanged = true;
		}

		Objects::ForeachMobileEntityIncludingStored([&anythingChanged](const Objects::PMobileEntity&_v)
		{
			Objects::MobileEntity&v = *_v;
			if (v.changeFlags)
			{
				anythingChanged = true;
				//LogEvent(EventType::MinorEntityProcess,&v,"State change detected");	//this says relatively little

				bool othersNeedToKnow = (v.changeFlags & State::NotifyOthers) != 0;

				TCP::Serial<Protocol::MobileState>	w0,w1;
				Protocol::Timestamp timestamp(State::phase == Protocol::Phase::CommandPhase,v.changeTimer);
				State::Describe(w0.Ref(),v,false,timestamp,v.changeFlags);
				State::Describe(w1.Ref(),v,true,timestamp,v.changeFlags);
				w0.Seal();
				w1.Seal();
				if (!othersNeedToKnow)
				{
					State::SendToOwner(Channels::setMobileState,v,w1);
				}
				else
					foreach(State::clients,cl)
					{
						if (Receiver::HasAccess(*cl,v))
							Channels::setMobileState.SendTo((*cl)->peer.lock(),w1);
						else
							if (Receiver::CanSee(*cl,_v,0))
								Channels::setMobileState.SendTo((*cl)->peer.lock(),w0);
					}			
				v.ResetAllChange();
			}
		});

		foreach (Objects::structures, _s)
		{
			Objects::Structure&s = **_s;
			if (s.changeFlags)
			{
				anythingChanged = true;
				LogEvent(EventType::MinorEntityProcess,&s,"State change detected");
				
				bool othersNeedToKnow = (s.changeFlags & State::NotifyOthers) != 0;

				Protocol::Timestamp timestamp(State::phase == Protocol::Phase::CommandPhase,s.changeTimer);

				TCP::Serial<Protocol::StructureState>	w0,w1;
				State::Describe(w0.Ref(),**_s,false,timestamp,s.changeFlags);
				State::Describe(w1.Ref(),**_s,true,timestamp,s.changeFlags);
				w0.Seal();
				w1.Seal();
				if (!othersNeedToKnow)
				{
					State::SendToOwner(Channels::setStructureState,s,w1);
				}
				else
					foreach(State::clients,cl)
					{
						if (Receiver::HasAccess(*cl,s))
							Channels::setStructureState.SendTo((*cl)->peer.lock(),w1);
						else
							if (Receiver::CanSee(*cl,*_s,0))
								Channels::setStructureState.SendTo((*cl)->peer.lock(),w0);
						//Channels::setStructureState.SendTo((*cl)->peer.lock(),Receiver::HasAccess(*cl,s) ? w1 : w0);
					}
				s.ResetAllChange();
			}
		}


		foreach (State::activePlayers,cl)
		{
			//if ((*cl)->faction->global.coverageChanged)
			{
				static Protocol::WorldDelta	delta;
				const Objects::PFaction&faction = (*cl)->faction;

				BYTE numChanged = 0;
				for (BYTE family = 0; family < BaseGrid::NumFamilies; family++)
					if (faction->visibility.families[family].coverageChanged)
						numChanged++;

				if (numChanged)
				{
					anythingChanged = true;
					BYTE at = 0;
					for (BYTE family = 0; family < BaseGrid::NumFamilies; family++)
						if (faction->visibility.families[family].coverageChanged)
						{
							at++;
							State::GenerateWorldDelta(&faction->visibility,&faction->currentState,delta,family,State::phase == Protocol::Phase::CommandPhase,at == numChanged,State::ProgressiveUpdate);
							Channels::updateWorld.SendTo((*cl)->peer.lock(),delta);
						}
				}
			}
		}



		if (!anythingChanged && State::phase == Protocol::Phase::CommandPhase && State::lastSaveRound != State::roundNumber && !(State::roundNumber%State::Config::saveEveryNRounds))
		{
			State::AutoSave();

		}


		#ifndef _DEBUG
			foreach (State::clients,cl)
			{
				if (timer.GetSecondsSince((*cl)->lastPing) > 30.f)
				{
					TCP::PPeer peer = (*cl)->peer.lock();
					if (peer)
					{
						LogEvent(EventType::SessionRelated,*peer,"Client ping timeout exceeded");
						peer->Disconnect();
					}
					else
					{
						State::activePlayers.FindAndErase(*cl);
						LogEvent(EventType::SessionRelated,nullptr,"Client ping timeout exceeded & disconnected: "+(*cl)->ToString());
						State::clients.erase(cl);
						break;	//loop breaks
					}
				}
		
			}
		#endif

		UDPBroadCast::Update();
		//break;
	}
	mainThreadDone.release();

	TerminateNow(true);

	return 0;
}

