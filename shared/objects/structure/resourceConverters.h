#pragma once

#include "../structure.h"

namespace Objects
{
	class OrganicCompoundFactory : public WorkingStructureT<OrganicCompoundFactory>
	{
	private:
		typedef WorkingStructureT<OrganicCompoundFactory>			Super;

		friend class WorkingStructureT<OrganicCompoundFactory>;

		/**/						OrganicCompoundFactory(BaseAttributeTable*table, const OwnerParameter&owner, const Grid::LCellIdI&location, float atTime) 
									: Super(table,owner,location,atTime)
		{}
		/**/						OrganicCompoundFactory()
		{}	//global prototype
		
	public:

		static const int		EnergyDemand = 10,
								CrystallineDemand = 10,
								FuelDemand = 10,
								OrganicOutput = 5
								;


		//BaseEntity:
		virtual String			GetTypeName() const override { return "Organic Compound Factory"; };
		virtual EntityType		GetType() const override {return ConstName::Type_OrganicCompoundFactory;}


		//WorkingStructure:
		#ifdef HAS_WINDOW
			virtual void		DescribeType(Description&label, UINT32 descFlags, const OwnerParameter&owner) override;
		#endif

		#ifndef CLIENT
			virtual void		Tick(Resource::Vector&v) override;
		#endif
		virtual void			GetResourceVector(Resource::Vector&balance, Resource::Vector&storageCurrent, Resource::Vector&storageMax) const override;
		virtual Category		GetCategory() const override {return Category::Resources;}
	};


	class CrystallineCompoundFactory : public WorkingStructureT<CrystallineCompoundFactory>
	{
	private:
		typedef WorkingStructureT<CrystallineCompoundFactory>			Super;

		friend class WorkingStructureT<CrystallineCompoundFactory>;

		/**/						CrystallineCompoundFactory(BaseAttributeTable*table, const OwnerParameter&owner, const Grid::LCellIdI&location, float atTime) 
									: Super(table,owner,location,atTime)
		{}
		/**/						CrystallineCompoundFactory()
		{}	//global prototype
		
	public:

		static const int		EnergyDemand = 10,
								OrganicDemand = 10,
								MineralDemand = 10,
								CrystallineOutput = 5
								;


		//BaseEntity:
		virtual String			GetTypeName() const override { return "Crystalline Compound Factory"; };
		virtual EntityType		GetType() const override {return ConstName::Type_CrystallineCompoundFactory;}


		//WorkingStructure:
		#ifdef HAS_WINDOW
			virtual void		DescribeType(Description&label, UINT32 descFlags, const OwnerParameter&owner) override;
		#endif

		#ifndef CLIENT
			virtual void		Tick(Resource::Vector&v) override;
		#endif
		virtual void			GetResourceVector(Resource::Vector&balance, Resource::Vector&storageCurrent, Resource::Vector&storageMax) const override;
		virtual Category		GetCategory() const override {return Category::Resources;}
	};
}


