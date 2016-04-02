#include <global_root.h>
#include "prototypes.h"
#include "satellite.h"
#include "vehicle.h"
#include "structure.h"
#include "subOrbital.h"


namespace Objects
{
	Buffer0<PStructure>			structurePrototypes;
	#ifndef NO_SATELLITES
		Buffer0<PSatellite>			satellitePrototypes;
	#endif
	namespace Component
	{
		Buffer0<VehicleBase*>		baseComponents;
		Buffer0<Turret*>			vehicleTurretComponents,
									allTurretComponents;
	}

	namespace SubOrbital
	{
		Buffer0<PPlane>			prototypes;


		PPlane		GetPrototypeByName(ConstName name)
		{
			if (name != ConstName::Empty)
			{
				foreach (prototypes,p)
					if ((*p)->GetCTypeName() == name)
						return *p;
			}
			return PPlane();
		}

		PPlane		GetPrototypeByName(const String& name)
		{
			if (name.IsNotEmpty())
			{
				foreach (prototypes,p)
					if ((*p)->GetTypeName() == name)
						return *p;
			}
			return PPlane();
		}	
	}

	

	#ifndef NO_SATELLITES

		PSatellite		GetSatellitePrototypeByName(const String&name)
		{
			if (name.IsNotEmpty())
			{
				foreach (satellitePrototypes,p)
					if ((*p)->GetTypeName() == name)
						return *p;
			}
			return PSatellite();
		}
		PSatellite		GetSatellitePrototypeByName(ConstName name)
		{
			if (name != ConstName::Empty)
			{
				foreach (satellitePrototypes,p)
					if ((*p)->GetCTypeName() == name)
						return *p;
			}
			return PSatellite();
		}

	#endif

	PStructure		GetPrototypeByName(ConstName structureTypeName)
	{
		if (structureTypeName != ConstName::Empty)
		{
			foreach (structurePrototypes,p)
				if ((*p)->GetCTypeName() == structureTypeName)
					return *p;
		}
		return PStructure();
	}
	
	PStructure		GetPrototypeByName(const String&structureTypeName)
	{
		if (structureTypeName.IsNotEmpty())
		{
			foreach (structurePrototypes,p)
				if ((*p)->GetTypeName() == structureTypeName)
					return *p;
		}
		return PStructure();
	}


	PWorldEntity	GetNonVehiclePrototypeByName(ConstName typeName)
	{
		if (typeName == ConstName::Empty)
			return PWorldEntity();
		PWorldEntity rs;
		rs = GetPrototypeByName(typeName);
		if (rs)
			return rs;
		#ifndef NO_SATELLITES
			rs = GetSatellitePrototypeByName(typeName);
			if (rs)
				return rs;
		#endif
		rs = SubOrbital::GetPrototypeByName(typeName);
		if (rs)
			return rs;
		return PWorldEntity();
	}

	PWorldEntity	GetNonVehiclePrototypeByName(const String&typeName)
	{
		if (typeName.IsEmpty())
			return PWorldEntity();
		PWorldEntity rs;
		rs = GetPrototypeByName(typeName);
		if (rs)
			return rs;
		#ifndef NO_SATELLITES
			rs = GetSatellitePrototypeByName(typeName);
			if (rs)
				return rs;
		#endif
		rs = SubOrbital::GetPrototypeByName(typeName);
		if (rs)
			return rs;
		return PWorldEntity();
	}


	namespace Component
	{

		VehicleBase*	GetVehicleBaseByName(ConstName name)
		{
			//Buffer0<VehicleBase*>		baseComponents;
			//Buffer0<Turret*>			turretComponents;
			//if (structureTypeName.IsNotEmpty())
			{
				foreach(baseComponents, p)
					if ((*p)->constName == name)
						return *p;
			}
			return nullptr;
		}

		VehicleBase*	GetVehicleBaseByName(const String&name)
		{
			//Buffer0<VehicleBase*>		baseComponents;
			//Buffer0<Turret*>			turretComponents;
			//if (structureTypeName.IsNotEmpty())
			{
				foreach(baseComponents, p)
					if ((*p)->typeName == name)
						return *p;
			}
			return nullptr;
		}


		Turret*			GetTurretByName(const String&name)
		{
			//Buffer0<VehicleBase*>		baseComponents;
			//Buffer0<Turret*>			turretComponents;
			//if (structureTypeName.IsNotEmpty())
			{
				foreach(allTurretComponents, p)
					if ((*p)->typeName == name)
						return *p;
			}
			return nullptr;
		}

		Turret*			GetTurretByName(ConstName name)
		{
			//Buffer0<VehicleBase*>		baseComponents;
			//Buffer0<Turret*>			turretComponents;
			//if (structureTypeName.IsNotEmpty())
			{
				foreach(allTurretComponents, p)
					if ((*p)->constName == name)
						return *p;
			}
			return nullptr;
		}


		void ConfigureAll()
		{
			Artillery::Configure(artillery);
			Launcher::Configure(launcher);
			EMP::Configure(emp);
			#ifndef NO_SATELLITES
				SkyGuardian::Configure(protonCannon);
				SkyHubTurret::Configure(skyHubTurret);
				Ragnarok::Configure(ragnarok);
				StarPort::Configure(starPort);
			#endif

			Bomb::Configure(bomb);
			Armored::Configure(armored);
			Radar::Configure(radar);
			MunitionTransporter::Configure(munitionTransporter);
			Mechanic::Configure(mechanic);
			Terraformer::Configure(terraformer);
			ResourceScanner::Configure(resourceScanner);

			Hoverbase::Configure(hoverbase);
			Heavybase::Configure(heavybase);
			Catbase::Configure(catbase);
			SubOrbital::Base::Configure(SubOrbital::planeBase);


			SubOrbital::Raptor::Configure(SubOrbital::raptor);
			SubOrbital::CargoPlane::Configure(SubOrbital::cargoPlane);
			SubOrbital::SensorDrone::Configure(SubOrbital::sensorDrone);


			allTurretComponents << &radar 
								#ifndef NO_SATELLITES
									<< &skyHubTurret << &protonCannon << &ragnarok << &starPort
								#endif
								<< &armored << &artillery << &launcher << &emp
								<< &bomb
								<< &terraformer << &resourceScanner
								#if !defined ONE_COMPOUND_PER_FACTION && !defined NO_MUNITION
									<< &munitionTransporter
								#endif
								<< &mechanic;
								;
			for (auto t: allTurretComponents)
				if (t->mountType == Component::Turret::MountType::Vehicle)
					vehicleTurretComponents << t;
			baseComponents << &hoverbase << &heavybase << &catbase;
		}

		void DestroyAll()
		{

			//hrm, those components have no real destructor...
			vehicleTurretComponents.Clear();
			allTurretComponents.Clear();
			baseComponents.Clear();
		}
	}
}

