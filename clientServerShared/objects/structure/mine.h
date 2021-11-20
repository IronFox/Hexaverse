#pragma once
#include "storageStructures.h"

namespace Objects
{

	
	class Mine : public StorageStructure<Mine,3>
	{
	private:
		typedef StorageStructure<Mine,3>			Super;


		friend class WorkingStructureT<Mine>;
		/**/						Mine(BaseAttributeTable*table, const OwnerParameter&owner, const Grid::LCellIdI&location, float atTime) 
									: Super(table,owner,location,atTime)	{maxStorage.Set(Resource::Minerals, MineralStorage); isLinkedToCore = LinkToCore(atTime);}
		/**/						Mine()	{
			maxStorage.Set(Resource::Minerals, MineralStorage);
		}	//global prototype

		bool						isLinkedToCore;
		


	public:
		static const int			EnergyConsumption = 10;
		#ifndef ONE_COMPOUND_PER_FACTION
			static const int		MineralStorage = 10;
		#else
			static const int		MineralStorage = 0;
		#endif

		bool						LinkToCore(float atTime);
		bool						IsLinkedToCore()	const;

		#ifdef CLIENT
			void					SetLinkedToCore(bool, bool controlEnabled);
			#ifdef HAS_WINDOW
				virtual void		DescribeType(Description&label, UINT32 descFlags, const OwnerParameter&owner, const T2::PConstProperties&t2) override;
			#endif

		#endif

		Resource::CoreVector		naturalResources;

		float						GetEfficiency() const {return 1.f;/*location.height <= -8 ? 2.f : 1.f;*/}


		virtual bool				WouldLikeToEnable() const override {return true;}
		virtual bool				CanEnable() const override {return IsLinkedToCore();}

		//void						SetNaturalResourceVector(const Resource::CoreVector&res)	{naturalResources = res;}

		virtual void				PrototypeSetup()	override;

		virtual void				GetResourceVector(Resource::Vector&v,  Resource::Vector&storageCurrent, Resource::Vector&storageMax) const override
									{
										Super::GetResourceVector(v,storageCurrent,storageMax); 
										v += naturalResources; 
										v.Add(Resource::Energy, -EnergyConsumption);
									}

		#ifndef NO_HIBERNATION
			virtual int				GetUplinkTraffic() const override {return 10;}
			virtual int				GetUplinkPriority() const override {return naturalResources.fuel*5 + naturalResources.minerals + naturalResources.uranium;};
		#endif


		virtual String				GetTypeName() const override { return "Mine"; };
		virtual EntityType			GetType() const override {return ConstName::Type_Mine;}


		virtual void				SignalEnvironmentChange(const TStructureNeighborhood&neighborhood, float atTime) override
		{
			#ifndef CLIENT
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
			Super::SignalEnvironmentChange(neighborhood,atTime);

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

