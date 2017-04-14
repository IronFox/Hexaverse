#pragma once

namespace World
{
	class Data;
}


namespace Grid
{
	class CellStack;
	class ResourceDiscovery;
	struct TSharedPlanetConfig;
	#ifndef CLIENT
		class ExtendedPlanetConfig;
		typedef ExtendedPlanetConfig	TPlanetConfig;
	#else
		typedef TSharedPlanetConfig	TPlanetConfig;
	#endif
}
