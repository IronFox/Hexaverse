# Mirrors BuildCounter.project.

hexaverse_collect_sources(buildcounter_sources "${CMAKE_CURRENT_SOURCE_DIR}")

add_executable(BuildCounter ${buildcounter_sources})
hexaverse_apply_common_settings(BuildCounter SUBSYSTEM Console)
target_link_libraries(BuildCounter PRIVATE DeltaWorks::Core)
hexaverse_set_output(BuildCounter "BuildCounter")
set_target_properties(BuildCounter PROPERTIES FOLDER "Tools")
