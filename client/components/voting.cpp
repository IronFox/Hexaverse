#include <global_root.h>
#include "voting.h"
#include "clientEvents.h"
#include "network.h"


/*static*/ PVote	Vote::Create(const TUniqueID&id, float remainingSeconds, const String&phrase)
{
	LogEvent(EventSignificance::Common,"Creating vote "+ToString(id)+", timeout after "+String(remainingSeconds)+" seconds: '"+phrase+"'");
	PVote rs;
	if (rs = FindByID(id))
	{
		LogEvent(EventSignificance::NonFatalError,"Vote already exists");
		DBG_FATAL__("Vote already exists");
		return rs;
	}

	rs.reset(new Vote(id,remainingSeconds,phrase));
	all.Set(id,rs);
	voteQueue.Append(rs);
	GlobalEvents::voteCreated(rs);
	return rs;
}

/*static*/ PVote	Vote::FindByID(const TUniqueID&id)
{
	PVote rs;
	all.Query(id,rs);
	return rs;
}

void			Vote::CastVote(bool inFavor)
{
	if (closed)
	{
		LogEvent(EventSignificance::NonFatalError,"Trying to vote on "+ToString(id)+", but vote is already closed");
		return;
	}
	if (GetRemainingSeconds() <= 0)
	{
		LogEvent(EventSignificance::NonFatalError,"Trying to vote on "+ToString(id)+", but vote should have already closed");
		return;
	}
	Protocol::ClientVote vote;
	vote.voteID = id;
	vote.inFavor = inFavor;
	Network::Channels::ClientVote.SendTo(Network::client,vote);

	haveVoted = true;
	haveVotedInFavor = inFavor;

	GlobalEvents::voteCast(shared_from_this());
}

void			Vote::Close(const TOutcome&outcome)
{
	if (closed)
		FATAL__("Double Handling result of vote "+ToString(id)+", timeout after "+String(timer.GetSecondsSince(created))+" seconds: '"+phrase+"'");
	PVote v;
	if (all.QueryAndUnset(id,v))
	{
		ASSERT__(v.get() == this);
		LogEvent(EventSignificance::Common,"Handling result of vote "+ToString(id)+", timeout after "+String(timer.GetSecondsSince(created))+" seconds: '"+phrase+"'");
		switch (outcome.result)
		{
			case Protocol::VoteResultType::Passed:
				LogEvent(EventClass(EventSignificance::Important,EventFlavor::Gain),"Vote '"+phrase+"' has passed with "+String(outcome.votedInFavor)+" in favor vs "+String(outcome.votedAgainst)+" against");
			break;
			case Protocol::VoteResultType::Rejected:
				LogEvent(EventClass(EventSignificance::Important,EventFlavor::Loss),"Vote '"+phrase+"' was rejected with "+String(outcome.votedInFavor)+" in favor vs "+String(outcome.votedAgainst)+" against");
			break;
			case Protocol::VoteResultType::TimedOut:
				LogEvent(EventClass(EventSignificance::Important,EventFlavor::Loss),"Vote '"+phrase+"' timed out with "+String(outcome.votedAgainst + outcome.votedInFavor)+" of "+String(outcome.quorum)+" vote(s)");
			break;
		}
		voteQueue.FindAndErase(v);
		this->outcome = outcome;
		GlobalEvents::voteClosed(v);
	}
	else
		FATAL__("Can't find vote "+ToString(id)+", anymore");
}

/*static*/ void			Vote::StaticCreate()
{
	GlobalEvents::objectFlush << []()
	{
		all.Clear();
		voteQueue.Clear();
	};

}


/*static*/ HashTable<TUniqueID,PVote>	Vote::all;
/*static*/ Buffer0<PVote>	Vote::voteQueue;
