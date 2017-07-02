#include <global_root.h>
#include "prototypes.h"
#include "satellite.h"
#include "vehicle.h"
#include "structure.h"


namespace Objects
{
	Ctr::Buffer0<PStructure>			structurePrototypes;
	#ifndef NO_SATELLITES
		Ctr::Buffer0<PSatellite>			satellitePrototypes;
	#endif
	namespace Component
	{
		Ctr::Buffer0<VehicleBase*>		baseComponents;
		Ctr::Buffer0<Turret*>			vehicleTurretComponents,
									allTurretComponents;
	}


	ConstName		ComponentOrPrototypeToCName(const String&name)
	{
		PWorldEntity e;
		
		#ifndef NO_SATELLITES
			e = GetSatellitePrototypeByName(name);
			if (e)
				return e->GetCTypeName();
		#endif
		e = GetPrototypeByName(name);
		if (e)
			return e->GetCTypeName();
		Component::Base*b = Component::GetTurretByName(name);
		if (b)
			return b->constName;
		b = Component::GetVehicleBaseByName(name);
		if (b)
			return b->constName;
		return ConstName::Empty;
	}


	String			ComponentOrPrototypeToString(ConstName name)
	{
		if (name == ConstName::Empty)
			return "";
		PWorldEntity e;
 		#ifndef NO_SATELLITES
			e = GetSatellitePrototypeByName(name);
			if (e)
				return e->GetTypeName();
		#endif
		e = GetPrototypeByName(name);
		if (e)
			return e->GetTypeName();
		Component::Base*b = Component::GetTurretByName(name);
		if (b)
			return b->typeName;
		b = Component::GetVehicleBaseByName(name);
		if (b)
			return b->typeName;
		return "";
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
			foreach (structurePrototypes,p)
				if ((*p)->GetTypeName().find(' '))
				{
					String pn = (*p)->GetTypeName();
					pn.eraseCharacter(' ');
					if (pn == structureTypeName)
						return *p;
				}
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
		return PWorldEntity();
	}


	namespace Component
	{

		Base*			GetByName(const String&name)
		{
			Base* rs = GetVehicleBaseByName(name);
			if (rs)
				return rs;
			return GetTurretByName(name);
		}

		Base*			GetByName(ConstName name)
		{
			Base* rs = GetVehicleBaseByName(name);
			if (rs)
				return rs;
			return GetTurretByName(name);
		}


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

				foreach (baseComponents,p)
					if ((*p)->typeName.find(' '))
					{
						String pn = (*p)->typeName;
						pn.eraseCharacter(' ');
						if (pn == name)
							return *p;
					}
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
				{
					const String&cname = (*p)->typeName;
					if (cname == name)
						return *p;
				}

				foreach (allTurretComponents,p)
					if ((*p)->typeName.find(' '))
					{
						String pn = (*p)->typeName;
						pn.eraseCharacter(' ');
						if (pn == name)
							return *p;
					}

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
			/*order change: by moving registration of applicable turret and base components before Configure() calls,
			research registration can check whether a congigured component should be able to register research at all
			*/
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

