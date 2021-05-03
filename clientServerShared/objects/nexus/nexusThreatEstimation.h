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
		CONSTRUCT_ENUMERATION5(Zone
			,Irrelevant
			,Noticed
			,Yellow
			,Red
			,Black
		);

		struct TSupport;
		struct TLevel;

		struct TAspect
		{
			float				impact = 0,		//!< Significance of the unit, regardless of distance
								scaled = 0,		//!< Product of impact and opacity, possibly also including an external factor
								airActivity = 0,
								groundActivity = 0;


			static TAspect		Max(const TAspect&a, const TAspect&b)
			{
				TAspect rs;
				rs.impact = std::max(a.impact,b.impact);
				rs.scaled = std::max(a.scaled,b.scaled);
				rs.airActivity = std::max(a.airActivity,b.airActivity);
				rs.groundActivity = std::max(a.groundActivity,b.groundActivity);
				return rs;
			}


			void				Scale(float factor)
			{
				scaled *= factor;
			}
		};

		/**
		Threat or support estimation of an entity
		*/
		struct TEstimation
		{
			TAspect				attack,
								sensor;
			float				significance = 0;		//!< Opacity value [0-4] (usually [0-1]) to the respective pole. Estimations are 0(none), 1/3(noticed: <9 steps), 1(yellow: <6 steps), 2(red: <4 steps), 4(black: at port)
			bool				isConstructor = false,	//!< True if this entity can build things
								isHostile = false,		//!< True if this entity is hostile
								isAirborne = false,		//!< True if this entity is currently flying
								isPlane = false,		//!< True if this entity is a plane
								isStationary = false;	//!< True if this entity cannot move
			UINT32				distanceToPole = std::numeric_limits<UINT32>::max();	//!< Detected cell distance to the estimation pole

			Zone				GetZone() const;
			float				TotalHostileImpact() const {return std::max(attack.scaled,sensor.scaled);}
			float				TotalFriendlyImpact() const {return std::min(attack.scaled,sensor.scaled);}

			TSupport			ToSupport() const;
			TLevel				ToThreat(bool includeThreatEstimationFactor) const;

			/**
			Multiplies the scalable aspects of this estimation by the specified factor
			*/
			void				Scale(float factor)
			{
				attack.Scale(factor);
				sensor.Scale(factor);
			}

		};



		/**
		Activity domain.
		Entities are generally classified as air-active
		or ground-active
		*/
		struct TDomain
		{
			float				mobile = 0,
								stationary = 0;

			float				Total() const {return mobile + stationary;}
			float				Level(bool sourceStationary) const {return sourceStationary ? stationary : mobile;}
			void				Inc(const TEstimation&est, const TAspect&aspect)
			{
				if (est.isStationary)
					stationary += aspect.scaled;
				else
					mobile += aspect.scaled;
			}
			void				operator*=(float factor)
			{
				mobile *= factor;
				stationary *= factor;
			}

			friend String		ToString(const TDomain&d)
			{
				return "m="+String(d.mobile)+",s="+String(d.stationary);
			}
		};


		/**
		Generic presence vector.
		Provides accumulated estimations results (totalImpact)
		of all detected entities (either hostile or friendly).
		May be affected by estimation factor.
		*/
		struct TVector
		{
			TDomain				air,
								ground;

			float				AirTotal() const {return air.Total();}
			float				GroundTotal() const {return ground.Total();}
			float				Total() const {return AirTotal() + GroundTotal();}
			float				MobileTotal() const {return air.mobile + ground.mobile;}
			float				MobileTotal(bool targetGround) const {return targetGround ? ground.mobile : air.mobile;}
			float				StationaryTotal() const {return air.stationary + ground.stationary;}
			float				StationaryTotal(bool targetGround) const {return targetGround ? ground.stationary : air.stationary;}

			const TDomain&		Domain(bool targetGround) const {return targetGround ? ground : air;}
			TDomain&			Domain(bool targetGround) {return targetGround ? ground : air;}

			void				Inc(const TEstimation&est, const TAspect&aspect);
			void				operator*=(float factor)
			{
				air *= factor;
				ground *= factor;
			}
			friend String		ToString(const TVector&v)
			{
				return "A{"+ToString(v.air)+"},G{"+ToString(v.ground)+"}";
			}
		};

		/**
		Support vector (all friendly entities)
		*/
		struct TSupport
		{
			TVector				attack, 
								sensor;

			float				AttackTotal() const {return attack.Total();}
			float				SensorTotal() const {return sensor.Total();}
			float				Total() const {return std::min(attack.Total(),sensor.Total());}
			float				MobileTotal() const {return std::min(attack.MobileTotal(),sensor.MobileTotal());}
			float				MobileTotal(bool targetGround) const {return std::min(attack.MobileTotal(targetGround),sensor.MobileTotal(targetGround));}
			float				AirMobileTotal() const {return std::min(attack.air.mobile,sensor.air.mobile);}
			float				GroundMobileTotal() const {return std::min(attack.ground.mobile,sensor.ground.mobile);}

			const TVector&		Aspect(bool attackSupport) const {return attackSupport ? attack : sensor;}

			void				operator+=(const TEstimation&);
			friend String		ToString(const TSupport&v)
			{
				return "At{"+ToString(v.attack)+"},Se{"+ToString(v.sensor)+"}";
			}
		};

		/**
		Threat level vector (all hostile entities)
		*/
		struct TLevel: public TVector
		{
			/**
			Sum of all hostile constructor significances.
			*/
			float				constructorSignificanceSum=0;
			/**
			Highest hostile constructor opacity.
			*/
			float				highestConstructorSignificance=0;

			void				operator+=(const TEstimation&est);
			TLevel				operator*(float factor) const
			{
				TLevel rs = *this;
				rs *= factor;
				return rs;
			}
			friend String		ToString(const TLevel&v)
			{
				return ToString((const TVector&)v)+",constSigSum="+String(v.constructorSignificanceSum)+",highest="+String(v.highestConstructorSignificance);
			}

		};




		/**
		Estimates the impact of a unit type at a given location, even if no entity is known there
		@param poleIndex Index of the pole to estimate impact relative to (0-11)
		@param location Entity location
		@param type Entity type
		@param owner Entity owner. May be null which is always hostile but with null research state
		@return Threat estimation
		*/
		TEstimation				Estimate(int poleIndex, const Grid::LCellIdI&location, const Objects::EntityType&type, Objects::PFaction&owner);
		TEstimation				Estimate(int poleIndex, const PWorldEntity&e);
		TEstimation				Estimate(int poleIndex, const PWorldEntity&e, const Grid::LCellIdI&location);



		bool					IsBase(int poleIndex);
		bool					HasPort(int poleIndex);
		bool					IsInEmpZone(int poleIndex);
		const TLevel&			Get(int poleIndex);
		const TSupport&			GetSupport(int support);
		bool					DeployHeavyVehicleBases(int poleIndex);
		bool					IsPlaneFactoryHere(UINT32 stackID);

		ArrayRef<PVehiclePlant>	GetBaseVehiclePlants(int poleIndex);
		ArrayRef<PPlaneFactory>	GetBasePlaneFactories(int poleIndex);
		ArrayRef<PMobileEntity>	GetOwnMobiles(int poleIndex);

		Objects::PNexusPort		GetPort(int poleIndex);


		void					SetEstimationFactor(float estimationFactor);
		float					GetEstimationFactor();

		#ifdef CLIENT
			/**
			Updates the server-provided hostile presence values
			*/
			void				UpdatePole(int poleIdx, const TLevel&, bool deployHeavyVehicleBases, bool hasBase);

		#endif
		/**
		Updates support and threat (server only) computation.
		Should be called once before client is handed controll.
		@param poleHasBase Function to determine whether a pole has a base. May be null (on client)
		@param deployHeavyVehicleBase Function to determine whether heavy vehicle bases should be used on a pole. May be null (on client)
		*/
		void					Update(const std::function<bool(UINT32)>&poleHasBase, const std::function<bool(const Objects::PNexusPort&)>&deployHeavyVehicleBases);
	}

}
