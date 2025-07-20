cmake_minimum_required(VERSION 3.24)
include(FetchContent)
include(CheckIPOSupported)

project(MacWolfSDL C)
set(CMAKE_C_STANDARD 23)

check_ipo_supported(RESULT USE_IPO)
if (USE_IPO)
	set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
	set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_MINSIZEREL TRUE)
endif()

option(SDL3_VENDORED "Vendor SDL3 library" OFF)
if(NOT SDL3_VENDORED)
	find_package(SDL3 3.2 QUIET)
endif()
if(NOT SDL3_FOUND)
	set(FETCHCONTENT_QUIET OFF)
	set(SDL_DEPS_SHARED ON CACHE INTERNAL "")
	set(SDL_ALSA_SHARED ON CACHE INTERNAL "")
	set(SDL_HIDAPI_LIBUSB_SHARED ON CACHE INTERNAL "")
	set(SDL_JACK_SHARED ON CACHE INTERNAL "")
	set(SDL_KMSDRM_SHARED ON CACHE INTERNAL "")
	set(SDL_PIPEWIRE_SHARED ON CACHE INTERNAL "")
	set(SDL_PULSEAUDIO_SHARED ON CACHE INTERNAL "")
	set(SDL_SNDIO_SHARED ON CACHE INTERNAL "")
	set(SDL_WAYLAND_SHARED ON CACHE INTERNAL "")
	set(SDL_X11_SHARED ON CACHE INTERNAL "")
	set(SDL_STATIC ON CACHE INTERNAL "")
	set(SDL_SHARED OFF CACHE INTERNAL "")
	FetchContent_Declare(
		SDL3
		URL            https://github.com/libsdl-org/SDL/releases/download/release-3.2.18/SDL3-3.2.18.tar.gz
		URL_HASH       SHA256=1a775bde924397a8e0c08bfda198926c17be859d0288ad0dec1dea1b2ee04f8f
		DOWNLOAD_EXTRACT_TIMESTAMP OFF
	)
	FetchContent_MakeAvailable(SDL3)
endif()

if(APPLE OR WIN32)
	set(exe_name ${CMAKE_PROJECT_NAME})
else()
	set(exe_name macwolfsdl)
endif()
add_executable(${exe_name} WIN32 MACOSX_BUNDLE
	Burger.c Data.c Doors.c EnMove.c EnThink.c InterMis.c Intro.c Level.c
	MacFont.c MacLoader.c MacMenu.c Missiles.c Music.c PlMove.c PlStuff.c
	PlThink.c PushWall.c RefBsp.c Refresh2.c Refresh.c RefSprite.c SDLWolf.c
	SetupScalers.c Sight.c SnesMain.c StateDef.c WolfIO.c WolfMain.c)
target_link_libraries(${exe_name} PRIVATE SDL3::SDL3)

if(WIN32)
	target_sources(${exe_name} PRIVATE pkg/MacWolfSDL.rc)
endif()
if(APPLE)
	target_sources(${exe_name} PRIVATE pkg/MacWolfSDL.icns)
	set_source_files_properties(pkg/MacWolfSDL.icns PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
	set_property(TARGET ${exe_name} PROPERTY MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/pkg/Info.plist")
endif()
