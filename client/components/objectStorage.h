#include <objects/types.h>
#include <container/vectorList.h>
#include <objects/markers.h>

namespace Objects
{
	
	/**
	Backup storage for all objects.
	*/
	class Storage
	{
		Objects::MarkerStore markers;
		Vector<PWorldEntity>	objects;
		Vector<std::pair<PHangar, PMobileEntity> > storedMobiles;
		Array<std::pair<EntityType,UINT16> >	unitLimits;
		bool	loaded = false;
	public:
		void	Clear();
		bool	IsEmpty() const	{return !loaded;}
		bool	IsNotEmpty() const	{return loaded;}
		/**
		Loads all global objects, and purges the global object state.
		The local storage must be empty
		*/
		void	Import(bool preserveGridVisibility);
		/**
		Restores the global object state according to local storage.
		All local content is cleared afterward
		*/
		void	Export();
		
		
		
	};
	
	
	
}
