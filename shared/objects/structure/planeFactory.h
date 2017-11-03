#pragma once
#include "../structure.h"

namespace Objects
{
	class PlaneFactory : public WorkingStructureT<PlaneFactory>, public MobileEntityProducer
	{
	public:
	protected:
		typedef WorkingStructureT<PlaneFactory>	Super;


		friend class Super;
		/**/						PlaneFactory():MobileEntityProducer(ConstructionSpeed) {MobileEntityProducer::SetCommandOffset(int2(3,1));}
		/**/						PlaneFactory(BaseAttributeTable*table, const OwnerParameter&owner, const Grid::LCellIdI&location, float atTime):Super(table,owner,location,atTime), MobileEntityProducer(ConstructionSpeed){MobileEntityProducer::SetCommandOffset(int2(3,1));}
	public:
		virtual bool				CanEnable() const override { return MobileEntityProducer::queue.IsNotEmpty(); }
		static const int			ConstructionSpeed = 20;
		static const int			FuelDemand = 20;
		static const int			EnergyConsumption = 0;

		static Buffer0<EntityType>	prototypes;


		//BaseEntity overrides:
		virtual String				GetTypeName() const override { return "Plane Factory"; };
		virtual EntityType			GetType() const override {return ConstName::Type_PlaneFactory;}

		//WorkingStructure overrides:
		virtual void				GetResourceVector(ResourceVector&v,ResourceVector&, ResourceVector&) const override;

		//WorldEntity overrides:
		virtual int					GetHeight() const override {return 2;}
		virtual void				PrototypeSetup() override;
		#ifndef NO_HIBERNATION
			virtual int				GetUplinkTraffic() const override {return 10;}
			virtual int				GetUplinkPriority() const override {return 750;};	//not much point going far beyond generators. if they shut off, this thing shuts off, too
		#endif
		virtual bool				AwaitsUserInteraction() const override;

		#ifndef CLIENT
			virtual void			OnSleepChange(float&t) override;

			//virtual void			OnConstructionFinished(float atTime) override;	//has to register with owner	//some day
			//virtual void			OnDestroy(float atTime) override;	//has to unregister with owner	//some day
			virtual void			Tick(ResourceVector&) override;
			virtual void			SignalLastProductionDone(float&t) override {LogEvent(EventSignificance::Common, this, "Queue is empty. Shutting off"); SetEnabled(false,t);}

		#endif
		#ifdef HAS_WINDOW
			virtual float			GetSoundLevel() const {return IsEnabled() ? GetActivityLevel() : 0.f;}
			virtual void			DescribeType(Description&label, UINT32 descFlags) override
			{
				Super::DescribeType(label,descFlags);
				//label << " Max Vehicles per Store: "+String(MaxVehiclesPerStore);
				label.MarkBriefEnd();
				label  << "Plane factories produce planes";
			}
			virtual const CC*		GetCommands(CC&dynamicStorage) const override;
		#endif
		virtual const MobileEntityProducer*CastToProducer() const override {return this;}

		//MobileEntityProducer overrides
		#ifndef CLIENT
			virtual void			SignalNewProductionEnqueued() override {float t = 0; SetEnabled(true,t);}
		#endif
		virtual bool				CanProduce(const PWorldEntity&e) const override;


		//Structure overrides:
		virtual bool				IsBuilding(std::pair<int,int>&outProgress) const override
		{
			return MobileEntityProducer::IsBuilding(outProgress);
		}
		virtual Category			GetCategory() const override {return Category::Factories;}
	};
}
