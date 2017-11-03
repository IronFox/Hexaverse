#pragma once
#include "../structure.h"

namespace Objects
{
class VehiclePlant : public WorkingStructureT<VehiclePlant>, public MobileEntityProducer
	{
		friend class WorkingStructureT<VehiclePlant>;

        /**/						VehiclePlant(BaseAttributeTable*table, const OwnerParameter&owner, const Grid::LCellIdI&location, float atTime);
		/**/						VehiclePlant():  MobileEntityProducer(DefaultProductionPerRound) {}//global prototype
	protected:

	public:
		typedef WorkingStructureT<VehiclePlant>	Super;



		static const int			EnergyConsumption = 0;
		static const int			DefaultProductionPerRound = 10;


		/**
		All base components known to VehiclePlant.
		Vehicle may use these as bases
		*/
		static Ctr::Buffer0<Component::VehicleBase*>
									baseComponents;
		/**
		All turret components known to VehiclePlant.
		Effectively a subset of all turret types available.
		Vehicle may use these as turrets.
		*/
		static Ctr::Buffer0<Component::Turret*>
									turretComponents;


		#ifdef CLIENT
			Component::VehicleBase*	selectedBase;
			Component::Turret*		selectedTurret;
		#endif

		#ifdef CLIENT
			Component::VehicleBase*	GetProductionBase() const;

			#ifdef HAS_WINDOW
				virtual void		DescribeType(Description&label, UINT32 descFlags) override;
			#endif

		#else
			bool					AbortConstruction(const TUniqueID&);
			PVehicle				SpawnFinishedProductionAnywhere();
			PVehicle				Enqueue(Component::VehicleBase*, Component::Turret*);
			PVehicle				Enqueue(EntityType t);
			PVehicle				GetProduction() const;
			//PVehicle				CompleteProduction();
			virtual void			Tick(ResourceVector&) override;
		#endif




		//Structure overrides:
		virtual bool				IsBuilding(std::pair<int,int>&outProgress) const override { return MobileEntityProducer::IsBuilding(outProgress); }
		virtual Category			GetCategory() const override {return Category::Factories;}


		//WorldEntity overrides:
		virtual void				PrototypeSetup()	override;
		//virtual int					GetConstructionCost() const override {return 80;}
		#ifndef NO_HIBERNATION
			virtual int				GetUplinkTraffic() const override {return 20;}
		#endif
		//virtual bool				IsVehiclePlant() const override {return true;}
		virtual bool				AwaitsUserInteraction() const override;
		#ifdef HAS_WINDOW
			virtual float			GetSoundLevel() const {return IsEnabled() ? GetActivityLevel() : 0.f;}
		#endif

		//MobileEntityProducer overrides
		#ifndef CLIENT
			virtual void			OnSleepChange(float&t) override;
			virtual void			SignalLastProductionDone(float&t) override {LogEvent(EventSignificance::Common, this, "Queue is empty. Shutting off"); SetEnabled(false,t);}
			virtual void			SignalNewProductionEnqueued() override {float t = 0; SetEnabled(true,t);}
			virtual bool			DirectTo(CursorMode, Ctr::ArrayRef<Grid::LCellIdI> waypoints, bool detailedPath, bool leftClick = false) override;
		#endif
		virtual bool				CanProduce(const PWorldEntity&e) const override;

		#ifndef NO_HIBERNATION
			//Hibernating overrides:
			virtual int				GetUplinkPriority() const override {return 800;};
		#endif

		//BaseEntity overrides:
		virtual String				GetTypeName() const override { return "Vehicle Plant"; };
		virtual EntityType			GetType() const override {return ConstName::Type_VehiclePlant;}


		#ifdef HAS_WINDOW
			virtual const CC*		GetCommands(CC&dynamicStorage) const override;
			virtual void			OnSelect()	override;
			virtual void			OnDeselect() override;
		#endif
		virtual const MobileEntityProducer*CastToProducer() const override {return this;}


		//WorkingStructure overrides:
		virtual void				GetResourceVector(ResourceVector&v,ResourceVector&, ResourceVector&) const override	{v.level[Resource::Energy] = -EnergyConsumption; v.level[Resource::Minerals] = -(int)MobileEntityProducer::productionPerRound;}
		virtual bool				CanEnable() const override {return MobileEntityProducer::queue.IsNotEmpty();}

	};


}
