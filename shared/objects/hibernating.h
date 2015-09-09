#ifndef objectHibernatingH
#define objectHibernatingH

//#include "types.h"
#include "types.h"
#include "../config.h"

namespace Objects
{

	#ifndef NO_HIBERNATION
	/**
	Object extension that enables hibernation control
	*/
	class Hibernating
	{
		bool	isHibernating = false;

	public:
		/**
		Queries whether or not the local structure has shut down due to insufficient uplink bandwidth
		*/
		bool						IsHibernating() const {return this && isHibernating;}

		/**
		Retrieves the priority of this structure to not hibernate due to insufficient uplink bandwidth
		*/
		virtual int					GetUplinkPriority() const = 0;


	protected:
		bool						UpdateHibernation(WorldEntity*self,  bool hibernate, float atTime);


	};

	#endif

}

#endif
