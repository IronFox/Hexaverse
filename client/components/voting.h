#pragma once

#include <typeDecl.h>
#include <protocol.h>

DECLARE_POINTER_CLASS(Vote);

class Vote : public std::enable_shared_from_this<Vote>
{
public:
	const Timer::Time	created = timer.Now();
	const float			ttlSeconds;
	const String		phrase;
	const TUniqueID		id;

	struct TOutcome
	{
		Protocol::VoteResultType::T result = Protocol::VoteResultType::TimedOut;
		count_t			votedInFavor=0,
						votedAgainst=0,
						quorum=1,
						eligibleToVote=1;
	};

	/**
	Queries the recorded outcome.
	Only valid on closed votes.
	Effectively available only during the global event GlobalEvents::voteClosed
	*/
	const TOutcome&		GetOutcome() const {return outcome;}
	/**
	Checks if the local vote has been closed. Typically this is known through circumstances.
	*/
	bool				IsClosed() const	{return closed;}
	/**
	Checks if the local user has voted
	*/
	bool				HaveVoted() const {return haveVoted;}
	/**
	Checks if the local user has voted in favor. False of the user has not voted at all
	*/
	bool				HaveVotedInFavor() const {return haveVotedInFavor;}
	/**
	Sets the user's vote status.
	If the server did not automatically close the vote as a result,
	the vote may be changed subsequently by calling this method again.
	If the local vote was the last necessary to reach completion,
	then even an immediate re-vote may be rejected, however.
	@param inFavor Local player vote
	@param hideVote Visually remove local vote immediately
	*/
	void				CastVote(bool inFavor, bool hideVote);

	/**
	Creates a new vote given the specified configuration.
	If a vote by that ID is already registered, the existing instance is returned instead (as is).
	This is undesirable behavior and causes program termination in debug mode.
	*/
	static PVote		Create(const TUniqueID&id, float remainingSeconds, const String&phrase);
	/**
	Attempts to find an existing vote by ID
	*/
	static PVote		FindByID(const TUniqueID&id);

	/**
	Closes the local vote.
	The vote is removed from the static registry and can no longer be found by FindByID()
	Double-closing causes program termination
	*/
	void				Close(const TOutcome&result);
	/**
	Calculates the seconds that the local vote remains active. May be negative if the server does not close the local vote in time
	*/
	float				GetRemainingSeconds() const {return ttlSeconds - timer.GetSecondsSince(created);}
	
	static void			StaticCreate();
	/**
	Counts the number of registered votes. That is the number of concurrent visible votes that the local user may or already has voted on
	*/
	static count_t		CountActiveVotes();
	/**
	Counts the number of registered votes that the local user has not voted on yet
	*/
	static count_t		CountNewVotes();

	/**
	Iterates through all non-hidden registered votes in order of creation
	*/
	static void			VisitActiveVotes(const std::function<void(const PVote&)>visitor);

private:
	/**/				Vote(const TUniqueID&id, float remainingSeconds, const String&phrase):ttlSeconds(remainingSeconds), phrase(phrase),id(id){}

	TOutcome			outcome;

	bool				haveVoted = false,
						haveVotedInFavor = false,
						closed = false,
						hidden = false;

	static HashTable<TUniqueID,PVote>	all;
	static Buffer0<PVote>	voteQueue;	//in order of creation
};
