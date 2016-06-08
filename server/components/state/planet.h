#pragma once

#include <uniqueID.h>
#include <math/vclasses.h>
#include <objects/types.h>

namespace Planet
{

	/**
	Planet location identifier.
	*/
	typedef float3 Location;
	typedef std::function<void(TCP::Peer&peer)> FPeerVisitor;
	typedef std::function<void(const Location&loc, TCP::Peer*optionalPeer)> FLocationVisitor;
		
	/**
	Queries or issues instructions enqueued for neighbors.
	Operations in this category are queued for transmission during the next command phase.
	*/
	namespace NeighborOperations
	{
		/**
		Registers remote sensor access for the specified faction and planet location.
		The sensor state is maintained until UnregisterSensorAccess is called.
		New requests are transferred to the targeted peer during the next SignalCommandPhaseBegin().
		Repeated calls have no effect.
		@param f Faction to acquire access for. The remote peer will create the requested faction if necessary.
		@param loc Peer planet location. The location must be known to the local planet
		@return true, if the targeted peer exists and the operation could be enqueued, false otherwise.
		The actual transmission result is unknown at this state.
		*/
		bool	RegisterSensorAccess(const Objects::PFaction&f, const Location&loc);
		/**
		Unregisters remote sensor access for the specified faction and planet location.
		New requests are transferred to the targeted peer during the next SignalCommandPhaseBegin().
		Repeated calls have no effect.
		*/
		void	UnregisterSensorAccess(const Objects::PFaction&, const Location&);
		/**
		Queries wether a given faction has (theoretically) access to a given planet location.
		*/
		bool	HasSensorAccess(const Objects::PFaction&, const Location&);

		bool	TransferMobileEntity(const Objects::PMobileEntity&, const Location&dest);
		/**
		Visits all connected peers
		*/
		void	VisitPeers(const FPeerVisitor&f);
		/**
		Visits all known neighbor locations.
		If a peer is known, then it will be passed to f, otherwise null
		*/
		void	VisitLocations(const FLocationVisitor&f);
	}
	/**
	Queries the location of the local planet
	*/
	Location	GetMyLocation();

	/**
	Signals that the command phase has ended.
	Applies received remote requests to the local state.
	*/
	void		SignalCommandPhaseEnd();
	/**
	Transmits sensor access requests and mobile entity transfers to their target peers.
	Transfers will occur as long as the command phase lasts.
	*/		
	void		SignalCommandPhaseBegin();

	

	void		Create(const String&masterServer,UINT16 port);
	void		Destroy();

	/**
	Resolve per-frame synchronous events
	*/
	void		Resolve();
	
}



