# Copyright (c) 2023 Bank of Italy
# Distributed under the GNU AGPLv3 software license, see the accompanying COPYING file.

cmake_minimum_required(VERSION 3.19 FATAL_ERROR)
project(itcoin-fbft
    LANGUAGES C CXX
    HOMEPAGE_URL https://github.com/bancaditalia/itcoin-fbft.git
    DESCRIPTION "A prototype FBFT implementation for itcoin"
)
set(PROJECT_VERSION "0.1.0")
set(${PROJECT_NAME}_VERSION ${PROJECT_VERSION})

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set(ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(ROOT_CMAKE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

include(ExternalProject)

if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    # require at least gcc 10
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 10)
        message(FATAL_ERROR "GCC version must be at least 10!")
    endif()
# TODO: also support CLang
else()
    message(WARNING "You are using an unsupported compiler! Compilation has only been tested with Clang and GCC.")
endif()

if("${ITCOIN_CORE_SRC_DIR}" STREQUAL "")
    # if ITCOIN_CORE_SRC_DIR was not given on the command line, or its value is
    # an empty string, let's delete it from cache, so that the default behaviour
    # of downloading the repo kicks in.
    unset(ITCOIN_CORE_SRC_DIR CACHE)
endif()

if(DEFINED ITCOIN_CORE_SRC_DIR)
    # if we arrive here, we are sure that ITCOIN_CORE_SRC_DIR is present and not
    # empty.
    #
    # If ITCOIN_CORE_SRC_DIR is a relative path, it will be interpreted relative
    # to the "build" directory.
    get_filename_component(ITCOIN_CORE_SRC_DIR "${ITCOIN_CORE_SRC_DIR}" REALPATH BASE_DIR "${CMAKE_CURRENT_BINARY_DIR}")
    if(NOT IS_DIRECTORY "${ITCOIN_CORE_SRC_DIR}")
        message(FATAL_ERROR "-- ITCOIN_CORE_SRC_DIR points to '${ITCOIN_CORE_SRC_DIR}', which is not a directory")
    endif()
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

message("-- Using build type ${CMAKE_BUILD_TYPE}")

if((ENABLE_COVERAGE) AND (CMAKE_BUILD_TYPE STREQUAL "Debug"))
    message("-- Code coverage enabled")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fprofile-arcs")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -ftest-coverage")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fPIC")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --coverage")
endif()

if(PROFILE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-omit-frame-pointer")
endif()


add_definitions(-DITCOIN_FBFT_VERSION="${PROJECT_VERSION}")
# make ROOT_DIR available as preprocessor definition
add_definitions(-DROOT_DIR="${ROOT_DIR}")

add_compile_definitions(BOOST_TEST_DYN_LINK)
add_compile_definitions(BOOST_LOG_DYN_LINK)
add_compile_definitions(DEBUG_LOCKORDER)
add_compile_definitions(HAVE_CONFIG_H)
add_compile_definitions(__SWI_PROLOG__)
add_compile_definitions(__SWI_EMBEDDED__)

enable_testing()

# set up usrlocal directory
set(USR_DIR ${CMAKE_CURRENT_BINARY_DIR}/usrlocal)
set(USR_LIB_DIR ${USR_DIR}/lib)
file(MAKE_DIRECTORY ${USR_DIR})

# MUXATOR 2022-07-09: pre-arrange the build/usrlocal layout for compatibility
# with distributions (usually Red Hat based) which split the libraries between
# lib and lib64: immediately create the directory usrlocal/lib, and symlink
# usrlocal/lib64 to it.
#
# This is harmless for debian-based distros and (for example) fixes Fedora,
# where almost everything would be installed in lib64, except for boost, which
# would end up in lib and would not be found later.
#
# Anyway, we are still talking about an ugly hack that needs to be removed.
file(MAKE_DIRECTORY ${USR_LIB_DIR})
file(CREATE_LINK "${USR_LIB_DIR}" "${USR_DIR}/lib64" SYMBOLIC)

list(APPEND CMAKE_LIBRARY_PATH ${USR_DIR}/lib)
list(APPEND CMAKE_PROGRAM_PATH ${USR_DIR}/bin)
list(APPEND CMAKE_INCLUDE_PATH ${USR_DIR}/include)
# directory where all external project libraries reside
link_directories(${USR_DIR}/lib)
link_directories(${USR_DIR}/lib/swipl/lib/x86_64-linux)
include_directories(SYSTEM ${USR_DIR}/include)

set(CMAKE_MODULE_PATH
    "${CMAKE_MODULE_PATH}"
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake"
)


set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
find_package(CURL REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(argtable REQUIRED)

#
# Add third party dependencies
#

add_subdirectory(thirdparty)

#
# Generated files
#

set(GENERATED_INCLUDE_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated-include-dir)
set(GENERATED_OUTPUT_DIR ${GENERATED_INCLUDE_DIR}/generated)

add_subdirectory(specs)
add_subdirectory(engine)

add_custom_target (generated_src DEPENDS generated_specs generated_engine)

#
# Cpp src files
#

add_subdirectory(src)
