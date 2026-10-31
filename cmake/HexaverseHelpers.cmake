# Helpers that mirror how Projector builds the Hexaverse .project files, so the
# CMake targets compile the same files with the same settings and put the
# executables where the game expects them.
#
#   <source path="x" include="true">   -> SOURCES x, and x becomes an include directory
#       <exclude find="s"/>            -> FIND s   (relative path contains s)
#       <exclude dir="d"/>             -> DIR d    (any folder named exactly d)
#       <exclude file="f"/>            -> FILE f   (file named exactly f)
#
# Folders whose name starts with '.' are skipped, like Projector does.
# Keep the target definitions in sync with the matching .project files.

# Extensions Projector compiles (Project.cs, ExtensionMap). Shaders (.hlsl) and
# icons are only listed there, not compiled, so they are left out here.
set(HEXAVERSE_SOURCE_EXTENSIONS c cpp cc c++ h hpp hh h++ rc rc2)

# hexaverse_collect_sources(<out-var> <root>
#     [FIND <substr>...] [DIR <name>...] [FILE <name>...])
function(hexaverse_collect_sources out_var root)
	cmake_parse_arguments(PARSE_ARGV 2 arg "" "" "FIND;DIR;FILE")

	set(patterns)
	foreach(ext IN LISTS HEXAVERSE_SOURCE_EXTENSIONS)
		list(APPEND patterns "${root}/*.${ext}")
	endforeach()
	file(GLOB_RECURSE candidates CONFIGURE_DEPENDS RELATIVE "${root}" ${patterns})

	set(result)
	foreach(rel IN LISTS candidates)
		set(skip FALSE)
		get_filename_component(name "${rel}" NAME)
		get_filename_component(dir "${rel}" DIRECTORY)
		string(REPLACE "/" ";" dir_parts "${dir}")
		foreach(part IN LISTS dir_parts)
			if(part MATCHES "^\\." OR part IN_LIST arg_DIR)
				set(skip TRUE)
			endif()
		endforeach()
		if(name IN_LIST arg_FILE)
			set(skip TRUE)
		endif()
		foreach(substr IN LISTS arg_FIND)
			string(FIND "${rel}" "${substr}" pos)
			if(NOT pos EQUAL -1)
				set(skip TRUE)
			endif()
		endforeach()
		if(NOT skip)
			list(APPEND result "${root}/${rel}")
		endif()
	endforeach()

	# Projector compiles .rc2 files as resource scripts, like .rc files.
	foreach(file IN LISTS result)
		if(file MATCHES "\\.rc2$")
			set_source_files_properties("${file}" PROPERTIES LANGUAGE RC)
		endif()
	endforeach()

	set(${out_var} "${result}" PARENT_SCOPE)
endfunction()

# Name suffix Projector gives Win32 builds ("Server x86.exe"). The game uses the
# same suffix to find its sibling executables (globalShared/startProcess.cpp).
if(CMAKE_SIZEOF_VOID_P EQUAL 4)
	set(HEXAVERSE_PLATFORM_SUFFIX " x86")
	set(HEXAVERSE_PROJECTOR_PLATFORM_DIR "")
else()
	set(HEXAVERSE_PLATFORM_SUFFIX "")
	set(HEXAVERSE_PROJECTOR_PLATFORM_DIR "x64/")
endif()

# Settings every Projector-generated Hexaverse project gets.
#   SUBSYSTEM Console|Windows   adds _CONSOLE / _WINDOWS like Projector
function(hexaverse_apply_common_settings target)
	cmake_parse_arguments(PARSE_ARGV 1 arg "" "SUBSYSTEM" "")

	target_compile_features(${target} PRIVATE cxx_std_${HEXAVERSE_CXX_STANDARD})
	set_target_properties(${target} PROPERTIES CXX_EXTENSIONS OFF)

	target_compile_definitions(${target} PRIVATE
		WIN32 UNICODE _UNICODE
		"PLATFORM_TARGET_NAME_EXTENSION_STR=\"${HEXAVERSE_PLATFORM_SUFFIX}\""
		$<$<CONFIG:Debug>:_DEBUG>
		$<$<CONFIG:OptimizedDebug>:_OPTIMIZED_DEBUG>)
	if(CMAKE_SIZEOF_VOID_P EQUAL 8)
		target_compile_definitions(${target} PRIVATE PLATFORM_X64)
	else()
		target_compile_definitions(${target} PRIVATE PLATFORM_X86)
	endif()
	if(arg_SUBSYSTEM)
		string(TOUPPER "${arg_SUBSYSTEM}" subsystem)
		target_compile_definitions(${target} PRIVATE _${subsystem})
	endif()

	if(MSVC)
		# Same as the Projector output: /W3 /sdl /MP, noexcept warning off.
		# Projector's projects don't enable conformance mode, so Visual Studio
		# passes /permissive. Without it, /std:c++latest implies /permissive-
		# (two-phase name lookup), which the Hexaverse sources don't compile with.
		target_compile_options(${target} PRIVATE
			/W3 /sdl /MP /wd26439
			$<$<COMPILE_LANGUAGE:CXX>:/Zc:__cplusplus /permissive>)
	endif()
endfunction()

# Puts an executable where Projector puts it, so the programs find each other
# and their data files:
#   Release (and RelWithDebInfo, MinSizeRel): <project folder>/<Name>.exe
#   Debug, OptimizedDebug: <project folder>/.projector/[x64/]<Config>/<Name>.exe
# With HEXAVERSE_OUTPUT_TO_PROJECT_FOLDERS=OFF they stay in the build tree.
function(hexaverse_set_output target name)
	set_target_properties(${target} PROPERTIES
		OUTPUT_NAME "${name}${HEXAVERSE_PLATFORM_SUFFIX}"
		VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

	if(NOT HEXAVERSE_OUTPUT_TO_PROJECT_FOLDERS)
		return()
	endif()

	set(projector_dir "${CMAKE_CURRENT_SOURCE_DIR}/.projector/${HEXAVERSE_PROJECTOR_PLATFORM_DIR}")
	get_property(multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
	if(multi_config)
		set(configs ${CMAKE_CONFIGURATION_TYPES})
	else()
		set(configs ${CMAKE_BUILD_TYPE})
	endif()
	foreach(config IN LISTS configs)
		string(TOUPPER "${config}" config_upper)
		if(config STREQUAL "Debug" OR config STREQUAL "OptimizedDebug")
			set(dir "${projector_dir}${config}")
		else()
			set(dir "${CMAKE_CURRENT_SOURCE_DIR}")
		endif()
		set_target_properties(${target} PROPERTIES
			RUNTIME_OUTPUT_DIRECTORY_${config_upper} "${dir}")
	endforeach()
	if(NOT multi_config)
		if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "OptimizedDebug")
			set_target_properties(${target} PROPERTIES
				RUNTIME_OUTPUT_DIRECTORY "${projector_dir}${CMAKE_BUILD_TYPE}")
		else()
			set_target_properties(${target} PROPERTIES
				RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
		endif()
	endif()
endfunction()
