#pragma once


#include <objects/types.h>
#include <container/array.h>
#include <grid/stack.h>
#include <objects/vehicle.h>
#include <protocol.h>

using namespace DeltaWorks;

/**
Namespace for the OnPlayerControlBegin() function
*/
namespace PlayerControlBegin
{

	extern bool pauseAutomaticRoundProgression;
	/**
	Triggered when entering command mode and animations have finished (whichever of those events happens later)
	*/
	void		OnPlayerControlBegin();
}


