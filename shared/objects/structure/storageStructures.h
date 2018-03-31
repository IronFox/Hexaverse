#pragma once
#include "../structure.h"

namespace Objects
{
	namespace StorageCommon
	{
		static const int						MaxStorage=500;

		void	HandleStorageDestruction(const Grid::LCellIdI&location,const ResourceVector&storage,float&atTime);
	}


	template <class Derivative, index_t LeftoverUpdateIteration=2>
	class StorageStructure : public WorkingStructureT<Derivative>
	{
	protected:
		typedef WorkingStructureT<Derivative>			Super;
		friend class Super;
		/**/						StorageStructure(BaseAttributeTable*table, const OwnerParameter&owner, const Grid::LCellIdI&location, float atTime) : Super(table,owner,location,atTime)	{}
		/**/						StorageStructure()	{ }	//global prototype

	public:
		ResourceVector				storage,maxStorage;
		static const int			MaxStorage = StorageCommon::MaxStorage;

		virtual void				GetResourceVector(ResourceVector&, ResourceVector&storage, ResourceVector&max) const override	{storage = this->storage; max = maxStorage;}
		virtual void				SetResourceStorage(const ResourceVector&store)	override
		{
			this->storage = store;
			this->storage.Cap(maxStorage);
		};

		virtual bool				WouldLikeToEnable() const override {return true;}

		#ifndef CLIENT

			virtual void			ProcessLeftovers(ResourceVector&v, index_t iteration, float atTime) override
			{
				if (iteration != LeftoverUpdateIteration && iteration != FinalLeftoverIteration)
					return;
				for (index_t i = 0; i < Resource::NumberOfPossibilities; i++)
				{
					if (v.level[i] == 0)
						continue;
					if (v.level[i] < 0 )
					{
						if (storage.level[i] > 0)
						{
							int delta = std::min(-v.level[i], storage.level[i]);
							ChangeStorage(*this,atTime,Resource::Reinterpret(i),-delta);

							v.level[i] += delta;
							//storage.level[i] -= delta;
						}
					}
					else
					{
						int capacity = maxStorage.level[i] - storage.level[i];
						if (capacity > 0)
						{
							int delta = std::min(v.level[i], capacity);
							v.level[i] -= delta;

							ChangeStorage(*this,atTime,Resource::Reinterpret(i),delta);
						}	
					}
				}
			}
			//WorldEntity overrides:
			virtual void		OnDestroy(RemovalType t, float&atTime) override
			{
				Super::OnDestroy(t,atTime);
				if (t == RemovalType::UncontrolledExplosion)
					StorageCommon::HandleStorageDestruction(location,storage,atTime);
			}

		#else
			#ifdef HAS_WINDOW
				virtual void			DescribeType(Description&label, UINT32 descFlags) override
				{
					Super::DescribeType(label,descFlags);
					if (maxStorage.IsNotEmpty())
					{
						label.MarkBriefEnd();
						label << "Storage structures accumulate excess production of the respective resources from the local compound until they are full."
						" Disabling a storage structure will change its priority such, that excess resources are stored here only if all other storage capacities are exhausted."
						" Likewise, resource demand will remove resources from disabled storage structures first, such that they tend towards empty."
						" Storage structures do not require "+String(ResourceTag(Resource::Energy))+" to operate."
						;
					}
				}
			#endif
		#endif
		//virtual int					GetConstructionCost() const override {return 10;}
		#ifndef NO_HIBERNATION
			virtual int				GetUplinkTraffic() const override {return 2;}
			virtual int				GetUplinkPriority() const override {return 1000;};
		#endif
		virtual Category			GetCategory() const override {return Category::Resources;}


	};




	class FuelContainer : public StorageStructure<FuelContainer>
	{
		typedef StorageStructure<FuelContainer>	Super;
		friend class Super;
		friend class WorkingStructureT<FuelContainer>;
		/**/						FuelContainer(BaseAttributeTable*table, const OwnerParameter&owner, const Grid::LCellIdI&location,float atTime) 
									: Super(table,owner,location,atTime)	{maxStorage.level[Resource::Fuel] = MaxStorage;}
		/**/						FuelContainer()	{maxStorage.level[Resource::Fuel] = MaxStorage; }	//global prototype
	public:
		//BaseEntity overrides:
		virtual String				GetTypeName() const override { return "Fuel Container"; };
		virtual EntityType			GetType() const override {return ConstName::Type_FuelContainer;}
	};


	class MineralContainer : public StorageStructure<MineralContainer>
	{
		typedef StorageStructure<MineralContainer>	Super;
		friend class Super;
		friend class WorkingStructureT<MineralContainer>;
		/**/						MineralContainer(BaseAttributeTable*table, const OwnerParameter&owner, const Grid::LCellIdI&location,float atTime) 
									: Super(table,owner,location,atTime)	{maxStorage.level[Resource::Minerals] = MaxStorage;}
		/**/						MineralContainer()	{maxStorage.level[Resource::Minerals] = MaxStorage; }	//global prototype
	public:
		//BaseEntity overrides:
		virtual String				GetTypeName() const override { return "Mineral Container"; };
		virtual EntityType			GetType() const override {return ConstName::Type_MineralContainer;}
	};


	class UraniumStorage : public StorageStructure<UraniumStorage>
	{
		typedef StorageStructure<UraniumStorage>	Super;
		friend class Super;
		friend class WorkingStructureT<UraniumStorage>;
		/**/						UraniumStorage(BaseAttributeTable*table, const OwnerParameter&owner, const Grid::LCellIdI&location,float atTime) 
									: Super(table,owner,location,atTime)	{maxStorage.level[Resource::Uranium] = MaxStorage;}
		/**/						UraniumStorage()	{maxStorage.level[Resource::Uranium] = MaxStorage; }	//global prototype
	public:
		//BaseEntity overrides:
		virtual String				GetTypeName() const override { return "Uranium Storage"; };
		virtual EntityType			GetType() const override {return ConstName::Type_UraniumStorage;}
	};

	
	class MunitionStorage : public StorageStructure<MunitionStorage>
	{
		typedef MunitionStorage		Self;

		typedef StorageStructure<Self>	Super;
		friend class Super;
		friend class WorkingStructureT<Self>;
		/**/						Self(BaseAttributeTable*table, const OwnerParameter&owner, const Grid::LCellIdI&location,float atTime) 
									: Super(table,owner,location,atTime)	
		{
			#ifndef NO_MUNITION
				maxStorage.level[Resource::Munitions] = MaxStorage;
			#endif
		}
		/**/						Self()
		{
			#ifndef NO_MUNITION
				maxStorage.level[Resource::Munitions] = MaxStorage; 
			#endif
		}	//global prototype
	public:
		//BaseEntity overrides:
		virtual String				GetTypeName() const override { return "Munition Storage"; };
		virtual EntityType			GetType() const override {return ConstName::Type_MunitionStorage;}
	};

	

	class MunitionFactory : public StorageStructure<MunitionFactory,3>
	{
		typedef MunitionFactory		Self;
		typedef StorageStructure<Self,3>	Super;
		friend class Super;
		friend class WorkingStructureT<Self>;


		static const int LocalMunitionStorage = 4;
		/**/						Self(BaseAttributeTable*table, const OwnerParameter&owner, const Grid::LCellIdI&location, float atTime) 
									: Super(table,owner,location,atTime)
		{
			#ifndef NO_MUNITION
				maxStorage.level[Resource::Munitions] = LocalMunitionStorage;
			#endif
			#ifndef CLIENT
				SetEnabled(false,atTime,false,ForceEnable);
			#endif
		}
		/**/						Self()
		{
			#ifndef NO_MUNITION
				maxStorage.level[Resource::Munitions] = LocalMunitionStorage;
			#endif
		}	//global prototype
	protected:

	public:


		static const int			EnergyConsumption = 10,
									MineralConsumption = 10,
									MunitionOutput = 2;

		virtual void				GetResourceVector(ResourceVector&v,ResourceVector&storage, ResourceVector&max) const override
		{
			v.level[Resource::Energy] = -EnergyConsumption; 
			v.level[Resource::Minerals] = -MineralConsumption; 
			#ifndef NO_MUNITION
				v.level[Resource::Munitions] = MunitionOutput; 
			#endif
			Super::GetResourceVector(v,storage,max);
		}
		virtual Category			GetCategory() const override {return Category::Resources;}

		//virtual int					GetConstructionCost() const override {return 40;}
		#ifndef NO_HIBERNATION
			virtual int				GetUplinkTraffic() const override {return 5;}
			virtual int				GetUplinkPriority() const override {return 400;};
		#endif


		virtual String				GetTypeName() const override { return "Munition Factory"; };
		virtual EntityType			GetType() const override {return ConstName::Type_MunitionFactory;}


		#ifdef HAS_WINDOW
			virtual void			DescribeType(Description&label, UINT32 descFlags) override
			{
				label.BeginBrief();
				Super::DescribeType(label,descFlags);
				label.MarkBriefEnd();
				#ifdef NO_MUNITION
					label << "Enabled munition factories continuiously produce munition to supply connected offensive turrets. This is a deprecated building type."
				#else
					label << "Enabled munition factories continuiously produce "+String(ResourceTag(Resource::Munitions))+" to supply connected offensive turrets."
				#endif
				;
			}
		#endif
	};

}
