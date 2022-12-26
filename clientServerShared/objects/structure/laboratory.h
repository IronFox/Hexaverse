#pragma once
#include "../structure.h"

namespace Objects
{

	class Laboratory : public WorkingStructureT<Laboratory>
	{
		typedef WorkingStructureT<Laboratory>	Super;
		friend class Super;

		Ctr::Vector<WLaboratory>	labNeighbors;


        /**/						Laboratory(const PBaseAttributeTable&table, const OwnerParameter&owner, const Grid::LCellIdI&location, float atTime) : Super(table,owner, location, atTime)	{};
		/**/						Laboratory() {}//global prototype
	protected:
		//virtual bool				CanEnable() const override {return MobileEntityProducer::queue.IsNotEmpty();}

	public:
		static const int			EnergyConsumption = 10;

		#ifdef HAS_WINDOW
			virtual void			DescribeType(Description&label, UINT32 descFlags, const OwnerParameter&owner, const T2::PConstProperties&t2) override;
		#endif

		count_t						GetTotalResearchOutput() const;


		//Structure overrides:
		#ifdef HAS_WINDOW
		#ifdef ONE_COMPOUND_PER_FACTION
			virtual const Geometry*	GetSourceForConnection(const PStructure&connected)	override {return To<Laboratory>::DynamicCast(connected) ? &standardConnector : Super::GetSourceForConnection(connected);}
		#endif
		#endif
		virtual bool				IsBuilding(std::pair<int,int>&outProgress) const override;
		virtual Category			GetCategory() const override {return Category::Miscellaneous;}
		virtual void				OnNewConnection(const PStructure&, UINT32 acrossHighCell, float atTime) override;
		virtual void				OnConnectionLost(const PStructure&, UINT32 acrossHighCell, float atTime) override;

		//WorldEntity overrides:
		virtual void				PrototypeSetup()	override;
		#ifndef NO_HIBERNATION
			virtual int				GetUplinkTraffic() const override {return 20;}
		#endif
		virtual bool				AwaitsUserInteraction() const override {return false;} //place holder for later

		#ifndef NO_HIBERNATION
			//Hibernating overrides:
			virtual int				GetUplinkPriority() const override {return 800;};
		#endif

		//BaseEntity overrides:
		virtual String				GetTypeName() const override { return "Laboratory"; };
		virtual EntityType			GetType() const override {return ConstName::Type_Laboratory;}


		//WorkingStructure overrides:
		virtual void				GetResourceVector(Resource::Vector&v,Resource::Vector&, Resource::Vector&) const override	
		{
			v.Set(Resource::Energy, -EnergyConsumption);
		}
		virtual bool				WouldLikeToEnable() const {return true;}
		virtual bool				IsLaboratory() const override {return true;}
		#ifndef CLIENT
			virtual void			Tick(Resource::Vector&) override;
		#endif
	};
	


}
