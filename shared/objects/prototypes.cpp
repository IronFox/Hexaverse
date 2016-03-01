#include <global_root.h>
#include "prototypes.h"
#include "satellite.h"
#include "vehicle.h"
#include "structure.h"


namespace Objects
{
	Buffer0<PStructure>			structurePrototypes;
	Buffer0<PSatellite>			satellitePrototypes;
	namespace Component
	{
		Buffer0<VehicleBase*>		baseComponents;
		Buffer0<Turret*>			vehicleTurretComponents,
									allTurretComponents;
	}



	


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
			SkyGuardian::Configure(protonCannon);
			SkyHubTurret::Configure(skyHubTurret);
			Ragnarok::Configure(ragnarok);
			StarPort::Configure(starPort);

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



			allTurretComponents << &radar 
								<< &skyHubTurret << &protonCannon << &ragnarok << &starPort
								<< &armored << &artillery << &launcher << &emp
								<< &bomb
								<< &terraformer << &resourceScanner
								#ifndef ONE_COMPOUND_PER_FACTION
									<< &munitionTransporter
								#endif
								<< &mechanic;
								;
			for (auto t: allTurretComponents)
				if (!t->isSatelliteRing)
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

