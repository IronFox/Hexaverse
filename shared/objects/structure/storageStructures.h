#pragma once
#include "../structure.h"

namespace Objects
{
	namespace StorageCommon
	{
		static const int						MaxStorage=500;

		void	HandleStorageDestruction(const Grid::LCellIdI&location,const Resource::Vector&storage,float&atTime);
		float	GetUncontrolledExplosionRadius(const Resource::Vector&storage, float*damageOut, bool*doomsdayOut);
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
		Resource::Vector			storage,maxStorage;
		static const int			MaxStorage = StorageCommon::MaxStorage;

		virtual void				GetResourceVector(Resource::Vector&, Resource::Vector&storage, Resource::Vector&max) const override	{storage = this->storage; max = maxStorage;}
		virtual void				SetResourceStorage(const Resource::Vector&store)	override
		{
			this->storage = store;
			this->storage.Cap(maxStorage);
		};

		virtual bool				WouldLikeToEnable() const override {return true;}

		#ifndef CLIENT

			virtual void			ProcessLeftovers( Resource::Vector&v, index_t iteration, float&atTime ) override
			{
				if (iteration != LeftoverUpdateIteration && iteration != FinalLeftoverIteration)
					return;
				for (index_t i = 0; i < Resource::table.GetSize(); i++)
				{
					if (v[i] == 0)
						continue;
					if (v[i] < 0 )
					{
						if (storage[i] > 0)
						{
							int delta = std::min(-v[i], storage[i]);
							ChangeStorage(*this,atTime,Resource::ID(i),-delta);

							v.Add(i, delta);
							//storage[i] -= delta;
						}
					}
					else
					{
						int capacity = maxStorage[i] - storage[i];
						if (capacity > 0)
						{
							int delta = std::min(v[i], capacity);
							v.Add(i, -delta);

							ChangeStorage(*this,atTime,Resource::ID(i),delta);
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
				virtual void			DescribeType(Description&label, UINT32 descFlags, const OwnerParameter&owner) override
				{
					Super::DescribeType(label,descFlags,owner);
					if (maxStorage.IsNonZero())
					{
						label.MarkBriefEnd();
						label << "Storage structures accumulate excess production of the respective resources from the local compound until they are full."
						" Disabling a storage structure will change its priority such, that excess resources are stored here only if all other storage capacities are exhausted."
						" Likewise, resource demand will remove resources from disabled storage structures first, such that they tend towards empty."
						" Storage structures do not require "+Resource::Energy.GetTagStr()+" to operate."
						;
					}
				}
			#endif
		#endif
		#ifndef NO_HIBERNATION
			virtual int				GetUplinkTraffic() const override {return 2;}
			virtual int				GetUplinkPriority() const override {return 1000;};
		#endif
		virtual Category			GetCategory() const override {return Category::Resources;}


		//WorldEntity overrides:
		virtual float				GetMaxUncontrolledExplosionRadius() const override {return StorageCommon::GetUncontrolledExplosionRadius(maxStorage,nullptr,nullptr);}
		virtual float				GetUncontrolledExplosionRadius() const override {return StorageCommon::GetUncontrolledExplosionRadius(storage,nullptr,nullptr);}
	};




	class FuelContainer : public StorageStructure<FuelContainer>
	{
		typedef StorageStructure<FuelContainer>	Super;
		friend class Super;
		friend class WorkingStructureT<FuelContainer>;
		/**/						FuelContainer(BaseAttributeTable*table, const OwnerParameter&owner, const Grid::LCellIdI&location,float atTime) 
									: Super(table,owner,location,atTime)	{maxStorage.Set(Resource::Fuel, MaxStorage);}
		/**/						FuelContainer()	{maxStorage.Set(Resource::Fuel, MaxStorage); }	//global prototype
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
									: Super(table,owner,location,atTime)	{maxStorage.Set(Resource::Minerals, MaxStorage);}
		/**/						MineralContainer()	{maxStorage.Set(Resource::Minerals, MaxStorage); }	//global prototype
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
									: Super(table,owner,location,atTime)	{maxStorage.Set(Resource::Uranium, MaxStorage);}
		/**/						UraniumStorage()	{maxStorage.Set(Resource::Uranium, MaxStorage);}	//global prototype
	public:
		//BaseEntity overrides:
		virtual String				GetTypeName() const override { return "Uranium Storage"; };
		virtual EntityType			GetType() const override {return ConstName::Type_UraniumStorage;}
	};
	
	
	class CrystalStorage : public StorageStructure<CrystalStorage>
	{
		typedef StorageStructure<CrystalStorage>	Super;
		friend class Super;
		friend class WorkingStructureT<CrystalStorage>;
		/**/						CrystalStorage(BaseAttributeTable*table, const OwnerParameter&owner, const Grid::LCellIdI&location,float atTime) 
									: Super(table,owner,location,atTime)	{maxStorage.Set(Resource::CrystallineCompounds, MaxStorage);}
		/**/						CrystalStorage()	{maxStorage.Set(Resource::CrystallineCompounds, MaxStorage);}	//global prototype
	public:
		//BaseEntity overrides:
		virtual String				GetTypeName() const override { return "Crystal Storage"; };
		virtual EntityType			GetType() const override {return ConstName::Type_CrystalStorage;}
	};

	class OrganicStorage : public StorageStructure<OrganicStorage>
	{
		typedef StorageStructure<OrganicStorage>	Super;
		friend class Super;
		friend class WorkingStructureT<OrganicStorage>;
		/**/						OrganicStorage(BaseAttributeTable*table, const OwnerParameter&owner, const Grid::LCellIdI&location,float atTime) 
									: Super(table,owner,location,atTime)	{maxStorage.Set(Resource::OrganicCompounds, MaxStorage);}
		/**/						OrganicStorage()	{maxStorage.Set(Resource::OrganicCompounds, MaxStorage);}	//global prototype
	public:
		//BaseEntity overrides:
		virtual String				GetTypeName() const override { return "Organic Storage"; };
		virtual EntityType			GetType() const override {return ConstName::Type_OrganicStorage;}
	};
	
	class CompositeMaterialStorage : public StorageStructure<CompositeMaterialStorage>
	{
		typedef StorageStructure<CompositeMaterialStorage>	Super;
		friend class Super;
		friend class WorkingStructureT<CompositeMaterialStorage>;
		/**/						CompositeMaterialStorage(BaseAttributeTable*table, const OwnerParameter&owner, const Grid::LCellIdI&location,float atTime) 
									: Super(table,owner,location,atTime)	{maxStorage.Set(Resource::CompositeMaterials, MaxStorage);}
		/**/						CompositeMaterialStorage()	{maxStorage.Set(Resource::CompositeMaterials, MaxStorage);}	//global prototype
	public:
		//BaseEntity overrides:
		virtual String				GetTypeName() const override { return "Composite Material Storage"; };
		virtual EntityType			GetType() const override {return ConstName::Type_CompositeMaterialStorage;}
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
				maxStorage[Resource::Munitions] = MaxStorage;
			#endif
		}
		/**/						Self()
		{
			#ifndef NO_MUNITION
				maxStorage[Resource::Munitions] = MaxStorage; 
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
				maxStorage[Resource::Munitions] = LocalMunitionStorage;
			#endif
			#ifndef CLIENT
				SetEnabled(false,atTime,false,ForceEnable);
			#endif
		}
		/**/						Self()
		{
			#ifndef NO_MUNITION
				maxStorage[Resource::Munitions] = LocalMunitionStorage;
			#endif
		}	//global prototype
	protected:

	public:


		static const int			EnergyConsumption = 10,
									MineralConsumption = 10,
									MunitionOutput = 2;

		virtual void				GetResourceVector(Resource::Vector&v,Resource::Vector&storage, Resource::Vector&max) const override
		{
			v.Set(Resource::Energy, -EnergyConsumption);
			v.Set(Resource::Minerals, -MineralConsumption);
			#ifndef NO_MUNITION
				v.Set(Resource::Munitions, MunitionOutput);
			#endif
			Super::GetResourceVector(v,storage,max);
		}
		virtual Category			GetCategory() const override {return Category::Resources;}

		#ifndef NO_HIBERNATION
			virtual int				GetUplinkTraffic() const override {return 5;}
			virtual int				GetUplinkPriority() const override {return 400;};
		#endif


		virtual String				GetTypeName() const override { return "Munition Factory"; };
		virtual EntityType			GetType() const override {return ConstName::Type_MunitionFactory;}


		#ifdef HAS_WINDOW
			virtual void			DescribeType(Description&label, UINT32 descFlags, const OwnerParameter&owner) override
			{
				label.BeginBrief();
				Super::DescribeType(label,descFlags,owner);
				label.MarkBriefEnd();
				#ifdef NO_MUNITION
					label << "Enabled munition factories continuiously produce munition to supply connected offensive turrets. This is a deprecated building type."
				#else
					label << "Enabled munition factories continuiously produce "+Resource::Munitions.GetTagStr()+" to supply connected offensive turrets."
				#endif
				;
			}
		#endif
	};

}
