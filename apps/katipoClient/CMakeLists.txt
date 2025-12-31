cmake_minimum_required(VERSION 3.5)

project(katipoClient VERSION 0.1)

set(CMAKE_BUILD_TYPE RelWithDebInfo)

IF(APPLE)
    set(CMAKE_THREAD_LIBS_INIT "-lpthread")
    set(CMAKE_HAVE_THREADS_LIBRARY 1)
    set(CMAKE_USE_WIN32_THREADS_INIT 0)
    set(CMAKE_USE_PTHREADS_INIT 1)
    set(THREADS_PREFER_PTHREAD_FLAG ON)
ENDIF()

#set(GCC_COVERAGE_LINK_FLAGS    "-no-pie")
#set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${GCC_COVERAGE_LINK_FLAGS} -static-libgcc -static-libstdc++")
#set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--disable-new-dtags,-rpath='$ORIGIN'")
#set(CMAKE_INSTALL_RPATH "$ORIGIN")
set (CMAKE_CXX_STANDARD 17)
set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
set(THREADS_PREFER_PTHREAD_FLAG TRUE)
find_package(Threads REQUIRED)

set(SOURCES 
    ../../thirdParty/tui/source/TuiFileUtils.cpp
    ../../thirdParty/tui/source/TuiFunction.cpp
    ../../thirdParty/tui/source/TuiRef.cpp
    ../../thirdParty/tui/source/TuiSha1.cpp
    ../../thirdParty/tui/source/TuiBuiltInFunctions.cpp

    ../../thirdParty/enet-1.3.18/callbacks.c
    ../../thirdParty/enet-1.3.18/compress.c
    ../../thirdParty/enet-1.3.18/host.c
    ../../thirdParty/enet-1.3.18/list.c
    ../../thirdParty/enet-1.3.18/packet.c
    ../../thirdParty/enet-1.3.18/peer.c
    ../../thirdParty/enet-1.3.18/protocol.c
    ../../thirdParty/enet-1.3.18/unix.c

    ../../thirdParty/liblmdb/mdb.c
    ../../thirdParty/liblmdb/midl.c

    #../../core/ClientHttpInterface.cpp # requires curl and isn't currently used
    ../../core/Database.cpp
    ../../core/NetServerClient.cpp
    ../../core/ServerNetInterface.cpp
    ../../core/ClientNetInterface.cpp
    ../../core/DatabaseEnvironment.cpp
    ../../core/Server.cpp
    ../../core/Timer.cpp

    ../../apps/katipoClient/Controller.cpp

    main.cpp
)

set(CMAKE_BINARY_DIR ${CMAKE_SOURCE_DIR}/bin)
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR})
set(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR})

if ( CMAKE_COMPILER_IS_GNUCC )
    set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -Wno-deprecated")
endif ( CMAKE_COMPILER_IS_GNUCC )

include_directories(
    ../../core
    ../../thirdParty/tui/source
    ../../thirdParty/enet-1.3.18/include
    ../../thirdParty/liblmdb
    ../../thirdParty/tui/thirdParty/glm/glm
)

add_executable(katipoClient ${SOURCES})
target_link_libraries(katipoClient Threads::Threads)

set (CMAKE_CXX_LINK_EXECUTABLE "${CMAKE_CXX_LINK_EXECUTABLE} -ldl")
