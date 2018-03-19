#include <global_root.h>
#include "prototypes.h"
#include "satellite.h"
#include "vehicle.h"
#include "structure.h"
#include "subOrbital.h"
#include "entityType.h"

#include "structure/tower.h"
#include "structure/mine.h"
#include "structure/vehiclePlant.h"
#include "structure/planeFactory.h"
#include "structure/vehicleStore.h"
#include "structure/laboratory.h"


namespace Objects
{
	namespace Component
	{
		Collection<VehicleBase>		baseComponents;
		Collection<Turret>			vehicleTurretComponents,
									allTurretComponents;
		Collection<Base>			allComponents;
	}


	
	String			ToString(const EntityType&t)
	{
		if (t.IsEmpty())
			return "<empty>";
		if (t.HasTurret())
		{
			StringBuffer rs;
			rs	<< Component::allTurretComponents.ToTypeName(t.turretType)
				<< ':';
			if (t.baseType == ConstName::Type_Tower)
				rs << "Tower";
			else
				rs << Component::baseComponents.ToTypeName(t.baseType);
			return rs.ToStringRef();
		}
		return Prototypes::all.ToTypeName(t);
	}

	EntityType		DecodeType(const String&combined)
	{
		if (combined == "<empty>")
			return EntityType();
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
				rs.baseType = Component::baseComponents.ToType(baseName);
			rs.turretType = Component::allTurretComponents.ToType(turretName);
		}
		else
			rs = Prototypes::all.ToType(base);
		if (rs.IsInvalid())
			throw Except::Program::ParameterFault(CLOCATION,"Final type declaration '"+ToString(rs)+"' is not valid");
		return rs;
	}


	namespace Prototypes
	{

		Collection<WorldEntity>		all;
		Collection<Structure>		structures;
		Collection<SubOrbital::Plane>		planes;
		#ifndef NO_SATELLITES
			Collection<Satellite>	satellites;
		#endif


		void	Destroy()
		{
			all.Clear();
			structures.Clear();
			planes.Clear();
			#ifndef NO_SATELLITES
				satellites.Clear();
			#endif
		}

		void RegisterAll()
		{
			Component::ConfigureAll();



			structures	
				<< Mine::globalPrototype
				<< Generator::globalPrototype
				<< ReactorCore::globalPrototype
				<< MunitionFactory::globalPrototype
				<< VehiclePlant::globalPrototype
				<< Laboratory::globalPrototype
				<< Connector::globalPrototype
				<< FuelContainer::globalPrototype
				<< MineralContainer::globalPrototype
				<< MunitionStorage::globalPrototype //DO NOT COMMENT. Nexus bases feature these
				<< UraniumStorage::globalPrototype
				<< NexusPort::globalPrototype
				<< VehicleStore::globalPrototype
				<< PlaneFactory::globalPrototype
				;
		
			#ifndef NO_SATELLITES
				structures << SkyPort::globalPrototype;

				satellites 
					<< SkyHub::globalPrototype
					<< SkyGuardian::globalPrototype
					<< Ragnarok::globalPrototype
					;
			#endif

			planes	<< SubOrbital::Raptor::globalPrototype
					<< SubOrbital::Bomber::globalPrototype
					<< SubOrbital::CargoPlane::globalPrototype
					<< SubOrbital::SensorDrone::globalPrototype
					;


			Tower::globalPrototypes.Append().reset(new Tower::Prototype(&Component::launcher));
			Tower::globalPrototypes.Append().reset(new Tower::Prototype(&Component::rockets));
			//Tower::globalPrototypes.Append().reset(new Tower::Prototype(artillery));
			Tower::globalPrototypes.Append().reset(new Tower::Prototype(&Component::radar));
			Tower::globalPrototypes.Append().reset(new Tower::Prototype(&Component::armored));
			Tower::globalPrototypes.Append().reset(new Tower::Prototype(&Component::resourceScanner));
			foreach (Tower::globalPrototypes,p)
				structures.Add(p->get()->instance);









			//all specitic prototypes must be set at this point
			all.Include(structures);
			all.Include(planes);
			#ifndef NO_SATELLITES
				all.Include(satellites);
			#endif
		}



		PWorldEntity	Find(EntityType t)
		{
			return all.Find(t);
		}


	}

	namespace Component
	{
		void ConfigureAll()
		{
			Artillery::Configure(artillery);
			Launcher::Configure(launcher);
			Rockets::Configure(rockets);
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
			SubOrbital::Bomber::Configure(SubOrbital::bomber);
			SubOrbital::CargoPlane::Configure(SubOrbital::cargoPlane);
			SubOrbital::SensorDrone::Configure(SubOrbital::sensorDrone);










			allTurretComponents << &radar 
								#ifndef NO_SATELLITES
									<< &skyHubTurret << &protonCannon << &ragnarok << &starPort
								#endif
								<< &armored << &artillery << &launcher << &rockets << &emp
                                << &SubOrbital::raptor << &SubOrbital::bomber << &SubOrbital::cargoPlane << &SubOrbital::sensorDrone
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

			allComponents.Include(baseComponents);
			allComponents.Include(allTurretComponents);


		
		}

		void DestroyAll()
		{

			//hrm, those components have no real destructor...
			vehicleTurretComponents.Clear();
			allTurretComponents.Clear();
			baseComponents.Clear();
			allComponents.Clear();
		}
	}
}

