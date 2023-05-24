set (LIB_ITCOIN_PBFT itcoin-pbft)
set (LIB_MODULE_PATH "${PROJECT_SOURCE_DIR}/src")
set (LIB_SOURCE_FILES
    block/extract.cpp
    block/generate.cpp
    block/psbt_utils.cpp
    blockchain/BitcoinBlockchain.cpp
    blockchain/Blockchain.cpp
    blockchain/HexSerializableCBlock.cpp
    blockchain/HexSerializablePsbt.cpp
    network/NetworkListener.cpp
    network/NetworkTransport.cpp
    pbft/actions/Action.cpp
    pbft/actions/Execute.cpp
    pbft/actions/ProcessNewView.cpp
    pbft/actions/ReceiveBlock.cpp
    pbft/actions/ReceiveCommit.cpp
    pbft/actions/ReceiveNewView.cpp
    pbft/actions/ReceivePrepare.cpp
    pbft/actions/ReceivePrePrepare.cpp
    pbft/actions/ReceiveRequest.cpp
    pbft/actions/ReceiveViewChange.cpp
    pbft/actions/RecoverView.cpp
    pbft/actions/RoastInit.cpp
    pbft/actions/RoastReceivePreSignature.cpp
    pbft/actions/RoastReceiveSignatureShare.cpp
    pbft/actions/SendCommit.cpp
    pbft/actions/SendNewView.cpp
    pbft/actions/SendPrepare.cpp
    pbft/actions/SendPrePrepare.cpp
    pbft/actions/SendViewChange.cpp
    pbft/messages/Block.cpp
    pbft/messages/Commit.cpp
    pbft/messages/Message.cpp
    pbft/messages/NewView.cpp
    pbft/messages/Prepare.cpp
    pbft/messages/PrePrepare.cpp
    pbft/messages/Request.cpp
    pbft/messages/RoastPreSignature.cpp
    pbft/messages/RoastSignatureShare.cpp
    pbft/messages/ViewChange.cpp
    pbft/state/ReplicaState.cpp
    pbft/Replica2.cpp
    transport/btcclient.cpp
    wallet/ThreeFBFTWalletImpl.cpp
    wallet/BitcoinRpcWallet.cpp
    wallet/RoastWalletImpl.cpp
    wallet/RoastWallet.cpp
    wallet/Wallet.cpp
    PbftConfig.cpp
    utils.cpp
    frost/secp256k1_extension.c
    frost/frost_helpers.cpp
    frost/three_fbft_helpers.cpp
    frost/frost.cpp
)

set (TEST_SOURCE_FILES_AUX
    test/stubs/DummyBlockchain.cpp
    test/stubs/DummyNetwork.cpp
    test/stubs/DummyWallet.cpp
    test/stubs/DummyRoastWallet.cpp
)

set (TEST_SOURCE_FILES
    test/test_5frosted_bft.cpp
    test/test_block_extract.cpp
    test/test_block_generate.cpp
    test/test_block_psbt_utils.cpp
    test/test_blockchain_wallet_bitcoin.cpp
    test/test_blockchain_frost_wallet_bitcoin.cpp
    test/test_blockchain_threefbft_wallet_bitcoin.cpp
    test/test_messages_encoding.cpp
    test/test_pbft_normal_operation.cpp
    test/test_pbft_replica2.cpp
    test/test_pbft_view_change_empty.cpp
    test/test_pbft_view_change_prepared.cpp
    test/test_transport_btcclient.cpp
    test/test_utils.cpp
)

# set property "GENERATED" for output files of protoc and json-rpc-cpp
set_source_files_properties(${GEN_PROTOBUF_OUTPUTS} PROPERTIES GENERATED TRUE)
set_source_files_properties(${GEN_JSONRPC_BITCOIN_CLIENT_STUB_OUTPUTS} PROPERTIES GENERATED TRUE)

# set includes
set(LIB_ITCOIN_PBFT_INCLUDE_PATH ${LIB_MODULE_PATH})
include_directories(${LIB_ITCOIN_PBFT_INCLUDE_PATH}
    ${THIRDPARTY_INCLUDE_PATH}
    ${GENERATED_INCLUDE_DIR}
)
add_library(${LIB_ITCOIN_PBFT}
    OBJECT
    ${LIB_SOURCE_FILES}
    # Putting tests here so that they are not added as a dependency
    ${TEST_SOURCE_FILES_AUX}
    ${TEST_SOURCE_FILES}
    ${GEN_PROTOBUF_OUTPUTS}
    ${GEN_JSONRPC_BITCOIN_CLIENT_STUB_OUTPUTS}
)
add_dependencies(
    ${LIB_ITCOIN_PBFT}
    generated_src
    boost-libraries
    itcoin-core-libraries
    libjson-rpc-cpp
    swi-prolog
    itcoin-secp256k1-libraries
)
target_link_libraries (${LIB_ITCOIN_PBFT}
    ${THIRDPARTY_LIBRARIES}
    ${SQLite3_LIBRARIES}
    ${OPENSSL_LIBRARIES}
    ${CURL_LIBRARIES}
    ${ITCOIN_SECP256K1_LIBRARIES}
)

set (TEST_SOURCE_FILES ${TEST_SOURCE_FILES} PARENT_SCOPE)
set (LIB_ITCOIN_PBFT ${LIB_ITCOIN_PBFT} PARENT_SCOPE)
set (LIB_ITCOIN_PBFT_INCLUDE_PATH  ${LIB_ITCOIN_PBFT_INCLUDE_PATH} PARENT_SCOPE)
