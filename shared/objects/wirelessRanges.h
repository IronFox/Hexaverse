#ifndef wirelessRangesH
#define wirelessRangesH

#include "../config.h"
namespace Objects
{
	#ifdef WIRELESS_VEHICLES
		static const int			BaseWirelessTransferRange = 2;
		static const int			PylonWirelessTransferRange = 10;
		static const int			MobileWirelessTransferRange = 6;
		static const int			OrbitalWirelessRangeMultiplier = 3;
	#endif
}

#endif
