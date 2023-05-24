:- consult("../specs/pbft-replica-engine.pl").
:- consult("test_00_utils.pl").

:- begin_tests(test_aux_05_pre_prepared).

setup_test() :-
  init_all.

build_config(Config) :-
  Config = _{
    view0: 0,
    seqno1: 1,
    req_digest: "req_digest_1",
    req_timestamp: 34,
    associated_data: "block_1"
,    signatures : _{
      0: "sig_0",
      1: "sig_1",
      2: "sig_2",
      3: "sig_3"
    }
  }.

% replica pre-prepared
test(test_aux_05_pre_prepared_00, [setup((setup_test(), build_config(Config)))]) :-
  V = Config.view0,
  N = Config.seqno1,
  PrimaryId = 0,
  TestReplicaId = 1,
  msg_log_add_request(TestReplicaId, Config.req_digest, Config.req_timestamp),
  msg_log_add_pre_prepare(TestReplicaId, V, N, Config.req_digest, Config.associated_data, PrimaryId, Config.signatures.PrimaryId),
  assertion( pre_prepared(Config.req_digest, V, N, TestReplicaId) ),
  !.

% replica not pre-prepared
test(test_aux_05_pre_prepared_01, [setup((setup_test(), build_config(Config)))]) :-
  V = Config.view0,
  N = Config.seqno1,
  PrimaryId = 0,
  TestReplicaId = 1,
  msg_log_add_pre_prepare(TestReplicaId, V, N, Config.req_digest, Config.associated_data, PrimaryId, Config.signatures.PrimaryId),
  % replica didn't receive request, so not prepared
  assertion( \+pre_prepared(Config.req_digest, V, N, TestReplicaId) ),
  !.

% Command to debug
% guitracer, trace.

:- end_tests(test_aux_05_pre_prepared).
