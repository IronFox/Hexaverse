# Steamworks SDK for the Steam build (steam-public branch).
#
# The SDK is not redistributable, so it is not in the repository. Copy it into
# steam_sdk/ (see the "Steam build" section of README.md) or point
# HEXAVERSE_STEAMWORKS_SDK_DIR at an unpacked Steamworks SDK ("sdk" folder).
#
# Two layouts are accepted:
#   Hexaverse layout (what the original steam branch used):
#     steam_sdk/steam/*.h
#     steam_sdk/lib/win32/steam_api.lib, sdkencryptedappticket.lib
#     steam_sdk/lib/win64/steam_api.lib, sdkencryptedappticket.lib
#       (the 64-bit import libraries renamed, they still load *64.dll)
#   Steamworks SDK layout:
#     public/steam/*.h
#     redistributable_bin/steam_api.lib|dll, redistributable_bin/win64/steam_api64.lib|dll
#     public/steam/lib/win32|win64/sdkencryptedappticket(64).lib|dll
#
# Defines the imported target Steamworks::Steamworks and the function
# hexaverse_copy_steam_dlls(<target>), which copies the runtime DLLs next to
# the executable after each build.

set(HEXAVERSE_STEAMWORKS_SDK_DIR "${CMAKE_SOURCE_DIR}/steam_sdk" CACHE PATH
	"Steamworks SDK: Hexaverse's steam_sdk/ folder, or the 'sdk' folder of the downloaded Steamworks SDK")

set(_sdk "${HEXAVERSE_STEAMWORKS_SDK_DIR}")
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
	set(_bits 64)
	set(_hv_dir win64)
else()
	set(_bits "")
	set(_hv_dir win32)
endif()

if(EXISTS "${_sdk}/steam/steam_api.h")
	set(_inc "${_sdk}")
	set(_api_lib "${_sdk}/lib/${_hv_dir}/steam_api.lib")
	set(_ticket_lib "${_sdk}/lib/${_hv_dir}/sdkencryptedappticket.lib")
	set(_api_dll_dirs "${_sdk}/lib/${_hv_dir}" "${CMAKE_SOURCE_DIR}/client" "${CMAKE_SOURCE_DIR}/server")
	set(_ticket_dll_dirs ${_api_dll_dirs})
elseif(EXISTS "${_sdk}/public/steam/steam_api.h")
	set(_inc "${_sdk}/public")
	if(_bits)
		set(_api_lib "${_sdk}/redistributable_bin/win64/steam_api64.lib")
		set(_api_dll_dirs "${_sdk}/redistributable_bin/win64")
	else()
		set(_api_lib "${_sdk}/redistributable_bin/steam_api.lib")
		set(_api_dll_dirs "${_sdk}/redistributable_bin")
	endif()
	set(_ticket_lib "${_sdk}/public/steam/lib/${_hv_dir}/sdkencryptedappticket${_bits}.lib")
	set(_ticket_dll_dirs "${_sdk}/public/steam/lib/${_hv_dir}")
else()
	message(FATAL_ERROR
		"Steamworks SDK not found in ${_sdk}.\n"
		"The SDK can't be redistributed, so it isn't part of this repository. "
		"Download it from https://partner.steamgames.com/downloads/list and either copy it "
		"into steam_sdk/ or set HEXAVERSE_STEAMWORKS_SDK_DIR to its 'sdk' folder. "
		"See the 'Steam build' section of README.md.")
endif()

foreach(_f IN ITEMS "${_api_lib}" "${_ticket_lib}")
	if(NOT EXISTS "${_f}")
		message(FATAL_ERROR "Steamworks SDK: ${_f} is missing. See the 'Steam build' section of README.md.")
	endif()
endforeach()

find_file(HEXAVERSE_STEAM_API_DLL NAMES steam_api${_bits}.dll PATHS ${_api_dll_dirs} NO_DEFAULT_PATH)
find_file(HEXAVERSE_STEAM_TICKET_DLL NAMES sdkencryptedappticket${_bits}.dll PATHS ${_ticket_dll_dirs} NO_DEFAULT_PATH)
if(NOT HEXAVERSE_STEAM_API_DLL OR NOT HEXAVERSE_STEAM_TICKET_DLL)
	message(WARNING "Steamworks SDK: steam_api${_bits}.dll or sdkencryptedappticket${_bits}.dll not found. "
		"Copy them next to the executables yourself, or the game won't start.")
endif()

add_library(Steamworks::Steamworks INTERFACE IMPORTED)
target_include_directories(Steamworks::Steamworks INTERFACE "${_inc}")
target_link_libraries(Steamworks::Steamworks INTERFACE "${_api_lib}" "${_ticket_lib}")
message(STATUS "Hexaverse: Steamworks SDK from ${_sdk}")

function(hexaverse_copy_steam_dlls target)
	foreach(_dll IN ITEMS "${HEXAVERSE_STEAM_API_DLL}" "${HEXAVERSE_STEAM_TICKET_DLL}")
		if(_dll)
			add_custom_command(TARGET ${target} POST_BUILD
				COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_dll}" "$<TARGET_FILE_DIR:${target}>"
				VERBATIM)
		endif()
	endforeach()
endfunction()
