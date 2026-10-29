#
# Doom 3 BFG Edition - Linux port build system
#
# Targets: SDL3 (windowing/input), glad (OpenGL loading),
#          FAudio (XAudio2 API implementation), Bink-Decoder (movies)
#
cmake_minimum_required(VERSION 3.16)

project(Doom3BFG LANGUAGES C CXX)

# Developer menu and console. Off for normal builds; enable with -DDOOM3_DEVELOPER_MODE=ON.
option(DOOM3_DEVELOPER_MODE "Enable the developer menu and console" OFF)
if(DOOM3_DEVELOPER_MODE)
	add_compile_definitions(DOOM3_DEVELOPER_MODE)
endif()

# Opt-in link-time optimization. Configure with -DDOOM3_LTO=ON for a release build.
option(DOOM3_LTO "Enable link-time optimization (LTO/IPO)" OFF)
if(DOOM3_LTO)
	include(CheckIPOSupported)
	check_ipo_supported(RESULT DOOM3_IPO_SUPPORTED OUTPUT DOOM3_IPO_ERROR LANGUAGES C CXX)
	if(NOT DOOM3_IPO_SUPPORTED)
		message(FATAL_ERROR "DOOM3_LTO requested but link-time optimization is not supported: ${DOOM3_IPO_ERROR}")
	endif()
	set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Default to a production Release build. Use -DCMAKE_BUILD_TYPE=Debug or
# RelWithDebInfo only when you explicitly want a development build.
if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE "Release" CACHE STRING "" FORCE)
endif()

set(CMAKE_CXX_FLAGS_DEBUG   "${CMAKE_CXX_FLAGS_DEBUG} -D_DEBUG -O0 -g")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -Os")

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
	message(FATAL_ERROR "This build system only supports Linux.")
endif()

find_package(SDL3 REQUIRED)
find_package(Threads REQUIRED)

# FAudio (implements the XAudio2 API on top of SDL3 audio)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build static libraries" FORCE)
add_subdirectory(neo/libs/FAudio)

add_subdirectory(neo)
