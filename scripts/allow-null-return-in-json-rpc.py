#!/usr/bin/env python3
"""
Post-processing of the jsonrpcstup output.

The purpose of this script is to post-process the output of 'jsonrpcstub' due to
a limitation of the specification language of jsonrpcstub. The specification
language does not allow the return value of an RPC call to be a JSON null.

The support for a null JSON response is needed for both the "submitblock" request
and the "testblockvalidity" request that, in case of success, returns 'null'.

This script updates the condition that decides whether a response is valid, by
also accepting result.isNull().

This script must be called right after the generation of the JSON RPC client
stub.

"""
import re
from pathlib import Path

SCRIPTS_DIRECTORY = Path(__file__).parent.resolve()
ROOT_DIRECTORY = SCRIPTS_DIRECTORY.parent

BITCOIN_CLIENT_STUB_FILE = ROOT_DIRECTORY / "build" / "specs" / "generated-outputs" / "generated" / "bitcoin_jsonrpc" / "BitcoinClientStub.h"


def allow_null_return_in_json_rpc(stub_file: Path, rpc_method_name: str) -> None:
    """
    Proces the JSON-RPC method given in input.

    Allow 'null' as possible JSON response.
    This function does side-effect on the stub file.

    :param stub_file: the generated client stub file to edit.
    :return: None
    """
    old_content = stub_file.read_text()
    to_replace  = f"this->CallMethod\(\"{rpc_method_name}\",p\);\n            if \(result.isObject\(\)\)"
    replacement = f"this->CallMethod(\"{rpc_method_name}\",p);\n            if (result.isObject() or result.isNull())"
    new_content = re.sub(to_replace, replacement, old_content)
    stub_file.write_text(new_content)


def process_submitblock(stub_file: Path) -> None:
    """
    Allow 'submitblock' Bitcoin JSON-RPC method to return 'null'.

    :param stub_file: the generated client stub file to edit.
    :return: None
    """
    allow_null_return_in_json_rpc(stub_file, "submitblock")


def process_testblockvalidity(stub_file: Path) -> None:
    """
    Allow 'testblockvalidity' Bitcoin JSON-RPC method to return 'null'.

    :param stub_file: the generated client stub file to edit.
    :return: None
    """
    allow_null_return_in_json_rpc(stub_file, "testblockvalidity")


def main():
    if not BITCOIN_CLIENT_STUB_FILE.exists():
        raise ValueError(f"{BITCOIN_CLIENT_STUB_FILE} does not exist")

    process_submitblock(BITCOIN_CLIENT_STUB_FILE)
    process_testblockvalidity(BITCOIN_CLIENT_STUB_FILE)


if __name__ == '__main__':
    try:
        main()
    except Exception as e:
        print("Error: ", str(e))
        exit(1)
