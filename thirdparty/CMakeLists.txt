# Copyright (c) 2023 Bank of Italy
# Distributed under the GNU AGPLv3 software license, see the accompanying COPYING file.

# CMake build : third party

#configure directories
set (THIRDPARTY_MODULE_PATH "${PROJECT_SOURCE_DIR}/thirdparty")

# ------------------------------------------------------------------------------
# libjson-cpp
# ------------------------------------------------------------------------------
ExternalProject_Add(jsoncpp
    GIT_REPOSITORY https://github.com/open-source-parsers/jsoncpp.git
    GIT_TAG 1.9.5
    GIT_PROGRESS true
    CMAKE_ARGS
        # do not define CMAKE_C_COMPILER here, otherwise we would have a warning
        # because this library does not use it
        -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
        -DCMAKE_INSTALL_PREFIX=${USR_DIR}
)

ExternalProject_Get_Property(jsoncpp SOURCE_DIR BINARY_DIR)
set(LIB_JSON_CPP_SOURCE_DIR ${SOURCE_DIR})
set(LIB_JSON_CPP_BINARY_DIR ${BINARY_DIR})

set(JSON_CPP_LIBRARIES
    ${USR_LIB_DIR}/libjsoncpp.so
)

set(LIB_JSON_CPP_INCLUDE_DIR ${LIB_JSON_CPP_SOURCE_DIR}/include)
include_directories(${LIB_JSON_CPP_INCLUDE_DIR})

# ------------------------------------------------------------------------------
# libjson-rpc-cpp
# ------------------------------------------------------------------------------
ExternalProject_Add(libjson-rpc-cpp
    GIT_REPOSITORY https://github.com/cinemast/libjson-rpc-cpp
    GIT_TAG v1.3.0
    GIT_PROGRESS true
    CMAKE_ARGS
        -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
        -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
        -DCMAKE_INSTALL_PREFIX=${USR_DIR}
        -DJSONCPP_WITH_TESTS=NO
        -DJSONCPP_WITH_POST_BUILD_UNITTEST=NO
        -DCOMPILE_TESTS=NO
        -DREDIS_SERVER=NO
        -DREDIS_CLIENT=NO
        -DHTTP_SERVER=NO
    DEPENDS jsoncpp
)

ExternalProject_Get_Property(libjson-rpc-cpp SOURCE_DIR BINARY_DIR)
set(LIB_JSON_RPC_CPP_SOURCE_DIR ${SOURCE_DIR})
set(LIB_JSON_RPC_CPP_BINARY_DIR ${BINARY_DIR})

set(JSON_RPC_CPP_LIBRARIES
    ${USR_LIB_DIR}/libjsonrpccpp-common.so
    ${USR_LIB_DIR}/libjsonrpccpp-client.so
)

set(LIB_JSON_RPC_CPP_INCLUDE_DIR ${LIB_JSON_RPC_CPP_SOURCE_DIR}/src/jsonrpccpp)
include_directories(${LIB_JSON_RPC_CPP_INCLUDE_DIR})

set(JSONRPCSTUB_BIN ${LIB_JSON_RPC_CPP_BINARY_DIR}/bin/jsonrpcstub PARENT_SCOPE)

# ------------------------------------------------------------------------------
# libzmq
# ------------------------------------------------------------------------------
# libzmq is compiled with draft mode enabled, because we need RADIO/DISH.
ExternalProject_Add(libzmq
    GIT_REPOSITORY https://github.com/zeromq/libzmq
    GIT_TAG v4.3.4
    GIT_PROGRESS true
    CMAKE_ARGS
        -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
        -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
        -DCMAKE_INSTALL_PREFIX=${USR_DIR}
        -DCMAKE_BUILD_TYPE=Release
        -DWITH_PERF_TOOL=OFF
        -DZMQ_BUILD_TESTS=OFF
        -DENABLE_CURVE=OFF
        -DENABLE_DRAFTS=ON
)

ExternalProject_Get_Property(libzmq SOURCE_DIR BINARY_DIR)
set(LIB_ZMQ_SOURCE_DIR ${SOURCE_DIR})
set(LIB_ZMQ_BINARY_DIR ${BINARY_DIR})

set(ZMQ_LIBRARIES
    ${USR_LIB_DIR}/libzmq.so
)

# The libzmq block was originally copied from the "libjson-rpc-cpp" block.
# I have a doubt: why is LIB_JSON_RPC_CPP_INCLUDE_DIR created and added to the
# include directories?
# I think that - as part of the install step - all the necessary files should
# already be in usrlocal/include, isn't it?
#set(LIB_ZMQ_INCLUDE_DIR ${LIB_ZMQ_SOURCE_DIR}/src/jsonrpccpp)
#include_directories(${LIB_ZMQ_INCLUDE_DIR})
# ------------------------------------------------------------------------------

# ------------------------------------------------------------------------------
# cppzmq
# ------------------------------------------------------------------------------
ExternalProject_Add(cppzmq
    GIT_REPOSITORY https://github.com/zeromq/cppzmq.git
    GIT_TAG d96155cbc36a # v4.9.0+
    GIT_PROGRESS true
    CMAKE_ARGS
        -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
        -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
        -DCMAKE_INSTALL_PREFIX=${USR_DIR}
        -DCMAKE_BUILD_TYPE=Release
        -DENABLE_DRAFTS=ON
        -DCPPZMQ_BUILD_TESTS=OFF
    DEPENDS libzmq
)

ExternalProject_Get_Property(cppzmq SOURCE_DIR BINARY_DIR)
set(CPPZMQ_SOURCE_DIR ${SOURCE_DIR})
set(CPPZMQ_BINARY_DIR ${BINARY_DIR})
# ------------------------------------------------------------------------------

# ------------------------------------------------------------------------------
# boost
# ------------------------------------------------------------------------------
set(MINIMAL_BOOST_VERSION 1.71)
set(REQUIRED_BOOST_PACKAGES filesystem log program_options thread unit_test_framework)
find_package(Boost ${MINIMAL_BOOST_VERSION} COMPONENTS ${REQUIRED_BOOST_PACKAGES})
if (Boost_FOUND)
    message("Boost version ${Boost_VERSION} (>=${MINIMAL_BOOST_VERSION}) was found in the system at include dir: ${Boost_INCLUDE_DIRS}, lib dir: ${Boost_LIBRARY_DIRS}. The found libraries were: ${Boost_LIBRARIES}. Will use those ones.")
    # MUXATOR: we should use Boost_INCLUDE_DIRS and Boost_LIBRARIES and add them
    # via target_include_directories() and target_include_libraries() to our
    # binaries (main, main-test, ecc.). However, in its current state, our cmake
    # infrastructure does not support it easily.
    #
    # We can temporarily forego this, because - for the simple case of a
    # system-level installed boost - not adding anything to our targets will
    # work anyway.

    # Super HACK (it may or may not be related to the previous point): in ubuntu
    # 22.04 (cmake 3.22, boost 1.74) the Boost_LIBRARIES variable contains the
    # symbolic names of the libraries ("Boost::filesystem;Boost::log;
    # Boost::thread;Boost::unit_test_framework;Boost::program_options").
    #
    # In Fedora 36, instead (cmake 3.24, boost 1.76), Boost_LIBRARIES contains
    # the full path to the shared libraries (which is the expected behaviour).
    #
    # However, in both operating systems Boost_<COMPONENT>_LIBRARY_RELEASE
    # contains - correctly - an absolute path to the shared libraries (see
    # https://cmake.org/cmake/help/v3.22/module/FindBoost.html#cache-variables).
    #
    # Thus, let's recompute Boost_LIBRARIES from those variables and be
    # compatible with both OS versions.
    set(Boost_LIBRARIES
        ${Boost_FILESYSTEM_LIBRARY_RELEASE}
        ${Boost_LOG_LIBRARY_RELEASE}
        ${Boost_PROGRAM_OPTIONS_LIBRARY_RELEASE}
        ${Boost_THREAD_LIBRARY_RELEASE}
        ${Boost_UNIT_TEST_FRAMEWORK_LIBRARY_RELEASE}
    )
else()
    message("Could not Boost version ${MINIMAL_BOOST_VERSION} or greater. The required boost binary libraries were: ${REQUIRED_BOOST_PACKAGES}. Boost will be built from source")
    set(Boost_LIBRARIES
        ${USR_LIB_DIR}/libboost_filesystem.so.1.75.0
        ${USR_LIB_DIR}/libboost_log.so.1.75.0
        ${USR_LIB_DIR}/libboost_log_setup.so.1.75.0
        ${USR_LIB_DIR}/libboost_thread.so.1.75.0
        ${USR_LIB_DIR}/libboost_unit_test_framework.so.1.75.0
    )
    ExternalProject_Add(_boost-download-sources
        URL https://boostorg.jfrog.io/artifactory/main/release/1.75.0/source/boost_1_75_0.tar.gz
        URL_HASH SHA256=aeb26f80e80945e82ee93e5939baebdca47b9dee80a07d3144be1e1a6a66dd6a
        BUILD_IN_SOURCE true
        CONFIGURE_COMMAND ""
        BUILD_COMMAND ""
        INSTALL_COMMAND ""
    )
    ExternalProject_Get_Property(_boost-download-sources SOURCE_DIR)
    set(LIB_BOOST_SOURCE_DIR ${SOURCE_DIR})
    add_custom_command(
        OUTPUT ${Boost_LIBRARIES}
        WORKING_DIRECTORY ${LIB_BOOST_SOURCE_DIR}
        # The method for building boost with a custom gcc compiler is loosely based
        # on:
        #     https://github.com/bancaditalia/CoronABM/blob/973a6e967e02d17ae0c066af65e6c909738657d5/code/cppsim/build-boost.sh
        # which also supports clang, by the way.
        #
        # Please note the weird escaping of '\;': it is needed because otherwise
        # cmake would do tricks with escaping, ending up interpreting the snippet
        # as:
        #     echo stuff\ no\ quotes\ escaped\ spaces ; > user-config.jam
        #
        # This would result in an empty user-config.jam being created.
        COMMAND CXX=${CMAKE_CXX_COMPILER} ./bootstrap.sh --prefix=${USR_DIR} --with-toolset=gcc && echo "using gcc : : ${CMAKE_CXX_COMPILER} \;" > tools/build/src/user-config.jam && ./b2 install toolset=gcc --without-python --without-mpi --without-graph_parallel
        DEPENDS _boost-download-sources
    )
endif()
add_custom_target(boost-libraries DEPENDS ${Boost_LIBRARIES})
# ------------------------------------------------------------------------------


# ------------------------------------------------------------------------------
# itcoin-core
# ------------------------------------------------------------------------------
set(EXTERNAL_PROJECT_ITCOIN_CORE_TARGET_NAME _itcoin-core-external-project)
if (ITCOIN_CORE_SRC_DIR)
    message("itcoin-core project will be compiled from the following local directory: ${ITCOIN_CORE_SRC_DIR}")
    set(ITCOIN_CORE_MODE_DEPENDENT_VARS
        SOURCE_DIR "${ITCOIN_CORE_SRC_DIR}"
    )
else()
    if(NOT ITCOIN_CORE_GIT_REV)
        # default itcoin-core branch
        set(ITCOIN_CORE_GIT_REV itcoin)
    endif()
    if(NOT ITCOIN_CORE_GIT_URL)
        # default to SSH URL
        set(ITCOIN_CORE_GIT_URL https://github.com/bancaditalia/itcoin-core)
    endif()
    message("itcoin-core project will be downloaded from ${ITCOIN_CORE_GIT_URL}, and revision ${ITCOIN_CORE_GIT_REV} will be checked out")
    set(ITCOIN_CORE_SRC_DIR ${CMAKE_CURRENT_BINARY_DIR}/${EXTERNAL_PROJECT_ITCOIN_CORE_TARGET_NAME}-prefix/src/${EXTERNAL_PROJECT_ITCOIN_CORE_TARGET_NAME})
    set(ITCOIN_CORE_MODE_DEPENDENT_VARS
        GIT_REPOSITORY "${ITCOIN_CORE_GIT_URL}"
        GIT_TAG "${ITCOIN_CORE_GIT_REV}"
        GIT_PROGRESS true
    )
endif()

set(ITCOIN_CORE_LIBRARIES
    ${ITCOIN_CORE_SRC_DIR}/src/libbitcoin_common.a
    ${ITCOIN_CORE_SRC_DIR}/src/libbitcoin_consensus.a
    ${ITCOIN_CORE_SRC_DIR}/src/libbitcoin_util.a
    ${ITCOIN_CORE_SRC_DIR}/src/crypto/.libs/libbitcoin_crypto_base.a
    ${ITCOIN_CORE_SRC_DIR}/src/crypto/.libs/libbitcoin_crypto_avx2.a
    ${ITCOIN_CORE_SRC_DIR}/src/crypto/.libs/libbitcoin_crypto_x86_shani.a
    ${ITCOIN_CORE_SRC_DIR}/src/crypto/.libs/libbitcoin_crypto_sse41.a
    ${ITCOIN_CORE_SRC_DIR}/src/.libs/libunivalue.a
    ${ITCOIN_CORE_SRC_DIR}/src/secp256k1/.libs/libsecp256k1.a
)

list(JOIN ITCOIN_CORE_LIBRARIES " " ITCOIN_CORE_LIBRARIES_STR)

ExternalProject_Add(${EXTERNAL_PROJECT_ITCOIN_CORE_TARGET_NAME}
    ${ITCOIN_CORE_MODE_DEPENDENT_VARS}
    BUILD_IN_SOURCE true
    BUILD_ALWAYS true
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ${ROOT_DIR}/scripts/_build-itcoin-core-if-changed.sh "${ITCOIN_CORE_LIBRARIES_STR}" "${CMAKE_CURRENT_BINARY_DIR}" "${ITCOIN_CORE_SRC_DIR}" "${USR_DIR}" "${CMAKE_CXX_COMPILER}" "${CMAKE_C_COMPILER}"
    INSTALL_COMMAND ""
    DEPENDS boost-libraries libzmq
)


set(LIB_ITCOIN_CORE_INCLUDE_PATH
    ${ITCOIN_CORE_SRC_DIR}/src
    ${ITCOIN_CORE_SRC_DIR}/src/univalue/include
)
add_custom_target(itcoin-core-libraries DEPENDS _itcoin-core-external-project ${ITCOIN_CORE_LIBRARIES})
# ------------------------------------------------------------------------------

# -------
# SWI-cpp
# If you want to skip the prolog build, install swi-prolog in the system.
# E.g. in Ubuntu 22.04 you can install swi-prolog stable from https://www.swi-prolog.org/build/PPA.html
# -------
set(LIB_SWI_PROLOG_VERSION 8.4.2)
find_package(SWI-Prolog ${LIB_SWI_PROLOG_VERSION}...<9.1.0)

if (SWI-Prolog_FOUND)
    message("SWI-Prolog will be used from the system.")
    set(LIB_SWI_PROLOG_SWIPL swipl)
    set(LIB_SWI_PROLOG_INCLUDE_PATH
        ${THIRDPARTY_MODULE_PATH}/SWI-cpp
        ${SWI-Prolog_INCLUDE_DIR}
    )
    set(LIB_SWI_PROLOG_LIBRARIES ${SWI-Prolog_LIBRARIES})
    add_custom_target(SWI-Prolog)
else()
    message("SWI-Prolog will be compiled from source.")
    ExternalProject_Add(SWI-Prolog
        GIT_REPOSITORY https://github.com/SWI-Prolog/swipl
        GIT_TAG V${LIB_SWI_PROLOG_VERSION}
        GIT_PROGRESS true
        CMAKE_ARGS
            -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
            -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
            -DCMAKE_INSTALL_PREFIX=${USR_DIR}
            -DSWIPL_PACKAGES_ODBC=OFF
            -DSWIPL_PACKAGES_JAVA=OFF
            -DSWIPL_PACKAGES_X=OFF
            -DBUILD_TESTING=OFF
            -DINSTALL_TESTS=OFF
            -DINSTALL_DOCUMENTATION=OFF
    )
    set(LIB_SWI_PROLOG_SWIPL ${USR_DIR}/bin/swipl)
    set(LIB_SWI_PROLOG_INCLUDE_PATH
        ${THIRDPARTY_MODULE_PATH}/SWI-cpp
        ${USR_DIR}/lib/swipl/include
    )
    set(LIB_SWI_PROLOG_LIBRARIES swipl)
endif()
message(STATUS "SWI-Prolog - swipl executable: ${LIB_SWI_PROLOG_SWIPL}")
message(STATUS "SWI-Prolog - include path: ${LIB_SWI_PROLOG_INCLUDE_PATH}")
message(STATUS "SWI-Prolog - libraries to link: ${LIB_SWI_PROLOG_LIBRARIES}")

#set variables
set (THIRDPARTY_INCLUDE_PATH
    ${LIB_SHA256_INCLUDE_PATH}
    ${LIB_ITCOIN_CORE_INCLUDE_PATH}
    ${LIB_SWI_PROLOG_INCLUDE_PATH}
)

set (THIRDPARTY_LIBRARIES
    ${ITCOIN_CORE_LIBRARIES}
    ${JSON_RPC_CPP_LIBRARIES}
    ${JSON_CPP_LIBRARIES}
    ${Boost_LIBRARIES}
    ${LIB_SHA256_NAME}
    ${LIB_SWI_PROLOG_LIBRARIES}
)

if (NOT Boost_FOUND)
    # If boost was compiled from source, we will need to explicitly link the
    # custom-built libraries.
    list(APPEND ${THIRDPARTY_LIBRARIES} ${Boost_LIBRARIES})
endif()

#export vars
set (LIB_SWI_PROLOG_SWIPL ${LIB_SWI_PROLOG_SWIPL} PARENT_SCOPE)
set (THIRDPARTY_INCLUDE_PATH ${THIRDPARTY_INCLUDE_PATH} PARENT_SCOPE)
set (THIRDPARTY_LIBRARIES ${THIRDPARTY_LIBRARIES} PARENT_SCOPE)
set (PROTOC_BINARY ${PROTOC_BINARY} PARENT_SCOPE)

# Prints the contents of ITCOIN_CORE_LIBRARIES as a multiline string when
# invoked via "make print_itcoin_core_libraries". This is used in the CI
# pipeline.
#
# Normally, CMake TARGET_MESSAGES global property will be true, so this will
# also print "Built target print_itcoin_core_libraries", which will have to be
# grepped away.
#
# SAMPLE:
#     (set -o pipefail ; make print_itcoin_core_libraries  | grep -v "Built target" )
list(JOIN ITCOIN_CORE_LIBRARIES "\\n" ITCOIN_CORE_LIBRARIES_MULTILINE)
add_custom_target(print_itcoin_core_libraries
    COMMAND echo -e "\"${ITCOIN_CORE_LIBRARIES_MULTILINE}\""
)
