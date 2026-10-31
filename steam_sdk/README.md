# Steamworks SDK (not included)

Valve does not allow the Steamworks SDK to be redistributed, so it isn't in this repository. Put it here yourself, or point CMake at it with `HEXAVERSE_STEAMWORKS_SDK_DIR`. The steps are in the **Steam build** section of the [README](../README.md#steam-build). Never commit the SDK: `.gitignore` excludes everything in this folder except this file.
