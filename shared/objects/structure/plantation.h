#pragma once
#include "storageStructures.h"

namespace Objects
{


	/**
	Grows, harvests, or removes trees/crystals
	*/
	class Plantation : public StorageStructure<Plantation,3>
	{
	private:
		typedef StorageStructure<Plantation,3>			Super;


		friend class WorkingStructureT<Plantation>;
		/**/						Plantation(BaseAttributeTable*table, const OwnerParameter&owner, const Grid::LCellIdI&location, float atTime) 
									: Super(table,owner,location,atTime)
		{
			maxStorage.Set(Resource::CrystallineCompounds, CompoundStorage);
			maxStorage.Set(Resource::OrganicCompounds, CompoundStorage);
		}
		/**/						Plantation()
		{
			maxStorage.Set(Resource::CrystallineCompounds, CompoundStorage);
			maxStorage.Set(Resource::OrganicCompounds, CompoundStorage);
		}	//global prototype
		


	public:
		static const int			EnergyConsumption = 10;
		static const int			GrowthMineralCost = 10;
		#ifndef ONE_COMPOUND_PER_FACTION
			static const int		CompoundStorage = 10;
		#else
			static const int		CompoundStorage = 0;
		#endif

		#ifdef HAS_WINDOW
			virtual void			DescribeType(Description&label, UINT32 descFlags, const OwnerParameter&owner) override;
		#endif

		int							dendritePresence=0,
									treePresence=0;

		enum class Mode
		{
			Grow,
			Maintain,
			Remove
		}							mode = Mode::Maintain;



		

		virtual bool				WouldLikeToEnable() const override {return true;}
		virtual bool				CanEnable() const override;

		virtual void				PrototypeSetup()	override;

		virtual void				GetResourceVector(Resource::Vector&v,  Resource::Vector&storageCurrent, Resource::Vector&storageMax) const override
		{
			Super::GetResourceVector(v,storageCurrent,storageMax); 
			switch (mode)
			{
				case Mode::Grow:
					v.Add(Resource::Minerals, -GrowthMineralCost);
					break;
				case Mode::Maintain:
					v.Add(Resource::CrystallineCompounds, dendritePresence);
					v.Add(Resource::OrganicCompounds, treePresence);
					break;
				case Mode::Remove:
					v.Add(Resource::CrystallineCompounds, dendritePresence*2);
					v.Add(Resource::OrganicCompounds, treePresence*2);
					break;
			}
			v.Add(Resource::Energy, -EnergyConsumption);
		}

		#ifndef NO_HIBERNATION
			virtual int				GetUplinkTraffic() const override {return 10;}
			virtual int				GetUplinkPriority() const override {return dendritePresence + treePresence;};
		#endif


		virtual String				GetTypeName() const override { return "Plantation"; };
		virtual EntityType			GetType() const override {return ConstName::Type_Plantation;}


		virtual bool				SignalEnvironmentChange(const TStructureNeighborhood&neighborhood, float atTime) override
		{
			#ifndef CLIENT
				int d2 = GetPresence(Grid::Content::
				bool linked = IsLinkedToCore();
				if (linked != isLinkedToCore)
				{
					if (linked)
					{
						this->SetEnabled(false,atTime,false);
					}
					else
						SignalChangeByExternalCause(EnableChanged,atTime);
					isLinkedToCore = linked;
				}
			#endif
			return Super::SignalEnvironmentChange(neighborhood,atTime);

		}


		virtual bool				IsMine() const {return true;}
		virtual Category			GetCategory() const override {return Category::Resources;}
	};


	
	inline PMine		ToMine(const PWorldEntity&e)
	{
		PWorkingStructure ws = ToWorkingStructure(e);
		if (!ws || !ws->IsMine())
			return PMine();
		return To<Mine>::Cast(ws);
	}
}

