#include <global_root.h>
#include "prototypes.h"
#include "satellite.h"
#include "vehicle.h"
#include "structure.h"
#include "entityType.h"


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


	
	String			ToString(const EntityType&t)
	{
		if (t.HasTurret())
		{
			StringBuffer rs;
			Component::Base*b = Component::GetTurretByName(t.turretType);
			if (b)
				rs << b->typeName;
			else
				rs << "#"<<int(t.turretType);
			rs << ':';

			if (t.baseType == ConstName::Type_Tower)
				rs << "Tower";
			else
			{
				b = Component::GetVehicleBaseByName(t.baseType);
				if (b)
					rs << b->typeName;
				else
					rs << "#"<<int(t.baseType);
			}
			return rs.ToStringRef();
		}
		auto e = GetPrototypeByName(t.baseType);
		if (e)
			return e->GetTypeName();
		return "#"+String(int(t.baseType));
	}

	EntityType		DecodeType(const String&combined)
	{
		EntityType rs;
		index_t at = combined.Find(':');
		StringRef base = combined.ToRef();
		if (at != InvalidIndex)
		{
			const StringRef baseName = combined.SubStringRef(at+1);
			const StringRef turretName = combined.SubStringRef(0,at);
			if (baseName == "Tower")
				rs.baseType = ConstName::Type_Tower;
			else
			{
				auto base = Component::GetVehicleBaseByName(baseName);
				if (base == nullptr)
					throw Except::Program::ParameterFault(CLOCATION,"Unable to decode base type name '"+String(baseName)+"'");
				rs.baseType = base->constName;
			}

			auto turret = Component::GetTurretByName(turretName);
			if (turret == nullptr)
				throw Except::Program::ParameterFault(CLOCATION,"Unable to decode turret type name '"+String(turretName)+"'");
			rs.turretType = turret->constName;
		}
		else
			rs = ComponentOrPrototypeToCName(base);
		if (rs.IsInvalid())
			throw Except::Program::ParameterFault(CLOCATION,"Final type declaration '"+ToString(rs)+"' is not valid");
		return rs;
	}


	ConstName		ComponentToCName(const StringRef&name)
	{
		Component::Base*b = Component::GetTurretByName(name);
		if (b)
			return b->constName;
		b = Component::GetVehicleBaseByName(name);
		if (b)
			return b->constName;
		if (name == "Tower")
			return ConstName::Type_Tower;
		throw Except::Program::ParameterFault(CLOCATION,"Unable to encode component type '"+String(name)+"'");
	}

	ConstName		ComponentToCName(const String&name)
	{
		return ComponentToCName(name.ToRef());
	}


	EntityType		ComponentOrPrototypeToCName(const StringRef&name)
	{
		PWorldEntity e;
		
		#ifndef NO_SATELLITES
			e = GetSatellitePrototypeByName(name);
			if (e)
				return e->GetType();
		#endif
		e = GetPrototypeByName(name);
		if (e)
			return e->GetType();
		return ComponentToCName(name);
	}


	EntityType		ComponentOrPrototypeToCName(const String&name)
	{
		return ComponentOrPrototypeToCName(name.ToRef());
	}


	String			ComponentOrPrototypeToString(EntityType name)
	{
		if (name.IsEmpty() || !name.IsValid())
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

		if (name.HasTurret())
			return "";	//dual type cannot resolve to component

		Component::Base*b = Component::GetTurretByName(name.baseType);
		if (b)
			return b->typeName;
		b = Component::GetVehicleBaseByName(name.baseType);
		if (b)
			return b->typeName;
		if (name == ConstName::Type_Tower)
			return "Tower";
		return "";
	}

	

	#ifndef NO_SATELLITES

		PSatellite		GetSatellitePrototypeByName(const String&name)
		{
			return GetSatellitePrototypeByName(name.ToRef());
		}
		PSatellite		GetSatellitePrototypeByName(const StringRef&name)
		{
			if (name.IsNotEmpty())
			{
				foreach (satellitePrototypes,p)
					if ((*p)->GetTypeName() == name)
						return *p;
			}
			return PSatellite();
		}
		PSatellite		GetSatellitePrototypeByName(EntityType name)
		{
			if (name != ConstName::Empty)
			{
				foreach (satellitePrototypes,p)
					if ((*p)->GetType() == name)
						return *p;
			}
			return PSatellite();
		}

	#endif

	PStructure		GetPrototypeByName(EntityType structureTypeName)
	{
		if (structureTypeName.IsNotEmpty())
		{
			foreach (structurePrototypes,p)
				if ((*p)->GetType() == structureTypeName)
					return *p;
		}
		return PStructure();
	}
	
	PStructure		GetPrototypeByName(const String&structureTypeName)
	{
		return GetPrototypeByName(structureTypeName.ToRef());
	}

	PStructure		GetPrototypeByName(const StringRef&structureTypeName)
	{
		if (structureTypeName.IsNotEmpty())
		{
			foreach (structurePrototypes,p)
				if ((*p)->GetTypeName() == structureTypeName)
					return *p;
			foreach (structurePrototypes,p)
				if ((*p)->GetTypeName().Contains(' '))
				{
					String pn = (*p)->GetTypeName();
					pn.EraseCharacter(' ');
					if (pn == structureTypeName)
						return *p;
				}
		}
		return PStructure();
	}


	PWorldEntity	GetNonVehiclePrototypeByName(EntityType typeName)
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

	PWorldEntity	GetNonVehiclePrototypeByName(const String&typeName)
	{
		return GetNonVehiclePrototypeByName(typeName.ToRef());
	}
	PWorldEntity	GetNonVehiclePrototypeByName(const StringRef&typeName)
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
			return GetByName(name.ToRef());
		}

		Base*			GetByName(const StringRef&name)
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
			return GetVehicleBaseByName(name.ToRef());
		}

		VehicleBase*	GetVehicleBaseByName(const StringRef&name)
		{
			//Buffer0<VehicleBase*>		baseComponents;
			//Buffer0<Turret*>			turretComponents;
			//if (structureTypeName.IsNotEmpty())
			{
				foreach(baseComponents, p)
					if ((*p)->typeName == name)
						return *p;

				foreach (baseComponents,p)
					if ((*p)->typeName.Contains(' '))
					{
						String pn = (*p)->typeName;
						pn.EraseCharacter(' ');
						if (pn == name)
							return *p;
					}
			}
			return nullptr;
		}


		Turret*			GetTurretByName(const String&name)
		{
			return GetTurretByName(name.ToRef());
		}

		Turret*			GetTurretByName(const StringRef&name)
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
					if ((*p)->typeName.Contains(' '))
					{
						String pn = (*p)->typeName;
						pn.EraseCharacter(' ');
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

