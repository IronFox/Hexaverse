#ifndef sharedRuntimeConfigH
#define sharedRuntimeConfigH

#include "stringTable.h"
#include <general/enumeration.h>
#include <global_string.h>


namespace RuntimeConfig
{
	CONSTRUCT_ENUMERATION2(GameGoal, None, ConquerNexus);
	CONSTRUCT_ENUMERATION3(DoomsdayWeapons, Expose, Conceal, Disallow);
	CONSTRUCT_ENUMERATION7(Phase, 
							CommandPhase,
							StructurePhase,
							AttackPhase,
							MovementPhase,
							AnimationPhase, 
							EndOfGame,
							Uninitialized
							);

	extern DoomsdayWeapons doomsdayWeapons;
	extern GameGoal gameGoal;
	extern bool		empZones, autoNewRound;
	extern Phase	phase;

	extern bool hibernationSystemEnabled;
	extern bool bombsExplodeWhenDestroyed;

	extern UINT32 roundNumber/*=0*/;
	namespace UnitLimits
	{
		bool		IsEnabled();
		void		Enable(bool doEnable);
		void		Reset();
		bool		AnySet();
		bool		Set(ConstName, int);
		count_t		Get(ConstName, bool ignoreEnabledState=false);
		void		Export(Array<std::pair<UINT16,UINT16> >&);
		void		Import(const Array<std::pair<UINT16,UINT16> >&);
	}

	#ifndef CLIENT
		extern bool		loadingPhase;
	#endif


//	PathString	ConfigFile(const String&base);
	PathString	ConfigFile(PathString base);
	void		InitConfigDir(const String&subDir);
	PathString	ConfigDir();

	inline bool IsInAnyCommandPhase()	{return phase == Phase::CommandPhase;}


}


#endif

