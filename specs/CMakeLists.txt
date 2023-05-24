set(SPECS_DIR ${ROOT_DIR}/specs)

set(GENERATED_INCLUDE_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated-outputs)
set(GENERATED_OUTPUT_DIR ${GENERATED_INCLUDE_DIR}/generated)

file(MAKE_DIRECTORY ${GENERATED_OUTPUT_DIR}/bitcoin_jsonrpc)


set (GEN_JSONRPC_BITCOIN_CLIENT_STUB_OUTPUTS ${GENERATED_OUTPUT_DIR}/bitcoin_jsonrpc/BitcoinClientStub.h)
add_custom_command(
    OUTPUT ${GEN_JSONRPC_BITCOIN_CLIENT_STUB_OUTPUTS}
    COMMAND ${JSONRPCSTUB_BIN} ${SPECS_DIR}/bitcoin_jsonrpc.json
        --cpp-client-file=${GENERATED_OUTPUT_DIR}/bitcoin_jsonrpc/BitcoinClientStub.h
        --cpp-client=BitcoinClientStub
    COMMAND ${ROOT_DIR}/scripts/allow-null-return-in-json-rpc.py
    WORKING_DIRECTORY ${ROOT_DIR}
    DEPENDS ${SPECS_DIR}/bitcoin_jsonrpc.json libjson-rpc-cpp
)

# Prolog pbft engine
file(MAKE_DIRECTORY ${GENERATED_OUTPUT_DIR}/prolog_pbft_engine)
set (GEN_PROLOG_PBFT_ENGINE_OUTPUT ${GENERATED_OUTPUT_DIR}/prolog_pbft_engine/resource_db_mem.h)
add_custom_command(
    OUTPUT ${GEN_PROLOG_PBFT_ENGINE_OUTPUT}
    COMMAND ${LIB_SWI_PROLOG_SWIPL} -f none -F none -g true -t "\"consult(['${SPECS_DIR}/pbft-replica-engine.pl']),qsave_program('resource_db_mem.bin',[goal=version,toplevel=prolog,init_file=none])\""
    COMMAND xxd -i resource_db_mem.bin > ${GEN_PROLOG_PBFT_ENGINE_OUTPUT}
    COMMAND rm resource_db_mem.bin
    WORKING_DIRECTORY ${GENERATED_OUTPUT_DIR}/prolog_pbft_engine
    DEPENDS ${SPECS_DIR}/pbft-replica-engine.pl swi-prolog
)

# add dummy targets
add_custom_target (generated_src DEPENDS ${GEN_PROTOBUF_OUTPUTS} ${GEN_JSONRPC_BITCOIN_CLIENT_STUB_OUTPUTS} ${GEN_PROLOG_PBFT_ENGINE_OUTPUT})

# export vars to parent scope
set (GEN_JSONRPC_BITCOIN_CLIENT_STUB_OUTPUTS ${GEN_JSONRPC_BITCOIN_CLIENT_STUB_OUTPUTS} PARENT_SCOPE)
set (GEN_PROTOBUF_OUTPUTS ${GEN_PROTOBUF_OUTPUTS} PARENT_SCOPE)
set (GENERATED_OUTPUT_DIR ${GENERATED_OUTPUT_DIR} PARENT_SCOPE)
set (GENERATED_INCLUDE_DIR ${GENERATED_INCLUDE_DIR} PARENT_SCOPE)
