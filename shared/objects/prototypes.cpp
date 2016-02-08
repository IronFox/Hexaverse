#include <global_root.h>
#include "prototypes.h"
#include "orbitalStructure.h"
#include "orbitalVehicle.h"
#include "vehicle.h"
#include "structure.h"


namespace Objects
{
	Buffer0<PStructure>			structurePrototypes;
	namespace Component
	{
		Buffer0<VehicleBase*>		baseComponents;
		Buffer0<Turret*>			vehicleTurretComponents,
									allTurretComponents;
	}

	namespace Orbital
	{
		Buffer0<PShip>			shipPrototypes;
		Buffer0<PStation>		stationPrototypes;


		


		PShip		GetShipPrototypeByName(const String&name)
		{
			if (name.IsNotEmpty())
			{
				foreach (shipPrototypes,p)
					if ((*p)->GetTypeName() == name)
						return *p;
			}
			return PShip();
		}
		PShip		GetShipPrototypeByName(ConstName name)
		{
			if (name != ConstName::Empty)
			{
				foreach (shipPrototypes,p)
					if ((*p)->GetCTypeName() == name)
						return *p;
			}
			return PShip();
		}

		


		PStation		GetStationPrototypeByName(const String&name)
		{
			if (name.IsNotEmpty())
			{
				foreach (stationPrototypes,p)
					if ((*p)->GetTypeName() == name)
						return *p;
			}
			return PStation();
		}
		PStation		GetStationPrototypeByName(ConstName name)
		{
			if (name != ConstName::Empty)
			{
				foreach (stationPrototypes,p)
					if ((*p)->GetCTypeName() == name)
						return *p;
			}
			return PStation();
		}

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


	PWorldEntity	GetAnyNonVehiclePrototypeByName(ConstName structureTypeName)
	{
		PWorldEntity rs;

		rs = GetPrototypeByName(structureTypeName);
		if (rs)
			return rs;
		rs = Orbital::GetShipPrototypeByName(structureTypeName);
		if (rs)
			return rs;
		rs = Orbital::GetStationPrototypeByName(structureTypeName);
		if (rs)
			return rs;
		return rs;
	}

	PWorldEntity	GetAnyNonVehiclePrototypeByName(const String&structureTypeName)
	{
		PWorldEntity rs;

		rs = GetPrototypeByName(structureTypeName);
		if (rs)
			return rs;
		rs = Orbital::GetShipPrototypeByName(structureTypeName);
		if (rs)
			return rs;
		rs = Orbital::GetStationPrototypeByName(structureTypeName);
		if (rs)
			return rs;
		return rs;
	}

	PStructure		GetAnyStructurePrototypeByName(ConstName structureTypeName)
	{
		PStructure rs;

		rs = GetPrototypeByName(structureTypeName);
		if (rs)
			return rs;
		rs = Orbital::GetStationPrototypeByName(structureTypeName);
		if (rs)
			return rs;
		return rs;

	}

	PStructure		GetAnyStructurePrototypeByName(const String&structureTypeName)
	{
		PStructure rs;

		rs = GetPrototypeByName(structureTypeName);
		if (rs)
			return rs;
		rs = Orbital::GetStationPrototypeByName(structureTypeName);
		if (rs)
			return rs;
		return rs;

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
			Orbital::Guardian::Configure(Orbital::protonCannon);
			Orbital::Hub::Configure(Orbital::hub);
			Orbital::Ragnarok::Configure(Orbital::ragnarok);
			Orbital::StarPort::Configure(Orbital::starPort);
			Orbital::Engineer::Configure(Orbital::engineer);

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
								<< &Orbital::hub << &Orbital::protonCannon << &Orbital::ragnarok << &Orbital::starPort << &Orbital::engineer
								<< &armored << &artillery << &launcher << &emp
								<< &terraformer << &resourceScanner
								#if !defined CONNECTOR_LESS && !defined NO_MUNITION
									<< &munitionTransporter
								#endif
								<< &mechanic;
								;
			for (auto t: allTurretComponents)
				if (!t->isOrbital)
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

