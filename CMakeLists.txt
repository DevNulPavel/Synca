project(synca)
cmake_minimum_required(VERSION 2.8)

# Если у нас есть компилятор clang на юникс - используем его
if (UNIX)
    message("********* Unix compilation check *********")

    # Используем ccache (если есть) для ускорения сборки
    find_program(CCACHE_FOUND ccache)
    if(CCACHE_FOUND)
        message("-> ССache will used for compilation")
        set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ccache)
        set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK ccache)
    endif(CCACHE_FOUND)

    # Вариант с путями
    # execute_process(COMMAND which clang OUTPUT_VARIABLE CLANG_PATH)
    # execute_process(COMMAND which clang++ OUTPUT_VARIABLE CLANGXX_PATH)
    # if(NOT ("${CLANG_PATH}" STREQUAL ""))
    find_program(CLANG_FOUND clang)
    if(CLANG_FOUND)
        message("-> Clang will used for compilation")
        set(CMAKE_C_COMPILER "clang")
        set(CMAKE_CXX_COMPILER "clang++")
        # Скрываем ворнинги на clang + цвета
        set(CUSTOM_COMPILER_C_FLAGS "-Wno-error=unused-command-line-argument -Qunused-arguments -fcolor-diagnostics")
        set(CUSTOM_COMPILER_CXX_FLAGS "-Wno-error=unused-command-line-argument -Qunused-arguments -fcolor-diagnostics")
    endif(CLANG_FOUND)
    message("**********************************")
endif()

option(STATIC_ALL "Use static libraries" ON)
option(LOG_MUTEX "Use log output under mutex" ON)
option(LOG_DEBUG "Use debug output" ON)

if(LOG_MUTEX)
    add_definitions(-DflagLOG_MUTEX)
endif()

if(LOG_DEBUG)
    add_definitions(-DflagLOG_DEBUG)
endif()

if("${CMAKE_CXX_COMPILER_ID}" MATCHES "GNU" OR "${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
    set(GCC_LIKE_COMPILER ON)
endif()

if(NOT MSVC AND NOT GCC_LIKE_COMPILER)
    message(FATAL_ERROR "Supports only gcc, clang or msvc compilers")
endif()

if(STATIC_ALL)
    set(Boost_USE_STATIC_LIBS ON)
    set(Boost_USE_STATIC_RUNTIME ON)
    if(MSVC)
        set(MSVC_RUNTIME static)
    endif()
    #if(GCC_LIKE_COMPILER)
    #    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static")
    #endif()
else()
    set(Boost_USE_STATIC_LIBS OFF)
    set(Boost_USE_STATIC_RUNTIME OFF)
    if(MSVC)
        set(MSVC_RUNTIME dynamic)
    endif()
endif()

if(MSVC)
    add_definitions(-DflagMSC)
    add_definitions(-D_WIN32_WINNT=0x0501)
    add_definitions(-DBOOST_ASIO_HAS_MOVE)
    include(MSVCRuntime.cmake)
    configure_msvc_runtime()
endif()

if(GCC_LIKE_COMPILER)
    add_definitions(-std=c++11)
    if("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
        add_definitions(-stdlib=libc++)
    endif()
    find_package(Threads)
endif()

set(Boost_USE_MULTITHREADED ON)

find_package(Boost 1.56 REQUIRED COMPONENTS thread context coroutine system date_time regex)

# Проверка версии буста
set(BOOST_VERSION "${Boost_MAJOR_VERSION}.${Boost_MINOR_VERSION}")
set(BOOST_CHECK_VERSION "1.60")
message("Boost version check ${BOOST_VERSION}")
if(${BOOST_VERSION} VERSION_GREATER ${BOOST_CHECK_VERSION} OR ${BOOST_VERSION} VERSION_EQUAL ${BOOST_CHECK_VERSION})
    message("New coro code compilation")
    add_definitions(-DCORO_NEW)
endif()

file(GLOB SYNCA_SRC src/*)
file(GLOB SYNCA_HDR include/*)

include_directories(include ${Boost_INCLUDE_DIR})

add_library(synca ${SYNCA_SRC} ${SYNCA_HDR})
target_link_libraries(synca ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})

add_subdirectory(tests)
add_subdirectory(examples)

