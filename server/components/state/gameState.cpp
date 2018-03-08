#include <global_root.h>
#include "gameState.h"
#include "networkState.h"
#include "../networkChannels.h"
#include "../aiProcess.h"
#include "../journalSaveGame.h"
#include <objects/structure/mine.h>
#include <objects/structure/vehiclePlant.h>

void			ApplicationInterface::SignalFactionsChanged()
{
	Game::mostFactionsAlive = std::max(Game::mostFactionsAlive, (UINT32)Game::CountLivingPlayerFactions());
	Game::mostAlliancesAlive = std::max(Game::mostAlliancesAlive, (UINT32)Game::CountLivingPlayerAlliances());
	Reflection::SignalFactionsChanged();
}



namespace Game
{
	UINT32				mostFactionsAlive=0, mostAlliancesAlive=0;
	Evolver::Abstract	*evolver = nullptr;
	Objects::WAlliance	winner;	//if set indicates game is over
	Objects::WAlliance	postEvolutionWinner;	//if winner is declared while evolver is active, this variable is set instead for later
	Timer::Time			lastPhaseChange = timer.Now();
	bool				roundTimerPaused = true,endRoundNow=false, evolverActive = false;
	float				factionStartMineralValue=1.f;	//determined during loading
	float				effectiveAnimationLength = 1.f;

	INT32				tutorialAt=1;

	#pragma region vote

	/*static*/ HashTable<TUniqueID,PVote>	Vote::all;
	/*static*/ HashTable<Array<TUniqueID>,PVote>	Vote::map;

	/*static*/ void		Vote::Flush()
	{
		all.Clear();
		map.Clear();
	}


	/*static*/ PVote	Vote::CreateOrGet(const ArrayRef<const TUniqueID>&concerning, const String&phrase, float closingAfterSeconds, const FScope&scope, const FCallback&onClose)
	{
		PVote rs = FindConcerning(concerning);
		if (!rs)
		{
			rs.reset(new Vote(concerning,phrase,closingAfterSeconds));
			rs->scope = scope;
			rs->onClose = onClose;
			all.Set(rs->id,rs);
			map.Set(concerning,rs);
		}
		return rs;
	}



	/*static*/ PVote	Vote::CreateNew(const ArrayRef<const TUniqueID>&concerning, const String&phrase, float closingAfterSeconds, const FScope&scope, const FCallback&onClose)
	{
		PVote rs = FindConcerning(concerning);
		if (rs)
			return nullptr;
		{
			rs.reset(new Vote(concerning,phrase,closingAfterSeconds));
			rs->scope = scope;
			rs->onClose = onClose;
			all.Set(rs->id,rs);
			map.Set(concerning,rs);
			rs->Advertise();
		}
		return rs;
	}


	/*static*/ void		Vote::CheckAllTimeouts()
	{
		all.FilterEntries([](const TUniqueID&, const PVote&v) -> bool
		{
			if (!v->CheckTimeouts())
			{
				map.Unset(v->concerning);
				return false;
			}
			return true;
		});
	}

	/*static*/ void		Vote::CheckAllVotes()
	{
		all.VisitAllValues([](const PVote&v) 
		{
			v->CheckVotes();
		});
	}

	/*static*/ void		Vote::CastVote(const TUniqueID&voteID, const Objects::PFaction&f, bool inFavor)
	{
		auto vote = all.QueryPointer(voteID);
		if (!vote)
		{
			LogEvent(EventClass::Preset::commandRejected,f,"Vote "+ToString(voteID)+" does no longer exist");
			return;
		}
		(*vote)->CastVote(f,inFavor);
	}

	void			Vote::CastVote(const Objects::PFaction&f, bool inFavor)
	{
		if (!MayVote(f))
		{
			LogEvent(EventClass::Preset::commandRejected,f,"Faction not allowed to vote on this issue");
			return;
		}

		if (closed)
		{
			LogEvent(EventClass::Preset::commandRejected,f,"Vote is closed");
			return;
		}
		
		bool*vote = this->votes.QueryPointer(f->id);
		if (vote)
		{
			if (*vote != inFavor)
			{
				*vote = inFavor;
				LogEvent(EventSignificance::Common,f,"Vote: Changed oppinion on "+ToString(id)+": "+String(inFavor));
			}
			else
			{
				LogEvent(EventSignificance::Debug,f,"Vote: Voted but did not change oppinion on "+ToString(id)+": "+String(inFavor));
				return;
			}
		}
		else
		{
			votes.Set(f->id,inFavor);
			LogEvent(EventSignificance::Common,f,"Vote: Voted on "+ToString(id)+": "+String(inFavor));
		}
		if (RuntimeConfig::IsInAnyCommandPhase())
			CheckVotes();
	}

	/*static*/ void		Vote::AdvertiseAllToNewlyConnectedPeer(const Objects::PFaction&f, TCP::Peer&peer)
	{
		all.VisitAllValues([&peer,f](const PVote&vote)
		{
			if (!vote->closed && vote->MayVote(f))
			{
				Protocol::VoteCreation out;
				out.phrase = vote->phrase;
				out.remainingSeconds = vote->closingAfterSeconds;
				out.voteID = vote->id;
				Channels::VoteCreation.SendTo(peer,out);
			}
		});
	}


	void			Vote::Advertise() const
	{
		Protocol::VoteCreation vote;
		vote.phrase = phrase;
		vote.remainingSeconds = closingAfterSeconds;
		vote.voteID = id;

		if (!scope)
			Channels::VoteCreation.SendTo(::Network::server,vote,Channels::Level::Faction);
		else
			::Network::playerMap.VisitAllEntries([vote,this](const Objects::PFaction&f, const ::Network::PClient&client)
			{
				if (scope(f))
					Channels::VoteCreation.SendTo(client->peer.lock(),vote);
			});
	}

	bool			Vote::CheckTimeouts()
	{
		float seconds = GetAge();
		if (seconds < closingAfterSeconds && !closed)
			return true;
		Close();
		
		if (seconds > clearedAfterSeconds)
		{
			LogEvent(EventSignificance::Common,"Clearing out vote "+ToString(id)+" after "+String(seconds)+" sec.");
			return false;
		}
		return true;
	}

	count_t			Vote::CountEligibleVoters() const
	{
		count_t rs = 0;
		Objects::ForeachFaction([&rs,this](const Objects::PFaction&f){if (!scope || scope(f)) rs++;},false);
		return rs;
	}

	/*static*/ count_t	Vote::GetQuorum(Quorum q, count_t eligibleVoters)
	{
		switch (q)
		{
			case Quorum::All:
				return eligibleVoters;
			case Quorum::TwoThirds:
				return Math::CeilDiv<count_t>(eligibleVoters*2,3);
			case Quorum::Majority:
				return eligibleVoters /2 + 1;
			case Quorum::OneThird:
				return Math::CeilDiv<count_t>(eligibleVoters,3);
		}
		FATAL__("Unexpected type for quorum: #"+String(index_t(q)));
		return 1;
	}

	/*static*/ count_t	Vote::GetVotesToPass(VotesToPass v, count_t voters)
	{
		switch (v)
		{
			case VotesToPass::SimpleMajority:
				return (voters / 2) +1;
			case VotesToPass::TwoThirds:
				return Math::CeilDiv<count_t>(voters*2,3);
			case VotesToPass::Unanimous:
				return voters;
		}
		FATAL__("Unexpected votes to pass type: #"+String(index_t(v)));
		return 1;
	}


	void Vote::CheckResults(Protocol::VoteResult&rs, bool timedOut)	const
	{

		bool test = Array<int>() == Array<char>();

		rs.eligibleToVote = (UINT32)CountEligibleVoters();
		rs.quorum = (UINT32)GetQuorum(this->quorum, rs.eligibleToVote);
		rs.votedAgainst = 0;
		rs.votedInFavor = 0;
		rs.voteID = id;

		ASSERT_LESS_OR_EQUAL__(votes.Count(),rs.eligibleToVote);

		votes.VisitAllValues([&rs](bool inFavor)
		{
			if (inFavor)
				rs.votedInFavor ++;
			else
				rs.votedAgainst ++;
		});

		const bool quorumReached = votes.Count() >= rs.quorum && timedOut;
		const count_t votesToPassTotal = GetVotesToPass(votesToPass,rs.eligibleToVote);
		const count_t votesToPassQuorum = GetVotesToPass(votesToPass,votes.Count());
		const bool passed = rs.votedInFavor >= votesToPassTotal || (quorumReached && rs.votedInFavor >= votesToPassQuorum);
		const count_t abstainedVotes = rs.eligibleToVote - votes.Count();


		if (passed)
			rs.resultType = Protocol::VoteResultType::Passed;
		else
			if (quorumReached || rs.votedInFavor + abstainedVotes < votesToPassTotal)
				rs.resultType = Protocol::VoteResultType::Rejected;
			else
				rs.resultType = Protocol::VoteResultType::TimedOut;
	}



	void			Vote::Close()
	{
		if (closed)
			return;

		Protocol::VoteResult rs;
		CheckResults(rs,true);
		Close(rs);
	}


	void			Vote::Cancel()
	{
		Protocol::VoteResult rs;
		CheckResults(rs,true);
		rs.resultType = Protocol::VoteResultType::Canceled;
		this->clearedAfterSeconds = std::min(this->clearedAfterSeconds, this->GetAge() + 2.f);	//reduce to 2 seconds on cancel
		Close(rs);
	}


	void			Vote::Close(const Protocol::VoteResult&rs)
	{
		if (closed)
			return;
		closed = true;

		LogEvent(EventClass::Preset::common,"Closing vote "+ToString(id));
		const count_t votesToPassTotal = GetVotesToPass(votesToPass,rs.eligibleToVote);
		const count_t votesToPassQuorum = GetVotesToPass(votesToPass,votes.Count());

		String state = "  voted: "+String(votes.Count())+"/"+String(rs.eligibleToVote)+", quorum="+String(rs.quorum)+", toPassTotal="+String(votesToPassTotal)+", toPassQ="+String(votesToPassQuorum);

		switch (rs.resultType)
		{
			case Protocol::VoteResultType::Passed:
				state += "  passed";
			break;
			case Protocol::VoteResultType::Rejected:
				state += "  rejected";
			break;
			case Protocol::VoteResultType::TimedOut:
				state += "  timed out";
			break;
			case Protocol::VoteResultType::Canceled:
				state += "  canceled";
			break;
			default:
				state += "  unknown(#"+String(rs.resultType)+")";
			break;
		}
		LogEvent(EventClass::Preset::common,state);


		if (!scope)
			Channels::VoteResult.SendTo(::Network::server,rs,Channels::Level::Faction);
		else
			::Network::playerMap.VisitAllEntries([rs,this](const Objects::PFaction&f, const ::Network::PClient&client)
			{
				if (scope(f))
					Channels::VoteResult.SendTo(client->peer.lock(),rs);
			});

		onClose(shared_from_this(), rs);
	}

	void			Vote::CheckVotes()
	{
		if (closed)
			return;

		Protocol::VoteResult rs;
		CheckResults(rs, false);
		if (rs.resultType != Protocol::VoteResultType::TimedOut)
			Close(rs);
	}

	#pragma endregion



	count_t				Config::CountActuallyAccessibleZones(count_t temporaryFactionCountBonus/*=0*/)
	{
		count_t numZones = CountTheoreticallyAccessibleZones(temporaryFactionCountBonus);
		if (numZones == 0)
			return 1;

		count_t unlocked = 0;
		for (index_t i = 0; i < numZones; i++)
		{
			if (Nexus::bases.havePlaced && BaseGrid::IsIsolatedCorner(unlocked))
			{
				unlocked++;
			}
			unlocked++;
		}
		return unlocked;
	}


	void				SetDefaultEvolver()
	{
		evolver = Evolver::GetDefault();
	}

	Evolver::Abstract*	GetEvolver()
	{
		return evolver;
	}

	void			BeginRoundProcessing()
	{
		ASSERT__(!evolverActive);
		evolverActive = true;
		evolver->BeginRoundProcessing();
	}

	void			FinishRoundProcessing()
	{
		ASSERT__(evolverActive);
		evolverActive = false;
		evolver->FinishRoundProcessing();
	}

	bool			ContinueRoundProcessing()
	{
		ASSERT__(evolverActive);
		return evolver->ContinueRoundProcessing();
	}

	bool			IsProcessingRound()
	{
		return evolverActive;
	}


	namespace Config
	{
		int			roundTimer = -1;
		UINT32		uplinkBandwidth = 500;

		bool		closedGame = false,
					startBases = false,
					basesRequired = false,
					exposeActionLeftToHostileFactions = true;
		UINT32		factionsPerZone = 
					#ifdef LOWER_RESOLUTION_PLANET
						1
					#else
						3
					#endif
					;

		UINT32		factionSupportFlags = Protocol::GameFlag::AllFactionSupport;

		namespace Nexus
		{
			Placement	bases,
						allPorts,
						satellitesAtopBases
						;

			bool		enableAI = false;
			UINT32		roundsPerResearchPoint=10;
			float		threatEstimationFactor = 1.f;
			Ctr::Array<Objects::Component::Turret*>	vehicleTurrets;
		}

		count_t	GetTargetNexusCount()
		{
			if (RuntimeConfig::empZones)
				return CountActuallyAccessibleZones();
			if (Game::Config::Nexus::allPorts.havePlaced)
			{
				return 10;
			}
			else
				return 2; //should be 2 here, because in normal ConquerNexus mode, only nexus bases are spawned, whereas in EMP ConsquerNexus mode ports are spawned on all of the 12 poles
		}


	}
	


	namespace Network
	{
		static void		_UpdateResourceCoverage(const Objects::PFaction&f, const Grid::LCellIdI&center,float range, const Protocol::Timestamp&timestamp)
		{
			static Ctr::Buffer0<UINT32>	changed;
			if (!f)
				return;
			f->knownResources.Update(center,range,changed);
			if (changed.IsEmpty())
				return;
			::Network::PClient client;
			if (!::Network::playerMap.Query(f,client))
				return;
			TCP::PPeer peer = client->peer.lock();
			if (!peer)
				return;

			Protocol::ResourceUpdate	package;
			package.timestamp = timestamp;
			package.cells.SetSize(changed.Count());
			for (index_t i = 0; i < changed.Count(); i++)
				package.cells[i].cellIndex = changed[i];
			foreach (Grid::Resource::all,res)
			{
				package.resourceName = res->GetCName();
				for (index_t i = 0; i < changed.Count(); i++)
				{
					BYTE sourceCov = res->coverage[changed[i]];
					package.cells[i].coverage = sourceCov;
				}

				Channels::resourceUpdate.SendTo(peer,package);
			}
		}

		void		UpdateResourceCoverage(const Objects::PFaction&f, const Grid::LCellIdI&center,float range, const Protocol::Timestamp&timestamp)
		{
			if (!f)
				return;
			if (!f->alliance->config.shareSensors)
				_UpdateResourceCoverage(f,center,range,timestamp);
			else
				f->alliance->ForeachMember([center,range,timestamp](const Objects::PFaction&f2)
				{
					_UpdateResourceCoverage(f2,center,range,timestamp);
				});
		}



		void SendStatisticsTo(TCP::Peer & peer, const Objects::PFaction & faction)
		{
			Objects::Statistics::EnterNetworkMode();
			Channels::Statistics::beginSequence.SendTo(peer,faction->id);

			faction->statistics.VisitRounds([&peer](const Objects::Statistics::Round&r)
			{
				Channels::Statistics::round.SendTo(peer,r);
			},true);
			faction->statistics.VisitEntityTypes([&peer](const Objects::Statistics::EntityType&t)
			{
				Channels::Statistics::entityType.SendTo(peer,t);
			});
			faction->statistics.VisitFactionInteractions([&peer](const Objects::Statistics::FactionInteraction&i)
			{
				Channels::Statistics::factionInteraction.SendTo(peer,i);
			});
			peer.SendSignal(Protocol::ChannelID::EndStatisticsSequence);
		}


		void		HandleSessionSuspension()
		{
			if (Config::Nexus::enableAI)
				AIProcess::End();
		}

		void		HandleSessionClosure()
		{
			roundTimerPaused = true;
			mostFactionsAlive = 0;
			mostAlliancesAlive = 0;
			tutorialAt=1;
			endRoundNow = false;
			winner.reset();
			postEvolutionWinner.reset();
			HandleSessionSuspension();
			Vote::Flush();
		}

		void		HandleSessionRestart()
		{
			if (Config::Nexus::enableAI)
				AIProcess::Begin(Objects::Faction::nexus->accessToken);
			ApplicationInterface::SignalFactionsChanged();
		}


		UINT32		AssembleGameFlags(bool allowSpectatorFactions)
		{
			UINT32 rs = 0;
			if (Config::closedGame)
				rs |= Protocol::GameFlag::ClosedGame;
			if (RuntimeConfig::empZones)
				rs |= Protocol::GameFlag::EMPZones;
			if (Config::basesRequired)
				rs |= Protocol::GameFlag::BasesRequired;
			if (Config::startBases)
				rs |= Protocol::GameFlag::StartBases;
			if (Config::Nexus::enableAI)
				rs |= Protocol::GameFlag::NexusAI;
			if (RuntimeConfig::tutorialMode)
				rs |= Protocol::GameFlag::TutorialMode;
			if (World::Config::simulateLiquids)
				rs |= Protocol::GameFlag::SimulateLiquids;
			if (Config::Nexus::allPorts.havePlaced)
				rs |= Protocol::GameFlag::NexusPortsOnAllPoles;
			if (Config::Nexus::bases.havePlaced)
				rs |= Protocol::GameFlag::NexusHasBases;

			rs |= Config::factionSupportFlags;
			if (!allowSpectatorFactions)
				rs &= ~Protocol::GameFlag::SupportSpectatorFactions;

			if (RuntimeConfig::phase == RuntimeConfig::Phase::EndOfGame)
			{
				rs |= Protocol::GameFlag::GameHasEnded | Protocol::GameFlag::SupportSpectatorFactions;	//always support spectator in this case
			}
			return rs;
		}


		void		AssembleGameConfig(Protocol::GameConfig&gameConfig, bool allowSpectatorFactions)
		{
			gameConfig.researchScript = Objects::ResearchState::GetScriptIdentifier();
			gameConfig.gameGoal = (UINT32)RuntimeConfig::gameGoal;
			gameConfig.gameFlags = AssembleGameFlags(allowSpectatorFactions);
			gameConfig.worldDiscovery = (BYTE)World::Config::worldDiscovery;

		}

		void	AssembleGameInfo(Protocol::GameInfo&info, bool allowSpectatorFactions)
		{
			AssembleGameConfig(info.gameConfig,allowSpectatorFactions);
			info.activePhase = RuntimeConfig::phase;
			info.numFactionsOnline = (UINT16)::Network::activePlayersAndSpectators.Count();
			info.numFactionsCreated = (UINT16)Objects::Faction::all.Count() + (Config::Nexus::enableAI ? 1 : 0);
			info.roundNumber = RuntimeConfig::roundNumber;
			info.worldID = World::worldID;
		}


		void	ChangePhase( RuntimeConfig::Phase to, float timeWindow/* = -1*/)
		{
			if (RuntimeConfig::phase == RuntimeConfig::Phase::EndOfGame)
			{
				DBG_FATAL__("Trying to change from end-of-game to "+String(to.ToString()));
				return;
			}

			LogEvent(EventSignificance::Common,"Changing phase to "+String(to.ToString())+" with time window "+String(timeWindow)+" after "+String(timer.GetSecondsSince(lastPhaseChange))+" seconds");


			if (RuntimeConfig::IsACommandPhase(to))
			{
				Objects::ForeachFaction([](const Objects::PFaction&f)
				{
					f->DropPerRoundData();	//make sure instantaneous destruction commands are processed with correct visibility information

					if (f->researchState.changed)
					{
						f->researchState.changed = false;
						Protocol::ResearchState state;
						state.ownerID = f->id;
						f->researchState.Export(state.serializedData);
						ASSERT_LESS__(state.serializedData.Count(),state.serializedData.MaxElementCount);
						Channels::researchState.SendTo(::Network::server,state,(UINT32)::Network::UserLevel::CommonFaction);
					}
					if (f->researchState.queueChanged)
					{
						f->researchState.queueChanged = false;
						::Network::PClient client;
						if (::Network::playerMap.Query(f,client))
						{
							Protocol::ResearchQueue queue;
							f->researchState.ExportResearchQueue(queue);
							::Channels::researchQueue.SendTo(client->peer.lock(),queue);
						}
					}
				});
				Vote::CheckAllVotes();
			}

			Protocol::PhaseChange	ph;
			ph.newPhase = to;
			ph.roundNumber = RuntimeConfig::roundNumber;
			ph.timeFactor = timeWindow;
			Channels::phaseChange.SendTo(::Network::server,ph,(UINT32)::Network::UserLevel::PreFaction);
			RuntimeConfig::phase = to;
			lastPhaseChange = timer.Now();

			#ifdef _DEBUG
				if (to == RuntimeConfig::Phase::AnimationPhase)	//should have enough time during animation RuntimeConfig::phase
				{
					//Objects::VerifyVisibilityNetwork();	 //seems stable
					Objects::CheckMobileEntitySanity();

					Objects::VerifyGrid();
				}
			#endif



			if (to == RuntimeConfig::Phase::EndOfGame)
			{
				if (winner.expired())
				{
					UINT64 biggestScore = 0;
					Objects::PAlliance newWinner;

					Objects::Alliance::Foreach([&](const Objects::PAlliance&a)
					{
						if (a == Objects::Faction::nexus->alliance)
							return;
						const UINT64 score = a->GetWinningScore();
						if (score > biggestScore)
						{
							biggestScore = score;
							newWinner = a;
						}
					});
					if (newWinner)
						DeclareWinner(newWinner);
				}

				foreach (::Network::activePlayersAndSpectators,p)
				{
					auto alliance = (*p)->faction ? (*p)->faction->alliance : nullptr;
					Objects::Alliance::TNexusStats stats = alliance ? alliance->GetNexusStats() : Objects::Alliance::TNexusStats();
					(*p)->SendProgressUpdate("",stats);
				}
			}

			//if (to == RuntimeConfig::Phase::CommandPhase)
				//DebugCheckWorld();	//this assumes the world does not change. if it does, this will fail
			if (RuntimeConfig::IsACommandPhase(to))
				LogEvent(EventSignificance::Uncommon,"Begin round "+RuntimeConfig::roundNumber.ToString());
		}




		void	Describe( Protocol::ProductionTask&task, const Objects::MobileEntityProducer::WorkItem&element)
		{
			task.type = element.producing->GetType();
			task.id = element.id;
		}

		void	Describe( Protocol::MobileTask&task, const Objects::MobileEntity&v, const Grid::LCellIdI&origin, const Objects::MobileEntity::Task&t,index_t taskIndex)
		{
			task.foci.SetSize(t.foci.Count());
			for (index_t i = 0; i < task.foci.Count(); i++)
				task.foci[i] = t.foci[i].target;
			task.mode = (BYTE)t.mode;
			task.name = t.name;
			task.type = t.type;
			task.taskFlags = 0;
			if (t.job)
			{
				task.jobAffinity = t.job->GetOperationAffinity();
				Objects::VehicleJob::PSequence seq = To<Objects::VehicleJob::Sequence>::DynamicCast(t.job);
				if (!seq)
				{
					task.resourceCostLeft = t.job->resourceCostLeft > 0 ? t.job->resourceCostLeft : 0;
					task.continuationsToJobCompletion = t.job->continuationsToJobCompletion;
				}
				else
				{
				
					seq->Guess(v, origin, taskIndex, task.continuationsToJobCompletion,task.resourceCostLeft);
				}
				if (t.job->movesToFocusWhenDone)
					task.taskFlags |= Protocol::TaskFlags::VisualizePathToFocus;
				if (t.job->DelayedBySleep())
					task.taskFlags |= Protocol::TaskFlags::DelayedBySleep;
			

				std::shared_ptr<Objects::VehicleJob::PathJob> pathJob = To<Objects::VehicleJob::PathJob>::DynamicCast(t.job);
				if (pathJob)
				{
					task.buildPath.SetSize(pathJob->path.GetLength());
					for (index_t i = 0; i < pathJob->path.GetLength(); i++)
						task.buildPath[i] = pathJob->path[i];
					//pathJob->path.CopyToArray(task.buildPath);
				}

				std::shared_ptr<Objects::VehicleJob::Construct> constJob = To<Objects::VehicleJob::Construct>::DynamicCast(t.job);
				if (constJob)
				{
					task.visualizationName = constJob->constructWhat->GetType();
				}
				else
				{
					std::shared_ptr<Objects::VehicleJob::SelfDetonate> detonate = To<Objects::VehicleJob::SelfDetonate>::DynamicCast(t.job);

					if (detonate)
					{
						task.visualizationName = detonate->doomsday ? ConstName::DetonationSize_Doomsday : ConstName::DetonationSize_Regular;
					}
				}

			}
			else
			{
				task.continuationsToJobCompletion = -1;
			}
		}

		void	Describe( Protocol::EntityState&state, const Objects::WorldEntity&e, bool owned, const Protocol::Timestamp&timestamp, UINT32 changeFlags)
		{
			state.health = e.health;
			state.id = e.id;
			state.empDamageLevel = e.empDamageLevel;
			state.location = (changeFlags & Objects::WorldEntity::PositionChanged) ? e.location : Grid::LCellIdI::Invalid;
			state.entityStateFlags = 0;
			if (owned)
			{
				if (e.unstable)
					state.entityStateFlags |= Protocol::EntityStateFlags::IsUnstable;
				if (e.IsSleeping())
					state.entityStateFlags |= Protocol::EntityStateFlags::IsSleeping;
			}
			state.timestamp = timestamp;
		}

		void	Describe( Protocol::MobileState&state, const Objects::MobileEntity&v, bool owned, const Protocol::Timestamp&timestamp, UINT32 changeFlags)
		{
			if (v.IsAboveGrid())
			{
				DBG_ASSERT_GREATER1__(v.location.height, Grid::CellStack::TopHeight,v.GetName());
			}
	
			state.flags = 0;
			if (owned)
			{
				auto&info = state.myMobileEntity.Set0();
				Objects::PHangar h = v.storedIn.lock();
				//Objects::PVehicleStore storedIn = To<Objects::VehicleStore>::DynamicCast(v.storedIn.lock());
				if (h)
					info.storedIn.Set0() = h->ToEntity().id;
				info.tasks.SetSize(v.myTasks.Count());
				Grid::LCellIdI loc = v.location;
				for	(index_t i = 0; i < info.tasks.Count(); i++)
				{
					const auto&task = v.myTasks[i];
					Describe(info.tasks[i],v,loc, task,i);
					if (task.ContainsMovement())
						loc = task.foci.GetNewest().target;
				}
				#ifndef ONE_COMPOUND_PER_FACTION
					info.resourceVector = v.currentStorage;
				#endif
				if (v.isAutomated)
					state.flags |= Protocol::VehicleFlags::Automated;
				info.constructionActionCounter = v.constructionActionCounter;
				state.attackActionLeft = v.attackActionLeft;
				state.movementActionLeft = v.movementActionLeft;


				const Objects::MobileEntityProducer*p = dynamic_cast<const Objects::MobileEntityProducer*>(&v);
				if (p)
				{
					Protocol::ProducerState&subState = info.producerState.Set0();
					subState.productionTasks.SetSize(p->queue.Count());
					subState.productionDone = p->productionDone;
					subState.gatheringPoint = p->gatheringPoint.current;
					for (index_t i = 0; i < subState.productionTasks.Count(); i++)
					{
						Describe(subState.productionTasks[i],p->queue[i]);
					}
				}

			}
			else
			{
				if (Config::exposeActionLeftToHostileFactions)
				{
					state.attackActionLeft = v.attackActionLeft;
					state.movementActionLeft = v.movementActionLeft;
				}
				else
				{
					state.attackActionLeft = 0;
					state.movementActionLeft = 0;
				}
			}
			if (v.IsHibernating())
				state.flags |= Protocol::VehicleFlags::Hibernating;
			if (!owned && v.IsBuilding())
				state.flags |= Protocol::VehicleFlags::Building;

			Describe(state.entityState,v,owned,timestamp,changeFlags);
			if (owned)
			{
				if ((v.myTasks.IsEmpty() && v.completeOnMove) || v.AwaitsUserInteraction())
					state.entityState.entityStateFlags |= Protocol::EntityStateFlags::IsUnstable;
			}

		}

		void	Describe( Protocol::NewMobileEntity&cnst, const Objects::MobileEntity&v, bool owned, const Protocol::Timestamp&timestamp, UINT32 changeFlags)
		{
			cnst.type = v.GetType();
			cnst.owner = v.owner->id;
			Describe(cnst.state,v,owned,timestamp,changeFlags);
		}



		void	Describe( Protocol::AttackTask&task, const Objects::Tower::AttackTask&element)
		{
			//task.id = element.id;
			//task.target = element.target;
			task = element.target;
		}

		void	Describe( Protocol::StructureState&state, const Objects::Structure&s, bool owned, const Protocol::Timestamp&timestamp, UINT32 changeFlags)
		{
			const Objects::WorkingStructure*w = dynamic_cast<const Objects::WorkingStructure*>(&s);
			//state.productionDone = 0;
			state.flags = 0;
			if (!w || w->IsEnabled())
				state.flags |= Protocol::StructureFlags::Enabled;
			if (w && w->IsHibernating())
				state.flags |= Protocol::StructureFlags::Hibernating;

			if (w && owned)
			{
				Objects::ResourceVector dontCare;
				w->GetResourceVector(dontCare, state.resourceVector, dontCare);

				//const Objects::WorkingStructure*p = dynamic_cast<const Objects::WorkingStructure*>(&s);
				const Objects::MobileEntityProducer*p = dynamic_cast<const Objects::MobileEntityProducer*>(&s);
				if (p)
				{
					Protocol::ProducerState&subState = state.selective.Set1();
					subState.productionTasks.SetSize(p->queue.Count());
					subState.productionDone = p->productionDone;
					subState.gatheringPoint = p->gatheringPoint.current;
					for (index_t i = 0; i < subState.productionTasks.Count(); i++)
					{
						Describe(subState.productionTasks[i],p->queue[i]);
					}
				}
				const Objects::Tower*t = dynamic_cast<const Objects::Tower*>(&s);
				if (t)
				{
					Protocol::TowerState&subState = state.selective.Set3();
					subState.attackActionLeft = t->attackActionLeft;
					subState.attackTasks.SetSize(t->attackQueue.Count());
					for (index_t i = 0; i < subState.attackTasks.Count(); i++)
					{
						Describe(subState.attackTasks[i],t->attackQueue[i]);
					}
				
				}
				const Objects::NexusPort*n = dynamic_cast<const Objects::NexusPort*>(&s);
				if (n)
				{
					Protocol::NexusPortState&subState = state.selective.Set2();
					subState = n->resistence;
				}

				const Objects::Mine*m = dynamic_cast<const Objects::Mine*>(&s);
				if (m)
				{
					Protocol::MineState&subState = state.selective.Set0();
					subState.linkedToCore = m->IsLinkedToCore();
					subState.naturalResources = m->naturalResources;
				}
			}
		
			Describe(state.entityState,s,owned,timestamp,changeFlags);
		}

		void	Describe( Protocol::StructureConstruction&cnst, const Objects::Structure&s, bool owned, const Protocol::Timestamp&timestamp)
		{
			cnst.type = s.GetType();
			cnst.owner = s.owner->id;
			Describe(cnst.state,s,owned,timestamp,0xFFFFFFFF);

		}

	}


	void RegisterClassGain(const Objects::PWorldEntity&e, Objects::Faction::UnitClass cl, const String&debugName)
	{
		if (Config::basesRequired && !(e->owner->hasUnitClasses & cl))
		{
			const auto&f = e->owner;
			LogEvent(EventSignificance::Debug,e.get(),"Recognized as "+debugName+". Reevaluating survivial conditions");
			f->hasUnitClasses |= cl;
			String whyNot;
			bool canLive = f->CanLive(whyNot);
		
			if (canLive)	//conditions changed
			{
				if (f->deathInRounds > 0)
				{
					f->deathInRounds = 0;
					::Network::PClient client;
					if (::Network::playerMap.Query(f,client))
						client->SendProgressUpdate(whyNot, f->alliance->GetNexusStats());
				}
			}
			else
				LogEvent(EventSignificance::Debug,e.get(),"Nope: "+whyNot);
		}
	}

	void	ResetFactionStartMineralValue()
	{
		if (Config::startBases)
		{
			factionStartMineralValue = Objects::Mine::globalPrototype->GetConstructionCost()
			+ 
			Objects::Generator::globalPrototype->GetConstructionCost()
			+ 
			Objects::MineralContainer::globalPrototype->GetConstructionCost()
			+
			Objects::Vehicle::GetPrice(Objects::Component::hoverbase, Objects::Component::terraformer,Objects::OwnerParameter())
			+
			Objects::Vehicle::GetPrice(Objects::Component::hoverbase, Objects::Component::resourceScanner,Objects::OwnerParameter());
		}
		else
		{
			factionStartMineralValue = 
				3 * Objects::Vehicle::GetPrice(Objects::Component::hoverbase, Objects::Component::terraformer,Objects::OwnerParameter())
				+
				Objects::Vehicle::GetPrice(Objects::Component::hoverbase, Objects::Component::radar,Objects::OwnerParameter())
				+
				Objects::Vehicle::GetPrice(Objects::Component::hoverbase, Objects::Component::resourceScanner,Objects::OwnerParameter())
				+
				Objects::Vehicle::GetPrice(Objects::Component::hoverbase, Objects::Component::artillery,Objects::OwnerParameter());
		}
		//if (State::Config::Nexus::haveBases)
		//{
		//	Config::nexusStartMineralValue = 
		//		Objects::Connector::globalPrototype->GetConstructionCost()*5
		//		+
		//		Objects::VehiclePlant::globalPrototype->GetConstructionCost()*4
		//		+
		//		Objects::MunitionFactory::globalPrototype->GetConstructionCost()
		//		+
		//		Objects::Tower::globalPrototypes[0]->GetConstructionCost()*5
		//		+
		//		Objects::Tower::globalPrototypes[1]->GetConstructionCost()*5
		//		;
		//}
		//else
		//	Config::nexusStartMineralValue = 0;
	}




	void	ProcessChanges(bool thoroughly)
	{
		World::Network::CheckAllFactionCoverageAndSendAsNeeded();

		Objects::ForeachMobileEntityIncludingStored([/*&anythingChanged*/](const Objects::PMobileEntity&_v)
		{
			Objects::MobileEntity&v = *_v;
			if (v.changeFlags)
			{
				//anythingChanged = true;
				LogEvent(EventSignificance::Common,&v,"State change detected ("+String(v.changeFlags)+")");

				bool othersNeedToKnow = (v.changeFlags & Network::NotifyOthers) != 0 && v.storedIn.expired();

				TCP::Serial<Protocol::MobileState>	w0,w1;
				Protocol::Timestamp timestamp(RuntimeConfig::IsInAnyCommandPhase(),v.changeTimer);
				Network::Describe(w0.Ref(),v,false,timestamp,v.changeFlags);
				Network::Describe(w1.Ref(),v,true,timestamp,v.changeFlags);
				w0.Seal();
				w1.Seal();
				if (!othersNeedToKnow)
				{
					::Network::SendToOwner(Channels::setMobileState,v,w1);
				}
				else
					foreach(::Network::activePlayersAndSpectators,cl)
					{
						if (::Network::CanSeeAsOwner(*cl,v))
							Channels::setMobileState.SendTo((*cl)->peer.lock(),w1);
						else
							if (::Network::CanSee(*cl,_v,v.entityTime))
								Channels::setMobileState.SendTo((*cl)->peer.lock(),w0);
					}			
				v.ResetAllChange();
			}
		},false);

		Objects::structures.Foreach([](const Objects::PStructure&_s)
		{
			Objects::Structure&s = *_s;
			if (s.changeFlags)
			{
				//anythingChanged = true;
				LogEvent(EventSignificance::Common,&s,"State change detected");
				
				bool othersNeedToKnow = (s.changeFlags & Network::NotifyOthers) != 0;

				Protocol::Timestamp timestamp(RuntimeConfig::IsInAnyCommandPhase(),s.changeTimer);

				TCP::Serial<Protocol::StructureState>	w0,w1;
				Network::Describe(w0.Ref(),s,false,timestamp,s.changeFlags);
				Network::Describe(w1.Ref(),s,true,timestamp,s.changeFlags);
				w0.Seal();
				w1.Seal();
				if (!othersNeedToKnow)
				{
					::Network::SendToOwner(Channels::setStructureState,s,w1);
				}
				else
					foreach(::Network::activePlayersAndSpectators,cl)
					{
						if (::Network::CanSeeAsOwner(*cl,s))
							Channels::setStructureState.SendTo((*cl)->peer.lock(),w1);
						else
							if (::Network::CanSee(*cl,_s,0))
								Channels::setStructureState.SendTo((*cl)->peer.lock(),w0);
						//Channels::setStructureState.SendTo((*cl)->peer.lock(),Receiver::HasAccess(*cl,s) ? w1 : w0);
					}
				s.ResetAllChange();
			}
		});

		if (thoroughly)
		{
			Objects::ForeachFaction([](const Objects::PFaction&faction)
			{
				::Network::PClient client;
				if (::Network::playerMap.Query(faction,client))
					World::Network::CheckCoverageSendTerrain(faction,client->peer.lock());
				else
					World::Network::CheckCoverageSendTerrain(faction,TCP::PPeer());
			});
		}
		else
			foreach (::Network::activePlayersAndSpectators,cl)
			{
				//if ((*cl)->faction->global.coverageChanged)
				{
					const Objects::PFaction&faction = (*cl)->faction;
					if (faction)
						World::Network::CheckCoverageSendTerrain(faction,(*cl)->peer.lock());
						//anythingChanged = true;
				}
			}



	}



	struct TSortItem
	{
		Objects::PWorldEntity	target;
		int						priority;
		bool					wasHibernating,
								hibernating = false;

		bool					operator>(const TSortItem&other) const
		{
			return priority > other.priority;
		}
		bool					operator<(const TSortItem&other) const
		{
			return priority < other.priority;
		}

	};


	#ifndef NO_HIBERNATION
		void UpdateHibernation()
		{

			static Buffer0<TSortItem>	entityList;
			foreach (Objects::Faction::all,_f)
			{
				Objects::PFaction faction = _f->lock();
				if (faction)
				{
					UINT32 traffic = 0;
					entityList.Clear();
					for (const auto&_s : faction->structures)
					{
						Objects::PStructure s = _s.lock();
						DBG_ASSERT__(s);
						if (s)
						{
							traffic += s->GetUplinkTraffic();
							if (s->IsWorkingStructure())
							{
								Objects::PWorkingStructure ws = To<Objects::WorkingStructure>::Cast(s);
								entityList.Append().target = ws;
								entityList.Last().priority = ws->GetUplinkPriority();
								entityList.Last().wasHibernating = ws->IsHibernating();
							}
						}
					}
					for (const auto&_v : faction->allMobiles)
					{
						Objects::PMobileEntity v = _v.lock();
						DBG_ASSERT__(v);
						if (v)
						{
							traffic += v->GetUplinkTraffic();
							entityList.Append().target = v;
							entityList.Last().priority = v->GetUplinkPriority();
							entityList.Last().wasHibernating = v->IsHibernating();
						}
					}
					if (traffic > Config::uplinkBandwidth)
					{
						std::sort(entityList.begin(),entityList.end());
						for (auto&item : entityList)
						{
							traffic -= item.target->GetUplinkTraffic();
							item.hibernating = true;
							if (traffic <= Config::uplinkBandwidth)
								break;
						}
					}

					for (auto&item : entityList)
					{
						if (item.wasHibernating != item.hibernating)
						{
							if (item.target->IsGroundBoundVehicle())
							{
								To<Objects::Vehicle>::Cast(item.target)->Hibernate(item.hibernating,0);
							}
							else
								if (item.target->IsStructure())
								{
									To<Objects::WorkingStructure>::Cast(item.target)->Hibernate(item.hibernating,0);
								}
						}
					}
				}
			}

		}
	#endif


	count_t CountLivingPlayerFactions()
	{

		count_t livingPlayerFactions = 0;

		for (index_t i = 0; i < Objects::Faction::all.Count(); i++)
		{
			const Objects::PFaction&faction = Objects::Faction::all[i];	//only player factions here: nexus is not included in ::all
			if (!faction->isDead)
				livingPlayerFactions ++;
		}

		return livingPlayerFactions;
	}

	count_t CountLivingPlayerAlliances()
	{

		count_t rs = 0;

		Objects::Alliance::Foreach([&rs](const Objects::PAlliance&a)
		{
			if (a == Objects::Faction::nexus->alliance)
				return;
			if (a->CountLivingMembers() > 0)
				rs++;
		});
		return rs;
	}


	void	DeclareWinner(const Objects::PAlliance&w)
	{
		DBG_ASSERT__(w);
		DBG_ASSERT__(winner.expired());
		if (evolverActive)
		{
			postEvolutionWinner = w;
			return;
		}
		postEvolutionWinner.reset();
		LogEvent(EventClass{EventSignificance::Important,EventFlavor::ServerPrivate},"Declared winning alliance: "+w->id.ConvertToString());

		Buffer0<String> winners;
		w->ForeachMember([&winners](const Objects::PFaction&f)
		{
			winners << f->GetName();
		});
		if (winners.Count() == 1)
			LogEvent(EventClass::Preset::winningFaction,"Congratulations. "+winners.First()+" wins this game!");
		else
		{
			String str = implode(", ",winners.ToRef().SubRef(0,winners.Count()-1)) + " and "+winners.Last();
			LogEvent(EventClass::Preset::winningFaction,"Congratulations. "+str+" win this game!");
		}

		winner = w;
		Channels::winningFaction.SendTo(::Network::server,w->id,Channels::Level::Faction);
		if (RuntimeConfig::phase != RuntimeConfig::Phase::EndOfGame)
			Network::ChangePhase(RuntimeConfig::Phase::EndOfGame);
	}


	void PerformNewRoundLogic(float&atTime)
	{
		if (Config::basesRequired)
		{
			foreach (Objects::Faction::all,_f)
			{
				const Objects::PFaction&faction = *_f;
				faction->hasUnitClasses = Objects::Faction::UnitClass::None;
			}

			Objects::RefreshCompoundCoverageIfNeeded(atTime);

			Objects::ForeachCompound([](Objects::Compound&c)
			{
				if (c.workingStructures.IsEmpty())
					return;	//pure technite factions don't have units. Also, depending on server settings, some factions might not either
				typedef Objects::Faction::UnitClass	UC;

				UC	uc = UC::None;
				if (Objects::Mine::MineralStorage > 0)
					uc |= UC::MineralContainer;
					 


				#ifdef ONE_COMPOUND_PER_FACTION
					uc |= UC::MineralContainer;	//don't need
				#endif
				Objects::ResourceVector dummy,storage;
				Objects::OwnerParameter o;
				foreach (c.workingStructures,_s)
				{
					Objects::PWorkingStructure s = _s->lock();
					if (!s)
					{
						LogEvent(EventClass::Preset::internalError, "Unable to lock working structure in compound for living-check");
						continue;
					}
					o = s->owner;
					Objects::PMine mine = To<Objects::Mine>::DynamicCast(s);
					if (mine && mine->IsLinkedToCore())
					{
						if (mine->naturalResources.fuel >= 10)
							uc |= Objects::Faction::UnitClass::FuelMine;
						if (mine->naturalResources.minerals >= 5)
							uc |= Objects::Faction::UnitClass::MineralMine;
					}
					if (To<Objects::Generator>::DynamicCast(s))
						uc |= Objects::Faction::UnitClass::Generator;

					#ifndef ONE_COMPOUND_PER_FACTION
						s->GetResourceVector(dummy,dummy,storage);
						if (storage.level[Objects::Resource::Minerals] > 0)
							hasMineralContainer = true;
					#endif
					if (To<Objects::VehiclePlant>::DynamicCast(s))
						uc |= Objects::Faction::UnitClass::VehiclePlant;
				}
				Objects::PFaction f = o;//c.workingStructures.First().lock()->owner;
				if (f)
					f->hasUnitClasses |= uc;
			});

			Objects::ForeachMobileEntityIncludingStored([](const Objects::PMobileEntity&v)
			{
				if (v->GetOperationSpeed(Objects::Component::Construct))
					v->owner->hasUnitClasses |= Objects::Faction::UnitClass::Terraformer;
			},false);


			//const count_t livingPlayerFactions = CountLivingPlayerFactions();
			for (index_t i = 0; i < Objects::Faction::all.Count(); i++)
			{
				Objects::PFaction faction = Objects::Faction::all[i];	//only player factions here: nexus is not included in ::all
				if (faction)
				{
					String whyNot;
					bool canLive = faction->CanLive(whyNot);

					::Network::PClient client;


					if (!::Network::playerMap.Query(faction,client))
						client.reset();	//should be implied, but doesn't cost anything either

					if (canLive)
					{
						if (faction->deathInRounds > 0)
						{
							faction->deathInRounds = 0;
							if (client)
								client->SendProgressUpdate(whyNot,faction->alliance->GetNexusStats());
						}
					}
					else
					{
						if (!faction->deathInRounds)
						{
							faction->deathInRounds = 5;
							LogEvent(EventSignificance::Common, "Faction death imminent. " + String(faction->deathInRounds) + " (initial) round(s) remaining");
						}
						else
						{
							if (faction->deathInRounds > 1)
							{
								faction->deathInRounds--;
								LogEvent(EventSignificance::Common, "Faction death imminent. " + String(faction->deathInRounds) + " round(s) remaining");
							}
							else
							{
								String name = client ? client->ConvertToString() : "Faction "+ToString(faction->id);
											
								LogEvent(EventClass::Preset::factionDeath,name+" died: "+whyNot);

								/*
								telling the client that the faction status has been revoked forces it to flush all entities which will cause issues with subsequently sent animation events.
								the two possible solutions are to either not send any animation events to this client, or to not tell it that its faction status has been revoked.
								the former will cause a very uncomfortable client transition back to faction choice screen and no visual representation of what happened.
								therefore it's probably better to just kill off the faction. but will have to check
								if (client)
								{
									Protocol::ClientFactionResponse cr;
									cr.factionToken.value.Generate();
									cr.startFactionTypes = 0;
									cr.planetConfig = Grid::currentPlanetConfig;
									Channels::clientFactionResponse.SendTo(client->peer.lock(),cr);
								}
								*/

								Objects::Faction::Kill(faction);
								//ASSERT__(livingPlayerFactions > 0)
								//livingPlayerFactions--;
								#if 0
								if (client)
									::Network::activePlayersAndSpectators.FindAndErase(client);
								::Network::tokenMap.Unset(faction->accessToken);
								::Network::playerMap.Unset(faction);
								faction->online = false;
								if (client)
									client->faction.reset();
								i--;
								#endif /*0*/
							}
						}
						if (client)
							client->SendProgressUpdate(whyNot,faction->alliance->GetNexusStats());
					}
				}
			}

			if (Config::closedGame && RuntimeConfig::phase != RuntimeConfig::Phase::EndOfGame)
			{
				const count_t livingPlayerAlliances = CountLivingPlayerAlliances();
				if (livingPlayerAlliances == 0)
				{
					LogEvent(EventClass::Preset::factionDeath,"All alliances destroyed. Server should be reset.");
					Network::ChangePhase(RuntimeConfig::Phase::EndOfGame);
				}
				else
				if (livingPlayerAlliances == 1)
				{
					LogEvent(EventClass(EventSignificance::Important,EventFlavor::Gain),"Only one faction left. Declaring winner.");

					winner.reset();
					Objects::Alliance::Foreach([](const Objects::PAlliance&a)
					{
						if (a != Objects::Faction::nexus->alliance)
							winner = a;
					});
					DeclareWinner(winner.lock());
				}
			}


		}

		UpdateResidualEMPDamage(atTime);
	}


	void			ApplyEMPZoneDamage(const Objects::PWorldEntity&e,float&atTime)
	{
		if (e->HandleEMPDamage(100,atTime))
			LogEvent(EventSignificance::Common,e.get(),"Entering EMP-Zone");
	}

	void			UpdateResidualEMPDamage(float&atTime)
	{
		Objects::ForeachWorldEntity([&atTime](const Objects::PWorldEntity&e)
		{
			if (e->owner == Objects::Faction::nexus && e->IsAboveGrid())
				return;	//i know this is a hack, but i gotta do something to prevent nexus satellites from crashing.
			if (Grid::AffectedByEMP(e->location))
			{
				ApplyEMPZoneDamage(e,atTime);
			}
		},false);
	}


	




	void			SpawnFactionStartUnits(const Objects::PFaction&f, index_t startLocation)
	{
		const auto&n = Grid::low.grid.GetCell(startLocation).neighborhood;
		if (Config::startBases)
		{
			Grid::LCellIdI location(startLocation,0);	//first 12 are corner cells
			int h = Objects::FindApplicableGround(location,true,true);
			location.height = h;
			static const int want = 10;
			static const int stackHeight = want / Objects::NaturalResourceVector::TranslationFactor + (want % Objects::NaturalResourceVector::TranslationFactor ? 1 : 0);
			Journal::currentRound.RegisterResourceCoverageChange(true,location.stackID,Objects::Resource::Fuel,stackHeight);
			Journal::currentRound.RegisterResourceCoverageChange(true,location.stackID,Objects::Resource::Minerals,stackHeight);
			Objects::ForeachFaction([location](const Objects::PFaction&f)
			{
				Journal::currentRound.RegisterResourceCoverageKnowledgeLoss(true,f,location.stackID);
			});

			Grid::LCellIdI at0 = Grid::LCellIdI(n.neighborID[0],h);
			Grid::LCellIdI at1 = Grid::LCellIdI(n.neighborID[1],h);
			Grid::LCellIdI at2 = Grid::LCellIdI(n.neighborID[2],h);
			Grid::LCellIdI at3 = Grid::LCellIdI(n.neighborID[3],h);
			Grid::LCellIdI at4 = Grid::LCellIdI(n.neighborID[4],h);

			float t = 0;
			Objects::PMine mine = To<Objects::Mine>::Cast(
				Objects::Mine::globalPrototype->CreateNewInstance(f,location,0));
			mine->ClearOccupiedTerrain(0);
			Objects::InsertConstructed(mine,t);
			mine->SetEnabledOverride(true);
				
			Objects::PGenerator generator = To<Objects::Generator>::Cast(
				Objects::Generator::globalPrototype->CreateNewInstance(f,at1,0));
			generator->ClearOccupiedTerrain(0);
			Objects::InsertConstructed(generator,t);
			generator->SetEnabledOverride(true);
				
			std::shared_ptr<Objects::MineralContainer> container = To<Objects::MineralContainer>::Cast(
				Objects::MineralContainer::globalPrototype->CreateNewInstance(f,at2,0));
			container->storage = container->maxStorage;
			container->ClearOccupiedTerrain(0);
			Objects::InsertConstructed(container,t);
			Objects::RefreshCompoundCoverage(t);


			Objects::InsertNewVehicle(f,at0,Objects::Component::hoverbase, Objects::Component::terraformer,true,t);
			Objects::InsertNewVehicle(f,at3,Objects::Component::hoverbase, Objects::Component::terraformer,true,t);
			Objects::InsertNewVehicle(f,at4,Objects::Component::hoverbase, Objects::Component::resourceScanner,true,t);

		}
		else
		{
			Grid::LCellIdI at0 = Grid::LCellIdI(n.neighborID[0],0);
			Grid::LCellIdI at1 = Grid::LCellIdI(n.neighborID[1],0);
			Grid::LCellIdI at2 = Grid::LCellIdI(n.neighborID[2],0);
			Grid::LCellIdI at3 = Grid::LCellIdI(n.neighborID[3],0);
			Grid::LCellIdI at4 = Grid::LCellIdI(n.neighborID[4],0);

			float t = 0;
			Objects::InsertNewVehicle(f,Grid::LCellIdI((UINT32)startLocation,0),Objects::Component::hoverbase, Objects::Component::terraformer,true,t);
			Objects::InsertNewVehicle(f,at1,Objects::Component::hoverbase, Objects::Component::radar,true,t);
			Objects::InsertNewVehicle(f,at3,Objects::Component::hoverbase, Objects::Component::resourceScanner,true,t);
			Objects::InsertNewVehicle(f,at2,Objects::Component::hoverbase, Objects::Component::terraformer,true,t);
			Objects::InsertNewVehicle(f,at4,Objects::Component::hoverbase, Objects::Component::terraformer,true,t);
			Objects::InsertNewVehicle(f,at0,Objects::Component::hoverbase, Objects::Component::artillery,true,t);
		}
	}


}

