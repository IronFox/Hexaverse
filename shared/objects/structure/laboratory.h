#pragma once
#include "../structure.h"

namespace Objects
{

	class Laboratory : public WorkingStructureT<Laboratory>
	{
		typedef WorkingStructureT<Laboratory>	Super;
		friend class Super;

		Ctr::Buffer0<WLaboratory>	labNeighbors;


        /**/						Laboratory(BaseAttributeTable*table, const OwnerParameter&owner, const Grid::LCellIdI&location, float atTime) : Super(table,owner, location, atTime)	{};
		/**/						Laboratory() {}//global prototype
	protected:
		//virtual bool				CanEnable() const override {return MobileEntityProducer::queue.IsNotEmpty();}

	public:
		static const int			EnergyConsumption = 10;

		#ifdef HAS_WINDOW
			virtual void			DescribeType(Description&label, UINT32 descFlags) override;
		#endif

		count_t						GetTotalResearchOutput() const;


		//Structure overrides:
		virtual bool				IsBuilding(std::pair<int,int>&outProgress) const override;
		virtual Category			GetCategory() const override {return Category::Miscellaneous;}
		virtual void				OnNewConnection(const PStructure&, UINT32 acrossHighCell, float atTime) override;
		virtual void				OnConnectionLost(const PStructure&, UINT32 acrossHighCell, float atTime) override;

		//WorldEntity overrides:
		virtual void				PrototypeSetup()	override;
		//virtual int					GetConstructionCost() const override {return 80;}
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
		virtual ConstName			GetCTypeName() const override {return ConstName::Type_Laboratory;}


		//WorkingStructure overrides:
		virtual void				GetResourceVector(ResourceVector&v,ResourceVector&, ResourceVector&) const override	{v.level[Resource::Energy] = -EnergyConsumption;}
		virtual bool				WouldLikeToEnable() const {return true;}
		virtual bool				IsLaboratory() const override {return true;}
		#ifndef CLIENT
			virtual void			Tick(ResourceVector&) override;
		#endif
	};
	


}
