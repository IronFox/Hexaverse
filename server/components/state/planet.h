#pragma once

#include <uniqueID.h>
#include <math/vclasses.h>
#include <objects/types.h>

namespace Planet
{

	/**
	Planet location identifier.
	*/
	typedef int2 Location;
	typedef std::function<void(TCP::Peer&peer, const Location&loc)> FPeerVisitor;
		
	void		RegisterSensorAccess(const Objects::PFaction&, const Location&);
	void		UnregisterSensorAccess(const Objects::PFaction&, const Location&);
	bool		HasSensorAccess(const Objects::PFaction&, const Location&);
	Location	GetMyLocation();
	bool		TransferMobileEntity(const Objects::PMobileEntity&, const Location&dest);

	void		VisitPeers(const FPeerVisitor&f);
		
	void		SignalControlPhaseEnd();
	void		SignalControlPhaseBegin();

	

	void	Create(const String&masterServer,UINT16 port);
	void	Destroy();
	
}



