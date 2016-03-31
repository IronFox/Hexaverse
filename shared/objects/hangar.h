#ifndef objectHangarStructureH
#define objectHangarStructureH


#include "types.h"
#include "../sphereGrid.h"

#ifdef HAS_WINDOW
	#include "visualCommands.h"
#endif

namespace Objects
{
	class Hangar
	{
	protected:
		bool				RemoveStored(const PMobileEntity&v);
	public:
		#ifdef HAS_WINDOW
			static index_t			storeOffset;				//!< Button offset (kinda like a scrollbar position)
			static const count_t	MaxVisibleStoreItems = 8;	//!< Maximum number of vehicle buttons that can be shown before arrow buttons are added to scroll

			static int2				cmdLeftCoords,
									cmdRightCoords;
			static IconCommand		cmdLeft,		//!< Arrow button to scroll left
									cmdRight;		//!< Arrow button to scroll right
		#endif

		struct TStored
		{
			WMobileEntity	entity;
			#ifdef HAS_WINDOW
				PDeployMobileCommand	deployCommand;
			#endif

			/**/			TStored()	{}
			/**/			TStored(const PMobileEntity&e):entity(e)	{}

			bool			operator==(const PMobileEntity&e) const {return entity.lock() == e;}
		};
		Buffer0<TStored>	stored;

		virtual			~Hangar()	{}
		/**
		Attempts to store the specified mobile entity in the local entity (not any shared storage)
		This method is frequently called by the server and should not be called by the client
		unless as a response to a server notification.
		@param e Entity to store
		@param atTime Current server time
		@param setHealthToFullIfNew If the specified vehicle has not been registered in the world so far, this value is passed to RegisterNewOffGrid()
		@param maxStorage maximum amount of units that can be stored in the local entity
		@param sharedSelf Version of the local instance as a shared_ptr to be passed to the newly stored entry
		@return true, if the specified vehicle could be stored in the local storage,
				false, if anything (capacity, ownership, etc) prevented storage.
		*/
		bool			StoreLocally(const PMobileEntity&e, float&atTime, bool setHealthToFullIfNew, const PHangar&sharedSelf);
		/**
		Thoroughly checks if the local structure (not any shared storage) has the specified entity in store.
		Checks the actual storage, not the @a storedIn member of @a e
		*/
		bool			HasStoredLocally(const PMobileEntity&e) const;

		/**
		Must be called from the WorldEntity::OnDestroy() override.
		Signals destruction of any remaining entities in local storage.
		*/
		void			OnDestroy(float&t);

		static void		StaticSetup();

		virtual void	LocalCheckout(const PMobileEntity&v, const Grid::LCellIdI&targetLocation, float&atTime);


		virtual const WorldEntity&	ToEntity() const = 0;
		virtual WorldEntity&		ToEntity() = 0;
		virtual count_t				GetMaxStorage() const = 0;

		virtual bool	SharesStorageWith(const PHangar&other) const {return other && other.get() == this;}
		virtual bool	CanStoreType(const MobileEntity&e) const = 0;
		virtual void	StoreLocallyFromImport(const PMobileEntity&e) = 0;
		#ifndef CLIENT
		virtual bool	Store(const PMobileEntity&e, float&atTime) = 0;
		#else
		virtual void	Lose(const PMobileEntity&v);
		#endif
	};

}


#endif
