# Mirrors Server.project.

set(hexaverse_root "${CMAKE_CURRENT_SOURCE_DIR}/..")

hexaverse_collect_sources(server_shared  "${hexaverse_root}/clientServerShared")
hexaverse_collect_sources(server_global  "${hexaverse_root}/globalShared")
hexaverse_collect_sources(server_sources "${CMAKE_CURRENT_SOURCE_DIR}")

# Shared sources first, in the same order as Server.project. MSVC runs static
# initializers in link order, and server/ globals depend on statics from the
# shared folders. With server/ first, Server.exe crashes before main().
add_executable(Server ${server_shared} ${server_global} ${server_sources})
hexaverse_apply_common_settings(Server SUBSYSTEM Console)
target_compile_definitions(Server PRIVATE _SCL_SECURE_NO_WARNINGS)
target_include_directories(Server PRIVATE
	"${hexaverse_root}/clientServerShared"
	"${hexaverse_root}/globalShared")
target_link_libraries(Server PRIVATE DeltaWorks::Core lua Steamworks::Steamworks)
hexaverse_set_output(Server "Server")
hexaverse_copy_steam_dlls(Server)

source_group(TREE "${hexaverse_root}" FILES ${server_sources} ${server_shared} ${server_global})
set_target_properties(Server PROPERTIES FOLDER "Hexaverse")
