#pragma once

#include "../structure.h"

namespace Objects
{

	/**
	Vehicle storage class.
	Instances of this class can permanently store vehicles
	*/
	class VehicleStore : public OtherStructure<VehicleStore>, public Hangar
	{
	public:
		struct Park;
		typedef std::shared_ptr<Park>	PPark;
	private:
		typedef OtherStructure<VehicleStore>		Super;

		/**
		Store-neigbors - directly adjacent VehicleStore instances
		*/
		Buffer0<WVehicleStore>		sneighbors;

		

		void						PropagatePark(const PPark&compound);
		void						MergePark(const PPark&other);



		friend class Super;
		/**/						VehicleStore() {}
		/**/						VehicleStore(BaseAttributeTable*table, const OwnerParameter&owner, const Grid::LCellIdI&location, float atTime);
	public:

		/**
		Shared storage state. Neighboring (also indirectly) VehicleStore instances
		share one Park instance. It tracks Park membership and the number of vehicles currently checked in.
		*/
		struct Park
		{
			Buffer0<WVehicleStore>	members;	//!< All VehicleStore instances that share this Park
			count_t					vehiclesStored = 0;	//!< Total number of vehicles stored in this park

			//TUniqueID				id;

			/**/					Park()
			{
				//id.Generate();
			}


			/**
			Iteratively checks whether or not the local Park instance contains a particular VehicleStore instance
			*/
			bool					Contains(const PVehicleStore&s)	const
			{
				for (const auto&w : members)
					if (w.lock() == s)
						return true;
				return false;
			}
		};


		PPark						park;	//!< Shared state. Should not be set if the local VehicleStore is not connected to any other VehicleStore instances
		#ifdef HAS_WINDOW
			static Geometry			interConnector;


			virtual const Geometry*	GetSourceForConnection(const PStructure&connected)	override {return To<VehicleStore>::DynamicCast(connected) && connected->location.height == location.height ? &interConnector : &standardConnector;}

		#endif
		/**
		Maximum number of vehicles that can be checked into one instance of the local class.
		Neighboring VehicleStores share free slots, and can move vehicles in zero-time (ATM)
		*/
		static const count_t		MaxVehiclesPerStore = 4;



		virtual count_t				GetLoad() const override {return park? park->vehiclesStored : stored.Count();}
		count_t						GetParkVehicleCapacity() const {return park ? park->members.Count() * MaxVehiclesPerStore : MaxVehiclesPerStore;}



		#ifndef CLIENT
			bool					StoreInPark(const PMobileEntity&v, float&atTime);
		#else
			void					Lose(const PVehicle&v);
		#endif



		/**
		Checks if the local structure (not the park) is full (i.e. cannot store any additional vehicles)
		*/
		bool						LocalIsFull() const {return stored.Count() >= MaxVehiclesPerStore;}


		//Hangar overrides:
		virtual bool				CanStoreType(const MobileEntity&e) const override;
		virtual void				StoreLocallyFromImport(const PMobileEntity&e) override;
		#ifndef CLIENT
			virtual bool			Store(const PMobileEntity&e, float&atTime) override {return e && CanStoreType(*e) && StoreInPark(e,atTime);}
		#else
			virtual void			Lose(const PMobileEntity&v)	override;
		#endif
		virtual bool				SharesStorageWith(const PHangar&other)	const override;
		virtual void				LocalCheckout(const PMobileEntity&v, const Grid::LCellIdI&targetLocation, float&atTime) override;
		virtual count_t				GetMaxStorage() const override {return MaxVehiclesPerStore;}
		const WorldEntity&			ToEntity() const override {return *this;}
		WorldEntity&				ToEntity() override {return *this;}
		virtual void				OnDestroy(RemovalType, float&atTime) override;

		//Structure overrides:
		virtual void				OnNewConnection(const PStructure&, UINT32 acrossHighCell, float atTime) override;
		virtual void				OnConnectionLost(const PStructure&, UINT32 acrossHighCell, float atTime) override;
		virtual PHangar				CastToDestinationForType(const MobileEntity&e) override;
		virtual Category			GetCategory() const override {return Category::Miscellaneous;}


		//WorldEntity overrides:
		virtual bool				IsHangar() const {return true;}
		virtual String				GetTypeName() const override { return "Vehicle Store"; };
		virtual ConstName			GetCTypeName() const override {return ConstName::Type_VehicleStore;}
		virtual int					GetHeight() const override {return 2;}
		virtual void				PrototypeSetup() override;
		//virtual int					GetConstructionCost() const override {return 40;}
		#ifndef NO_HIBERNATION
			virtual int				GetUplinkTraffic() const override {return 2;}
		#endif
		#ifdef HAS_WINDOW
			virtual void			VisitAllCommands(FCommandVisitor visitor, bool force) override;
			virtual void			DescribeType(Description&label, UINT32 descFlags) override
			{
				label.BeginBrief();
				Super::DescribeType(label,descFlags);
				label << " Max Vehicles per Store: "+String(MaxVehiclesPerStore);
				label.MarkBriefEnd();
				label << "Vehicle stores can be used to store vehicles which will then enter hibernation until deployed. This structure shares storage with nearby structures of the same type."
									" A vehicle store can deploy any vehicle found within the vehicle store compound.";
			}
		#endif


	};
	
}

