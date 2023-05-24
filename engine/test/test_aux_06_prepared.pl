% Copyright (c) 2023 Bank of Italy
% Distributed under the GNU AGPLv3 software license, see the accompanying COPYING file.

:- consult("test_00_utils.pl").

:- begin_tests(test_aux_06_prepared).

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

% replica prepared
test(test_aux_06_prepared_00, [setup((setup_test(), build_config(Config)))]) :-
  V = Config.view0,
  N = Config.seqno1,
  TestReplicaId = 0,
  msg_log_add_request(TestReplicaId, Config.req_digest, Config.req_timestamp),
  msg_log_add_pre_prepare(TestReplicaId, V, N, Config.req_digest, Config.associated_data, TestReplicaId, Config.signatures.TestReplicaId),
  forall(
    between(1,3,SenderId),
    msg_log_add_prepare(TestReplicaId, V, N, Config.req_digest, SenderId, Config.signatures.SenderId)
  ),
  assertion( prepared(Config.req_digest, V, N, TestReplicaId) ),
  !.

% replica not prepared
test(test_aux_06_prepared_01, [setup((setup_test(), build_config(Config)))]) :-
  V = Config.view0,
  N = Config.seqno1,
  TestReplicaId = 0,
  msg_log_add_request(TestReplicaId, Config.req_digest, Config.req_timestamp),
  msg_log_add_pre_prepare(TestReplicaId, V, N, Config.req_digest, Config.associated_data, TestReplicaId, Config.signatures.TestReplicaId),
  SenderId = 1,
  msg_log_add_prepare(TestReplicaId, V, N, Config.req_digest, SenderId, Config.signatures.SenderId),
  % quorum not met, so replica is not prepared
  assertion( \+ prepared(Config.req_digest, V, N, TestReplicaId) ),
  !.

% Command to debug
% guitracer, trace.

:- end_tests(test_aux_06_prepared).
