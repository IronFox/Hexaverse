#pragma once


/**
This module handles Nexus Threat detection and handling.
The system uses the AI logic to determine threat impact, and radial measurement as a notion of distance.
Player terraformers are considered particularly hostile to a Nexus Port as they can take over ownership.
The AI client is not informed of this detection. Instead, vehicles plants are enabled as considered reasonable.
*/
namespace Nexus
{
	void	Setup();
	void	ProcessCoverage();
}
