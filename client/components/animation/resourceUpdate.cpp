#include "resourceUpdate.h"
//#include <expectedBehavior.h>
//#include "../visual/hud/hexGrid.h"

namespace Animation
{
	ResourceUpdate::ResourceUpdate(Protocol::ResourceUpdate&u):Super(u.timestamp,0,TUniqueID::Empty())
	{
		up.cells.adoptData(u.cells);
		up.resourceName = u.resourceName;
	}

	/*virtual override*/ void ResourceUpdate::Begin()
	{
		Grid::UpdateResourceMap(up.resourceName,up.cells,Grid::ResourceDiscovery::client);
	}

	/*virtual override*/ D::String		ResourceUpdate::Describe() const
	{
		return "ResourceUpdate("+ToString(up.resourceName)+")";
	}


}
