#ifndef objectHangarStructureH
#define objectHangarStructureH


#include "types.h"
#include "../sphereGrid.h"


namespace Objects
{
	class HangarStructure
	{
	public:
		virtual			~HangarStructure()	{}
		virtual const WorldEntity&	ToEntity() const = 0;
		virtual WorldEntity&		ToEntity() = 0;

		virtual bool	CanStoreType(const MobileEntity&e) const = 0;
		virtual bool	HasStoredLocally(const PMobileEntity&e) const = 0;
		virtual void	StoreLocallyFromImport(const PMobileEntity&e) = 0;
		virtual bool	Store(const PMobileEntity&e, float&atTime) = 0;
	};

}


#endif
