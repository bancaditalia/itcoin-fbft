# Copyright (c) 2023 Bank of Italy
# Distributed under the GNU AGPLv3 software license, see the accompanying COPYING file.

set (LIB_ITCOIN_FBFT itcoin-fbft)
set (LIB_MODULE_PATH "${PROJECT_SOURCE_DIR}/src")
set (LIB_SOURCE_FILES
    blockchain/BitcoinBlockchain.cpp
    blockchain/Blockchain.cpp
    blockchain/extract.cpp
    blockchain/generate.cpp
    blockchain/grind.cpp
    blockchain/HexSerializableCBlock.cpp
    config/FbftConfig.cpp
    fbft/actions/Action.cpp
    fbft/actions/Execute.cpp
    fbft/actions/ProcessNewView.cpp
    fbft/actions/ReceiveBlock.cpp
    fbft/actions/ReceiveCommit.cpp
    fbft/actions/ReceiveNewView.cpp
    fbft/actions/ReceivePrepare.cpp
    fbft/actions/ReceivePrePrepare.cpp
    fbft/actions/ReceiveRequest.cpp
    fbft/actions/ReceiveViewChange.cpp
    fbft/actions/RecoverView.cpp
    fbft/actions/RoastInit.cpp
    fbft/actions/RoastReceivePreSignature.cpp
    fbft/actions/RoastReceiveSignatureShare.cpp
    fbft/actions/SendCommit.cpp
    fbft/actions/SendNewView.cpp
    fbft/actions/SendPrepare.cpp
    fbft/actions/SendPrePrepare.cpp
    fbft/actions/SendViewChange.cpp
    fbft/messages/Block.cpp
    fbft/messages/Commit.cpp
    fbft/messages/Message.cpp
    fbft/messages/NewView.cpp
    fbft/messages/Prepare.cpp
    fbft/messages/PrePrepare.cpp
    fbft/messages/Request.cpp
    fbft/messages/RoastPreSignature.cpp
    fbft/messages/RoastSignatureShare.cpp
    fbft/messages/ViewChange.cpp
    fbft/state/ReplicaState.cpp
    fbft/Replica2.cpp
    transport/btcclient.cpp
    transport/NetworkListener.cpp
    transport/NetworkTransport.cpp
    utils/utils.cpp
    wallet/BitcoinRpcWallet.cpp
    wallet/RoastWalletImpl.cpp
    wallet/RoastWallet.cpp
    wallet/Wallet.cpp
)

set (TEST_SOURCE_FILES_AUX
    test/fixtures/BitcoinInfraFixture.cpp
    test/fixtures/BitcoinRpcTestFixture.cpp
    test/fixtures/BtcClientFixture.cpp
    test/fixtures/PrologTestFixture.cpp
    test/fixtures/ReplicaSetFixture.cpp
    test/fixtures/ReplicaStateFixture.cpp
    test/stubs/DummyBlockchain.cpp
    test/stubs/DummyNetwork.cpp
    test/stubs/DummyWallet.cpp
    test/stubs/DummyRoastWallet.cpp
)

set (TEST_SOURCE_FILES
    test/test_blockchain_generate.cpp
    test/test_blockchain_wallet_bitcoin.cpp
    test/test_blockchain_frost_wallet_bitcoin.cpp
    test/test_messages_encoding.cpp
    test/test_fbft_normal_operation.cpp
    test/test_fbft_replica2.cpp
    test/test_fbft_signing_with_roast.cpp
    test/test_fbft_view_change_empty.cpp
    test/test_fbft_view_change_prepared.cpp
    test/test_transport_btcclient.cpp
    test/test_utils.cpp
)

# set property "GENERATED" for output files of protoc and json-rpc-cpp
set_source_files_properties(${GEN_JSONRPC_BITCOIN_CLIENT_STUB_OUTPUTS} PROPERTIES GENERATED TRUE)

# set includes
set(LIB_ITCOIN_FBFT_INCLUDE_PATH ${LIB_MODULE_PATH})
include_directories(${LIB_ITCOIN_FBFT_INCLUDE_PATH}
    ${THIRDPARTY_INCLUDE_PATH}
    ${GENERATED_INCLUDE_DIR}
)
add_library(${LIB_ITCOIN_FBFT}
    OBJECT
    ${LIB_SOURCE_FILES}
    # Putting tests here so that they are not added as a dependency
    ${TEST_SOURCE_FILES_AUX}
    ${TEST_SOURCE_FILES}
    ${GEN_JSONRPC_BITCOIN_CLIENT_STUB_OUTPUTS}
)
add_dependencies(
    ${LIB_ITCOIN_FBFT}
    generated_src
    boost-libraries
    itcoin-core-libraries
    libjson-rpc-cpp
    SWI-Prolog
)
target_link_libraries (${LIB_ITCOIN_FBFT}
    ${THIRDPARTY_LIBRARIES}
    ${OPENSSL_LIBRARIES}
    ${CURL_LIBRARIES}
)

set (TEST_SOURCE_FILES ${TEST_SOURCE_FILES} PARENT_SCOPE)
set (LIB_ITCOIN_FBFT ${LIB_ITCOIN_FBFT} PARENT_SCOPE)
set (LIB_ITCOIN_FBFT_INCLUDE_PATH  ${LIB_ITCOIN_FBFT_INCLUDE_PATH} PARENT_SCOPE)

# ------------------------------------------------------------------------------
# main
# ------------------------------------------------------------------------------
set(APP_MAIN_NAME main)
add_executable(${APP_MAIN_NAME}
    main.cpp
    transport/zcomm.cpp
)
target_link_libraries(${APP_MAIN_NAME}
    PRIVATE
    ${LIB_ITCOIN_FBFT}
    ${THIRDPARTY_LIBRARIES}
    ${OPENSSL_LIBRARIES}
    ${CURL_LIBRARIES}
    zmq # TODO: use find_package(ZeroMQ REQUIRED) instead
    Threads::Threads
)
target_include_directories(${APP_MAIN_NAME}
    PRIVATE
    ${THIRDPARTY_INCLUDE_PATH}
    ${LIB_ITCOIN_FBFT_INCLUDE_PATH}
    ${GENERATED_INCLUDE_DIR}
)

# ------------------------------------------------------------------------------
# Test targets
# ------------------------------------------------------------------------------
set(APP_MAIN_TEST_NAME main-test)
add_executable(${APP_MAIN_TEST_NAME} main-test.cpp)
target_link_libraries(${APP_MAIN_TEST_NAME}
    ${LIB_ITCOIN_FBFT}
    ${THIRDPARTY_LIBRARIES}
    ${OPENSSL_LIBRARIES}
    ${CURL_LIBRARIES}
    Threads::Threads
)
target_include_directories(${APP_MAIN_TEST_NAME}
    PRIVATE
    ${THIRDPARTY_INCLUDE_PATH}
    ${LIB_ITCOIN_FBFT_INCLUDE_PATH}
    ${GENERATED_INCLUDE_DIR}
)

foreach(test_src ${TEST_SOURCE_FILES})
    # Extract the filename without an extension (NAME_WE)
    get_filename_component(test_name ${test_src} NAME_WE)

    # Finally add it to test execution -
    # Notice the WORKING_DIRECTORY and COMMAND
    add_test(NAME ${test_name}
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
        COMMAND ${CMAKE_CURRENT_BINARY_DIR}/${APP_MAIN_TEST_NAME} --run_test=${test_name} )

    set_property(TEST ${test_name} PROPERTY ENVIRONMENT "LD_LIBRARY_PATH=${LD_LIBRARY_PATH}:${USR_LIB_DIR}")
endforeach(test_src)
