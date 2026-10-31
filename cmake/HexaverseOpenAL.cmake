# Provides the OpenAL import library (OpenAL32.lib) for the Visual Client.
#
# The OpenAL headers are bundled with DeltaWorks (Engine/al), and
# <al/openal.h> requests OpenAL32.lib through #pragma comment(lib). Only that
# import library is needed at link time. It comes from:
#
#   SDK       the Creative OpenAL 1.1 SDK (registry, OPENAL_SDK_DIR or OPENALDIR),
#             like the Projector build
#   GENERATED an import library made from cmake/OpenAL32.def with lib.exe.
#             The .def lists the exports of the OpenAL 1.1 router DLL, so the
#             result is equivalent to the SDK's OpenAL32.lib
#   AUTO      SDK if it is installed, otherwise GENERATED (default)
#
# Result: HEXAVERSE_OPENAL_LIBRARY_DIR (folder that contains OpenAL32.lib).

set(HEXAVERSE_OPENAL "AUTO" CACHE STRING "Where OpenAL32.lib comes from: AUTO, SDK or GENERATED")
set_property(CACHE HEXAVERSE_OPENAL PROPERTY STRINGS AUTO SDK GENERATED)
set(OPENAL_SDK_DIR "" CACHE PATH "Install folder of the OpenAL 1.1 SDK (found through the registry if empty)")

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
	set(_hv_openal_sdk_subdir "libs/Win64")
	set(_hv_openal_machine "x64")
else()
	set(_hv_openal_sdk_subdir "libs/Win32")
	set(_hv_openal_machine "x86")
endif()

set(HEXAVERSE_OPENAL_LIBRARY_DIR "")

if(NOT HEXAVERSE_OPENAL STREQUAL "GENERATED")
	find_path(HEXAVERSE_OPENAL_SDK_LIBRARY_DIR
		NAMES OpenAL32.lib
		HINTS
			"${OPENAL_SDK_DIR}/${_hv_openal_sdk_subdir}"
			"$ENV{OPENALDIR}/${_hv_openal_sdk_subdir}"
			"[HKEY_LOCAL_MACHINE\\SOFTWARE\\Wow6432Node\\Creative Labs\\OpenAL 1.1 Software Development Kit\\3.05;InstallDir]/${_hv_openal_sdk_subdir}"
			"[HKEY_LOCAL_MACHINE\\SOFTWARE\\Wow6432Node\\Creative Labs\\OpenAL 1.1 Software Development Kit\\3.03;InstallDir]/${_hv_openal_sdk_subdir}"
		NO_DEFAULT_PATH)
	if(HEXAVERSE_OPENAL_SDK_LIBRARY_DIR)
		set(HEXAVERSE_OPENAL_LIBRARY_DIR "${HEXAVERSE_OPENAL_SDK_LIBRARY_DIR}")
		message(STATUS "Hexaverse: OpenAL32.lib from the OpenAL SDK (${HEXAVERSE_OPENAL_LIBRARY_DIR})")
	elseif(HEXAVERSE_OPENAL STREQUAL "SDK")
		message(FATAL_ERROR
			"HEXAVERSE_OPENAL=SDK, but the OpenAL 1.1 SDK was not found. "
			"Install it, set OPENAL_SDK_DIR, or use HEXAVERSE_OPENAL=GENERATED.")
	endif()
endif()

if(NOT HEXAVERSE_OPENAL_LIBRARY_DIR)
	set(_hv_openal_out_dir "${CMAKE_BINARY_DIR}/openal")
	set(_hv_openal_lib "${_hv_openal_out_dir}/OpenAL32.lib")
	set(_hv_openal_def "${CMAKE_CURRENT_LIST_DIR}/OpenAL32.def")
	file(MAKE_DIRECTORY "${_hv_openal_out_dir}")

	if(CMAKE_AR)
		set(_hv_lib_tool "${CMAKE_AR}")
		set(_hv_lib_tool_args)
	else()
		set(_hv_lib_tool "${CMAKE_LINKER}")
		set(_hv_lib_tool_args /lib)
	endif()

	execute_process(
		COMMAND "${_hv_lib_tool}" ${_hv_lib_tool_args} /nologo
			"/def:${_hv_openal_def}" "/machine:${_hv_openal_machine}" "/out:${_hv_openal_lib}"
		RESULT_VARIABLE _hv_openal_result
		OUTPUT_VARIABLE _hv_openal_output
		ERROR_VARIABLE _hv_openal_output)
	if(NOT _hv_openal_result EQUAL 0 OR NOT EXISTS "${_hv_openal_lib}")
		message(FATAL_ERROR
			"Could not create OpenAL32.lib from ${_hv_openal_def} with ${_hv_lib_tool}:\n${_hv_openal_output}")
	endif()
	set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_hv_openal_def}")

	set(HEXAVERSE_OPENAL_LIBRARY_DIR "${_hv_openal_out_dir}")
	message(STATUS "Hexaverse: OpenAL32.lib generated from cmake/OpenAL32.def (${_hv_openal_machine})")
endif()
