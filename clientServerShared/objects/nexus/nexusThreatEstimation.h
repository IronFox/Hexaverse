#pragma once

#include "../types.h"
#include <math/vector.h>
#include <grid/setup.h>

namespace Objects
{

	/**
	This module handles Nexus Threat detection.
	The system uses the AI logic to determine threat impact, and radial measurement as a notion of distance.
	Player terraformers are considered particularly hostile to a Nexus Port as they can take over ownership.
	All functions except Update() and UpdatePole() are thread-safe
	*/
	namespace NexusThreat
	{
		struct TEstimation
		{
			float				impact = 0,
								opacity = 0,
								totalImpact = 0;
			bool				isConstructor = false,
								isRedZone = false,
								isHostile = false,
								isPlane = false,
								isStructure = false;
		};



		/**
		Estimates the impact of a unit type at a given location, even if no entity is known there
		@param poleIndex Index of the pole to estimate impact relative to (0-11)
		@param location Entity location
		@param type Entity type
		@param owner Entity owner. May be null which is always hostile but with null research state
		@return Threat estimation
		*/
		TEstimation				Estimate(index_t poleIndex, const Grid::LCellIdI&location, const Objects::EntityType&type, Objects::PFaction&owner);
		TEstimation				Estimate(index_t poleIndex, const PWorldEntity&e);
		TEstimation				Estimate(index_t poleIndex, const PWorldEntity&e, const Grid::LCellIdI&location);



		bool					IsBase(index_t poleIndex);
		float					SumGroundHostiles(index_t poleIndex);
		float					SumAirHostiles(index_t poleIndex);
		float					SumGroundFriendlies(index_t poleIndex, bool includeStationaries);
		float					SumAirFriendlies(index_t poleIndex);
		float					SumHostileConstructorPresence(index_t poleIndex);
		float					GetClosestHostileConstructorPresence(index_t poleIndex);

		ArrayRef<PVehiclePlant>	GetBaseVehiclePlants(index_t poleIndex);
		ArrayRef<PPlaneFactory>	GetBasePlaneFactories(index_t poleIndex);
		Objects::PNexusPort		GetPort(index_t poleIndex);


		#ifdef CLIENT
			/**
			Updates the server-provided hostile presence values
			*/
			void				UpdatePole(index_t poleIdx, float groundSum, float airSum, bool hasBase, float constructorPresence, float highestConstructorPresence);

			/**
			Central creation method. Must be called once during client application start
			*/
			void				Create();
		#else
			void				Update(const std::function<bool(index_t)>&poleHasBase);
		#endif




	}

}
