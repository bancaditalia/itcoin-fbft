% Copyright (c) 2023 Bank of Italy
% Distributed under the GNU AGPLv3 software license, see the accompanying COPYING file.

:- consult("test_00_utils.pl").

:- begin_tests(test_action_03_receive_pre_prepare).

setup_test(Requests):-
  init_all,
  Requests = _{
    digest_1: "req_digest 1",
    timestamp_1: 34,
    associated_data: "block_1",
    digest_2: "req_digest_2",
    timestamp_2: 42
  }.
  receive_req_all(Requests.digest_1, Requests.timestamp_1).

% pre-prepare added to logs after it is received by other replicas
test(test_receive_pre_prepare_01, [setup(setup_test(Requests))]) :-
  Primary = 0, V=0, N=1,
  Request = Requests.digest_1,
  Data = Requests.associated_data,
  Signature = "dummy signature",
  forall(
    between(1,3,Replica),
    apply_RECEIVE_PRE_PREPARE(V, N, Request, Data, Primary, Signature, Replica)
  ),
  forall(
    between(1,3,Replica),
    assertion(msg_log_pre_prepare(Replica, V, N, Request, Data, Primary, Signature))
  ).

% replica doesn't receive pre-prepare when there is another pre-prepare with different digest
test(test_receive_pre_prepare_02, [setup(setup_test(Requests))]) :-
  Primary = 0, V=0, N=1,
  Data = Requests.associated_data,
  Signature = "dummy signature",
  TestReplica = 1,
  % log with a different digest is added
  msg_log_add_pre_prepare(TestReplica, V, N, Requests.digest_2, Data, Primary, Signature),
  % receive_pre_prepare with first digest fails
  \+apply_RECEIVE_PRE_PREPARE(V, N, Requests.digest_1, Data, Primary, Signature, TestReplica).

:- end_tests(test_action_03_receive_pre_prepare).
