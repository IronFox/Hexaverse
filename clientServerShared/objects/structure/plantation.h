#pragma once
#include "storageStructures.h"
#include <objects/packable.h>

namespace Objects
{


	/**
	Grows, harvests, or removes trees/crystals
	*/
	class Plantation : public StorageStructure<Plantation,3>, public Objects::Packable
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
		static const int			MaxPlantingPerRound = 5;
		static const int			MaxHarvestPerRound = 5;

		static const int			HarvestToCompoundFactor = 2;


		static const int			CompoundStorage = 5;

		#ifdef HAS_WINDOW
			virtual void			DescribeType(Description&label, UINT32 descFlags, const OwnerParameter&owner, const T2::PConstProperties&t2) override;
		#endif

		CONSTRUCT_ENUMERATION2(Triggers, 
			PlantTrees,
			PlantCrystals);
		Buffer0<UINT32>				plantStackIDs;

		UINT32						crystalPresence=0,
									treePresence=0;
		bool						plantTrees = true,
									plantCrystals = true;

		virtual void				OnInsert(float&atTime, bool freshConstruction) override
		{
			Grid::high.GetExtendedNeighborhood(Grid::ToHigh(location).stackID,5,plantStackIDs);
			Super::OnInsert(atTime,freshConstruction);
		}

		#ifndef CLIENT
		virtual void				HandleTrigger(UINT16 triggerIndex) override
		{
			switch (triggerIndex)
			{
				case Triggers::PlantTrees:
					plantTrees = !plantTrees;
					break;
				case Triggers::PlantCrystals:
					plantCrystals = !plantCrystals;
					break;
			}
			float t = 0;
			SignalChangeByExternalCause(TasksChanged,t);
		};
		virtual void				Tick(Resource::Vector&v) override;
		#endif

		virtual bool				WouldLikeToEnable() const override {return true;}
		virtual bool				CanEnable() const override {return true;}

		virtual void				PrototypeSetup()	override;

		virtual void				GetResourceVector(Resource::Vector&v,  Resource::Vector&storageCurrent, Resource::Vector&storageMax) const override
		{
			Super::GetResourceVector(v,storageCurrent,storageMax);
			#ifdef CLIENT //set zero output on server because unpredictable
				v.Add(Resource::CrystallineCompounds, crystalPresence * HarvestToCompoundFactor);
				v.Add(Resource::OrganicCompounds, treePresence * HarvestToCompoundFactor);
			#endif
			v.Add(Resource::Energy, -EnergyConsumption);
		}

		#ifndef NO_HIBERNATION
			virtual int				GetUplinkTraffic() const override {return 10;}
			virtual int				GetUplinkPriority() const override {return dendritePresence + treePresence;};
		#endif


		virtual String				GetTypeName() const override { return "Plantation"; };
		virtual EntityType			GetType() const override {return ConstName::Type_Plantation;}

		
		virtual void				OnResourceTableUpdate() override
		{
			maxStorage.Clear();
			maxStorage.Set(Resource::CrystallineCompounds, CompoundStorage);
			maxStorage.Set(Resource::OrganicCompounds, CompoundStorage);
		};

		virtual bool				SignalEnvironmentChange(const TStructureNeighborhood&neighborhood, float atTime) override
		{
			return Super::SignalEnvironmentChange(neighborhood,atTime);
		}
		virtual Category			GetCategory() const override {return Category::Resources;}

		virtual void Pack(D::IWriteStream&s,Packable::Target target) const override;
		virtual void Unpack(D::IReadStream&s) override;
		virtual void ToSavegame(D::IWriteStream&s) const override;
		virtual void FromSavegame(D::IReadStream&s) override;

	private:
	};


	
	inline PPlantation		ToPlantation(const PWorldEntity&e)
	{
		PWorkingStructure ws = ToWorkingStructure(e);
		if (!ws)
			return PPlantation();
		return To<Plantation>::DynamicCast(ws);
	}
}

