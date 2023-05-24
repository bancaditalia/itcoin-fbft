# Copyright (c) 2023 Bank of Italy
# Distributed under the GNU AGPLv3 software license, see the accompanying COPYING file.

set(THIS_DIR ${ROOT_DIR}/specs)

file(MAKE_DIRECTORY ${GENERATED_OUTPUT_DIR}/bitcoin_jsonrpc)
set (GEN_JSONRPC_BITCOIN_CLIENT_STUB_OUTPUTS ${GENERATED_OUTPUT_DIR}/bitcoin_jsonrpc/BitcoinClientStub.h)
add_custom_command(
    OUTPUT ${GEN_JSONRPC_BITCOIN_CLIENT_STUB_OUTPUTS}
    COMMAND ${JSONRPCSTUB_BIN} ${THIS_DIR}/bitcoin_jsonrpc.json
        --cpp-client-file=${GENERATED_OUTPUT_DIR}/bitcoin_jsonrpc/BitcoinClientStub.h
        --cpp-client=BitcoinClientStub
    COMMAND ${ROOT_DIR}/scripts/allow-null-return-in-json-rpc.py --build-dir ${CMAKE_BINARY_DIR}
    WORKING_DIRECTORY ${ROOT_DIR}
    DEPENDS ${THIS_DIR}/bitcoin_jsonrpc.json libjson-rpc-cpp
)

# export vars to parent scope
set (GEN_JSONRPC_BITCOIN_CLIENT_STUB_OUTPUTS ${GEN_JSONRPC_BITCOIN_CLIENT_STUB_OUTPUTS} PARENT_SCOPE)

add_custom_target (generated_specs DEPENDS ${GEN_JSONRPC_BITCOIN_CLIENT_STUB_OUTPUTS})
