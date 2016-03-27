#ifndef objectHangarStructureH
#define objectHangarStructureH


#include "types.h"
#include "../sphereGrid.h"


namespace Objects
{
	class Hangar
	{
	public:
		struct TStored
		{
			WMobileEntity	entity;
			#ifdef HAS_WINDOW
				DeployMobileCommand	deployCommand;
			#endif

			/**/			TStored()	{}
			/**/			TStored(const PMobileEntity&e):entity(e)	{}

			bool			operator==(const PMobileEntity&e) const {return entity.lock() == e;}
		};
		Buffer0<TStored>	stored;

		virtual			~Hangar()	{}
		bool			StoreLocally(const PMobileEntity&e, float&atTime, count_t maxStorage, const PHangar&sharedSelf);

		virtual const WorldEntity&	ToEntity() const = 0;
		virtual WorldEntity&		ToEntity() = 0;

		virtual bool	CanStoreType(const MobileEntity&e) const = 0;
		virtual bool	HasStoredLocally(const PMobileEntity&e) const = 0;
		virtual void	StoreLocallyFromImport(const PMobileEntity&e) = 0;
		virtual bool	Store(const PMobileEntity&e, float&atTime) = 0;
	};

}


#endif
