#include <global_root.h>
#include "journalSaveGame.h"
#include "state.h"
#include <iostream>
#include <fstream>
#include <io/file_system.h>
#include <objects/gridAndChoices.h>
#include <objects/structure/tower.h>
#include <objects/structure/mine.h>
#include "networkChannels.h"
#include "reflection.h"
#include "aiProcess.h"
#include "serverLogEvent.h"

namespace Journal
{

	Writer globalWriter;
	Round currentRound;

	static bool	lockEvents = false;



	static TCP::PPeer	GetPeerOfFaction(const Objects::PFaction&f)
	{
		Network::PClient client;
		if (Network::playerMap.Query(f,client))
			return client->peer.lock();
		return TCP::PPeer();
	}

	void	Round::Include(Round&other, bool execute)
	{
		ASSERT__(other.roundNumber == roundNumber);
		if (execute)
			other.Apply(false,false);
		
		globalEvents.Append(other.globalEvents);
		other.factionStrands.VisitAllEntries([this](const TUniqueID&key, FactionStrand&strand)
		{
			factionStrands.Set(key).Merge(strand);
		});
	}

	void Round::Apply(bool process, bool updateRoundNumber)
	{
		lockEvents = true;
	
		LogEvent(EventSignificance::Uncommon ,"Applying round "+roundNumber.ToString()+" to state...");

		ApplicationInterface::globalVersionOfCurrentFile = fileDataVersion;
		if (updateRoundNumber)
			RuntimeConfig::roundNumber = roundNumber;

		Reflection::state.PushAndReplaceFocus(Reflection::state.GetRoot());
		foreach (globalEvents,ev)
			(*ev)->Execute();
		Reflection::state.PopFocus();


		factionStrands.VisitAllEntries([this](const TUniqueID&factionID, const FactionStrand&fstrand)
		{
			Objects::PFaction f = Objects::Faction::FindByID(factionID);
			if (!f)
				throw RoundException(CLOCATION, "Round "+roundNumber.ToString()+": Referencing faction '"+ToString(factionID)+"', which does not or no longer exist, and is not being created");

			for (const auto&ev : fstrand.factionEvents)
				ev->Execute(f);

			ASSERT__(f);

			static Ctr::GenericHashSet<TUniqueID>	processed;
			processed.Clear();

			for (index_t retry = 0; retry < fstrand.entityStrands.Count(); retry++)
			{
				count_t begin = processed.Count();
				fstrand.entityStrands.VisitAllEntries([this,f](const TUniqueID&entityID, const EntityStrand&estrand)
				{
					if (processed.IsSet(entityID))
						return;
					Objects::PWorldEntity e = Objects::GetEntity(entityID);
					if (!e)
					{
						return; //retry
						//if (estrand.IsNotEmpty() && typeid(*estrand.first()) == typeid(Round::CreateEntity))
						//{
						//	const CreateEntity*c = (const CreateEntity*)estrand.first().get();
						//	e = c->desc.Instantiate(f,entityID,c->location);
						//	if (e->IsMobile())
						//		Objects::InsertConstructed(To<Objects::MobileEntity>::Cast(e),0);
						//	else
						//	{
						//		if (e->IsStructure())
						//		{
						//			Objects::PStructure s = To<Objects::Structure>::Cast(e);
						//			s->ClearOccupiedTerrain(false,0,true);
						//		}
						//		Objects::InsertConstructed(To<Objects::Structure>::Cast(e),0);

						//		if (e->IsWorkingStructure())
						//		{
						//			Objects::PMine m = To<Objects::Mine>::DynamicCast(e);
						//			if (m)
						//				m->LinkToCore(0);
						//			Objects::PWorkingStructure ws = To<Objects::WorkingStructure>::Cast(e);
						//			ws->SetEnabled(c->enableWorkingStructure,0,false,Objects::WorkingStructure::ForceEnable,false);
						//		}
						//			

						//	}
						//}
						//else
//							throw RoundException(CLOCATION, "Operating on non-existent entity "+entityID.ToString());
					}
					else
						if (e->owner != f)
						{
							throw RoundException(CLOCATION, "Operating on entity "+e->GetName()+" that is logged as owned by "+f->GetName()+", but registers as owned by "+e->owner->GetName());
						}

					processed.Set(entityID);


					for (const auto&ev : estrand)
					{
						ev->Execute(e);
					}
				});
				if (processed.Count() == fstrand.entityStrands.Count())
					break;
				if (begin == processed.Count())
				{
					fstrand.entityStrands.VisitAllEntries([this,f](const TUniqueID&entityID, const EntityStrand&estrand)
					{
						if (processed.IsSet(entityID))
							return;
						//ASSERT__(!Objects::GetEntity(entityID));
						//foreach (f->allMobiles,_m)
						//{
						//	Objects::PMobileEntity m = _m->lock();
						//	if (!m)
						//		continue;
						//	ASSERT1__(m->id != entityID,entityID.ToString());
						//	std::cout << m->id.ToFullString() << std::endl;
						//	//LogEvent(EventSignificance::Debug,m.get(),"mobile entity: "+m->id.ToFullString());
						//	//if (m->id == entityID)
						//		
						//}
						throw RoundException(CLOCATION, "Operating on non-existent entity "+entityID.ToFullString());
					});
				}
			}


			Objects::RefreshCompoundCoverage(0);

		});

		Objects::ForeachWorldEntityIncludingStored([](const Objects::PWorldEntity&e)
		{
			e->ReSeedRandom();
		},false);

		Game::ProcessChanges(true);

		if (process)
		{
			if (!evolver)
				throw RoundException(CLOCATION,"Evolver not set. Unable to process round");

			LogEvent(EventSignificance::Uncommon,"Processing round using evolver '"+String(evolver->name)+"'...");
			Timer::Time t0 = timer.Now();
			evolver->ProcessRound();
			LogEvent(EventSignificance::Uncommon,"Round processed in "+String(timer.GetSecondsSince(t0))+" seconds");
			Game::ProcessChanges(true);
		}

		lockEvents = false;

	}


	/*static*/ void Storage::ApplyToGlobalState(const Entry&e, bool process)
	{
		Round round;
		round.Decode(e);
		round.Apply(process);
	}


	Round::PureEntityDescriptor::PureEntityDescriptor(const Objects::PWorldEntity&we)
	{
		this->type = we->GetType();
	}

	Round::CreateEntity::CreateEntity(const Objects::PWorldEntity&we):desc(we),location(we->location),entityID(we->id)
	{
		if (we->IsWorkingStructure())
		{
			enableWorkingStructure = ((const Objects::WorkingStructure&)*we).IsEnabled();
		}
		else
			enableWorkingStructure = false;
	
	}


	Round::CreateFaction::CreateFaction(const Objects::Faction&f, Round::CreateFaction::FactionType type):factionType(type)
	{
		this->accessToken = f.accessToken;
		this->factionID = f.id;
	}
	Round::CreateFaction::CreateFaction(Round::CreateFaction::FactionType type, const TUniqueID&factionID, const CryptographicID&accessToken):factionType(type)
	{
		this->accessToken = accessToken;
		this->factionID = factionID;
	}
	
	/*virtual override*/ void Round::CreateFaction::Execute() const
	{
		Objects::PFaction f = Objects::Faction::FindByID(factionID);
		if (!f)
		{
			if (factionType == CreateFaction::FactionType::Nexus)
				f = Objects::Faction::nexus;
			else
			{
				f.reset(new Objects::Faction(factionID));
				Objects::Faction::all << f;
			}
			f->knownWorld = World::initial;
		}
		else
			if (factionType != GetType(f))
				throw RoundException(CLOCATION,"Faction already exists. Types diverge");
		f->accessToken = accessToken;
		Network::tokenMap.Set(f->accessToken,f);

		World::UpdateEMPZones();
	}



	/*virtual override*/ void	Round::UpdateVariable::Compile(Entry&e)	const
	{
		e.stringParameters.Set("key",key);
		e.stringParameters.Set("value",value);
	}

	/*virtual override*/ void	Round::UpdateVariable::Decode(const Entry&e)
	{
		key = e.stringParameters.Require(CLOCATION,"key");
		value = e.stringParameters.Require(CLOCATION,"value");
	}

	/*virtual override*/ void	Round::UpdateVariable::Execute() const
	{
		CLI::PVariable v = Reflection::state.FindVar(key);
		if (!v)
		{
			LogEvent(EventClass::Preset::error,"Unable to find variable '"+key+"' for update");
			return;
		}
		if (!v->Set(value))
		{
			LogEvent(EventClass::Preset::error,"Unable to update variable '"+key+"' to '"+value+"'. Supported values are "+v->GetSupportedValues());
		}

	}



	/*static*/ int		Round::EntityStateCheck::GetStoredMinerals(const Objects::PWorldEntity&e)
	{
		if (e->IsWorkingStructure())
		{
			const Objects::WorkingStructure*s = (const Objects::WorkingStructure*)(e.get());
			{
				Objects::ResourceVector dummy,storage;
				s->GetResourceVector(dummy,storage,dummy);
				return storage.level[Objects::Resource::Minerals];
			}
		}
		return 0;
	}

	/*static*/ Grid::LCellIdI		Round::EntityStateCheck::GetNextTaskFocus(const Objects::PWorldEntity&_e)
	{
		const Objects::WorldEntity&e = *_e;
		if (e.IsMobile())
		{
			const Objects::MobileEntity&m = *(const Objects::MobileEntity*)&e;
			if (m.myTasks.IsEmpty() || m.myTasks.Peek().foci.IsEmpty())
				return Grid::LCellIdI::Invalid;
			return m.myTasks.Peek().foci.Peek().target;
		}
		if (e.IsStructure())
		{
			const Objects::Tower*t = dynamic_cast<const Objects::Tower*>(&e);
			if (t)
			{
				if (t->attackQueue.IsNotEmpty())
					return t->attackQueue.Peek().target;
				return Grid::LCellIdI::Invalid;
			}
		}

		const Objects::MobileEntityProducer*prod = e.CastToProducer();
		if (prod)
			return prod->gatheringPoint.current;

		return Grid::LCellIdI::Invalid;
	}


	/*static*/ count_t		Round::EntityStateCheck::GetTaskListLength(const Objects::PWorldEntity&_e)
	{
		const Objects::WorldEntity&e = *_e;

		if (e.IsMobile())
		{
			const Objects::MobileEntity&m = *(const Objects::MobileEntity*)&e;
			return m.myTasks.Count();
		}
		if (e.IsStructure())
		{
			const Objects::Tower*t = dynamic_cast<const Objects::Tower*>(&e);
			if (t)
			{
				return t->attackQueue.Count();
			}
		}
		const Objects::MobileEntityProducer*prod = e.CastToProducer();
		if (prod)
			return prod->queue.Count();
		return 0;
	}



	/*static*/ Objects::CursorMode		Round::EntityStateCheck::GetNextTaskMode(const Objects::PWorldEntity&_e)
	{
		const Objects::WorldEntity&e = *_e;
		if (e.IsMobile())
		{
			const Objects::MobileEntity&m = *(const Objects::MobileEntity*)&e;
			if (m.myTasks.IsNotEmpty())
				return m.myTasks.Peek().mode;
			return Objects::CursorMode::Idle;
		}
		if (e.IsStructure())
		{
			const Objects::Tower*t = dynamic_cast<const Objects::Tower*>(&e);
			if (t)
			{
				return t->attackQueue.IsNotEmpty() ? Objects::CursorMode::AttackMode : Objects::CursorMode::Idle;
			}
		}
		return Objects::CursorMode::Idle;
	}

	/*static*/ bool		Round::EntityStateCheck::IsEnabled(const Objects::PWorldEntity&_e)
	{
		const Objects::WorldEntity&e = *_e;

		if (e.IsWorkingStructure())
		{
			return ((const Objects::WorkingStructure&)e).IsEnabled();
		}

		if (e.IsMobile())
		{
			return ((const Objects::MobileEntity&)e).storedIn.expired();	//use to indicate storage state
		}

		return true;	//default to true
	}




	Round::EntityStateCheck::EntityStateCheck(const Objects::PWorldEntity&we)
	{
		this->empDamageLevel = we->empDamageLevel;
		this->health = we->health;
		this->location = we->location;
		this->nextTaskFocus = GetNextTaskFocus(we);
		this->nextTaskMode = GetNextTaskMode(we);
		this->storedMinerals = GetStoredMinerals(we);
		this->taskListLength = GetTaskListLength(we);
		this->jobMineralsLeft = GetJobMineralsLeft(we);
		isEnabled = IsEnabled(we);
	}

	/*static*/ int		Round::EntityStateCheck::GetJobMineralsLeft(const Objects::PWorldEntity&_e)
	{
		const Objects::WorldEntity&e = *_e;

		if (!e.IsMobile())
			return 0;
		const Objects::MobileEntity&m = (const Objects::MobileEntity&)e;
		if (m.myTasks.IsEmpty() || !m.myTasks.Peek().job)
			return 0;
		return m.myTasks.Peek().job->resourceCostLeft;
	}


	/*virtual override*/ void			Round::EntityStateCheck::Compile(Entry&e)	const
	{
		
		if (empDamageLevel != 0)
			e.intParameters.Set("EMPDamageLevel",empDamageLevel);
		e.intParameters.Set("Health",health);
		e.locationParameters.Set("Location",location);
		if (nextTaskFocus != Grid::LCellIdI::Invalid)
			e.locationParameters.Set("NextTaskFocus",nextTaskFocus);
		if (nextTaskMode != Objects::CursorMode::Idle)
			e.intParameters.Set("NextTaskMode",(int)nextTaskMode);
		if (storedMinerals != 0)
			e.intParameters.Set("StoredMinerals",storedMinerals);
		if (taskListLength != 0)
			e.intParameters.Set("TaskListLength",(int)taskListLength);
		if (jobMineralsLeft != 0)
			e.intParameters.Set("JobMineralsLeft",jobMineralsLeft);
		if (!isEnabled)
			e.intParameters.Set("Enabled",0);
	}
	/*virtual override*/ void			Round::EntityStateCheck::Decode(const Entry&e)
	{
		
		if (!e.intParameters.TryGet("EMPDamageLevel",empDamageLevel))
			empDamageLevel = 0;
		health = e.intParameters.Require(CLOCATION,"Health");
		location = e.locationParameters.Require(CLOCATION,"Location");
		if (!e.locationParameters.TryGet("NextTaskFocus",nextTaskFocus))
			nextTaskFocus = Grid::LCellIdI::Invalid;
		int mode;
		if (!e.intParameters.TryGet("NextTaskMode",mode))
			nextTaskMode = (Objects::CursorMode)0;
		else
			nextTaskMode = (Objects::CursorMode)mode;
		if (!e.intParameters.TryGet("StoredMinerals",storedMinerals))
			storedMinerals = 0;
		if (!e.intParameters.TryGet("JobMineralsLeft",jobMineralsLeft))
			jobMineralsLeft = 0;

		int val;
		if (!e.intParameters.TryGet("Enabled",val))
			val = 1;
		isEnabled = val != 0;


		int len;
		if (!e.intParameters.TryGet("TaskListLength",len))
			len = 0;
		taskListLength = (count_t)len;
	}

	/*virtual override*/ void			Round::EntityStateCheck::Execute(const Objects::PWorldEntity&e)	const
	{
		using StringConversion::ToString;
		#undef EVERIFY
		#define EVERIFY(_VAR_)	if (_VAR_ != e->_VAR_) LogEvent(EventClass::Preset::configuration,e->GetName()+ ": Verification failed " #_VAR_ ": "+ToString(_VAR_)+" expected, but found "+ToString(e->_VAR_));
		//throw RoundException(CLOCATION,e->GetName()+ ": Verification failed " #_VAR_ ": "+String(_VAR_)+" expected, but found "+String(e->_VAR_));
		EVERIFY(empDamageLevel);
		EVERIFY(health);
		EVERIFY(location);

		#undef EVERIFY2
		#define EVERIFY2(_VAR_,_OTHER_)	if (_VAR_ != _OTHER_) LogEvent(EventClass::Preset::configuration,e->GetName()+ ": Verification failed " #_VAR_ ": "+ToString(_VAR_)+" expected, but found "+ToString(_OTHER_));
		//throw RoundException(CLOCATION,e->GetName()+ ": Verification failed " #_VAR_ ": "+String(_VAR_)+" expected, but found "+String(_OTHER_));
		EVERIFY2((int)nextTaskMode,(int)GetNextTaskMode(e));
		EVERIFY2(storedMinerals,GetStoredMinerals(e));
		EVERIFY2(jobMineralsLeft,GetJobMineralsLeft(e));
		EVERIFY2(taskListLength,GetTaskListLength(e));
		EVERIFY2(nextTaskFocus,GetNextTaskFocus(e));
		EVERIFY2(isEnabled,IsEnabled(e));

	}

	/*virtual override*/ void			Round::ChangeAutomation::Execute(const Objects::PWorldEntity&e)	const
	{
		if (!e->IsMobile())
		{
			throw RoundException(CLOCATION,e->GetName()+": Not a mobile entity. Cannot apply automation");
		}
		Objects::PMobileEntity m = To<Objects::MobileEntity>::Cast(e);
		m->isAutomated = isNowAutomated;
		m->RegisterChange(Objects::WorldEntity::AutomationChanged,0);
		LogEvent(EventSignificance::Common,e.get(),"Modified automation-state to "+String(m->isAutomated));
	}

	/*virtual override*/ void			Round::ChangeAutomation::Compile(Entry&e)	const
	{
		
		if (isNowAutomated)
			e.intParameters.Set("Auto",1);
	}

	/*virtual override*/ void			Round::ChangeAutomation::Decode(const Entry&e)
	{
		
		int v;
		if (e.intParameters.TryGet("Auto",v))
			isNowAutomated = v != 0;
		else
			isNowAutomated = false;
	}

	/*virtual override*/ void			Round::ChangeSleep::Execute(const Objects::PWorldEntity&e)	const
	{
		float t = 0;
		e->SetSleep(isNowSleeping,t);
	}

	/*virtual override*/ void			Round::ChangeSleep::Compile(Entry&e)	const
	{
		
		if (isNowSleeping)
			e.intParameters.Set("Sleep",1);
	}

	/*virtual override*/ void			Round::ChangeSleep::Decode(const Entry&e)
	{
		
		int v;
		if (e.intParameters.TryGet("Sleep",v))
			isNowSleeping = v != 0;
		else
			isNowSleeping = false;
	}


	





	/*virtual override*/ void	Round::FactionOnlineChange::Compile(Entry&e)	const
	{
		
		e.intParameters.Set("Online",(int)nowOnline);
	}

	/*virtual override*/ void	Round::FactionOnlineChange::Decode(const Entry&e)
	{
		
		nowOnline = e.intParameters.Require(CLOCATION,"Online") != 0;
	}


	/*virtual override*/ void	Round::CommitRound::Compile(Entry&e)	const
	{
		
		e.Set("RoundNumber",roundNumber);
		e.stringParameters.Set("EvolverName",evolverName);
		e.intParameters.Set("FileDataVersion",(int)fileDataVersion);
	}
	/*virtual override*/ void	Round::CommitRound::Decode(const Entry&e)
	{
		
		e.Require(CLOCATION,"RoundNumber",roundNumber);
		evolverName = e.stringParameters.Require(CLOCATION,"EvolverName");
		int v;
		if (!e.intParameters.TryGet("FileDataVersion",v))
			v = 1040;	//old format
		
		fileDataVersion = (UINT32)v;
	}


	Objects::PWorldEntity Round::PureEntityDescriptor::Instantiate(const Objects::PFaction&f, const TUniqueID&id, const Grid::LCellIdI&loc) const
	{
		Objects::PWorldEntity proto = Objects::Prototypes::Find(type);
		Objects::PWorldEntity e;
		if (proto)
		{
			e = proto->CreateNewInstance(f, loc,0);
			e->id = id;
		}
		else
		{
			e = Objects::AssembleVehicle(f,id,loc,type);
		}
		LogEvent(EventSignificance::Debug,e.get(),"PureEntityDescriptor::Instantiate("+ToString(id)+")");
		return e;
	}

	void	Round::PureEntityDescriptor::Compile(Entry&e) const
	{
		e.stringParameters.Set("EntityType",ToString(type));
	}

	void	Round::PureEntityDescriptor::Decode(const Entry&e)
	{
		String t = e.stringParameters.Require(CLOCATION,"EntityType");
		type = Objects::DecodeType(t);
	}


	/*virtual override*/ void	Round::CreateEntity::Compile(Entry&e) const
	{
		
		desc.Compile(e);
		e.uuidParameters.Set("ID",entityID);
		e.locationParameters.Set("Location",location);
		e.intParameters.Set("Enable",enableWorkingStructure);
	}
	/*virtual override*/ void	Round::CreateEntity::Execute(const Objects::PFaction&f) const
	{
		Objects::PWorldEntity e = desc.Instantiate(f,entityID,location);
		float t = 0;
		if (e->IsMobile())
			Objects::InsertConstructed(To<Objects::MobileEntity>::Cast(e),t);
		else
		{
			if (e->IsStructure())
			{
				Objects::PStructure s = To<Objects::Structure>::Cast(e);
				s->ClearOccupiedTerrain(0);
			}
			Objects::InsertConstructed(To<Objects::Structure>::Cast(e),t);

			if (e->IsWorkingStructure())
			{
				Objects::PMine m = To<Objects::Mine>::DynamicCast(e);
				if (m)
					m->LinkToCore(0);
				Objects::PWorkingStructure ws = To<Objects::WorkingStructure>::Cast(e);
				ws->SetEnabled(enableWorkingStructure,t,false,Objects::WorkingStructure::ForceEnable,false);
			}
		}
	}

	/*virtual override*/ void	Round::CreateEntity::Decode(const Entry&e)
	{
		
		desc.Decode(e);
		location = e.locationParameters.Require(CLOCATION,"Location");
		int val = e.intParameters.Require(CLOCATION,"Enable");
		entityID = e.uuidParameters.Require(CLOCATION,"ID");

		enableWorkingStructure = val != 0;
	}

	/*virtual override*/ void	Round::CreateFaction::Compile(Entry&e) const
	{
		
		e.stringParameters.Set("FactionType",factionType.ToString());
		e.stringParameters.Set("FactionSecret",accessToken.ToFullString());
		e.uuidParameters.Set("ID",factionID);
	}

	/*virtual override*/ void	Round::CreateFaction::Decode(const Entry&e)
	{
		
		const String&ftype = e.stringParameters.Require(CLOCATION,"FactionType");
		if (!factionType.Decode(ftype.c_str()))
		{
			throw RoundException(CLOCATION,"Unknown faction type '"+ftype+"'");
		}
		accessToken.Decode(e.stringParameters.Require(CLOCATION,"FactionSecret"));
		factionID = e.uuidParameters.Require(CLOCATION,"ID");
	}


	/*virtual override*/ void	Round::ChoiceIntersection::Compile(Entry&e) const
	{
		
		e.locationParameters.Set("Location",target);
		e.intParameters.Set("DirectMode",(int)mode);
		if (deployingWhat != TUniqueID::Empty())
			e.uuidParameters.Set("EntityID",deployingWhat);
	}
	/*virtual override*/ void	Round::ChoiceIntersection::Execute(const Objects::PWorldEntity&e) const
	{
		Objects::PWorldEntity deploy = deployingWhat!=TUniqueID::Empty() ? Objects::GetEntity(deployingWhat) : Objects::PWorldEntity();
		Objects::PMobileEntity md = deploy && deploy->IsMobile() ? To<Objects::MobileEntity>::Cast(deploy) : Objects::PMobileEntity();
		if (!Objects::ProcessChoiceIntersection(e,target,mode,md))
		{
			throw RoundException(CLOCATION, "Unable to handle choice intersection of "+e->GetName()+" with "+ToString(target)+" in mode "+String(int(mode))+", while deploying "+(md?md->GetName():""));
		}

	}

	/*virtual override*/ void	Round::ChoiceIntersection::Decode(const Entry&e)
	{
		
		target = e.locationParameters.Require(CLOCATION,"Location");
		mode = (Objects::CursorMode) e.intParameters.Require(CLOCATION,"DirectMode");
		if (!e.uuidParameters.TryGet("EntityID",deployingWhat))
			deployingWhat = TUniqueID::Empty();
	}

	/*virtual override*/ void	Round::DirectToCommand::Compile(Entry&e) const
	{
		foreach (targets,t)
			e.locationParameters.Set("Waypoint",*t);
		if (mode != Objects::CursorMode::Idle)
			e.intParameters.Set("DirectMode",(int)mode);
		if (leftClick)
			e.intParameters.Set("LeftClick",leftClick);
		if (detailed)
			e.intParameters.Set("DetailedPath",detailed);
		if (clearSafeTasks)
			e.intParameters.Set("ClearSafeTasks",clearSafeTasks);
	}
	/*virtual override*/ void	Round::DirectToCommand::Execute(const Objects::PWorldEntity&e) const
	{
		if (clearSafeTasks)
		{
			e->ClearSafeTasks();
		}
		if (!e->DirectTo(mode,targets,detailed,leftClick))
		{
			#ifdef _DEBUG
				static StringBuffer buffer;
				buffer.Clear();
				foreach (targets,t)
					buffer << ' '<<ToString(*t);
				throw RoundException(CLOCATION, "Unable to direct "+e->GetName()+" to"+buffer.CopyToString()+" in mode "+String(int(mode)));
			#else
				throw RoundException(CLOCATION, "Unable to direct "+e->GetName()+" to destination");
			#endif
		}
	}

	/*virtual override*/ void	Round::DirectToCommand::Decode(const Entry&e)
	{
		
		e.locationParameters.GetArray("Waypoint",targets);
		if (targets.IsEmpty())
			throw RoundException(CLOCATION, "Expected to extract more than one target");
		int md;
		if (!e.intParameters.TryGet("DirectMode",md))
			md = 0;
		mode = (Objects::CursorMode)md;
		if (e.intParameters.TryGet("LeftClick",md))
			leftClick = md != 0;
		else
			leftClick = false;
		if (e.intParameters.TryGet("DetailedPath",md))
			detailed = md != 0;
		else
			detailed = false;
		if (e.intParameters.TryGet("ClearSafeTasks",md))
			clearSafeTasks = md != 0;
		else
			clearSafeTasks = false;
	}


	/*virtual override*/ void	Round::JobCommand::Compile(Entry&e) const
	{
		
		e.stringParameters.Set("MobileTaskJob", jobID.ToString());
		e.binaryParameters.Set("MobileTaskJobData", jobPayload);
		if (focus != Grid::LCellIdI::Invalid)
			e.locationParameters.Set("MobileTaskFocus",focus);
	}
	/*virtual override*/ void	Round::JobCommand::Execute(const Objects::PWorldEntity&e) const
	{
		if (!e->IsMobile())
		{
			throw RoundException(CLOCATION, "Trying to assign job to non-mobile entity "+e->GetName());
		}
		Objects::PMobileEntity m = To<Objects::MobileEntity>::Cast(e);
		foreach (Objects::VehicleJob::BaseJob::allJobPrototypes,p)
			if (p->get()->GetDataKey() == this->jobID)
			{
				auto job = p->get()->Instantiate();
				DeserializeFromCompactMemory(*job,this->jobPayload.pointer(),(serial_size_t)jobPayload.size()), job->GetName(),jobPayload.size();
				job->Restart(*m);
				m->RegisterNewJob("",job,focus != Grid::LCellIdI::Invalid ? &focus : nullptr);
				return;
			}
		throw RoundException(CLOCATION, "Unable to find job "+String(jobID.ToString()));
	}

	/*virtual override*/ void	Round::JobCommand::Decode(const Entry&e)
	{
		
		jobID = Riff::TID(e.stringParameters.Require(CLOCATION,"MobileTaskJob").c_str());
		jobPayload = e.binaryParameters.Require(CLOCATION,"MobileTaskJobData");
		if (!e.locationParameters.TryGet("MobileTaskFocus",focus))
			focus = Grid::LCellIdI::Invalid;
	}

	/*virtual override*/ void	Round::EntityProduction::Compile(Entry&e) const
	{
		
		e.uuidParameters.Set("TaskID",taskID);
	}
	/*virtual override*/ void	Round::EntityProduction::Decode(const Entry&e)
	{
		
		taskID = e.uuidParameters.Require(CLOCATION,"TaskID");
	}


	/*virtual override*/ void	Round::DropEntityProduction::Execute(const Objects::PWorldEntity&e) const
	{
		if (!e->CastToProducer())
		{
			throw RoundException(CLOCATION, "Trying to drop production task from non-producer entity "+e->GetName());
		}
		Objects::PMobileEntityProducer p = To<Objects::MobileEntityProducer>::DynamicCast(e);

		if (!p->AbortConstruction(taskID))
			throw RoundException(CLOCATION, "Failed KOto remove production task '"+ToString(taskID)+"' from "+e->GetName());
	}


	/*virtual override*/ void	Round::NewEntityProduction::Execute(const Objects::PWorldEntity&e) const
	{
		if (!e->CastToProducer())
		{
			throw RoundException(CLOCATION, "Trying to assign production task to non-producer entity "+e->GetName());
		}
		float t = 0;
		Objects::PMobileEntityProducer p = To<Objects::MobileEntityProducer>::DynamicCast(e);
		Objects::PWorldEntity produced = desc.Instantiate(e->owner,this->newEntityID,Grid::LCellIdI(0,0));
		if (produced->IsMobile())
		{
			Objects::MobileEntityProducer::WorkItem*item = p->EnqueueProductionOf(To<Objects::MobileEntity>::Cast(produced),insertAt);
			if (!item)
				throw RoundException(CLOCATION,"Failed to enqueue production task");
			item->id = taskID;
			if (e->IsWorkingStructure())
			{
				Objects::PWorkingStructure&ws = To<Objects::WorkingStructure>::Cast(e);
				if (!ws->IsEnabled())
				{
					ws->SetEnabled(true,t);
				}

			}
		}
		else
			throw RoundException(CLOCATION, "Expected mobile entity being produced by "+e->GetName()+", but got "+produced->GetName());
	}

	/*virtual override*/ void	Round::NewEntityProduction::Compile(Entry&e) const
	{
		Super::Compile(e);
		if (insertAt != InvalidIndex)
			e.intParameters.Set("MobileProducerProductionTaskAt",(int)insertAt);
		e.uuidParameters.Set("EntityID",newEntityID);
		desc.Compile(e);
	}
	/*virtual override*/ void	Round::NewEntityProduction::Decode(const Entry&e)
	{
		Super::Decode(e);
		int at;
		if (e.intParameters.TryGet("MobileProducerProductionTaskAt",at))
			insertAt = (index_t)at;
		else
			insertAt = InvalidIndex;
		newEntityID = e.uuidParameters.Require(CLOCATION,"EntityID");
		desc.Decode(e);
	}


	#if 0
	void	Helper::LogUnitCommands(const Objects::WorldEntity&entity, const std::function<void( Journal::Entry&)>&sink)
	{
		Entry e;
		if (entity.IsMobile())
		{
			const Objects::MobileEntity&m = (const Objects::MobileEntity&)entity;
			foreach (m.myTasks,t)
			{
				Storage::Event::UnitCommand(e,*t);
				e.uuidParameters.Set(ParameterName::EntityID,entity.id);
				sink(e);
			}
		}
		const Objects::MobileEntityProducer*p = entity.CastToProducer();
		if (p)
		{
		//MobileProducerGatheringPoint,
		//MobileProducerProductionState

			e.Clear();
			e.operation = Entry::Operation::MobileProducerState;
			if (p->gatheringPoint.current != p->gatheringPoint.current.Invalid)
				e.locationParameters.Set(ParameterName::MobileProducerGatheringPoint,p->gatheringPoint.current);
			e.intParameters.Set(ParameterName::MobileProducerProductionState,p->productionDone);
			e.uuidParameters.Set(ParameterName::EntityID,entity.id);
			sink(e);
			foreach (p->queue,workItem)
			{
				Storage::Event::UnitCommand(e,*workItem);
				e.uuidParameters.Set(ParameterName::EntityID,entity.id);
				sink(e);
			}
		}
	}
	#endif /*0*/


	void	Entry::Write(std::ofstream&stream, count_t indention)	const
	{
		for (index_t i = 0; i < indention; i++)
			stream << '\t';
		stream << operation;
		foreach (uuidParameters.entries,p)
			stream << tab << p->key << "=u"<<p->value.ToFullString();
		foreach (binaryParameters.entries,p)
			stream << tab << p->key << "=b"<<Bin2Str::Convert(p->value);
		foreach (stringParameters.entries,p)
			stream << tab << p->key << "=s"<<p->value;
		foreach (locationParameters.entries,p)
		{
			stream << tab << p->key << "=l"<<p->value.stackID<<'.';
			#ifndef NO_SATELLITES
				if (p->value.height == Objects::Satellite::BaseResidenceHeight)
					stream << "sat";
				elif (p->value.height == Objects::Satellite::DeclinedResidenceHeight)
					stream << "declSat";
				else
			#endif
				stream << p->value.height;
		}
		foreach (intParameters.entries,p)
			stream << tab << p->key << "=i"<<p->value;
		stream << std::endl;
		foreach (children,c)
			c->Write(stream,indention+1);
	}

	count_t	Entry::Read(std::ifstream&stream)
	{
		//std::string line;
		if (stream.eof())
			return InvalidIndex;
		Clear();
		char _line[0x1000];
		stream.getline(_line,sizeof(_line));
		if (!_line[0])
			return InvalidIndex;
		char*line = _line;


		count_t rs = 0;
		while (*line == '\t')
		{
			rs++;
			line++;
		}

		size_t lineLen = strlen(line);
		const char*lineEnd = line + lineLen;
		//stream >> line;
		char* tabAt = strchr(line,'\t');
		if (tabAt == nullptr)
		{
			operation = StringRef(line,lineLen);
			ASSERT__(Round::eventMap.IsSet(operation));
			//if (!operation.Decode(line,lineLen))
			//	throw Except::IO::DriveAccess::FileFormatFault(CLOCATION, "Unexpected operation '"+String(line,lineLen)+"' encountered while decoding line");

			return rs;
		}
		operation = StringRef(line,tabAt-line);
		ASSERT1__(Round::eventMap.IsSet(operation) || operation == "FactionData" || operation == "EntityData",operation);
		//if (!operation.Decode(line,tabAt-line))
		//	throw Except::IO::DriveAccess::FileFormatFault(CLOCATION, "Unexpected operation '"+String(line,tabAt-line)+"' encountered while decoding line "+String(line,lineLen));
		do
		{
			char* nextTab = strchr(tabAt+1,'\t');
			char* opAt = strchr(tabAt,'=');
			if (opAt == nullptr)
			{
				throw Except::IO::DriveAccess::FileFormatFault(CLOCATION, "Expected operator in "+String(tabAt)+" while decoding line "+String(line,lineLen));
			}
			char opType = opAt[1];
			StringRef paraRef = StringRef(tabAt+1,opAt-tabAt-1);
			//String paraName = String(line.c_str()+tabAt+1,opAt-tabAt-1);
			const char*value = opAt+2;
			size_t valLen = nextTab ? nextTab-opAt-2 : lineEnd - opAt-2;
			switch (opType)
			{
				case 'u':
				{
					TUniqueID target;
					Bin2Str::Convert(target.bytes,target.NumBytes,value,valLen);
					uuidParameters.Set(paraRef,target);
				}
				break;
				case 'b':
				{
					Ctr::Array<BYTE>&binary = binaryParameters.Set(paraRef);
					binary.SetSize(valLen/2);
					Bin2Str::Convert(binary.pointer(),binary.GetLength(),value,valLen);
				}
				break;
				case 'i':
				{
					INT32 target;
					if (!convert(value,valLen,target))
					{
						throw Except::IO::DriveAccess::FileFormatFault(CLOCATION, "Expected integer for parameter '"+String(paraRef)+"' but got '"+String(value,valLen)+"' while decoding line "+String(line,lineLen));
					}
					intParameters.Set(paraRef,target);
				}
				break;
				case 's':
				{
					stringParameters.Set(paraRef,String(value,valLen));
				}
				break;
				case 'l':
				{
					const char*dotAt = strnchr(const_cast<char*>(value),valLen,'.');
					if (!dotAt)
						throw Except::IO::DriveAccess::FileFormatFault(CLOCATION, "Expected . in location parameter '"+String(paraRef)+"' while decoding line "+String(line,lineLen));
					Grid::LCellIdI target;
					if (!convert(value,dotAt - value,target.stackID))
					{
						throw Except::IO::DriveAccess::FileFormatFault(CLOCATION, "Expected valid stack ID for parameter '"+String(paraRef)+"', but got '"+String(value,dotAt-value)+"' while decoding line "+String(line,lineLen));
					}
					StringRef sHeight(dotAt+1, valLen  - (dotAt-value) -1);
					if (target.stackID >= Grid::LowSetup::GridType::CellCount)
					{
						throw Except::IO::DriveAccess::FileFormatFault(CLOCATION, "Decoded stack ID of "+String(paraRef)+"=" +ToString(target)+" is out of range, while decoding line "+String(line,lineLen));
					}

					#ifndef NO_SATELLITES
						if (sHeight == "sat")
							target.height = Objects::Satellite::BaseResidenceHeight;
						elif (sHeight == "declSat")
							target.height = Objects::Satellite::DeclinedResidenceHeight;
						else
					#endif
					{
						if (!convert(sHeight.pointer(),sHeight.length(),target.height))
						{
							throw Except::IO::DriveAccess::FileFormatFault(CLOCATION, "Expected valid height for parameter '"+String(paraRef)+"', but got '"+String(dotAt+1, valLen  - (dotAt-value)-1)+"' while decoding line "+String(line,lineLen));
						}
						if ((target.height < Grid::CellStack::BottomHeight || target.height > Grid::CellStack::TopHeight) 
							IF_SATELLITES(&& target.height != Objects::Satellite::BaseResidenceHeight && target.height != Objects::Satellite::DeclinedResidenceHeight)
							)
						{
							throw Except::IO::DriveAccess::FileFormatFault(CLOCATION, "Decoded height of "+String(paraRef)+"=" +ToString(target)+" is out of range, while decoding line "+String(line,lineLen));
						}
					}
					locationParameters.Set(paraRef,target);
				}
				break;
			}


			tabAt = nextTab;
		}
		while (tabAt != nullptr);

		return rs;
	}


	void	Helper::WriteToFile(World::Data&data, const PathString&filename)
	{
		Riff::Chunk	world;
		FileIO::WriteToListChunk(data,world);
		bool success = world.SaveToFile(filename.c_str());
		if (!success)
			throw Except::IO::DriveAccess::DataWriteFault(CLOCATION, "Unable to write world to "+String(filename));

	}

	void	Helper::LoadFromFile(World::Data&data, const PathString&filename)
	{
		Riff::Chunk world;
		if (!world.LoadFromFile(filename.c_str()))
			throw Except::IO::DriveAccess::DataReadFault(CLOCATION, "Unable to read world from "+String(filename));
		FileIO::ReadFromListChunk(world,data);
	}

	void	Helper::WriteToFile(const Grid::Resource&resource, const PathString&filename)
	{
		FILE*f = FOPEN(filename.c_str(),"wb");
		fwrite(resource.coverage,sizeof(resource.coverage),1,f);
		fclose(f);
	}

	void	Helper::LoadFromFile(Grid::Resource&resource, const PathString&filename)
	{
		FILE*f = FOPEN(filename.c_str(),"rb");
		fread(resource.coverage,sizeof(resource.coverage),1,f);
		fclose(f);
	}



	void	Round::RegisterResourceCoverageChange(bool execute, UINT32 stackID, Objects::Resource res, Grid::Resource::coverage_t newCoverage)
	{
		PGlobalEvent ev(new ResourceCoverageChange(stackID,res,newCoverage));
		globalEvents.Append(ev);
		if (execute)
			ev->Execute();
		ev->Execute();
	}

	void	Round::RegisterResourceCoverageKnowledgeLoss(bool execute, const Objects::PFaction&f, UINT32 stackID)
	{
		if (!f->knownResources.resourcesDiscovered[stackID])
			return;
		PFactionEvent ev(new ResourceCoverageKnowledgeLoss(stackID));
		factionStrands.Set(f->id).factionEvents << ev;
		if (execute)
			ev->Execute(f);
	}



	/*virtual override*/void			Round::ResourceCoverageChange::Compile(Entry&e)	const
	{
		e.intParameters.Set("StackID",(int)this->stackID);
		e.intParameters.Set("NewCoverage",(int)this->newCoverage);
		e.stringParameters.Set("Resource",this->resource.ToString());
	}

	/*virtual override*/void			Round::ResourceCoverageChange::Decode(const Entry&e)
	{
		this->stackID = (UINT32)e.intParameters.Require(CLOCATION,"StackID");
		const String&resName = e.stringParameters.Require(CLOCATION,"Resource");
		if (!this->resource.Decode(resName.c_str()))
			throw Except::Program::DataConsistencyFault( CLOCATION,"Unknown resource '"+resName+"'");
		this->newCoverage = (Grid::Resource::coverage_t)e.intParameters.Require(CLOCATION,"NewCoverage");
		if (this->stackID >= Grid::LowSetup::GridType::CellCount)
		{
			throw Except::Program::DataConsistencyFault( CLOCATION,"Stack ID out of range '"+String(stackID)+"'");
		}
	}

	/*virtual override*/void			Round::ResourceCoverageChange::Execute()	const
	{
		Grid::Resource::Require(resource.ToString()).coverage[this->stackID] = newCoverage;
	}


	/*virtual override*/void			Round::ResourceCoverageKnowledgeLoss::Compile(Entry&e)	const
	{
		e.intParameters.Set("StackID",(int)this->stackID);
	}

	/*virtual override*/void			Round::ResourceCoverageKnowledgeLoss::Decode(const Entry&e)
	{
		this->stackID = (UINT32)e.intParameters.Require(CLOCATION,"StackID");
		if (this->stackID >= Grid::LowSetup::GridType::CellCount)
		{
			throw Except::Program::DataConsistencyFault( CLOCATION,"Stack ID out of range '"+String(stackID)+"'");
		}
	}

	/*virtual override*/void			Round::ResourceCoverageKnowledgeLoss::Execute(const Objects::PFaction&f)	const
	{
		f->knownResources.resourcesDiscovered[stackID] = false;
	}


	bool Round::GlobalEvent::TryExecute() const
	{
		try
		{
			Execute();
			return true;
		}
		catch (const RoundException&ex)
		{
			LogEvent(EventClass::Preset::commandRejected,ex.GetCoreMessage());
		}
		catch (const Except::Exception&ex)
		{
			LogEvent(EventClass::Preset::error,ex.std::exception::what());
		}
		return false;
	}

	bool	Round::FactionEvent::TryExecute(const Objects::PFaction&f) const
	{
		try
		{
			Execute(f);
			return true;
		}
		catch (const RoundException&ex)
		{
			TCP::PPeer p = GetPeerOfFaction(f);;
			if (p)
				LogEvent(EventClass::Preset::commandRejected, *p,f->GetName()+": "+ ex.GetCoreMessage());
			else
				LogEvent(EventClass::Preset::error, f->GetName()+": "+ ex.GetCoreMessage());
		}
		catch (const Except::Exception&ex)
		{
			LogEvent(EventClass::Preset::error,f->GetName()+": "+ ex.std::exception::what());
		}
		return false;
	}

	bool	Round::EntityEvent::TryExecute(const Objects::PWorldEntity&e) const
	{
		try
		{
			Execute(e);
			return true;
		}
		catch (const RoundException&ex)
		{
			LogEvent(EventClass::Preset::commandRejected,e.get(),ex.GetCoreMessage());
		}
		catch (const Except::Exception&ex)
		{
			LogEvent(EventClass::Preset::error,e.get(),ex.std::exception::what());
		}
		return false;
	}



	void	Round::RegisterPersistentVariableChange(bool execute, const Reflection::Variable::PBase&var)
	{
		if (lockEvents)
			return;
		PGlobalEvent ev(new UpdateVariable(var->GetPath(),var->ConvertToString()));
		if (!execute || ev->TryExecute())
			return;
		this->globalEvents.Append(ev);
	}



	void	Writer::InitNew(const PathString&root)
	{
		FileSystem::CreateDirectory(root);
		Helper::WriteToFile(World::actual,root + FOLDER_SLASH+"actual.world");
		Helper::WriteToFile(World::initial,root + FOLDER_SLASH+"visible.world");
		foreach (Grid::Resource::all,res)
		{
			Helper::WriteToFile(*res,root+FOLDER_SLASH+res->GetName()+".coverage");
		}
		PathString file = root+FOLDER_SLASH+"journal.txt";
		logStream.open(file.c_str(),std::ios::binary | std::ios::out | std::ios::trunc);
		ASSERT__(logStream.is_open());


		Reflection::VisitAllVariables([this](const Reflection::Variable::PBase&v)
		{
			if (v->WriteToSavegame())
			{
				v->lastValue = v->ConvertToString();
				currentRound.RegisterPersistentVariableChange(false,v);
			}
		});


		Objects::ForeachFaction([this](const Objects::PFaction&f)
		{
			currentRound.LogFactionCreation(f,GetType(f));
		});
		ASSERT__(logStream.is_open());

		//#ifdef _DEBUG
		//CommitAndClear(currentRound);
		//#endif
	}


	void	Writer::ContinueExisting(const PathString&root)
	{
		bool closed = Network::CloseSession();

		LogEvent(EventSignificance::Important,"Restoring journal from "+String(root));

		Helper::LoadFromFile(World::actual,root + FOLDER_SLASH+"actual.world");
		Helper::LoadFromFile(World::initial,root + FOLDER_SLASH+"visible.world");

		foreach (Grid::Resource::all,res)
		{
			Helper::LoadFromFile(*res,root+FOLDER_SLASH+res->GetName()+".coverage");
		}


		PathString file = root+FOLDER_SLASH+"journal.txt";
		Storage journal;
		journal.Load(file);
		bool ai = Game::Config::Nexus::enableAI;
		Game::Config::Nexus::enableAI = false;

		World::UpdateEMPZones();

		if (closed)
			Network::RestartSession(true);



		journal.ApplyToGlobalState();

		Game::Config::Nexus::enableAI = ai;

		if (Game::Config::Nexus::enableAI)
			AIProcess::Begin(Objects::Faction::nexus->accessToken);


		//#ifndef _DEBUG
			logStream.open(file.c_str(),std::ios::binary | std::ios::out | std::ios::app);
			//journal.Write(logStream);
		//#endif
	}

	void		Storage::ApplyToGlobalState() const
	{
		try
		{
			foreach (this->entries,e)
				ApplyToGlobalState(*e,true);
		}
		catch (const Except::Exception&ex)
		{
			if (ex.hasLocation)
				Except::TriggerFatal(ex.location,ex.what());
			else
				FATAL__(ex.what());
		}
		catch (const std::exception&e)
		{
			FATAL__(e.what());
		}
		catch (...)
		{
			FATAL__("Expected exception caught while trying to apply recorded journal savegame");

		}
	}

	void	Storage::Load(const PathString&filename)
	{
		std::ifstream stream;
		stream.open(filename.c_str(),std::ios::binary | std::ios::in);
		Load(stream);
	}

	void	Storage::Load(std::ifstream&stream)
	{
		entries.Clear();
		Entry e;
		for (;;)
		{
			count_t indention = e.Read(stream);
			if (indention == InvalidIndex)
				break;
			if (indention == 0)
				entries.MoveAppend(e);
			else
			{
				ASSERT__(entries.IsNotEmpty());
				entries.last().MoveAppend(e,indention-1);
			}
		}
	}

	void	Storage::Write(const PathString&filename) const
	{
		std::ofstream stream;
		stream.open(filename.c_str(),std::ios::binary | std::ios::out | std::ios::trunc);
		Write(stream);
	}

	void	Storage::Write(std::ofstream&stream) const
	{
		foreach (entries,e)
			e->Write(stream,0);
	}

	void	Writer::Continue(const Storage&journal, const World::Data&world, const World::Data&initialWorld, const PathString&writeTo)
	{
		bool closed = Network::CloseSession();
		World::actual = world;
		World::initial = initialWorld;

		journal.ApplyToGlobalState();

		if (closed)
			Network::RestartSession(true);

		logStream.open(writeTo.c_str(),std::ios::binary | std::ios::out | std::ios::trunc);
		ASSERT__(logStream.is_open());
		journal.Write(logStream);
		Entry e;
		e.operation = "ContinuedAllOffline";
		Log(e);

	}




	void	Writer::Log(const Journal::Entry&e)
	{
		if (lockEvents)
			return;
		ASSERT__(logStream.is_open());
		e.Write(logStream,0);
		logStream.flush();
		ASSERT__(logStream.is_open());
	}

	void	Writer::Log(const Round::Event&ev)
	{
		Entry e;
		ev.ExportTo(e);
		Log(e);
	}


	void	Round::LogJobCommandStart(const Objects::PWorldEntity&e, const Objects::VehicleJob::BaseJob&job, const Grid::LCellIdI&focus)
	{
		try
		{
			std::shared_ptr<JobCommand> cmd(new JobCommand());
			cmd->jobID = job.GetDataKey();
			cmd->focus = focus;
			SerialSizeScanner scanner;
			job.Serialize(scanner);
			const serial_size_t size = scanner.GetTotalSize();
			cmd->jobPayload.SetSize(size);
			SerializeToCompactMemory(job,cmd->jobPayload.pointer(),(serial_size_t)cmd->jobPayload.GetContentSize());

			EntityStrand&strand = factionStrands.Set(e->owner->id).entityStrands.Set(e->id);
			strand.Append(cmd);
		}
		catch (const std::exception&ex)
		{
			LogException(e.get(),ex);
		}
	}


	void	Round::RegisterAutomationChange(bool execute, const Objects::PMobileEntity&e, bool isNowAutomated)
	{
		EntityStrand&strand = factionStrands.Set(e->owner->id).entityStrands.Set(e->id);
		PEntityEvent cmd(new ChangeAutomation(isNowAutomated));
		strand.Append(cmd);
		if (execute)
			cmd->Execute(e);
	}

	void	Round::RegisterSleepChange(bool execute, const Objects::PWorldEntity&e, bool isNowSleeping)
	{
		EntityStrand&strand = factionStrands.Set(e->owner->id).entityStrands.Set(e->id);
		PEntityEvent cmd(new ChangeSleep(isNowSleeping));
		strand.Append(cmd);
		if (execute)
			cmd->Execute(e);
	}


	void	Round::LogFactionCreation(const Objects::PFaction&f, Round::CreateFaction::FactionType type)
	{
		FactionStrand&strand = factionStrands.Set(f->id);
		PGlobalEvent ev(new CreateFaction(*f,type));
		globalEvents.Append(ev);


		foreach (f->allMobiles,m)
		{
			Objects::PWorldEntity w = m->lock();
			if (w)
				strand.factionEvents.Append().reset(new CreateEntity(w));
		}
		foreach (f->structures,s)
		{
			Objects::PWorldEntity w = s->lock();
			if (w)
				strand.factionEvents.Append().reset(new CreateEntity(w));
//				this->newEntities.Append().reset(new CreateEntity(*w));
		}

	}

	/*virtual override*/ void	Round::ClearEntityCommands::Execute(const Objects::PWorldEntity&e) const
	{
		if (e->IsMobile())
		{
			Objects::PMobileEntity&me = To<Objects::MobileEntity>::Cast(e);
			switch (dropMode)
			{
				case Protocol::DropMode::AllTasks:
					me->ClearAllTasks(false,CLOCATION);
					LogEvent(EventSignificance::Common,e.get(),"Cleared all tasks");
				break;
				case Protocol::DropMode::AllSafeTasks:
					me->ClearSafeTasks();
					LogEvent(EventSignificance::Common,e.get(),"Cleared all safe tasks");
				break;
				case Protocol::DropMode::LastTask:
					if (!me->DropLastTask())
						throw RoundException(CLOCATION,e->GetName()+": Failed to drop last command. Queue is empty");
					else
						LogEvent(EventSignificance::Common,e.get(),"Cleared last entity task");
				break;
			}
			return;
		}
		if (e->IsStructure())
		{
			Objects::PTower&te = To<Objects::Tower>::DynamicCast(e);
			if (te)
			{
				if (dropMode == Protocol::DropMode::LastTask)
				{
					if (!te->DropLastTask())
						throw RoundException(CLOCATION,e->GetName()+": Failed to drop last command. Queue is empty");
					else
						LogEvent(EventSignificance::Common,e.get(),"Cleared last entity task");
				}
				else
				{
					te->ClearTasks();	//all tasks are safe
					LogEvent(EventSignificance::Common,e.get(),"Cleared all tasks");
				}

				return;
			}
		}
		throw RoundException(CLOCATION,e->GetName()+": Unable to clear commands. Incompatible entity");
	}



	/*virtual override*/ void	Round::ClearEntityCommands::Compile(Entry&e)	const
	{
		e.stringParameters.Set("type",this->dropMode.ToString());
	}

	/*virtual override*/ void	Round::ClearEntityCommands::Decode(const Entry&e)
	{
		const String&t = e.stringParameters.Require(CLOCATION,"type");
		if (!dropMode.Decode(t.c_str()))
		{
			throw Except::Program::FunctionalityNotImplemented(CLOCATION,"Unknown task drop type '"+t+"'");
		}
	}


	/*virtual override*/ void	Round::DestroyEntity::Compile(Entry&e)	const
	{
		e.stringParameters.Set("type",this->type.ToString());
	}

	/*virtual override*/ void	Round::DestroyEntity::Decode(const Entry&e)
	{
		const String&t = e.stringParameters.Require(CLOCATION,"type");
		if (!type.Decode(t.c_str()))
		{
			throw Except::Program::FunctionalityNotImplemented(CLOCATION,"Unknown removal type '"+t+"'");
		}
	}

	/*virtual override*/ void	Round::DestroyEntity::Execute(const Objects::PWorldEntity&e) const
	{
		if (e->IsDeadOrGhost())
		{
			throw RoundException(CLOCATION,e->GetName()+": Unable to destroy entity. Entity already dead");
		}
		if (e->IsIndestructible())
		{
			throw RoundException(CLOCATION,e->GetName()+": Unable to destroy entity. Entity is indestructible");
		}
		float t = 0;
		Objects::Remove(e,type,t);
		if (To<Objects::Structure>::DynamicCast(e))
			Objects::RefreshCompoundCoverageIfNeeded(t);

	}

	static bool ConvertReferenceEntity(const Protocol::MarkerReferenceEntity&source, Objects::Marker&dest )
	{
		dest.referenceEntity = source.id;
		dest.referencedType = source.type;
		if (dest.referencedType.IsInvalid())
		{
			LogEvent(EventClass::Preset::commandRejected, "Unknown marker format");
			return false;
		}
		if (dest.referencedType.IsEmpty())
			return true;
		if (Objects::Prototypes::Find(dest.referencedType))
			return true;
		if (!Objects::Component::baseComponents.Find(dest.referencedType.baseType))
		{
			LogEvent(EventClass::Preset::commandRejected, "Unknown base type #" + String(int(dest.referencedType.baseType)));
			return false;
		}

		//String turretName = source.type.subStringRef((int)separatorAt);
		if (!Objects::Component::vehicleTurretComponents.Find(dest.referencedType.turretType))
		{
			LogEvent(EventClass::Preset::commandRejected, "Unknown turret type #" + String(int(dest.referencedType.turretType)));
			return false;
		}
		return true;
	}

	static void ExportReferenceEntity(const Protocol::MarkerReferenceEntity&source, Entry&result )
	{
		result.uuidParameters.Set("ReferenceID",source.id);
		if (source.type.IsInvalid())
		{
			throw Round::RoundException(CLOCATION, "Unknown marker format");
		}
		if (source.type.IsEmpty())
			return;
		result.stringParameters.Set("ReferenceType",ToString(source.type));
	}


	template <typename T> static T Require(T value)
	{
		if (!value)
			throw Round::RoundException(CLOCATION,"Required element is null");
		return value;
	}

	static void ImportReferenceEntity(Protocol::MarkerReferenceEntity&ref, const Entry&e )
	{
		e.uuidParameters.Require(CLOCATION,"ReferenceID",ref.id);
		String type;
		if (e.stringParameters.TryGet("ReferenceType",type))
			ref.type = Objects::DecodeType(type);
		else
			ref.type = Objects::EntityType();
	}



	void			Entry::Set(const String&name,const RoundNumber&n)
	{
		intParameters.Set(name+".main",(int)n.main);
	}

	void			Entry::Require(const TCodeLocation&loc, const String&name, RoundNumber&n)	const
	{
		intParameters.RequireCast(loc,name+".main",n.main);
	}


	/*virtual override*/ void Round::CreateMarker::Compile(Entry&e)	const
	{
		e.uuidParameters.Set("MarkerID",markerID);
		e.intParameters.Set("Type",(int)type);
		e.Set("CreatedInRound", createdInRound);
		e.locationParameters.Set("Location",location);
		ExportReferenceEntity(referenceEntity,e);
	}

	/*virtual override*/ void Round::CreateMarker::Decode(const Entry&e)
	{
		e.uuidParameters.Require(CLOCATION,"MarkerID",markerID);
		e.intParameters.RequireEnum(CLOCATION,"Type",type);
		e.Require(CLOCATION,"CreatedInRound", createdInRound);
		e.locationParameters.Require(CLOCATION,"Location",location);
		ImportReferenceEntity(referenceEntity,e);
	}

	/*virtual override*/ void Round::CreateMarker::Execute(const Objects::PFaction&f) const
	{

		Objects::Marker&marker = f->CreateMarker(markerID,f == Objects::Faction::nexus,type,referenceEntity.id,referenceEntity.owner);

		marker.createdInRound = createdInRound;
		marker.location = location;
		bool failed = false;
		if (marker.UtilizesReferenceEntity())
			failed = !ConvertReferenceEntity(referenceEntity, marker);

		TCP::PPeer peer = GetPeerOfFaction(f);
		if (failed)
		{
			f->DestroyMarker(marker.id);
			throw RoundException(CLOCATION,"Marker creation rejected. Invalid marker configuration");
			return;
		}
		//Protocol::Marker outMarker;
		//ConvertMarker(marker, outMarker);
		String ref;
		if (marker.UtilizesReferenceEntity())
			ref = ", referencing #"+String(int(marker.referencedType.baseType))+"-#"+String(int(marker.referencedType.turretType));
		LogEvent(EventSignificance::Common, peer, marker.GetName()+" created"+ref);
	}


	/*virtual override*/ void Round::DestroyMarker::Execute(const Objects::PFaction&f) const
	{
		if (f->DestroyMarker(markerID))
			LogEvent(EventSignificance::Common, GetPeerOfFaction(f), "Marker " + ToString(markerID) + " destroyed");
		else
			throw RoundException(CLOCATION, "Marker " + ToString(markerID) + " not found for destruction");
	}

	/*virtual override*/ void Round::DestroyMarker::Decode(const Entry&e)
	{
		e.uuidParameters.Require(CLOCATION,"MarkerID",markerID);
	}
	/*virtual override*/ void Round::DestroyMarker::Compile(Entry&e)	const
	{
		e.uuidParameters.Set("MarkerID",markerID);
	}

	/*virtual override*/ void Round::QueueResearch::Execute(const Objects::PFaction&f) const
	{
		if (drop)
		{
			if (!f->researchState.DropResearchTask(topic))
				throw RoundException(CLOCATION, "Unable to drop '"+topic->GetFullName()+"' from research queue. Has this been queued?");
		}
		else
		{
			if (f->researchState.ResearchIsPlanned(topic))
			{
				LogEvent(EventSignificance::Uncommon, GetPeerOfFaction(f), "Faction research "+topic->GetFullName()+" is already in the queue");
				return;
			}
			if (f->researchState.ResearchQueueIsFull())
				throw RoundException(CLOCATION, "Unable to append '"+topic->GetFullName()+"' to research queue. Queue is full");
			else
				if (!f->researchState.AppendResearchTask(topic))
					throw RoundException(CLOCATION, "Unable to append '"+topic->GetFullName()+"' to research queue. Is this a group?");
		}
		Network::PClient cl;
		if (Network::playerMap.Query(f,cl))
		{
			Protocol::ResearchQueue queue;
			f->researchState.ExportResearchQueue(queue);
			Channels::researchQueue.SendTo(cl->peer.lock(),queue);
		}
		if (drop)
			LogEvent(EventSignificance::Uncommon, GetPeerOfFaction(f), "Faction research "+topic->GetFullName()+" dropped");
		else
			LogEvent(EventSignificance::Uncommon, GetPeerOfFaction(f), "Faction research "+topic->GetFullName()+" appended");
	}

	/*virtual override*/ void Round::QueueResearch::Decode(const Entry&e)
	{
		String targetPath;
		e.stringParameters.Require(CLOCATION,"Target",targetPath);
		topic = Objects::ResearchState::FindTopic(targetPath);
		if (!topic)
		{
			throw Except::Program::FunctionalityNotImplemented(CLOCATION,"Unknown research target '"+targetPath+"'");
		}
		int i;
		if (e.intParameters.TryGet("Drop",i))
			drop = i != 0;
		else
			drop = false;
	}
	/*virtual override*/ void Round::QueueResearch::Compile(Entry&e)	const
	{
		e.stringParameters.Set("Target",topic->GetPath());
		if (drop)
			e.intParameters.Set("Drop",1);
	}


	/*virtual override*/ void	Round::EnableStructure::Execute(const Objects::PWorldEntity&e) const
	{
		float t = 0;
		Objects::RefreshCompoundCoverageIfNeeded(t);
		if (!e->IsWorkingStructure())
		{
			throw Round::RoundException(CLOCATION,"Cannot enable '"+e->GetName()+"': Not a working structure");
		}
		Objects::PWorkingStructure w = To<Objects::WorkingStructure>::Cast(e);
		#ifndef ONE_COMPOUND_PER_FACTION
			if (w->compoundID == Objects::Compound::InvalidNetwork)
			{
				Objects::RefreshCompoundCoverage(0);
				ASSERT__(w->compoundID != Objects::Compound::InvalidNetwork);
			}
		#endif

		if (w->SetEnabled(doEnable,t))
			LogEvent(EventSignificance::Common,e.get(),"Modified enabled-state "+String(doEnable));
		else
			throw RoundException(CLOCATION,"Cannot enable structure at this time");
	}

	/*virtual override*/ void	Round::EnableStructure::Decode(const Entry&e)
	{
		int v = e.intParameters.Require(CLOCATION,"DoEnable");
		doEnable = v != 0;
	}
	/*virtual override*/ void	Round::EnableStructure::Compile(Entry&e)	const
	{
		e.intParameters.Set("DoEnable",(int)doEnable);
	}


	/*virtual override*/ void	Round::RestoreCoreLink::Execute(const Objects::PWorldEntity&e) const
	{
		Objects::PMine m = To<Objects::Mine>::DynamicCast(e);
		if (!m)
			throw RoundException(CLOCATION,"Unable to restore core link of non-mine");

		if (m->LinkToCore(0))
			LogEvent(EventSignificance::Common,e.get(),"Core-link restored");
		else
			LogEvent(EventClass::Preset::operationAborted,e.get(),"Unable to restore Core-link");
	}

	/*virtual override*/ void	Round::ConstructionAbort::Execute(const Objects::PWorldEntity&e) const
	{
		Objects::PMobileEntityProducer p = To<Objects::MobileEntityProducer>::DynamicCast(e);
		if (!p)
			throw RoundException(CLOCATION,"Tried to abort construction-plant job on non-plant entity ");

		if (!p->AbortConstruction(taskID))
		{
			String extra = "";
			if (p->queue.IsNotEmpty())
			{
				extra = " (active jobs are";
				foreach (p->queue,q)
					extra += " "+ToString(q->id);
				extra += ")";
			}
			throw RoundException(CLOCATION,"Tried to abort non-existend construction-plant job '"+ToString(taskID)+"' on plant entity"+extra);
		}
		else
			LogEvent(EventSignificance::Common,e.get(),"Aborted construction job "+ToString(taskID));
	}


	/*virtual override*/ void	Round::ConstructionAbort::Decode(const Entry&e)
	{
		e.uuidParameters.Require(CLOCATION,"TaskID",taskID);
	}

	/*virtual override*/ void	Round::ConstructionAbort::Compile(Entry&e)	const
	{
		e.uuidParameters.Set("TaskID",taskID);
	}



	void	Round::Register(bool execute, const Objects::PFaction&f, Round::FactionEvent*newEvent)
	{
		PFactionEvent ev(newEvent);
		if (execute && !ev->TryExecute(f))
			return;
		factionStrands.Set(f->id).factionEvents.Append(ev);
	}

	void	Round::Register(bool execute, const TUniqueID&fID, Round::FactionEvent*newEvent)
	{
		PFactionEvent ev(newEvent);
		if (execute)
		{ 
			Objects::PFaction&f = Objects::Faction::FindByID(fID);
			if (!f)
			{
				LogEvent(EventClass::Preset::commandRejected,"Faction "+ToString(fID)+" not found for event execution "+String(typeid(newEvent).name()));
				return;
			}
			if (!ev->TryExecute(f))
				return;
		}
		factionStrands.Set(fID).factionEvents.Append(ev);
	}

	void	Round::Register(bool execute, const Objects::PWorldEntity&e, Round::EntityEvent*newEvent)
	{
		PEntityEvent ev(newEvent);
		if (execute && !ev->TryExecute(e))
			return;
		factionStrands.Set(e->owner->id).entityStrands.Set(e->id).Append(ev);
	}

	void	Round::Register(bool execute, Round::GlobalEvent*newEvent)
	{
		PGlobalEvent ev(newEvent);
		if (execute && !ev->TryExecute())
			return;
		globalEvents.Append(ev);
	}

	void	Round::RegisterDropEntityCommands(bool execute, const Objects::PWorldEntity&e, Protocol::DropMode mode)
	{
		EntityStrand&strand = factionStrands.Set(e->owner->id).entityStrands.Set(e->id);
		if (mode == Protocol::DropMode::AllTasks)
		{
			strand.ClearAll(typeid(Round::ClearEntityCommands));
			strand.ClearAll(typeid(Round::DirectToCommand));
			strand.ClearAll(typeid(Round::JobCommand));
		}
		elif(mode == Protocol::DropMode::LastTask)
		{
			index_t lastDropCommand = strand.FindLast(typeid(Round::ClearEntityCommands));
			index_t lastCommand = strand.FindLastOf<Round::DirectToCommand,Round::JobCommand>();
			PEntityEvent clear(new ClearEntityCommands(mode));
			if (execute && !clear->TryExecute(e))
				return;
			if (lastDropCommand == InvalidIndex)
			{
				if (lastCommand != InvalidIndex)
				{
					strand.Erase(lastCommand);
				}
				else
				{
					strand.Append(clear);
				}
			}
			else
			{
				if (lastCommand == InvalidIndex)
				{
					strand.Append(clear);
				}
				else
				{
					ASSERT_GREATER__(lastCommand,lastDropCommand);
					strand.Erase(lastCommand);
				}
			}
			return;
		}
		strand.Append().reset(new ClearEntityCommands(mode));
		if (execute)
			strand.last()->Execute(e);
	}


	void	Round::RegisterDirectEntityTo(bool apply, const Objects::PWorldEntity&e, Objects::CursorMode mode, const Ctr::ArrayRef<Grid::LCellIdI>&target, bool detailed, bool mouseLeftClick, bool clearSafeTasks)
	{
		EntityStrand&strand = factionStrands.Set(e->owner->id).entityStrands.Set(e->id);
		std::shared_ptr<DirectToCommand> cmd(new DirectToCommand());
		cmd->leftClick = mouseLeftClick;
		cmd->mode = mode;
		cmd->detailed = detailed;
		cmd->clearSafeTasks = clearSafeTasks;
		cmd->targets = target;
		if (!apply || cmd->TryExecute(e))
			strand.Append(cmd);
	}

	void	Round::RegisterChoiceIntersection(bool apply, const Objects::PWorldEntity&e,const Grid::LCellIdI&target, Objects::CursorMode mode, const Objects::PWorldEntity&deployingWhat)
	{
		EntityStrand&strand = factionStrands.Set(e->owner->id).entityStrands.Set(e->id);
		std::shared_ptr<ChoiceIntersection> cmd(new ChoiceIntersection());
		cmd->deployingWhat = deployingWhat?deployingWhat->id : TUniqueID::Empty();
		cmd->mode = mode;
		cmd->target = target;
		if (!apply || cmd->TryExecute(e))
			strand.Append(cmd);
	}


	void	Round::LogEntityCreation(const Objects::PWorldEntity&e)
	{
		FactionStrand&ftrand = factionStrands.Set(e->owner->id);
		PFactionEvent cmd(new CreateEntity(e));
		ftrand.factionEvents.Append(cmd);
	}


	void	Round::RegisterEntityDestruction(bool apply, const Objects::PWorldEntity&e, Objects::RemovalType t)
	{
		EntityStrand&strand = factionStrands.Set(e->owner->id).entityStrands.Set(e->id);
		PEntityEvent cmd(new DestroyEntity(t));
		if (!apply || cmd->TryExecute(e))
		{
			strand.ClearAll<Round::ClearEntityCommands>();
			strand.ClearAll<Round::DirectToCommand>();
			strand.ClearAll<Round::JobCommand>();
			strand.ClearAll<Round::NewEntityProduction>();
			strand.ClearAll<Round::DropEntityProduction>();
			strand.Append(cmd);
		}
	}


	void	Round::LogFactionOnline(const Objects::PFaction&f, bool nowOnline)
	{
		FactionStrand&strand = factionStrands.Set(f->id);
		index_t at = strand.factionEvents.FindLast<Round::FactionOnlineChange>();
		if (at != InvalidIndex)
		{
			PEvent ev = strand.factionEvents[at];
			FactionOnlineChange*ch = (FactionOnlineChange*)ev.get();
			if (ch->nowOnline == nowOnline)
				return;
			strand.factionEvents.Erase(at);
			return;
		}

		strand.factionEvents.Append().reset(new FactionOnlineChange(nowOnline));
	}

	void	Round::LogFactionOnline(const Objects::PFaction&f)
	{
		LogFactionOnline(f,true);
	}

	void	Round::LogFactionOffline(const Objects::PFaction&f)
	{
		LogFactionOnline(f,false);
	}

	void	Round::RegisterProductionJobStart(bool apply, const Objects::PWorldEntity&e, const Objects::PMobileEntityProducer&asProducer, const Objects::EntityType&t)
	{
		if (apply && asProducer->queue.GetLength() >= Objects::MaxProductionTasks)
		{
			LogEvent(EventClass::Preset::commandRejected,e.get(),"Job denied: Max production jobs reached");
			return;
		}

		std::shared_ptr<NewEntityProduction> cmd(new NewEntityProduction());
		cmd->insertAt = InvalidIndex;	//last

		Objects::PWorldEntity proto = Objects::Prototypes::Find(t);
		if (proto)
		{
			if (!proto->IsMobile())
			{
				LogEvent(EventClass::Preset::commandRejected,e.get(),"Cannot produce that");
				return;
			}
			if (!proto->IsUnlockedAtState(e->GetResearchState()))
			{
				LogEvent(EventClass::Preset::commandRejected,e.get(), "Cannot build "+proto->GetTypeName()+" since it has not been researched yet");
				return;
			}
		}
		else
		{
			Objects::Component::Turret*turret = Objects::Component::vehicleTurretComponents.Find(t.turretType);
			Objects::Component::Base*base = Objects::Component::baseComponents.Find(t.baseType);
			if (!turret || !base)
			{
				LogEvent(EventClass::Preset::commandRejected,e.get(),"Cannot produce that");
				return;
			}
			if (!turret->isUnlocked.GetValue(e->GetResearchState()))
			{
				LogEvent(EventClass::Preset::commandRejected,e.get(), "Cannot build "+turret->typeName+" since it has not been researched yet");
				return;
			}
			if (!base->isUnlocked.GetValue(e->GetResearchState()))
			{
				LogEvent(EventClass::Preset::commandRejected,e.get(), "Cannot build "+base->typeName+" since it has not been researched yet");
				return;
			}
		}

		EntityStrand&strand = factionStrands.Set(e->owner->id).entityStrands.Set(e->id);

		cmd->taskID.Generate();
		cmd->newEntityID.Generate();
		cmd->desc.type = t;
		strand.Append(cmd);
		if (apply)
			strand.last()->Execute(e);
	}


	void	Round::Clear()
	{
		factionStrands.Clear();
		globalEvents.Clear();
	}


	void	Round::Decode(const Entry&roundRootEntry)
	{
		Clear();
		if (roundRootEntry.operation != OperationNameOf(typeid(Round::CommitRound)))
		{
			throw RoundException(CLOCATION, "Expected CommitRound, but got "+String(roundRootEntry.operation));
		}
		
		{
			Round::CommitRound commit(0);
			commit.Decode(roundRootEntry);
			evolver = Evolver::Get(commit.evolverName);
			if (!evolver)
				throw Except::Program::ParameterFault(CLOCATION,"Unable to find round-processing evolver '"+String(commit.evolverName)+"'");

			roundNumber = commit.roundNumber;
			fileDataVersion = commit.fileDataVersion;
		}

		foreach (roundRootEntry.children,child)
		{
			if (child->operation == "FactionData")
			{
				const TUniqueID&factionID = child->uuidParameters.Require(CLOCATION,"FactionID");
				FactionStrand*strand = factionStrands.SetNew(factionID);
				if (!strand)
					throw Except::Program::UniquenessViolation(CLOCATION, "Faction '"+ToString(factionID)+"' is defined multiple times in this round");


				foreach(child->children,entityChild)
				{
					if (entityChild->operation == "EntityData")
					{
						const TUniqueID&entityID = entityChild->uuidParameters.Require(CLOCATION,"EntityID");
						EntityStrand*estrand = strand->entityStrands.SetNew(entityID);
						if (!estrand)
							throw Except::Program::UniquenessViolation(CLOCATION, "Entity '"+ToString(entityID)+"' is defined multiple times in this round, and faction");

						#ifdef _DEBUG
							factionStrands.VisitAllEntries([factionID, entityID](const TUniqueID&ofactionID, const FactionStrand&strand)
							{
								if (strand.entityStrands.IsSet(ofactionID))
									throw Except::Program::UniquenessViolation(CLOCATION, "Entity '"+ToString(entityID)+"' is defined in multiple factions ("+ToString(ofactionID)+" and "+ToString(factionID)+")");
							});
						#endif

						foreach (entityChild->children, entityStrandChild)
						{
							PEvent ev = eventMap.Require(entityStrandChild->operation);
							PEntityEvent nev = To<EntityEvent>::DynamicCast(PEvent(ev->Instantiate()));
							ASSERT__(nev);
							nev->Decode(*entityStrandChild);
							estrand->Append(nev);
						}
					}
					else
					{
						PEvent ev = eventMap.Require(entityChild->operation);
						PFactionEvent nev = To<FactionEvent>::DynamicCast(PEvent(ev->Instantiate()));
						ASSERT__(nev);
						nev->Decode(*entityChild);
						strand->factionEvents.Append(nev);
					}
				}
			}
			else
			{
				PEvent ev = eventMap.Require(child->operation);
				PGlobalEvent nev = To<GlobalEvent>::DynamicCast(PEvent(ev->Instantiate()));
				ASSERT__(nev);
				nev->Decode(*child);
				globalEvents.Append(nev);
			}
		}
	}


	void Round::AppendEntityVerification()
	{
		Objects::ForeachWorldEntityIncludingStored([this](const Objects::PWorldEntity&e)
		{
			PEntityEvent ev(new Round::EntityStateCheck(e));
			factionStrands.Set(e->owner->id).entityStrands.Set(e->id).Append(ev);
		},false);
	}

	/*static*/ StringRef	Round::OperationNameOf(const std::type_info&info)
	{
		const char*raw = info.name();
		const char*const begin = raw;
		raw += strlen(raw);
		const char*end = raw;
		raw--;
		while (raw != begin && *raw != ':')
			raw--;
		if (raw != begin)
			raw++;
		return StringRef(raw,end-raw);
	}

	void	Round::Event::ExportTo(Entry&e) const
	{
		e.operation = OperationNameOf(typeid(*this));

		Compile(e);
	}


	void	Writer::Commit(const Round&round)
	{
		ASSERT__(logStream.is_open());
		Entry commit;
		Round::PEvent ev(new Round::CommitRound(round.evolver->name,round.roundNumber));
		ev->ExportTo(commit);

		for (const auto&ev : round.globalEvents)
		{
			ev->ExportTo(commit.children.Append());
		}



		round.factionStrands.VisitAllEntries([this,&commit](const TUniqueID&factionID, const Round::FactionStrand&fstrand)
		{
			Entry&super = commit.children.Append();
			super.operation = "FactionData";
			super.uuidParameters.Set("FactionID",factionID);

			for (const auto&ev : fstrand.factionEvents)
			{
				ev->ExportTo(super.children.Append());
			}
			fstrand.entityStrands.VisitAllEntries([this,&super](const TUniqueID&key, const Round::EntityStrand&strand)
			{
				Entry&master = super.children.Append();
				master.uuidParameters.Set("EntityID",key);
				master.operation = "EntityData";



				for (const auto&ev : strand)
				{
					ev->ExportTo(master.children.Append());
				}


				//Log(master);
			});
		});

		Log(commit);
		ASSERT__(logStream.is_open());
	}



	//must be string (and not std::type_id) so that loading strings works
	/*static*/ Ctr::StringTable<Round::PEvent>	Round::eventMap;



	

	/*static*/ void	Round::StaticCreate()
	{
		#undef MAP_ADD
		#define MAP_ADD(X)	eventMap.Set(String(OperationNameOf(typeid(X))),PEvent(new X()));
		MAP_ADD(ClearEntityCommands);
		MAP_ADD(DirectToCommand);
		MAP_ADD(ChoiceIntersection);
		MAP_ADD(DropEntityProduction);
		MAP_ADD(CreateEntity);
		MAP_ADD(CreateFaction);
		MAP_ADD(FactionOnlineChange);
		MAP_ADD(JobCommand);
		MAP_ADD(NewEntityProduction);
		MAP_ADD(CommitRound);
		MAP_ADD(DestroyEntity);
		MAP_ADD(EntityStateCheck);
		MAP_ADD(ChangeAutomation);
		MAP_ADD(ChangeSleep);
		MAP_ADD(UpdateVariable);
		MAP_ADD(ResourceCoverageChange);
		MAP_ADD(ResourceCoverageKnowledgeLoss);
		MAP_ADD(EnableStructure);
		MAP_ADD(CreateMarker);
		MAP_ADD(DestroyMarker);
		MAP_ADD(RestoreCoreLink);
		MAP_ADD(ConstructionAbort);
		MAP_ADD(QueueResearch);
	}

	void	Create()
	{
		Evolver::Create();
		currentRound.evolver = Evolver::GetDefault();
		currentRound.roundNumber = 0;
		Round::StaticCreate();
	}

	void	Destroy()
	{


	}


	Round::CreateFaction::FactionType	GetType(const Objects::PFaction&f)
	{
		Round::CreateFaction::FactionType t = Round::CreateFaction::FactionType::Regular;
		if (f == Objects::Faction::nexus)
			t = Round::CreateFaction::FactionType::Nexus;
		return t;
	}


}
