#pragma once

#include "../structure.h"


namespace Objects
{
	namespace Prototypes
	{
		void	RegisterAll();
	}


	/**
	Stationary defence building
	All towers feature turrets on their top, which define their functionality. Towers and satellites are the only
	WorldEntity's that can fire automatically during movement phase. They are therefore particularly effective against approaching
	vehicles, and must typically be disabled via EMP first.
	*/
	class Tower : public WorkingStructure, public TurretBase
	{
		friend class Prototype;
	public:
		class Prototype
		{
		public:
			Component::Turret		*const turret;
			PTower					instance;
			#ifdef HAS_WINDOW
				CommandContainer	commands;
			#endif

				/**/					Prototype(Component::Turret*t):turret(t)
				{
					instance.reset(new Tower(this));
				}
		};
		typedef std::shared_ptr<Prototype> PProto;

		static Ctr::Buffer0<PProto>	globalPrototypes;
		
		const Prototype*const		prototype;

		ResourceVector				storage;


		struct AttackTask
		{
			//TUniqueID				id;
			Grid::LCellIdI			target;

			#ifdef CLIENT
				PClientAttachment	visualization;
			#else
				bool				autoTarget;
			#endif

		};


		static float				foundationHeight;	//unscaled height of the base geometry, and vertical offset to position turret
		static float				GetEffectiveFoundationHeight() {return foundationHeight * Grid::UnitScale;}



		float						attackActionLeft=1.f;

		#ifdef CLIENT
			Buffer0<AttackTask>			attackQueue;

			#ifdef HAS_WINDOW
				/**
				Render-ready, object-space instruction to be used for all instances of this structure type
				*/
				static PClientAttachment	staticShape;
				/**
				Intersectable, object-space geometry of all instances of this structure type
				*/
				static Geometry			geometry;
			#endif

			AttackTask&				AppendTask(const Grid::LCellIdI&attackTarget);
		#else
			Queue<AttackTask>		attackQueue;


			void					BeginAttackPhase()	{}

			bool					ClearTasks();
			bool					DropLastTask();

			bool					IsIdleButCouldAttack(float atTime) {return attackQueue.IsEmpty() && CanAttack();}

		#endif




		#ifndef ONE_COMPOUND_PER_FACTION
			ResourceVector			GetMaxStorageCapacity()	const	{return turret->baseStorageCapacity * 2;};
		#else
			ResourceVector			GetMaxStorageCapacity()	const	{return ResourceVector();};
		#endif




		//WorkingStructure overrides:
		virtual void				GetResourceVector(ResourceVector&, ResourceVector&storage, ResourceVector&max) const override	{storage = this->storage; max = GetMaxStorageCapacity();}
		#ifndef CLIENT
			virtual void			ProcessLeftovers(ResourceVector&v, index_t iteration, float atTime) override;
		#endif
		virtual void				SetResourceStorage(const ResourceVector&storage)override
		{
			this->storage = storage;
			this->storage.Cap(this->GetMaxStorageCapacity());
		};

		//Structure overrides:
		virtual bool				ConnectionPossibleTo(const PStructure&other, UINT32 acrossHighCell)	override	{return true;}
		virtual placement_t			CheckPlacement(const Grid::LCellIdI&location) override { return CommonCheckStructurePlacement(location,2,2); }
		virtual int					GetHeight() const override {return 2;}

		//BaseEntity overrides:
		virtual String				GetTypeName() const override { return turret->typeName+" Tower"; };
		virtual EntityType			GetType() const override {DBG_ASSERT__(turret && turret->constName != ConstName::Empty); return EntityType(ConstName::Type_Tower, turret->constName);}

		//WorldEntity overrides:
		#ifndef CLIENT
			virtual void			ClearSafeTasks() override	{this->ClearTasks();};
		#endif
		virtual bool				IsTower() const override {return true;}
		virtual float3				GetScanningCenter(const Grid::LCellIdI&loc) const override;
		virtual void				VisitSensors(const Grid::LCellIdI&loc, const std::function<void(const World::Sensor&)>&visitor, bool appearance = false) const override;
		virtual bool				IsSentry() const override {return true;}
		virtual void				PrototypeSetup()	override;
		#ifdef HAS_WINDOW
			virtual const CC*		GetCommands(CC&dynamicStorage) const override {return &prototype->commands;}
			virtual bool			TestIntersection(const M::TVec3<>&b, const M::TVec3<>&d, float & distance) const override;
			virtual void			DescribeType(Description&, UINT32 descFlags) override;
		#endif
		#ifndef CLIENT
			virtual void			OnInsert(float&atTime, bool freshConstruction) override;
			virtual bool			DirectTo(CursorMode, MutableArrayRef<Grid::LCellIdI> waypoints, bool detailedPath, bool leftClick = false) override;
		#endif
		//virtual int					GetConstructionCost() const override {return this->GetTable()->constructionCost.getValue(GetResearchState()) + turret->productionCosts(owner);}
		#ifndef NO_HIBERNATION
			virtual int				GetUplinkTraffic() const override {return turret->uplinkTraffic;}
			virtual int				GetUplinkPriority() const override {return 1000;}
		#endif
		virtual PWorldEntity		CreateNewInstance(const OwnerParameter&owner, const Grid::LCellIdI&location, float atTime) override {return PWorldEntity(new Tower(table,prototype,owner,location,atTime));};
		virtual PTurretBase			GetTurretBase(float&zOffset) override { zOffset = GetEffectiveFoundationHeight(); return To<Tower>::Cast(shared_from_this());}
		virtual PConstTurretBase	GetTurretBase(float&zOffset) const override { zOffset = GetEffectiveFoundationHeight(); return To<const Tower>::Cast(shared_from_this());}

		//Structure overrides:
		virtual int					GetBroadHeight() const override {return std::max(1,GetHeight()-1);}
		virtual Category			GetCategory() const override {return Category::Defense;}

		//TurretBase overrides:
		virtual OwnerParameter		GetOwner() const override {return owner;};
		virtual bool				NextTaskIsAttack() const override {return IsAlive() && attackQueue.IsNotEmpty();}
		virtual float				GetAttackActionLeft() const override {return attackActionLeft;};
		virtual float				GetFlatAttackRange() const override;
		virtual String				DescribeFlatAttackRange() const override;
		virtual bool				HasAmmoForShot() override;
		virtual float				GetAttackExplosionRadius() const override;
		virtual WorldEntity&		ToEntity() override {return *this;}
		virtual const WorldEntity&	ToEntity() const override {return *this;}
		virtual float				GetZOffset() const override {return GetEffectiveFoundationHeight();}
		virtual bool				AttacksMovingHostilesDuringMotionPhase() const {return true;}
		virtual int					GetOperationSpeed(Component::operation_t) const override;
		virtual String				DescribeOperationSpeed(Component::operation_t) const override;

		#ifndef CLIENT
			virtual void			FlushAutoAttacks() override;
			virtual bool			AttackNext(const Grid::LCellIdI&target, bool autoTarget) override;
			virtual void			Attack(float&)	override;
			virtual bool			FrontQueueAutoAttack(TurretBase::TargetChoice&lastChoice) override;
		#endif

	private:
		static bool					setup;
		typedef WorkingStructure	Super;
		friend class Super;
		friend void Prototypes::RegisterAll();
		//friend class WorkingStructureT<Tower>;
		/**/						Tower(const Prototype*proto) : TurretBase(proto->turret),prototype(proto)
		{
			#ifdef HAS_WINDOW
				Super::shape = staticShape;
			#endif
		}
		/**/						Tower(BaseAttributeTable*table, const Prototype*proto, const OwnerParameter&owner, const Grid::LCellIdI&location, float time) 
									: Super(table,owner,location,true,time),TurretBase(proto->turret),prototype(proto)
		{
			#ifdef HAS_WINDOW
				Super::shape = staticShape;
			#endif
		}

		//WorkingEntity overrides (private):
		virtual bool				WouldLikeToEnable() const override {return true;}

	};



	inline PTower		ToTower(const PWorldEntity&e)
	{
		PWorkingStructure ws = ToWorkingStructure(e);
		if (!ws || !ws->IsTower())
			return PTower();
		return To<Tower>::Cast(ws);
	}	
}
