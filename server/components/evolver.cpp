#include <global_root.h>
#include "evolver.h"

#include <container/hashtable.h>
#include "evolvers/core.h"

namespace Evolver
{

	namespace Details
	{
		namespace Evolvers
		{
			Core		core;

			Abstract	*def = &core;
		}


		StringTable<Abstract*>	evMap;


		void		Register(Abstract*abs)
		{
			evMap.Set(abs->name,abs);

		}

	}



	void			Create()
	{
		Details::Register(&Details::Evolvers::core);
	}

	Abstract*		Get(const String&name)
	{
		Abstract*rs;
		if (!Details::evMap.Query(name,rs))
			return nullptr;
		return rs;

	}

	Abstract*		GetDefault()
	{
		return Details::Evolvers::def;
	}
}
