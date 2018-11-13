#pragma once
#ifdef HAS_WINDOW

#include <al/openal.h>
#include <objects/types.h>
#include <global_string.h>

namespace Objects
{
	Engine::OpenAL::Wave		GetAmbientSound(const WorldEntity&e);
	Engine::OpenAL::WaveBuffer	GetAmbientSoundOf(const String&entityTypeName, EntityType entityType);
}
#endif
