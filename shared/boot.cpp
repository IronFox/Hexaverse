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

	#ifndef CLIENT
		World::actual.onTerrainChange << [](const World::Data&d, const BaseGrid::FamiliesChanged&changed, float t){
			Objects::ForeachWorldEntity([changed,t](const Objects::PWorldEntity&e)
			{
				if (e->IsAboveGrid())
					return;
				if (changed.IsSet( Grid::low.grid.GetCell(e->location.stackID).family ))
				{
					e->ClearOccupiedTerrain(t);
				}
			},false);
		};
	#endif

}

