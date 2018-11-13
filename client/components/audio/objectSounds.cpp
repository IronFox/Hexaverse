#include "objectSounds.h"
#include <objects/entity.h>
#include <objects/entityType.h>
#include <objects/vehicle.h>

namespace Objects
{
	Engine::OpenAL::Wave	GetAmbientSound(const WorldEntity&e)
	{
		if (e.IsVehicle())
			return ((const Vehicle&)e).base->ambientSound;
		return GetAmbientSoundOf(e.GetTypeName(),e.GetType());
	}


	Engine::OpenAL::WaveBuffer	GetAmbientSoundOf(const String&entityTypeName, EntityType entityType)
	{
		static HashTable<EntityType, Engine::OpenAL::Wave>	waveTable;
		Engine::OpenAL::WaveBuffer result;
		if (waveTable.Query(entityType,result))
			return result;
		result = Engine::OpenAL::Load("resources/objectSounds/"+entityTypeName+".wav");
		waveTable.Set(entityType,result);	//don't care if it's empty
		return result;
	}

}


