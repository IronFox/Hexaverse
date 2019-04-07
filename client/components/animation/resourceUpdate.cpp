#include "resourceUpdate.h"
//#include <expectedBehavior.h>
//#include "../visual/hud/hexGrid.h"

namespace Animation
{
	ResourceUpdate::ResourceUpdate(Protocol::ResourceUpdate&u):Super(TUniqueID::Empty())
	{
		up.cells.adoptData(u.cells);
		up.resourceIndex = u.resourceIndex;
	}

	/*virtual override*/ void ResourceUpdate::Begin( bool instant )
	{
		Grid::UpdateResourceMap(Objects::Resource::ID(up.resourceIndex),up.cells,Grid::ResourceDiscovery::client);
	}

	/*virtual override*/ D::String		ResourceUpdate::Describe() const
	{
		return "ResourceUpdate("+Objects::Resource::ID(up.resourceIndex.value).ToString()+")";
	}


}
