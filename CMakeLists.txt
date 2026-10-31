cmake_minimum_required(VERSION 3.21)

project(Hexaverse
	DESCRIPTION "Turn-based multiplayer 4X strategy game on volumetric planets"
	LANGUAGES C CXX RC)

if(NOT WIN32 OR NOT MSVC)
	message(FATAL_ERROR
		"Hexaverse builds only on Windows with the Visual Studio compiler (MSVC). "
		"It uses Direct3D 11, the Win32 API and MSVC extensions.")
endif()

# ---------------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------------
set(HEXAVERSE_DELTAWORKS_DIR "" CACHE PATH
	"DeltaWorks checkout. Empty: use ../DeltaWorks if it has a CMakeLists.txt, otherwise download it")
set(HEXAVERSE_DELTAWORKS_GIT_TAG "0abb292318cc2b275d471ccd7820c3c00a66ca94" CACHE STRING
	"DeltaWorks commit to download when no local checkout is used")
option(HEXAVERSE_BUILD_CLIENT        "Build the Visual Client and the Console Client" ON)
option(HEXAVERSE_BUILD_SERVER        "Build the dedicated server" ON)
option(HEXAVERSE_BUILD_CONTROLCONSOLE "Build the ControlConsole" ON)
option(HEXAVERSE_INCREMENT_BUILD_COUNTER
	"Increase client/buildCounter.h before every client build, like Projector's 'Increment Build Counter' project" ON)
option(HEXAVERSE_OUTPUT_TO_PROJECT_FOLDERS
	"Write executables into the project folders (client/, server/ ...), where the game looks for them, like Projector" ON)
set(HEXAVERSE_CXX_STANDARD 23 CACHE STRING
	"C++ standard (Projector uses /std:c++latest, which CMake selects for 23 on most MSVC versions)")

# ---------------------------------------------------------------------------
# Configurations and runtime, matching Projector and DeltaWorks
# ---------------------------------------------------------------------------
# Projector's third configuration, OptimizedDebug: optimized, with symbols,
# defines _OPTIMIZED_DEBUG.
get_property(_hv_multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(_hv_multi_config AND NOT "OptimizedDebug" IN_LIST CMAKE_CONFIGURATION_TYPES)
	list(APPEND CMAKE_CONFIGURATION_TYPES OptimizedDebug)
	set(CMAKE_CONFIGURATION_TYPES "${CMAKE_CONFIGURATION_TYPES}" CACHE STRING "" FORCE)
endif()
foreach(lang C CXX RC)
	set(CMAKE_${lang}_FLAGS_OPTIMIZEDDEBUG "${CMAKE_${lang}_FLAGS_RELWITHDEBINFO}" CACHE STRING "")
endforeach()
foreach(kind EXE SHARED STATIC MODULE)
	set(CMAKE_${kind}_LINKER_FLAGS_OPTIMIZEDDEBUG
		"${CMAKE_${kind}_LINKER_FLAGS_RELWITHDEBINFO}" CACHE STRING "")
endforeach()
set(CMAKE_MAP_IMPORTED_CONFIG_OPTIMIZEDDEBUG RelWithDebInfo Release "")
# Projector defines this for every project, DeltaWorks included.
add_compile_definitions($<$<CONFIG:OptimizedDebug>:_OPTIMIZED_DEBUG>)
if(NOT CMAKE_BUILD_TYPE AND NOT _hv_multi_config)
	set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Static runtime (/MT, /MTd) for everything, DeltaWorks included, so no VC++
# redistributable is needed. Set here because DeltaWorks only sets it in its
# own directory scope.
if(NOT DEFINED CMAKE_MSVC_RUNTIME_LIBRARY)
	set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

set(DELTAWORKS_CXX_STANDARD ${HEXAVERSE_CXX_STANDARD} CACHE STRING "C++ standard used for DeltaWorks")
set(DELTAWORKS_BUILD_TERRAIN OFF CACHE BOOL "Hexaverse does not use DeltaWorksTerrain")

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(HexaverseHelpers)
if(HEXAVERSE_BUILD_CLIENT OR HEXAVERSE_BUILD_SERVER)
	include(HexaverseSteam)
endif()

# ---------------------------------------------------------------------------
# DeltaWorks
# ---------------------------------------------------------------------------
if(NOT HEXAVERSE_DELTAWORKS_DIR AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../DeltaWorks/CMakeLists.txt")
	get_filename_component(_hv_sibling "${CMAKE_CURRENT_SOURCE_DIR}/../DeltaWorks" ABSOLUTE)
	set(HEXAVERSE_DELTAWORKS_DIR "${_hv_sibling}" CACHE PATH "DeltaWorks checkout" FORCE)
endif()

if(HEXAVERSE_DELTAWORKS_DIR)
	if(NOT EXISTS "${HEXAVERSE_DELTAWORKS_DIR}/CMakeLists.txt")
		message(FATAL_ERROR
			"HEXAVERSE_DELTAWORKS_DIR (${HEXAVERSE_DELTAWORKS_DIR}) has no CMakeLists.txt. "
			"Update that DeltaWorks checkout, or clear the variable to download DeltaWorks.")
	endif()
	message(STATUS "Hexaverse: using DeltaWorks from ${HEXAVERSE_DELTAWORKS_DIR}")
	add_subdirectory("${HEXAVERSE_DELTAWORKS_DIR}" "${CMAKE_BINARY_DIR}/DeltaWorks")
else()
	message(STATUS "Hexaverse: downloading DeltaWorks ${HEXAVERSE_DELTAWORKS_GIT_TAG}")
	include(FetchContent)
	FetchContent_Declare(DeltaWorks
		GIT_REPOSITORY https://github.com/IronFox/DeltaWorks.git
		GIT_TAG ${HEXAVERSE_DELTAWORKS_GIT_TAG})
	FetchContent_MakeAvailable(DeltaWorks)
endif()

# ---------------------------------------------------------------------------
# Hexaverse (same layout as the .solution / .project files)
# ---------------------------------------------------------------------------
add_subdirectory(ThirdParty/lua)

if(HEXAVERSE_BUILD_CLIENT OR HEXAVERSE_INCREMENT_BUILD_COUNTER)
	add_subdirectory(BuildCounter)
endif()
if(HEXAVERSE_BUILD_CLIENT)
	include(HexaverseOpenAL)
	add_subdirectory(client)
endif()
if(HEXAVERSE_BUILD_SERVER)
	add_subdirectory(server)
endif()
if(HEXAVERSE_BUILD_CONTROLCONSOLE)
	add_subdirectory(ControlConsole)
endif()
