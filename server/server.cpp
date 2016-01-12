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

UINT32 globalVersionOfCurrentFile = 0;	//file version being loaded right now


TUniqueID	serverID;

Random		random;


#ifdef _DEBUG
	bool	debugCheckProgress = false;
#endif




void	SignalNewMobileEntity(const Objects::PMobileEntity&_v, bool restoredFromStorage, float atTime)
{
	State::PClient client;
	State::playerMap.query(_v->owner,client);
		//return;
	const Objects::MobileEntity&v = *_v;


	Protocol::Timestamp timestamp(RuntimeConfig::IsInAnyCommandPhase(),atTime);
	if (client)
	{
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
	}

	if (_v->IsVehicle() && To<Objects::Vehicle>::Cast(_v)->turret->detectsResources)
	{
		State::UpdateResourceCoverage(_v->owner, _v->location,_v->GetScanningRange(),timestamp);
	}

	if (State::Config::basesRequired && v.GetConstructionSpeed() && !v.owner->hasTerraformer)
	{
		LogEvent(EventType::SessionRelated,_v.get(),"Recognized as terraformer. Reevaluating survivial conditions");
		v.owner->hasTerraformer = true;
		String whyNot;
		bool canLive = v.owner->CanLive(whyNot);
		
		if (canLive)	//conditions changed
		{
			if (v.owner->deathInRounds > 0)
			{
				v.owner->deathInRounds = 0;
				if (client)
					client->SendProgressUpdate(false,whyNot);
			}
		}
		else
			LogEvent(EventType::SessionRelated,_v.get(),"Nope: "+whyNot);
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
	Protocol::Timestamp timestamp(RuntimeConfig::IsInAnyCommandPhase(),atTime);
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
	Protocol::Timestamp timestamp(RuntimeConfig::IsInAnyCommandPhase(),atTime);

	if (e->IsMobile())
	{
		Objects::PMobileEntity v = To<Objects::MobileEntity>::Cast(e);
	//	TCP::Serial<Protocol::VehicleConstruction>	w;
		TCP::Serial<Protocol::NewMobileEntity>	w;
		State::Describe(w.Ref(),*v,false,timestamp,0xFFFFFFFF);
		w.Seal();
		Channels::newMobileEntity.SendTo(client->peer.lock(),w);
	}
	else
	{
		Objects::PStructure s = To<Objects::Structure>::DynamicCast(e);
		ASSERT1__(s,e->GetName());
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
	self.timestamp.rightNow = RuntimeConfig::IsInAnyCommandPhase();
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
	de.timestamp.rightNow = RuntimeConfig::IsInAnyCommandPhase();

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
		State::UpdateResourceCoverage(v->owner,v->location,v->GetScanningRange(),motion.timestamp);
	}


	foreach (State::clients,cl)
	{
		TCP::PPeer peer = (*cl)->peer.lock();
		if (peer && Receiver::CanSee(*cl,v,t0))
		{
			LogEvent(EventType::MinorEntityProcess,(*cl)->ToString()+" reported seeing motion of "+v->GetName()+" at time "+String(t0)+"+"+String(timeNeeded) );

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
	if (RuntimeConfig::phase == RuntimeConfig::Phase::EndOfGame)
		return;
	if (RuntimeConfig::gameGoal == State::Config::Goal::ConquerNexus || RuntimeConfig::gameGoal == State::Config::Goal::EMPConquerNexus)
	{
		if (f->nexusControl >= State::Config::GetTargetNexusCount())
		{
			State::ChangePhase(RuntimeConfig::Phase::EndOfGame);
			State::winner = f;

			foreach (State::activePlayers,p)
			{
				(*p)->SendProgressUpdate(true,"<null>");
			}
		}
		else
		{
			State::PClient client;
			if (State::playerMap.Query(f,client))
			{
				String why;
				if (State::Config::basesRequired && f->deathInRounds > 0)
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

	//static GenericHashSet<Objects::OwnerParameter,Objects::OwnerHash>	map;
	static Buffer0<Objects::OwnerParameter>	canSee;

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
			if (f->GridCanSee(volume) || f->CountSatelliteCoverage(explosionSphere))
			{
				canSee.Append(f);
			};
		});
	}
		//Objects::ForeachWorldEntity([center,explosionRadius](const Objects::PWorldEntity&e)
		//{
		//	if (!map.IsSet(e->owner) && e->CanSee(center,explosionRadius))
		//		map.Set(e->owner);
		//});


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

	for (const Objects::OwnerParameter&p : canSee)
	//map.VisitAllKeys([explosion,attackFrom,roughAttack, hasTrajectory](const Objects::OwnerParameter&p)
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
	}
	//);
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
				State::AssembleGameInfo(info.gameInfo);

				infoSender.SendTo(server,info);
			}
			server.EndService();
		}
	}		updater;


	void		Create(UINT16 basePort)
	{
		server.InstallChannel(pingChannel);


		UINT16 port = basePort + PortOffset::Info;

		if (!server.StartService(port))
			FATAL__("Unable to register information server. Chances are, a server is already running on port "+String(port)+".")
		else
		{
			LogEvent(EventType::SessionRelated,"Info server started on port "+String(port));
			updater.start();
		}



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



static Objects::PNexusPort ClosestPort(const Objects::PWorldEntity&e, const Objects::PNexusPort cornerPorts[12])
{
	float d2 = std::numeric_limits<float>::max();
	Objects::PNexusPort rs;
	for (int i = 0; i < 12; i++)
	{
		if (!cornerPorts[i])
			continue;
		float nd2 = Vec::quadraticDistance(e->system.w.xyz,cornerPorts[i]->system.w.xyz);
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
	if (State::Config::Nexus::enableAI
		//#ifdef _DEBUG
		//	|| State::Config::Nexus::haveBases
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
				if (RuntimeConfig::roundNumber - p->lastSpawnRound < 5 || p->owner != Objects::Faction::nexus)
					continue;	//not allowed to spawn anything for the moment

				float threatLevel = sqr(hostileNearNexus[i] )*State::Config::Nexus::threatEstimationFactor
						*1e-3f;
				LogEvent(EventType::Debug, "Threat level for " + ports[i]->GetName() + " considered " + String(threatLevel));
				if (State::Config::Nexus::haveBases)
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
								Objects::PMobileEntity v = plant->SpawnFinishedProductionAnywhere();
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
									index_t choice = vplant->localRandom.getIndex(State::Config::Nexus::vehicleTurrets.Count() - 1);
									//random.getIndex(State::Config::Nexus::vehicleTurrets.Count() - 1);
									ASSERT_LESS__(choice,State::Config::Nexus::vehicleTurrets.Count());
									//Objects::Component::Turret* turret = State::Config::Nexus::vehicleTurrets[choice];
									//Objects::MobileEntityProducer::WorkItem item = vplant->EnqueueProductionOfVehicle(&Objects::Component::hoverbase,turret);
									//Objects::PVehicle v = To<Objects::Vehicle>::Cast(item.producing);
									ConstName	parameters[2] = {ConstName::VehicleBase_HoverBase, State::Config::Nexus::vehicleTurrets[choice]->constName};
									LogEvent(EventType::MinorEntityProcess,vplant.get(),"Enqueueing vehicle production job due to high threat level");
									Journal::currentRound.RegisterProductionJobStart(true,vplant,plant,parameters,2);
									
									EstimatePresence(*vplant->queue.last().producing,vplant->location,hostileNearNexus,constructorNearNexus,friendlyNearNexus);
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
						Objects::Component::Turret* turret = State::Config::Nexus::vehicleTurrets[random.getIndex(State::Config::Nexus::vehicleTurrets.Count() - 1)];
						Objects::PVehicle v = Objects::SpawnVehicle(Objects::Faction::nexus, location, &Objects::Component::hoverbase, turret, true, 0);
						Journal::currentRound.LogEntityCreation(v);
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

		if (State::Config::Nexus::enableAI)
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



bool ApplyCoverageChange(const Objects::PFaction&faction)
{
	return State::CheckCoverageSendTerrain(faction,TCP::PPeer());
}


namespace ParameterConfig
{
	UINT16 basePort = DEFAULT_SERVER_BASE_PORT;
	
	enum mode_t
	{
		Idle,
		ExpectPort

	}		mode = Idle;


	String	loadFile;

	void	Process(const char*arg)
	{
		switch (mode)
		{
			case Idle:
				if (!strcmp(arg,"test"))
					inTestMode = true;
				elif (!strcmp(arg,"port"))
					mode = ExpectPort;
				else
					loadFile = arg;
			return;
			case ExpectPort:
				if (!convert(arg,basePort))
				{
					LogEvent(EventType::ConfigError,"Unable to convert '"+String(arg)+"' to a valid port number");
				}
				else
				{
					State::Config::sessionPort = basePort + PortOffset::Session;
					LogEvent(EventType::ConfigRelated,"Base port redefined as "+String(basePort)+", session port set to "+String(State::Config::sessionPort));
				}
				mode = Idle;
			return;
		}
	}


}


int main( int argc, const char* argv[])
{
	

	onFatal = MyFatal;


	try
	{

		RuntimeConfig::InitConfigDir("server");

		TCP::onIgnoredException = [](const TCodeLocation&loc, const std::exception&ex)
		{
			LogEvent(EventType::Debug,"TCP exception: "+String(loc.ToString())+": "+ex.what());
		};
		TCP::onIgnoredArbitraryException = [](const TCodeLocation&loc)
		{
			LogEvent(EventType::Debug,"TCP exception (arbitrary): "+String(loc.ToString()));
		};

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

		Evolver::Create();
		State::evolver = Evolver::GetDefault();

		Journal::Create();

		for (int i = 1; i < argc; i++)
			ParameterConfig::Process(argv[i]);

		RemoteControl::Start(ParameterConfig::basePort);



		#ifdef _DEBUG
			//inTestMode = true;
		#endif

		if (!inTestMode)
		{
			ASSERT__(FileSystem::CreateDirectory(RuntimeConfig::ConfigFile("logs")));
			Logging::Begin(RuntimeConfig::ConfigFile(PathString("logs/server_"+Logging::TimeStamp(true)+".log")));
			LogEvent(EventType::SessionRelated,nullptr,"Starting server v" PRODUCT_VERSION);
		}



		Grid::Create0();
		Objects::Create();
		Grid::Create1();


		Reflection::Create();
		if (!inTestMode)
			Reflection::Load(false);


		//Reflection::CreatePrototypeDeclaration();


		Grid::currentPlanetConfig.RandomizeSeeds(random);
		Grid::currentPlanetConfig.RandomizeSun(random);


		Grid::BuildWorld(Grid::currentPlanetConfig,State::Config::worldDiscovery);


		if (State::Config::Nexus::enableAI)
			Objects::Faction::nexus->currentState = Grid::initialWorld;

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



		if (inTestMode)
		{
			std::cout << "Running tests..." << std::endl;

			{
				Test::Run(Test::ServerClient());
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





		if (RuntimeConfig::gameGoal == State::Config::Goal::EMPConquerNexus)
		{
			LogEvent(EventType::SessionRelated, "Spawning Nexus Ports...");
			for (int pole = 0; pole < 12; pole++)
				State::SpawnNexusPortAtPole(pole);
			if (State::Config::Nexus::haveBases)
				for (int pole = 0; pole < 12; pole++)
					if (BaseGrid::IsIsolatedCorner(pole))
					{
						State::SpawnNexusBaseAtPoleWithPort(pole);
					}
		}


		if (RuntimeConfig::gameGoal == State::Config::Goal::ConquerNexus)
		{
			LogEvent(EventType::SessionRelated, "Spawning Nexus Ports...");
		
			for (int pole = 0; pole < 12; pole++)
				if (BaseGrid::IsIsolatedCorner(pole))
				{
					State::SpawnNexusPortAtPole(pole);
					if (State::Config::Nexus::haveBases)
						State::SpawnNexusBaseAtPoleWithPort(pole);
				}
		}




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




		{
			UUID id;
			UuidCreate(&id);
			ASSERT_EQUAL__(sizeof(serverID),sizeof(TUniqueID));
			memcpy(serverID.bytes,&id,sizeof(UUID));
		}
		UDPBroadCast::Start(ParameterConfig::basePort);

		State::GenerateWorldIdAndStartServer();

		InformationProvider::Create(ParameterConfig::basePort);

		if (false)
		{
			Logging::WriteEvent(EventType::SessionRelated,"Testing server.",nullptr,nullptr);

			State::Save(PathString("test0."+State::savFileExtension));
			State::Load(PathString("test0."+State::savFileExtension));
			State::Save(PathString("test1."+State::savFileExtension));

			Riff::File a,b;
			ASSERT__(a.Open(PathString("test0."+State::savFileExtension).c_str()));
			ASSERT__(b.Open(PathString("test1."+State::savFileExtension).c_str()));
			Buffer<Riff::TID>	stack;
			State::Compare(a,b,stack);

		}


		if (argc > 1)
		{
			PathString filename = argv[1];
			if (FileSystem::IsFile(filename))
			{
				State::Load(filename);
			}
			else
				Logging::WriteEvent(EventType::SessionRelated,"Specified file ("+String(filename)+") does not exist. Skipping loading operation.",nullptr,nullptr);
		}

		//Journal::globalWriter.InitNew(RuntimeConfig::ConfigFile("saves/log"));
		//Journal::globalWriter.ContinueExisting(RuntimeConfig::ConfigFile("saves/log"));

		Logging::WriteEvent(EventType::SessionRelated, "Preparation of world '" + State::worldID.ToString() + "' completed.", nullptr, nullptr);
		Logging::WriteEvent(EventType::SessionRelated,"Server "+serverID.ToString()+"/"+UDPBroadCast::packet.version+" is online.",nullptr,nullptr);
		State::RefreshWindowTitle();

	
		LogConfig("AI is configured as follows:");
		if (State::Config::Nexus::enableAI)
		{
			LogConfig(" -Nexus-AI enabled");
			LogConfig(" -ThreatEstimationFactor=" + String(State::Config::Nexus::threatEstimationFactor));
			//LogConfig(" -NexusTurrets=" + implode(',', State::Config::Nexus::vehicleTurretNames));
		}
		else
			LogConfig(" -Disabled");

		LogConfig("This game is configured as follows:");
		#ifndef NO_HIBERNATION
			LogConfig(" -Uplink traffic per faction is limited to "+String(State::Config::uplinkBandwidth));
		#endif

		LogConfig(" -Factions: "+State::Config::FactionSupportFlagString());

		if (State::Config::simulateLiquids)
			LogConfig(" -Liquids: Expanding.");
		else
			LogConfig(" -Liquids: Static.");
		switch (RuntimeConfig::gameGoal)
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
		if (State::Config::Nexus::haveBases)
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
		if (State::Config::Nexus::enableAI)
		{
			AIProcess::Begin(Objects::Faction::nexus->accessToken);
		}

		State::ResetFactionStartMineralValue();


		while (!application_shutting_down)
		{
			if (RuntimeConfig::IsInAnyCommandPhase() || RuntimeConfig::phase == RuntimeConfig::Phase::AnimationPhase)
				Sleep(1);
			State::server.Resolve();
			if (State::Config::Nexus::enableAI)
				AIProcess::Check();


			RuntimeConfig::Phase p0 = RuntimeConfig::phase;

			//bool anythingChanged = false;

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
				count_t numFactions=0, wantNewRound = 0;
				bool hasNexus = false,nexusWantsNewRound = false;
				foreach(State::activePlayers, p)
				{
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
					LogEvent(EventType::SessionRelated,"All players have requested a new round. Commencing...");
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
				Journal::currentRound.evolver = State::evolver;
				Reflection::CheckAllSaveGameVariables();
				Journal::currentRound.AppendEntityVerification();

				//Journal::globalWriter.CommitAndClear(Journal::currentRound);
				Journal::currentRound.Clear();

				foreach (State::activePlayers,p)
					(*p)->wantNewRound = false;

				State::evolver->BeginRoundProcessing();
				State::ProcessChanges(true);
			}

			if (RuntimeConfig::phase == RuntimeConfig::Phase::AnimationPhase)
			{
				if (timer.GetSecondsSince(State::lastPhaseChange) >= State::effectiveAnimationLength+0.5f)
				{
					if (State::activePlayers.IsNotEmpty())
						State::roundTimerPaused = false;
					State::evolver->FinishRoundProcessing();
				}
			}
			else if (!RuntimeConfig::IsInAnyCommandPhase())
			{
				if (!State::evolver->ContinueRoundProcessing())
				{
					float animationLength = State::evolver->animationLength;
					State::effectiveAnimationLength = std::min(10.f,animationLength);
					State::ChangePhase(RuntimeConfig::Phase::AnimationPhase,std::max(1.f,animationLength / 10.f));
				}
				State::ProcessChanges(true);
			}



			State::ProcessChanges(false);





			//#ifndef _DEBUG
				foreach (State::clients,cl)
				{
					TCP::PPeer peer = (*cl)->peer.lock();
					if (peer)
					{
						if (timer.GetSecondsSince(peer->GetLastPackageTime()) > 30.f)
						{
							LogEvent(EventType::SessionRelated,*peer,"Client ping timeout (30 sec) exceeded. Disconnecting client");
							peer->DisconnectPeer();
							break;	//loop breaks
						}
					}
					else
					{
						LogEvent(EventType::SessionRelated,nullptr,"Peer lost: "+(*cl)->ToString());
						State::HandleConnectionLoss(*cl);
						break;	//loop breaks
					}
		
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

