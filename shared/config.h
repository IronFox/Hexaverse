#ifndef gameConfigH
#define gameConfigH

/*
Global macro configuration file
*/


/** HYBRID_GRID
If defined, configures the engine to use both a high and a low resolution grid and link them.
If undefined, only the coarse grid exists, and the landscape is rendered to match.
Warning: hasn't been undefined in a long time. Some code sections might not compile/work as expected
*/
#define HYBRID_GRID


/** ONE_COMPOUND_PER_FACTION
If defined, connectors have no functionality. Resources are shared across all world entities
of a faction. Vehicles have no storage capacity in this mode, and no commands to alter their storage.
Vehicles (and satellites) can pull directly from the faction storage and overproduction.
Connectors cannot be built but are maintained for backward compatibility
Last verified to compile in both modes Feb 28 2016
*/
#define ONE_COMPOUND_PER_FACTION



/** NO_MUNITION
If defined, the ammunition resource is removed from the system. Vehicles and towers that would previous
require this resource type, now require no munition to fire.
If compiled in combination with connectors then vehicles that would store munition now store nothing.
MunitionFactory and MunitionStorage structures cannot be built but are maintained for backward compatibility
Last verified to compile in both modes Feb 28 2016
*/
#define NO_MUNITION


/** NO_HIBERNATION
Disables the hibernation mechanic, and replaces it was a type-table that maps types to max count per faction
*/
#define NO_HIBERNATION

/** NO_SATELLITES
Disables the satellite mechanic, sky grid, sky port, and all satellites
*/
#define NO_SATELLITES

#ifdef NO_SATELLITES
	#define IF_SATELLITES_P(X,Y)
	#define IF_SATELLITES(X)
#else
	#define IF_SATELLITES_P(X,Y)	X,Y
	#define IF_SATELLITES(X)	X
#endif


//macros defines in projects:
/*	CLIENT
Defined for clients only. If not defined, then the server is being compiled
*/

/* HAS_WINDOW
Defined for the visual client (i.e. not the AI-client).
If defined a client must be compiled, and a rendering target exists, complete with rendering
resource and dynamic user interaction.
If CLIENT is defined, but HAS_WINDOW is not defined, the console AI-client is being compiled.
It behaves the same in most aspects but holds no rendering resources, window handle, and thus
does not compute user interface functionality.
*/

//optional macro defines:
/* CHEAT_SHOW_ALL_RESOURCES
Must be defined for the server.
If defined, all resources are visible to all clients. Should be defined only ever in debug mode.
Define in the project settings, not here.
*/


/* CHEAT_FULL_STORAGE
Must be defined for the server.
If defined, mobile entities loaded from file are filled to their respective storage maximum.
Effective only if ONE_COMPOUND_PER_FACTION is not defined.
Define in the project settings, not here.
*/

/* DEBUG_ACCESS_RESTRICTIONS
If defined, activates visual controls on hostile entities and ghosts. Server must reject any such attempts.
Define in the project settings, not here.
*/

#endif
