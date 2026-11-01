# Mirrors "Visual Client.project", "Console Client.project" and
# "Increment Build Counter.project" (client/Client.solution).

set(hexaverse_root "${CMAKE_CURRENT_SOURCE_DIR}/..")

hexaverse_collect_sources(client_server_shared  "${hexaverse_root}/clientServerShared")
hexaverse_collect_sources(client_console_shared "${hexaverse_root}/clientConsoleShared")
hexaverse_collect_sources(client_global         "${hexaverse_root}/globalShared")
set(client_shared ${client_server_shared} ${client_console_shared} ${client_global})
set(client_shared_includes
	"${hexaverse_root}/clientServerShared"
	"${hexaverse_root}/clientConsoleShared"
	"${hexaverse_root}/globalShared")

# ---------------------------------------------------------------------------
# Increment Build Counter: runs "BuildCounter buildCounter.h" in client/
# before the clients are built.
# ---------------------------------------------------------------------------
if(HEXAVERSE_INCREMENT_BUILD_COUNTER)
	add_custom_target(IncrementBuildCounter
		COMMAND BuildCounter buildCounter.h
		WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
		COMMENT "Increasing client/buildCounter.h"
		VERBATIM)
	set_target_properties(IncrementBuildCounter PROPERTIES FOLDER "Tools")
endif()

# ---------------------------------------------------------------------------
# Visual Client
# ---------------------------------------------------------------------------
hexaverse_collect_sources(visual_sources "${CMAKE_CURRENT_SOURCE_DIR}"
	FILE console.rc2)

# Shared sources first, in the same order as the .project files. MSVC runs
# static initializers in link order, and client/ globals depend on statics
# from the shared folders. With client/ first, the clients exit before main().
add_executable(VisualClient WIN32
	${client_shared} ${visual_sources}
	"${CMAKE_CURRENT_SOURCE_DIR}/DeclareDPIAware.manifest")
hexaverse_apply_common_settings(VisualClient SUBSYSTEM Windows)
target_compile_definitions(VisualClient PRIVATE CLIENT HAS_WINDOW _SCL_SECURE_NO_WARNINGS)
target_include_directories(VisualClient PRIVATE ${client_shared_includes})
target_link_libraries(VisualClient PRIVATE DeltaWorks::Core DeltaWorks::Engine lua d3d11 d3dcompiler)
# <al/openal.h> pulls in OpenAL32.lib through #pragma comment(lib).
target_link_directories(VisualClient PRIVATE "${HEXAVERSE_OPENAL_LIBRARY_DIR}")
target_link_options(VisualClient PRIVATE /STACK:4000000)
hexaverse_set_output(VisualClient "Visual Client")
if(HEXAVERSE_INCREMENT_BUILD_COUNTER)
	add_dependencies(VisualClient IncrementBuildCounter)
endif()

source_group(TREE "${hexaverse_root}" FILES ${visual_sources} ${client_shared})
set_target_properties(VisualClient PROPERTIES FOLDER "Hexaverse")

# ---------------------------------------------------------------------------
# Console Client: same sources without rendering and audio. The server starts
# it to run Lua AI players.
# ---------------------------------------------------------------------------
hexaverse_collect_sources(console_sources "${CMAKE_CURRENT_SOURCE_DIR}"
	FIND consensusLink
	DIR  visual audio
	FILE visual.rc2)

add_executable(ConsoleClient ${client_shared} ${console_sources})
hexaverse_apply_common_settings(ConsoleClient SUBSYSTEM Console)
target_compile_definitions(ConsoleClient PRIVATE CLIENT _SCL_SECURE_NO_WARNINGS)
target_include_directories(ConsoleClient PRIVATE ${client_shared_includes})
target_link_libraries(ConsoleClient PRIVATE DeltaWorks::Core lua)
target_link_options(ConsoleClient PRIVATE /STACK:4000000)
hexaverse_set_output(ConsoleClient "Console Client")
if(HEXAVERSE_INCREMENT_BUILD_COUNTER)
	add_dependencies(ConsoleClient IncrementBuildCounter)
endif()

source_group(TREE "${hexaverse_root}" FILES ${console_sources} ${client_shared})
set_target_properties(ConsoleClient PROPERTIES FOLDER "Hexaverse")

# Projector's Client.solution starts the Visual Client.
set_property(DIRECTORY "${CMAKE_SOURCE_DIR}" PROPERTY VS_STARTUP_PROJECT VisualClient)
