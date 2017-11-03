#include <global_root.h>
#include "vehiclePlant.h"
#include "../prototypes.h"
#include "../vehicle.h"

namespace Objects
{
	

	/*static*/ Ctr::Buffer0<Component::VehicleBase*>		VehiclePlant::baseComponents;
	/*static*/ Ctr::Buffer0<Component::Turret*>			VehiclePlant::turretComponents;
	
	#ifndef CLIENT
		/*virtual override*/ void				VehiclePlant::Tick(ResourceVector&v)
		{
			v.level[Resource::Energy] -= EnergyConsumption;

			float t = 0.f;	//buildings always come at the beginning
			MobileEntityProducer::Tick(v,t);
		}


		/*virtual override*/ void			VehiclePlant::OnSleepChange(float&t)
		{
			Super::OnSleepChange(t);

			if (IsSleeping() && IsEnabled())
			{
				LogEvent(EventSignificance::Common,this,"Fell asleep. Disabling");
				SetEnabled(false,t);
			}
		}

		/*virtual override*/ bool			VehiclePlant::DirectTo(CursorMode mode, Ctr::ArrayRef<Grid::LCellIdI> waypoints, bool detailedPath, bool leftClick /*= false*/)
		{
			if (waypoints.Count() != 1)
				return false;
			MobileEntityProducer::UpdateGatheringPoint(waypoints.First());
			return true;
		}

		PVehicle					VehiclePlant::GetProduction() const
		{
			return To<Vehicle>::Cast(MobileEntityProducer::GetActiveProduction());
		}

		PVehicle				VehiclePlant::Enqueue(Component::VehicleBase*b, Component::Turret*t)
		{
			auto item = MobileEntityProducer::EnqueueProductionOfVehicle(b,t);
			return item ? To<Vehicle>::Cast(item->producing) : PVehicle();
		}
		PVehicle				VehiclePlant::Enqueue(EntityType t)
		{
			auto item = MobileEntityProducer::EnqueueProductionOfVehicle(t);
			return item ? To<Vehicle>::Cast(item->producing) : PVehicle();
		}


		bool				VehiclePlant::AbortConstruction(const TUniqueID&id)
		{
			return MobileEntityProducer::AbortConstruction(id);
		}

	#else
	
	
		Component::VehicleBase*					VehiclePlant::GetProductionBase() const
		{
			return queue.IsNotEmpty() ? queue.First().base : nullptr;
		}



		#ifdef HAS_WINDOW


			/*virtual override*/ void			VehiclePlant::OnSelect()
			{
				gatheringPoint.UpdateVisuals();
			}
			/*virtual override*/ void			VehiclePlant::OnDeselect()
			{
				gatheringPoint.selected = Grid::LCellIdI::Invalid;
			}


			/*virtual override*/ const CC*		VehiclePlant::GetCommands(CC&dynamicStorage) const
			{
				dynamicStorage.Append(Super::GetCommands(dynamicStorage));
				dynamicStorage.AppendRow();
				MobileEntityProducer::GetCommands(dynamicStorage);
				return &dynamicStorage;
			}

			/*virtual override*/ void			VehiclePlant::DescribeType(Command::Base::Label&label, UINT32 descFlags)
			{
				label.BeginBrief();
				Super::DescribeType(label,  descFlags);
				label.MarkBriefEnd();
				label << "Enabled vehicle plants produce vehicles of arbitrary configuration at the expense of "
				+String(ResourceTag(Resource::Minerals))+", and "+String(TimeResourceTag())+"."
				" A vehicle plant that does not have access to the necessary resources switches off immediately."
				" Vehicle plants should be placed so that any produced vehicles can stand in the immediate surroundings of the structure."
				;
			}

		#endif
	#endif


	VehiclePlant::VehiclePlant(BaseAttributeTable*table, const OwnerParameter&owner, const Grid::LCellIdI&location, float atTime) : Super(table,owner, location, atTime),
		MobileEntityProducer(DefaultProductionPerRound)
	{
		#ifdef CLIENT
			// make some base selected by default
			ASSERT__(baseComponents.IsNotEmpty())

			foreach (baseComponents,c)
				if (!owner || (*c)->isUnlocked(owner))
				{
					selectedBase = *c;
					break;
				}
			foreach (turretComponents,c)
				if (!owner || (*c)->isUnlocked(owner))
				{
					selectedTurret = *c;
					break;
				}
		#endif
	}


	/*virtual override*/ void				VehiclePlant::PrototypeSetup()
	{
		Super::PrototypeSetup();
		baseComponents = Objects::Component::baseComponents.GetStore().list;
		for (const auto& turret:Objects::Component::vehicleTurretComponents)
			if (turret->mountType == Component::Turret::MountType::Vehicle)
				turretComponents << turret;


		//commands << Command("Queue:",[](const PWorldEntity&){},AlwaysFalse<PWorldEntity const&>,AlwaysFalse<PWorldEntity const&>);

	}

	/*virtual override*/ bool				VehiclePlant::AwaitsUserInteraction() const 
	{
		return  MobileEntityProducer::AwaitsUserInteraction();
	}



	/*virtual override*/ bool				VehiclePlant::CanProduce(const PWorldEntity&e) const 
	{
		return e->IsVehicle() && baseComponents.Contains(To<Vehicle>::Cast(e)->base)  && turretComponents.Contains(To<Vehicle>::Cast(e)->turret);
	}


}

