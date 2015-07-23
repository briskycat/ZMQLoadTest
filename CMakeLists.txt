cmake_minimum_required(VERSION 3.2)

project(ZMQLoadTest)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")

if(NOT Sodium_ROOT_DIR AND ThirdPartyLibsDir)
    set(Sodium_ROOT_DIR ${ThirdPartyLibsDir})
endif()
if(NOT ZeroMQ_ROOT_DIR AND ThirdPartyLibsDir)
    set(ZeroMQ_ROOT_DIR ${ThirdPartyLibsDir})
endif()

if(MSVC)
    if(MSVC_VERSION LESS 1800)
        message(FATAL_ERROR "Insufficient MSVC version. This software must be compiled with VC++ 18.00 (AKA VC++ 120, Visual Studio 2013) or newer.")
    endif()

    macro(get_WIN32_WINNT version)
        if (WIN32 AND CMAKE_SYSTEM_VERSION)
            set(ver ${CMAKE_SYSTEM_VERSION})
            string(REPLACE "." "" ver ${ver})
            string(REGEX REPLACE "([0-9])" "0\\1" ver ${ver})

            set(${version} "0x${ver}")
        endif()
    endmacro()

    get_WIN32_WINNT(ver)
    add_definitions(-D_WIN32_WINNT=${ver})
    add_definitions("/W3 /wd4005 /wd4996 /nologo -D_CRT_SECURE_NO_WARNINGS")

    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /bigobj")

elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    include(CheckCXXCompilerFlag)
    CHECK_CXX_COMPILER_FLAG("-std=c++11" COMPILER_SUPPORTS_CXX11)
    if(COMPILER_SUPPORTS_CXX11)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
    else()
        message(FATAL_ERROR "The compiler ${CMAKE_CXX_COMPILER} has no C++11 support. Please use a different C++ compiler.")
    endif()

    CHECK_CXX_COMPILER_FLAG("-stdlib=libc++" COMPILER_SUPPORTS_STDLIBLIBCXX)
    if(COMPILER_SUPPORTS_STDLIBLIBCXX)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
    endif()

    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wshadow -pedantic")
endif()

include(GenerateExportHeader)
# restrict export of symbols from shared libraries
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN 1)

if(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
    add_definitions( -DBOOST_ALL_NO_LIB )
    set( Boost_USE_STATIC_LIBS ON )
    add_definitions( -DZMQ_STATIC )
else()
    set( Boost_USE_STATIC_LIBS OFF )
endif()
set(Boost_USE_MULTITHREADED ON)
add_definitions( -DBOOST_FILESYSTEM_NO_DEPRECATED )
find_package( Boost 1.58 REQUIRED COMPONENTS program_options system thread)
find_package( Sodium REQUIRED )
find_package( ZeroMQ REQUIRED )
find_package( CPPZMQ REQUIRED )
find_package( Spdlog REQUIRED )

# output directory for static libraries
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
# output directory for executables and dynamic libraries on Windows
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")


add_executable (zmqsend src/zmqsend.cpp)
target_include_directories (zmqsend SYSTEM PRIVATE ${Boost_INCLUDE_DIRS} ${ZeroMQ_INCLUDE_DIR} ${CPPZMQ_INCLUDE_DIR} ${Spdlog_INCLUDE_DIR})
target_link_libraries (zmqsend ${Boost_SYSTEM_LIBRARY} ${Boost_PROGRAM_OPTIONS_LIBRARY} ${ZeroMQ_LIBRARIES} ${Sodium_LIBRARY})

add_executable (zmqreceive src/zmqreceive.cpp)
target_include_directories (zmqreceive SYSTEM PRIVATE ${Boost_INCLUDE_DIRS} ${ZeroMQ_INCLUDE_DIR} ${CPPZMQ_INCLUDE_DIR} ${Spdlog_INCLUDE_DIR})
target_link_libraries (zmqreceive ${Boost_SYSTEM_LIBRARY} ${Boost_PROGRAM_OPTIONS_LIBRARY} ${ZeroMQ_LIBRARIES} ${Sodium_LIBRARY})
