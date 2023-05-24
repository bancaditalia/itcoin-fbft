% Copyright (c) 2023 Bank of Italy
% Distributed under the GNU AGPLv3 software license, see the accompanying COPYING file.

:- consult("test_00_utils.pl").

:- begin_tests(test_action_02_send_pre_prepare).

setup_test(Requests):-
  init_all,
  Requests = _{
    digest_1: "req_digest 1",
    timestamp_1: 34,
    associated_data: "block_1",
    digest_2: "req_digest_2",
    timestamp_2: 42
  }.

% primary is not ready to send pre-prepare because it didn't receive request, but is ready afterwards
test(test_send_pre_prepare_01, [setup(setup_test(Requests))]) :-
  V = 0, N=1,
  primary(V,Primary),
  assertion(\+pre_SEND_PRE_PREPARE(Requests.digest_1, V, N, Primary)),
  apply_RECEIVE_REQUEST(Requests.digest_1, Requests.timestamp_1, Primary),
  assertion(pre_SEND_PRE_PREPARE(Requests.digest_1, V, N, Primary)).

% primary is ready to send only pre-prepare with lowest timestamp
test(test_send_pre_prepare_02, [setup(setup_test(Requests))]) :-
  V=0, N=1,
  primary(V,Primary),
  apply_RECEIVE_REQUEST(Requests.digest_1, Requests.timestamp_1, Primary),
  apply_RECEIVE_REQUEST(Requests.digest_2, Requests.timestamp_2, Primary),
  assertion(pre_SEND_PRE_PREPARE(Requests.digest_1, V, N, Primary)),
  assertion(\+pre_SEND_PRE_PREPARE(Requests.digest_2, V, N, Primary)).

% replica different than primary is not ready to send pre-prepare
test(test_send_pre_prepare_03, [setup(setup_test(Requests))]) :-
  V=0, N=1,
  receive_req_all(Requests.digest_1, Requests.timestamp_1),
  primary(V,Primary),
  TestReplica = 1,
  TestReplica #\= Primary,
  assertion(\+pre_SEND_PRE_PREPARE(Requests.digest_1, V, N, TestReplica)).

% seqno and logs are updated after send_pre_prepare
test(test_send_pre_prepare_04, [setup(setup_test(Requests))]) :-
  V=0, N=1,
  primary(V,Primary),
  apply_RECEIVE_REQUEST(Requests.digest_1, Requests.timestamp_1, Primary),
  apply_SEND_PRE_PREPARE(Requests.digest_1, Requests.associated_data, V, N, Primary),
  assertion(seqno(Primary,N)),
  assertion(msg_log_pre_prepare(Primary, V, N, Requests.digest_1, Requests.associated_data, Primary, _)).

% primary is no longer ready to send pre_prepare after sending it
test(test_send_pre_prepare_05, [setup(setup_test(Requests))]) :-
  V=0, N=1,
  primary(V,Primary),
  apply_RECEIVE_REQUEST(Requests.digest_1, Requests.timestamp_1, Primary),
  apply_SEND_PRE_PREPARE(Requests.digest_1, Requests.associated_data, V, N, Primary),
  assertion(\+pre_SEND_PRE_PREPARE(Requests.digest_1, V, N, Primary)).

:- end_tests(test_action_02_send_pre_prepare).
