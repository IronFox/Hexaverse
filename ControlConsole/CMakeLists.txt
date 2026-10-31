# Mirrors ControlConsole.project.

set(hexaverse_root "${CMAKE_CURRENT_SOURCE_DIR}/..")

hexaverse_collect_sources(cc_sources "${CMAKE_CURRENT_SOURCE_DIR}")
hexaverse_collect_sources(cc_console "${hexaverse_root}/clientConsoleShared")
hexaverse_collect_sources(cc_global  "${hexaverse_root}/globalShared")

add_executable(ControlConsole ${cc_sources} ${cc_console} ${cc_global})
hexaverse_apply_common_settings(ControlConsole SUBSYSTEM Console)
target_include_directories(ControlConsole PRIVATE
	"${hexaverse_root}/clientConsoleShared"
	"${hexaverse_root}/globalShared")
target_link_libraries(ControlConsole PRIVATE DeltaWorks::Core)
hexaverse_set_output(ControlConsole "ControlConsole")

source_group(TREE "${hexaverse_root}" FILES ${cc_sources} ${cc_console} ${cc_global})
set_target_properties(ControlConsole PROPERTIES FOLDER "Hexaverse")
