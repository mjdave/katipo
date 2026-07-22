cmake_minimum_required(VERSION 3.5)

project(katipoHost VERSION 0.1)

set(CMAKE_BUILD_TYPE RelWithDebInfo)

#set(KATIPO_COMPILE_WITH_HTTP_INTERFACE TRUE) #allows hosts to send http get requests, but requires curl. Also needs to be defined in katipoHost/Controller.h

IF(APPLE)
    set(CMAKE_THREAD_LIBS_INIT "-lpthread")
    set(CMAKE_HAVE_THREADS_LIBRARY 1)
    set(CMAKE_USE_WIN32_THREADS_INIT 0)
    set(CMAKE_USE_PTHREADS_INIT 1)
    set(THREADS_PREFER_PTHREAD_FLAG ON)
ENDIF()

set (CMAKE_CXX_STANDARD 17)
set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
set(THREADS_PREFER_PTHREAD_FLAG TRUE)
find_package(Threads REQUIRED)


set(SOURCES 
    ../../thirdParty/tui/source/TuiFileUtils.cpp
    ../../thirdParty/tui/source/TuiFunction.cpp
    ../../thirdParty/tui/source/TuiRef.cpp
    ../../thirdParty/tui/source/TuiTable.cpp
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

    ../../core/Database.cpp
    ../../core/NetServerClient.cpp
    ../../core/ServerNetInterface.cpp
    ../../core/ClientNetInterface.cpp
    ../../core/DatabaseEnvironment.cpp
    ../../core/Server.cpp
    ../../core/Timer.cpp

    ../../apps/katipoHost/Controller.cpp

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

add_executable(katipoHost ${SOURCES})

if(KATIPO_COMPILE_WITH_HTTP_INTERFACE)
    find_package(CURL REQUIRED) #optional
    target_sources(katipoHost PRIVATE ../../core/ClientHttpInterface.cpp) #optional
    target_link_libraries(katipoHost CURL::libcurl) #optional
endif()

if(APPLE)
    include_directories(../../thirdParty/libsodium/macos/include)
    target_link_libraries(katipoHost ${CMAKE_SOURCE_DIR}/../../thirdParty/libsodium/macos/lib/libsodium.a)
elseif(WIN32)
    include_directories(../../thirdParty/libsodium/windows/include)
    #target_link_libraries(katipoHost ${CMAKE_SOURCE_DIR}/../../thirdParty/libsodium/windows/lib/libsodium.a) #hmm
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64" OR CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
    include_directories(../../thirdParty/libsodium/pi/include)
    target_link_libraries(katipoHost ${CMAKE_SOURCE_DIR}/../../thirdParty/libsodium/pi/lib/libsodium.a)
else()
    include_directories(../../thirdParty/libsodium/linux/include)
    target_link_libraries(katipoHost ${CMAKE_SOURCE_DIR}/../../thirdParty/libsodium/linux/lib/libsodium.a)
endif()

target_link_libraries(katipoHost Threads::Threads)

set (CMAKE_CXX_LINK_EXECUTABLE "${CMAKE_CXX_LINK_EXECUTABLE} -ldl")
