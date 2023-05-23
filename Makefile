.PHONY: test_fbft_normal_operation \
	cmake \

clean:
	rm -rf build/src

cmake:
	mkdir -p build
	cd build && cmake -DCMAKE_BUILD_TYPE=Debug -DITCOIN_CORE_SRC_DIR=/home/ubuntu/itcoin-core ..

cmake_build: cmake
	cd build && make -j8

cmake_test: cmake_build
	cd build && make test

cmake_build_serialized: cmake
	cd build && make main

build_main:
	cd build && make -j4 generated_src/fast
	cd build && make -j4 itcoin-fbft/fast
	cd build && make -j4 main/fast

build_test:
	cd build && make -j4 generated_src/fast
	cd build && make -j4 itcoin-fbft/fast
	cd build && make -j4 main-test/fast

send_to_address:
	./bitcoin-cli.sh 0 sendtoaddress tb1q36d5pcjhym68f4tcugzdsfayj8nmljvmsfestm 1 comment commentto false true null unset false 25 true

run_replica_0:
	cd build && make -j4 generated_src/fast
	cd build && make -j4 itcoin-fbft/fast
	cd build && make -j4 main/fast
	LD_LIBRARY_PATH=build/usrlocal/lib build/src/main -datadir=infra/node00

run_replica_1:
	cd build && make -j4 generated_src/fast
	cd build && make -j4 itcoin-fbft/fast
	cd build && make -j4 main/fast
	LD_LIBRARY_PATH=build/usrlocal/lib build/src/main -datadir=infra/node01

run_replica_2:
	cd build && make -j4 generated_src/fast
	cd build && make -j4 itcoin-fbft/fast
	cd build && make -j4 main/fast
	LD_LIBRARY_PATH=build/usrlocal/lib build/src/main -datadir=infra/node02

run_replica_3:
	cd build && make -j4 generated_src/fast
	cd build && make -j4 itcoin-fbft/fast
	cd build && make -j4 main/fast
	build/src/main -datadir=infra/node03
	LD_LIBRARY_PATH=build/usrlocal/lib build/src/main -datadir=infra/node03

test:
	cd build && make -j4 main-test
	cd build && make test

test_5frosted_bft: build_test
	cd build && ctest -R test_5frosted_bft --verbose

test_frost_wallet: build_test
	cd build && ctest -R test_blockchain_frost_wallet_bitcoin --verbose

test_blockchain_generate: build_test
	cd build && ctest -R test_blockchain_generate --verbose

test_blockchain_wallet_bitcoin: build_test
	cd build && ctest -R test_blockchain_wallet_bitcoin --verbose

test_blockchain_frost_wallet_bitcoin: build_test
	cd build && ctest -R test_blockchain_frost_wallet_bitcoin --verbose

test_messages_encoding: build_test
	cd build && ctest -R test_messages_encoding --verbose

test_fbft: build_test
	cd build && ctest -R test_fbft_ --verbose

test_fbft_normal_operation: build_test
	cd build && ctest -R test_fbft_normal_operation --verbose

test_fbft_view_change_empty: build_test
	cd build && ctest -R test_fbft_view_change_empty --verbose

test_fbft_view_change_prepared: build_test
	cd build && ctest -R test_fbft_view_change_prepared --verbose

test_fbft_replica2: build_test
	cd build && ctest -R test_fbft_replica2 --verbose

valgrind_test_full:
	cd build && valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes make test

#
# Samples
#

samples/boost_log_tutorial: $(GEN_SRC) $(OBJS) samples/boost_log_tutorial.cpp
	$(CC) $(CFLAGS) $(IFLAGS) samples/boost_log_tutorial.cpp $(LDFLAGS) -o $@

samples/swi_prolog_example: $(GEN_SRC) $(OBJS) samples/swi_prolog_example.cpp samples/swi_prolog_example.pl
	swipl-ld -v -c++ $(CC) -g samples/swi_prolog_example.cpp samples/swi_prolog_example.pl -o $@

samples: samples/boost_log_tutorial
