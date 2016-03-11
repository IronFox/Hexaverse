#include <global_root.h>
#include "evolver.h"

#include <container/hashtable.h>
#include "evolvers/core.h"
#include "evolvers/techniteEvolver.h"

namespace Evolver
{

	namespace Details
	{
		namespace Evolvers
		{
			Core		core;
			Technite	technite;

			Abstract	*def = &technite;
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
		Details::Register(&Details::Evolvers::technite);
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
