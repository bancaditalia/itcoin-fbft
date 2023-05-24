import os
import sys

#
# Add itcoin-core folders to PYTHONPATH
#

def configure_sys_paths():
    PATH_BASE_CURRENT_MODULE = os.path.abspath(os.path.dirname(os.path.realpath(__file__)))
    PATHS_TO_ADD = [
        os.path.abspath(os.path.join(PATH_BASE_CURRENT_MODULE, *s))
        for s in [
            ["..","..", "..", "..", "toxiproxy-python"],
            ["..","..", "..", "..", "itcoin-core", "test", "functional"],
            ["..","..", "..", "..", "itcoin-core", "contrib", "signet"],
            ["..", "..", "..", "generated", "grpc"]
        ]
    ]
    for P in PATHS_TO_ADD:
        print(P)
        if (P not in sys.path):
            sys.path.insert(0, P)
