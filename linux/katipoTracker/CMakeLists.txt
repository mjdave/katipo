# Specify the minimum version for CMake

cmake_minimum_required(VERSION 3.5)

# Project's name

project(katipoTracker VERSION 0.1)

set(CMAKE_BUILD_TYPE RelWithDebInfo)

set(GCC_COVERAGE_LINK_FLAGS    "-no-pie")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${GCC_COVERAGE_LINK_FLAGS} -static-libgcc -static-libstdc++")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--disable-new-dtags,-rpath='$ORIGIN'")
set(CMAKE_INSTALL_RPATH "$ORIGIN")

set(SOURCES 
    ../../thirdParty/tui/source/TuiFileUtils.cpp
    ../../thirdParty/tui/source/TuiFunction.cpp
    ../../thirdParty/tui/source/TuiRef.cpp
    ../../thirdParty/tui/source/TuiSha1.cpp
    ../../thirdParty/tui/source/TuiBuiltInFunctions.cpp

    ../../core/ClientHttpInterface.cpp
    ../../core/Database.cpp
    ../../core/NetServerClient.cpp
    ../../core/ServerNetInterface.cpp
    ../../core/ClientNetInterface.cpp
    ../../core/DatabaseEnvironment.cpp
    ../../core/Server.cpp
    ../../core/Timer.cpp

    ../../apps/katipoTracker/Controller.cpp

    main.cpp
)

set(CMAKE_BINARY_DIR ${CMAKE_SOURCE_DIR}/bin)
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR})
set(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR})
set (CMAKE_CXX_STANDARD 11)

if ( CMAKE_COMPILER_IS_GNUCC )
    set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -Wno-deprecated")
endif ( CMAKE_COMPILER_IS_GNUCC )

include_directories(
    ../../core
    ../../thirdParty/tui
    ../../thirdParty/tui/thirdParty/glm/glm

    ../../apps/katipoTracker
)

add_executable(katipoTracker ${SOURCES})

set (CMAKE_CXX_LINK_EXECUTABLE "${CMAKE_CXX_LINK_EXECUTABLE} -ldl")
