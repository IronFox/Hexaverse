#pragma once

//#include <serverNetwork.h>
#include <net/tcp.h>
#include <objects/resource.h>
#include <objects/types.h>
#include <protocol.h>



/**
Helper unit to handle interplanetary transit, and resource transfer.

Concept:
Transit gates link two cells on different planets along possible transit routes.
A first gate is constructed as a regular building on a specific planet location directional to the remote planet.
Only one gate of one faction may link along one specific interplanetary link.
Paying a given resource demand, an anchor rocket is constructed and dispatched.
The rocket lands on a random location on the remote planet (farthest from existing units), and establishes a counter-gate.
While under construction, the gate is particularly vulnerable.
As long as both gates exist and have finished construction, units can enter either and are dispatched to the remote end.
Regular resource transits may be scheduled from either end.
Units and resources in transit are lost if the remote gate is destroyed.

Each planet has a regular 3d vector to specify its location.
No rotation is gone at present to simplify interplanetary orientation.
Alternate possible corridors between two planets may be introduces in the future.


Authentication:
Servers authenticate by sending a login identifier and entering a challenge-response-sequence.
Specifically:
Identifier: Pair of [hash algorithm:hash length] byte sequences, compacted into one array.
The first half serves as the public identifier sent to remote hosts for login.
The second half is the private password which is never transferred unhashed.
Identifiers are not required to be unique.

Protocol:
end0: Send challenge
end1: Send public identifier+hash(challenge,password)
end0: Check credentials, and respond
*/
namespace PeerNetwork
{
	typedef SHA256	Hash;
	typedef Hash::HashContainer	HashValue;
	/**
	Login data requires to log into a remote host
	*/
	struct ID
	{
		HashValue	publicID,	//!< Data sent in plain text to a remote host
					secret;		//!< Secret only ever sent in a hashed value

		bool		operator==(const ID&other) const
		{
			return publicID == other.publicID && secret == other.secret;
		}
		bool		operator!=(const ID&other) const
		{
			return publicID != other.publicID || secret != other.secret;
		}
	};

	template <typename T>
		struct TTransit
		{
			UINT32	roundsSpentInTransit=0;
			T		item;
			TUniqueID	id;
			bool	confirmed=false;
		};

	static const constexpr float MaxTransitSpeed = 1;
	/**
	Interplanetary link descriptor
	*/
	struct TLink
	{
		float3		relativeRemoteEnd;
		UINT32		localLowCellID=0,
					remoteLowCellID=0,
					transitRounds=1;
		String		remoteServerAddress;
		bool		haveRemoteGate=false;	// valid only if localGate is not empty or dead
		UINT32		remoteGateConstructionRoundsLeft=0;	//valid only  if localGate is not empty or dead
		Objects::WStructure	localGate;	//also holds owner faction
		Buffer0<TTransit<Objects::WMobileEntity> >				unitsInTransit;
		Buffer0<TTransit<Protocol::PositiveResourceVector> >	resourcesInTransit;
	};



	/**
	Creates the service and registeres the given secret as remotely accepted identifier.
	*/
	void			Create(USHORT servicePort, const ID&myLogin, const float3&myLocation);
	void			Create(USHORT servicePort, const ID&myLogin, const float3&myLocation, time_t time0, time_t secondsPerRound);
	void			Destroy();
	bool			IsOnline();

	/**
	Identifier to be accepted if a remote server logs in using it
	*/
	void			AcceptRemoteIdentifier(const ID&id);
	bool			HaveRemoteIdentifier(const ID&id);
	bool			HaveRemoteIdentifier(const HashValue&publicID);
	void			RemoveRemoteIdentifier(const ID&id);;

	/**
	Begins connecting to the specified remote address.
	*/
	void			EstablishPlanetLink(const String&remoteAddress);
	bool			IsEstablishingPlanetLink(const String&remoteAddress);
	count_t			CountLinkedPlanets();
	const TLink&	GetLink(index_t);
	index_t			FindLinkFrom(UINT32 lowCellID);
	
	/**
	Queries the round number according to the current time
	*/
	RoundNumber		GetTargetRound();
	
	/**
	Initiates transit of a mobile entity. The entity may be a hangar entity thus carrying other entities according to game rules.
	*/
	void			BeginTransit(index_t targetLink, const Objects::PMobileEntity&);
	void			BeginTransit(index_t targetLink, const Protocol::PositiveResourceVector&);
	void			BeginRemoteGateConstruction(index_t targetLink, const Objects::PStructure&localGate);
	
	void			Resolve();
}
