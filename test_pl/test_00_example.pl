% Run the test file with:
%   swipl -g run_tests test_00_example.pl
% more details on the Prolog unit test framework: https://www.swi-prolog.org/pldoc/package/plunit.html
:- consult("../specs/pbft-replica-engine.pl").

% needed at the beginning of each test-unit
:- begin_tests(test_00_example).

% setup
setup_test() :-
  Block_hash = "GENESIS_BLOCK_HASH",
  Block_height = 10,
  Block_timestamp = 1000,
  Genesis_block_timestamp = 0,
  Target_block_time = 60,
  Db_filename = "file",
  Db_reset = true,
  init(0, 4, Block_height, Block_hash, Block_timestamp, Genesis_block_timestamp, Target_block_time, Db_filename, Db_reset),
  init(1, 4, Block_height, Block_hash, Block_timestamp, Genesis_block_timestamp, Target_block_time, Db_filename, Db_reset),
  init(2, 4, Block_height, Block_hash, Block_timestamp, Genesis_block_timestamp, Target_block_time, Db_filename, Db_reset),
  init(3, 4, Block_height, Block_hash, Block_timestamp, Genesis_block_timestamp, Target_block_time, Db_filename, Db_reset).


% Command to debug
% guitracer, trace.
% send_prepare_pre(fcaf17d9723627bc7e63f2b8342eb9819924dc8339102cc4ce7af4e127c703ac, 1647619527, 0, 0, 1, 1).

%
% Code of the test
%

test(main, [ setup(setup_test()) ]) :-
  view(0,V),
  assertion( V == 0 ).

:- end_tests(test_00_example).
