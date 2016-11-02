#ifndef wirelessRangesH
#define wirelessRangesH

#include "../config.h"
#include <grid.h>
namespace Objects
{
	#ifdef WIRELESS_VEHICLES
		static constexpr const int WirelessTransferRangeFactor =
			#ifdef LOWER_RESOLUTION_PLANET
				2;
			#else
				1;
			#endif



		static constexpr const int			BaseWirelessTransferRange = 2 * WirelessTransferRangeFactor;
		static constexpr const int			PylonWirelessTransferRange = 10 * WirelessTransferRangeFactor;
		static constexpr const int			MobileWirelessTransferRange = 6 * WirelessTransferRangeFactor;
		static constexpr const int			OrbitalWirelessRangeMultiplier = 3 * WirelessTransferRangeFactor;
	#endif
}

#endif
