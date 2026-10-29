#
# Doom 3 BFG Edition engine (Linux / SDL3)
#

# ------------------------------------------------------------------------------------------------
# Source lists
# ------------------------------------------------------------------------------------------------

# idlib (platform independent library + posix threading)
file(GLOB_RECURSE IDLIB_SOURCES idlib/*.cpp)
file(GLOB_RECURSE IDLIB_INCLUDES idlib/*.h)
list(FILTER IDLIB_SOURCES EXCLUDE REGEX "/sys/win32/")

# engine
file(GLOB   AAS_SOURCES          aas/*.cpp)
file(GLOB_RECURSE CM_SOURCES     cm/*.cpp)
file(GLOB   FRAMEWORK_SOURCES    framework/*.cpp)
file(GLOB_RECURSE RENDERER_SOURCES renderer/*.cpp)

# files that exist in the GPL drop but are not part of any retail project,
# so they are neither compiled nor supposed to be
list(REMOVE_ITEM FRAMEWORK_SOURCES
	${CMAKE_CURRENT_SOURCE_DIR}/framework/Session.cpp)

list(REMOVE_ITEM RENDERER_SOURCES
	${CMAKE_CURRENT_SOURCE_DIR}/renderer/BoundsTrack.cpp)
list(FILTER RENDERER_SOURCES EXCLUDE REGEX "/jpeg-6/")
file(GLOB   JPEG_SOURCES         renderer/jpeg-6/*.cpp)
list(REMOVE_ITEM JPEG_SOURCES
	${CMAKE_CURRENT_SOURCE_DIR}/renderer/jpeg-6/jmemansi.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/renderer/jpeg-6/jmemdos.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/renderer/jpeg-6/jmemname.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/renderer/jpeg-6/jpegtran.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/renderer/jpeg-6/jload.cpp)
file(GLOB_RECURSE SOUND_SOURCES  sound/*.cpp)
file(GLOB_RECURSE SWF_SOURCES    swf/*.cpp)
file(GLOB   UI_SOURCES           ui/*.cpp)
file(GLOB   SYS_SOURCES          sys/*.cpp)
file(GLOB_RECURSE D3XP_SOURCES   d3xp/*.cpp)

list(REMOVE_ITEM UI_SOURCES
	${CMAKE_CURRENT_SOURCE_DIR}/ui/GameWindow.cpp)

list(REMOVE_ITEM D3XP_SOURCES
	${CMAKE_CURRENT_SOURCE_DIR}/d3xp/EndLevel.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/d3xp/gamesys/Callbacks.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/d3xp/menus/MenuWidget_DevList.cpp)

# linux platform layer
file(GLOB PLATFORM_POSIX_SOURCES sys/posix/*.cpp)
file(GLOB PLATFORM_SDL_SOURCES   sys/sdl/*.cpp)
file(GLOB PLATFORM_LINUX_SOURCES sys/linux/*.cpp)
file(GLOB PLATFORM_COMMON_SOURCES sys/common/*.cpp)

# bundled Doom Classic (Doom 1/2) game code
file(GLOB DOOMCLASSIC_SOURCES ../doomclassic/doom/*.cpp)
list(REMOVE_ITEM DOOMCLASSIC_SOURCES
	# console (PS3/Xbox360) backends. i_video_ps3.cpp is *not* listed here:
	# the retail Windows project builds it as the PC video backend as well
	# (it only copies the paletted screen the engine uploads itself).
	${CMAKE_CURRENT_SOURCE_DIR}/../doomclassic/doom/i_net_ps3.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/../doomclassic/doom/i_net_xbox.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/../doomclassic/doom/i_video_xbox.cpp
	# BFG online/console code that depends on the retail tech5 headers
	${CMAKE_CURRENT_SOURCE_DIR}/../doomclassic/doom/DoomLeaderboards.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/../doomclassic/doom/Networking.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/../doomclassic/doom/PlayerProfile.cpp
	# Win32/XAudio2 backend (Windows-only); i_sound_faudio.cpp is the portable backend
	${CMAKE_CURRENT_SOURCE_DIR}/../doomclassic/doom/i_sound_win32.cpp
	# silent stub, replaced by the FAudio backend
	${CMAKE_CURRENT_SOURCE_DIR}/../doomclassic/doom/i_sound_stub.cpp
)

# bundled TiMidity MIDI synthesizer used by Doom Classic music
file(GLOB TIMIDITY_SOURCES ../doomclassic/timidity/*.cpp)

# third party (compiled as C where applicable)
file(GLOB ZLIB_SOURCES  framework/zlib/*.c)
list(FILTER ZLIB_SOURCES EXCLUDE REGEX "/(example|minigzip)\\.c$")
file(GLOB BINK_SOURCES  libs/bink/bink_*.c)
list(FILTER BINK_SOURCES EXCLUDE REGEX "bink_(test|player)\\.c")

set(GLAD_SOURCES libs/glad/glad.c)

# XAudio2 C++ interfaces implemented over FAudio
set(XAUDIO2_SOURCES libs/xaudio2/xaudio2.cpp)

set(ENGINE_SOURCES
	${IDLIB_SOURCES}
	${AAS_SOURCES}
	${CM_SOURCES}
	${FRAMEWORK_SOURCES}
	${RENDERER_SOURCES}
	${SOUND_SOURCES}
	${SWF_SOURCES}
	${UI_SOURCES}
	${SYS_SOURCES}
	${PLATFORM_POSIX_SOURCES}
	${PLATFORM_SDL_SOURCES}
	${PLATFORM_LINUX_SOURCES}
	${PLATFORM_COMMON_SOURCES}
	${DOOMCLASSIC_SOURCES}
	${D3XP_SOURCES}
	${JPEG_SOURCES}
	${ZLIB_SOURCES}
	${BINK_SOURCES}
	${GLAD_SOURCES}
	${XAUDIO2_SOURCES}
	${TIMIDITY_SOURCES}
)

list(REMOVE_DUPLICATES ENGINE_SOURCES)

# ------------------------------------------------------------------------------------------------
# Include directories / defines
# ------------------------------------------------------------------------------------------------

include_directories(
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/idlib
	${CMAKE_CURRENT_SOURCE_DIR}/sys
	${CMAKE_CURRENT_SOURCE_DIR}/libs/glad
	${CMAKE_CURRENT_SOURCE_DIR}/libs/bink
	${CMAKE_CURRENT_SOURCE_DIR}/framework/zlib
	${CMAKE_CURRENT_SOURCE_DIR}/libs/FAudio/include
	${CMAKE_CURRENT_SOURCE_DIR}/libs/xaudio2
)

# settings/flags matching the original MSVC build as closely as is practical on Linux
add_definitions(
	-D__DOOM__
	-DUSE_EXCEPTIONS
	-DID_LITTLE_ENDIAN
	-DUSE_DOOMCLASSIC
)

set(ENGINE_WARN_FLAGS
	-Wno-pragmas
	-Wno-unused-variable
	-Wno-unused-but-set-variable
	-Wno-unused-value
	-Wno-switch
	-Wno-parentheses
	-Wno-narrowing
	-Wno-sign-compare
	-Wno-write-strings
	-Wno-format-security
	-Wno-deprecated-declarations
)

add_compile_options(
	-pipe
	-mmmx -msse -msse2
	-fno-strict-aliasing
	-fsigned-char
	${ENGINE_WARN_FLAGS}
)

# force idlib/precompiled.h into every engine translation unit that expects it
set(PCH_SOURCES ${ENGINE_SOURCES})
list(FILTER PCH_SOURCES EXCLUDE REGEX "/jpeg-6/")
list(FILTER PCH_SOURCES EXCLUDE REGEX "/glad\\.c$")
# bundled C libraries are plain C and must not get the C++ precompiled header
list(FILTER PCH_SOURCES EXCLUDE REGEX "/framework/zlib/")
list(FILTER PCH_SOURCES EXCLUDE REGEX "/libs/bink/")
list(FILTER PCH_SOURCES EXCLUDE REGEX "/DXT/")
list(FILTER PCH_SOURCES EXCLUDE REGEX "/jobs/")
list(FILTER PCH_SOURCES EXCLUDE REGEX "/renderer/RenderLog\\.cpp$")
# doomclassic translation units include their own Precompiled.h
list(FILTER PCH_SOURCES EXCLUDE REGEX "/doomclassic/")

set_source_files_properties(${PCH_SOURCES}
	PROPERTIES COMPILE_FLAGS "-include ${CMAKE_CURRENT_SOURCE_DIR}/idlib/precompiled.h")

# ------------------------------------------------------------------------------------------------
# Executable
# ------------------------------------------------------------------------------------------------

add_executable(Doom3BFG ${ENGINE_SOURCES})

target_link_libraries(Doom3BFG
	FAudio-static
	SDL3::SDL3
	Threads::Threads
	dl
	rt
	m
)

# Statically link the compiler runtimes for release builds.
target_link_options(Doom3BFG PRIVATE
	$<$<CONFIG:Release>:-static-libgcc>
	$<$<CONFIG:Release>:-static-libstdc++>
)

set_target_properties(Doom3BFG PROPERTIES
	RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

install(TARGETS Doom3BFG RUNTIME DESTINATION .)
