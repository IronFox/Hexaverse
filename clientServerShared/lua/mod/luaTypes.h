#pragma once

#include <objects/structure.h>
#include <lua/luaInterface.h>
#include "../luaGameAPI.h"

namespace Lua
{
	typedef std::shared_ptr<API> PAPI;
	typedef std::weak_ptr<API> WAPI;



	
	DECLARE_POINTER_CLASS(RootType);
	DECLARE_POINTER_CLASS(StructureType);
	DECLARE_POINTER_CLASS(WorkingStructureType);

	class RootType
	{
	public:
		const String	name;
		const Objects::EntityType		type;
		int height;
		bool isBroad;
		WAPI api;
		Lua::Table self;
		Lua::Function<Lua::Reference(Lua::Table,Lua::Table,Lua::Table)> instantiateFunction;
		Lua::Function<void(Lua::Table)> destroyFunction;
		Lua::Function<void(Lua::Table)> onSelect;
		Lua::Function<void(Lua::Table)> onDeselect;
		Lua::Function<Lua::Table(Lua::Reference)> getCommands;
		String briefDescription, textDescription;

		template<typename F>
		void	RequireFunction(const char*name, Lua::Function<F>&targetFunction) const
		{
			targetFunction = self.GetField(name);
			if (!targetFunction)
				throw Lua::Exception(CLOCATION,"Missing function '"+String(this->name)+"."+name+"'");
		}

		/**/	RootType(const PAPI&api, const Objects::EntityType&type, const String&name, const Lua::Table&table);

		Lua::Reference Instantiate(const Objects::WorldEntity*entity) const;

		void OnDestroy(const Lua::Reference&ref) const
		{
			if (destroyFunction)
				Try([&ref,this](){destroyFunction(ref);});
		}

		void OnSelect(const Lua::Reference&ref) const
		{
			if (onSelect)
				Try([&ref,this](){onSelect(ref);});
		}

		void OnDeselect(const Lua::Reference&ref) const
		{
			if (onDeselect)
				Try([&ref,this](){onDeselect(ref);});
		}

		template <typename F>
		void Try(const F&f) const
		{
			try
			{
				return f();
			}
			catch (const std::exception&ex)
			{
				LogEvent(EventClass::Preset::scriptError,ex.what());
			}
		}

		template <typename F>
		void TryWithAPI(const F&f) const
		{
			try
			{
				auto api = this->api.lock();
				if (!api)
				{
					LogEvent(EventClass::Preset::scriptError,"Executing Lua operation on scripted type '"+name+"' failed because reference to API is lost");
					return;
				}

				f(*api);
			}
			catch (const std::exception&ex)
			{
				LogEvent(EventClass::Preset::scriptError,ex.what());
			}
		}


		const Objects::CC* GetCommands(const Lua::Reference&instance, Objects::CC&dynamicStorage) const;

	};


	class StructureType : public RootType
	{
		typedef RootType	Super;
	public:
		#ifdef HAS_WINDOW
			/**
			Build-ready, object-space instruction to be used for all instances of this structure type
			*/
			Objects::PClientAttachment	shape;
			/**
			Intersectable, object-space geometry of all instances of this structure type
			*/
			Objects::Geometry			geometry;
		#endif

		bool							hasFoundation;
		Objects::Structure::Category	category;
		Lua::Function<void(Lua::Reference,Lua::Table)> onLostNeighbor;
		Lua::Function<void(Lua::Reference,Lua::Table,Lua::Table)> onNewConnection;
		Lua::Function<void(Lua::Reference,Lua::Table,Lua::Table)> onConnectionLost;
		Lua::Function<bool(Lua::Reference,Lua::Table,Lua::Table)> connectionPossibleTo;
		Lua::Function<void(Lua::Reference,Lua::Table)> onEnvironmentChange;


		/**/	StructureType(const PAPI&api, const Objects::EntityType&type, const String&name, const Lua::Table&table);






		void OnLostNeighbor(const Lua::Reference&instance, const Objects::PStructure&s) const;
		void OnNewConnection(const Lua::Reference&instance, const Objects::PStructure&s, UINT32 acrossHighCell) const;
		void OnConnectionLost(const Lua::Reference&instance, const Objects::PStructure&s, UINT32 acrossHighCell) const;
		bool ConnectionPossibleTo(const Lua::Reference&instance, const Objects::PStructure&s, UINT32 acrossHighCell) const;
		void OnEnvironmentChange(const Lua::Reference&instance, const Objects::TStructureNeighborhood&neighborhood) const;

	};

	class WorkingStructureType : public StructureType
	{
		typedef StructureType Super;

		Lua::Function<bool(Lua::Table)> canEnable;
		Lua::Function<bool(Lua::Table, Lua::Table)> canProduce;
		Lua::Function<bool(Lua::Table)> wouldLikeToEnable;
		Lua::Function<bool(Lua::Table, Lua::Table, Lua::Table, Lua::Table)> getResourceVector;
		Lua::Function<void(Lua::Table, Lua::Table)> setResourceStorage;
		Lua::Function<bool(Lua::Table)> awaitsUserInteraction;
	public:
		UINT32	mobileProductionPerRound;
		bool	isMobileEntityProducer;



		bool CanEnable(const Lua::Reference&ref)	const
		{
			bool rs = false;
			Try([&ref,&rs,this](){rs = canEnable(ref);});
			return rs;
		}

		bool WouldLikeToEnable(const Lua::Reference&ref)	const
		{
			bool rs = false;
			if (wouldLikeToEnable)
				Try([&ref,&rs,this](){rs = wouldLikeToEnable(ref);});
			return rs;
		}
		bool CanProduce(const Lua::Reference&ref,const Objects::PWorldEntity&e) const;
		bool AwaitsUserInteraction(const Lua::Reference&ref) const
		{
			bool rs = false;
			if (awaitsUserInteraction)
				Try([&ref,&rs,this](){rs = awaitsUserInteraction(ref);});
			return rs;
		}

		void GetResourceVector(const Lua::Reference&instance, Objects::Resource::Vector&balance, Objects::Resource::Vector&storageCurrent, Objects::Resource::Vector&storageMax) const;
		void SetResourceStorage(const Lua::Reference&instance, const Objects::Resource::Vector&vec) const;

		/**/	WorkingStructureType(const PAPI&api, const Objects::EntityType&type, const String&name, const Lua::Table&sourceTable):Super(api,type,name,sourceTable)
		{
			isMobileEntityProducer = false;	//placeholder
			mobileProductionPerRound = 1;
			RequireFunction("CanEnable",canEnable);
			wouldLikeToEnable = self.GetField("WouldLikeToEnable");
			awaitsUserInteraction = self.GetField("AwaitsUserInteraction");
			RequireFunction("GetResourceVector",getResourceVector);
			RequireFunction("SetResourceStorage",setResourceStorage);
		}



	};


	class WorkingStructure : public Objects::WorkingStructure, Objects::MobileEntityProducer
	{
		typedef Objects::WorkingStructure Super;
	public:
		Lua::Reference instance;

		#ifdef HAS_WINDOW
			Objects::CommandContainer	commands;
		#endif


		/**/	WorkingStructure(const PWorkingStructureType&type):Objects::MobileEntityProducer(type->mobileProductionPerRound),type(type)
		{
			#ifdef HAS_WINDOW
				ASSERT__(type->shape);
				Super::shape = type->shape;
			#endif
		}

		/**/	WorkingStructure(const PWorkingStructureType&type, const Objects::PBaseAttributeTable&table, const Objects::OwnerParameter&owner, const Grid::LCellIdI&location, float atTime):
			Super(table,owner,location,true,atTime),
			Objects::MobileEntityProducer(type->mobileProductionPerRound),
			type(type)
		{
			#ifdef HAS_WINDOW
				ASSERT__(type->shape);
				Super::shape = type->shape;
			#endif
		}
		
		#ifdef HAS_WINDOW
			virtual bool				TestIntersection(const M::TVec3<>&b, const M::TVec3<>&d, float & distance) const override {return Super::TestIntersection(type->geometry,b,d,distance);}
			virtual const Objects::CC*	GetCommands(Objects::CC&dynamicStorage) const override {return type->GetCommands(instance,dynamicStorage);}
		#endif




		//BaseEntity overrides:
		virtual String					GetTypeName() const override { return type->name; };
		virtual Objects::EntityType		GetType() const override {return type->type;}


		//WorldEntity overrides:
		virtual Objects::PWorldEntity		CreateNewInstance(const Objects::OwnerParameter&owner, const Grid::LCellIdI&location, float atTime) override
		{
			auto rs = new WorkingStructure(type, table, owner,location,atTime);
			return Objects::PWorldEntity(rs);
		}

		virtual void					OnDestroy(Objects::RemovalType type, float&atTime) override
		{
			Super::OnDestroy(type,atTime);
			if (instance)
			{
				if (type != Objects::RemovalType::EnteredStorage)
					this->type->OnDestroy(instance);
				instance = Lua::nil;
			}
		};	

		virtual void					OnInsert(float&atTime, bool freshConstruction) override
		{
			Super::OnInsert(atTime,freshConstruction);

			instance = type->Instantiate(this);
		}

		virtual int						GetHeight() const override {return type->height;}
		virtual bool					AwaitsUserInteraction() const {return type->AwaitsUserInteraction(instance);}

		#ifndef CLIENT
			virtual void			OnSleepChange(float&t) override;
			virtual void			OnEnabledChange() override;

			virtual void			Tick(Resource::Vector&) override;
		#endif
		#ifdef HAS_WINDOW
			virtual void			DescribeType(Objects::Description&label, UINT32 descFlags, const Objects::OwnerParameter&owner, const Objects::T2::PConstProperties&t2) override
			{
				label.BeginBrief();
				Super::DescribeType(label,descFlags, owner, t2);

				Array<StringRef> lines;
				Explode('\n', type->briefDescription,lines);
				foreach (lines,line)
					label << line->Trim();
				//label << " Max Vehicles per Store: "+String(MaxVehiclesPerStore);
				if ((descFlags & DescExcludeExtended) == 0)
				{
					label.MarkBriefEnd();
					Explode('\n', type->textDescription,lines);
					foreach (lines,line)
						label << line->Trim();
				}
			}
			virtual void			OnSelect()	override
			{
				type->OnSelect(instance);
			}
			virtual void			OnDeselect() override
			{
				type->OnDeselect(instance);
			}
		#endif
		virtual const Objects::MobileEntityProducer*CastToProducer() const override {return type->isMobileEntityProducer ? this : nullptr;}

		//MobileEntityProducer overrides
		#ifndef CLIENT
			virtual void			SignalNewProductionEnqueued() override {float t = 0; if (type->isMobileEntityProducer) SetEnabled(true,t);}
		#endif
		virtual bool				CanProduce(const Objects::PWorldEntity&e) const override {return type->CanProduce(instance,e);}


		//Structure overrides:
		virtual bool				IsBuilding(std::pair<int,int>&outProgress) const override
		{
			return type->isMobileEntityProducer && MobileEntityProducer::IsBuilding(outProgress);
		}
		virtual Category			GetCategory() const override {return type->category;}
		virtual int					GetBroadHeight() const {return type->isBroad ? GetHeight() : 0;}
		virtual bool				HasFoundation() const override {return type->hasFoundation;}

		virtual void				SignalLostNeighbor(const Objects::PStructure&s, float&atTime) override {Super::SignalLostNeighbor(s,atTime); type->OnLostNeighbor(instance,s);}
		virtual void				OnNewConnection(const Objects::PStructure&s, UINT32 acrossHighCell, float atTime) override {Super::OnNewConnection(s,acrossHighCell,atTime); type->OnNewConnection(instance,s,acrossHighCell);}
		virtual void				OnConnectionLost(const Objects::PStructure&s, UINT32 acrossHighCell, float atTime) override {Super::OnConnectionLost(s,acrossHighCell,atTime); type->OnConnectionLost(instance,s,acrossHighCell);}
		virtual void				SignalEnvironmentChange(const Objects::TStructureNeighborhood&neighborhood, float atTime) override {Super::SignalEnvironmentChange(neighborhood,atTime); type->OnEnvironmentChange(instance,neighborhood); }
		virtual bool				ConnectionPossibleTo(const Objects::PStructure&other, UINT32 acrossHighCell) override {return Super::ConnectionPossibleTo(other,acrossHighCell) && type->ConnectionPossibleTo(instance,other,acrossHighCell);}
		virtual placement_t			CheckPlacement(const Grid::LCellIdI&location) override
		{
			return CommonCheckStructurePlacement(location,GetBroadHeight(), GetHeight());
		}


		//WorkingStructure overrides:
		virtual bool				WouldLikeToEnable() const {return type->WouldLikeToEnable(instance);}
		virtual bool				CanEnable() const	{return type->CanEnable(instance);}
		virtual void				GetResourceVector(Objects::Resource::Vector&balance, Objects::Resource::Vector&storageCurrent, Objects::Resource::Vector&storageMax) const override {type->GetResourceVector(instance,balance,storageCurrent,storageMax);}
		virtual void				SetResourceStorage(const Objects::Resource::Vector&vec)	override {type->SetResourceStorage(instance,vec);}

	private:
		const PWorkingStructureType		type;
	};

	

}

