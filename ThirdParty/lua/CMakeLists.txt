# Mirrors lua.project: Lua 5.3.3 as a static library, without the stand-alone
# interpreter (lua.c).

hexaverse_collect_sources(lua_sources "${CMAKE_CURRENT_SOURCE_DIR}/src"
	FILE lua.c)

add_library(lua STATIC ${lua_sources})
hexaverse_apply_common_settings(lua)
# <source path="src" include="true">
target_include_directories(lua PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
set_target_properties(lua PROPERTIES FOLDER "ThirdParty")
