:- consult("../specs/pbft-replica-engine.pl").
:- consult("test_00_utils.pl").

:- begin_tests(test_aux_07_committed).

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

% replica committed
test(test_aux_07_committed_00, [setup((setup_test(), build_config(Config)))]) :-
  V = Config.view0,
  N = Config.seqno1,
  PrimaryId = 0,
  TestReplicaId = 1,
  msg_log_add_request(TestReplicaId, Config.req_digest, Config.req_timestamp),
  msg_log_add_pre_prepare(TestReplicaId, V, N, Config.req_digest, Config.associated_data, PrimaryId, Config.signatures.PrimaryId),
  forall(
    between(2,3,SenderId),
    msg_log_add_prepare(TestReplicaId, V, N, Config.req_digest, SenderId, Config.signatures.SenderId)
  ),
  forall(
    (between(0,3,SenderId),SenderId =\= TestReplicaId),
    msg_log_add_commit(TestReplicaId, V, N, Config.associated_data, SenderId, Config.signatures.SenderId)
  ),
  assertion(committed(Config.req_digest, V, N, TestReplicaId)),
  !.

% replica not committed
test(test_aux_07_committed_01, [setup((setup_test(), build_config(Config)))]) :-
  V = Config.view0,
  N = Config.seqno1,
  PrimaryId = 0,
  TestReplicaId = 1,
  msg_log_add_request(TestReplicaId, Config.req_digest, Config.req_timestamp),
  msg_log_add_pre_prepare(TestReplicaId, V, N, Config.req_digest, Config.associated_data, PrimaryId, Config.signatures.PrimaryId),
  forall(
    between(2,3,SenderId),
    msg_log_add_prepare(TestReplicaId, V, N, Config.req_digest, SenderId, Config.signatures.SenderId)
  ),
  % adding only one commit
  msg_log_add_commit(TestReplicaId, V, N, Config.associated_data, 2, Config.signatures.2),
  % quorum not met, so the replica is not committed
  assertion(\+committed(Config.req_digest, V, N, TestReplicaId)),
  !.

% Command to debug
% guitracer, trace.

:- end_tests(test_aux_07_committed).
