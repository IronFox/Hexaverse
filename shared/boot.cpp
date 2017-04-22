#include <global_root.h>
#include "boot.h"
#include "niceName.h"
#include "scriptCache.h"
#include "grid/stack.h"
#include "grid/resources.h"
#include "grid/worldData.h"
#include "objects.h"

void Boot()
{
	NiceName::Load();
	ScriptCache::Create();
	Grid::CreateHighAndLow();
	Grid::CreateResources();
	World::Create();
	Objects::Create();
}

