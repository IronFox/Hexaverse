#ifndef sharedRuntimeConfigH
#define sharedRuntimeConfigH

#include "stringTable.h"
#include <general/enumeration.h>
#include <global_string.h>


namespace RuntimeConfig
{
	CONSTRUCT_ENUMERATION3(GameGoal, None, ConquerNexus, EMPConquerNexus);
	CONSTRUCT_ENUMERATION3(DoomsdayWeapons, Expose, Conceal, Disallow);

	CONSTRUCT_ENUMERATION8(Phase, 
		CommandPhase,
		TechniteCommandPhase,
		StructurePhase,
		AttackPhase,
		MovementPhase,
		TechnitePhase,
		AnimationPhase, 
		EndOfGame
		);

	extern DoomsdayWeapons doomsdayWeapons;
	extern GameGoal gameGoal;
	extern Phase	phase;

	extern bool hibernationSystemEnabled;
	extern UINT32 roundNumber/*=0*/;
	extern UINT32 techniteSubRoundNumber/*=0*/;
	namespace UnitLimits
	{
		void		Reset();
		bool		AnySet();
		void		Set(ConstName, int);
		count_t		Get(ConstName);
		void		Export(Array<std::pair<UINT16,UINT16> >&);
		void		Import(const Array<std::pair<UINT16,UINT16> >&);
	}


//	PathString	ConfigFile(const String&base);
	PathString	ConfigFile(PathString base);
	void		InitConfigDir(const String&subDir);
	PathString	ConfigDir();


}


#endif

