% Copyright (c) 2023 Bank of Italy
% Distributed under the GNU AGPLv3 software license, see the accompanying COPYING file.

:- consult("test_00_utils.pl").

:- begin_tests(test_action_04_send_prepare).

setup_test(Requests):-
  init_all,
  Requests = _{
    digest_1: "req_digest 1",
    timestamp_1: 34,
    associated_data: "block_1",
    digest_2: "req_digest_2",
    timestamp_2: 42
  }.

% replica not ready for prepare before receiving request
test(test_send_prepare_01, [setup(setup_test(Requests))]) :-
  Primary = 0, V=0, N=1,
  Request = Requests.digest_1,
  Data = Requests.associated_data,
  Signature = "dummy signature",
  TestReplica = 1,
  apply_RECEIVE_PRE_PREPARE(V, N, Request, Data, Primary, Signature, TestReplica),
  assertion(\+pre_SEND_PREPARE(Request, V, N, TestReplica)),
  !.

% replica not ready for prepare before receiving pre-prepare
test(test_send_prepare_02, [setup(setup_test(Requests))]) :-
  V=0, N=1,
  Request = Requests.digest_1,
  Timestamp = Requests.timestamp_1,
  TestReplica = 1,
  apply_RECEIVE_REQUEST(Request, Timestamp, TestReplica),
  assertion(\+pre_SEND_PREPARE(Request, V, N, TestReplica)),
  !.

% replica ready for prepare
test(test_send_prepare_03, [setup(setup_test(Requests))]) :-
  Primary = 0, V=0, N=1,
  Request = Requests.digest_1,
  Timestamp = Requests.timestamp_1,
  Data = Requests.associated_data,
  Signature = "dummy signature",
  TestReplica = 1,
  apply_RECEIVE_REQUEST(Request, Timestamp, TestReplica),
  apply_RECEIVE_PRE_PREPARE(V, N, Request, Data, Primary, Signature, TestReplica),
  assertion(pre_SEND_PREPARE(Request, V, N, TestReplica)),
  !.

% prepare added to logs
test(test_send_prepare_04, [setup(setup_test(Requests))]) :-
  Primary = 0, V=0, N=1,
  Request = Requests.digest_1,
  Timestamp = Requests.timestamp_1,
  Data = Requests.associated_data,
  Signature = "dummy signature",
  TestReplica = 1,
  apply_RECEIVE_REQUEST(Request, Timestamp, TestReplica),
  apply_RECEIVE_PRE_PREPARE(V, N, Request, Data, Primary, Signature, TestReplica),
  apply_SEND_PREPARE(Request, V, N, TestReplica),
  assertion(msg_log_prepare(TestReplica, V, N, Request, TestReplica, _)),
  !.

% replica not ready for prepare after sending one
test(test_send_prepare_05, [setup(setup_test(Requests))]) :-
  Primary = 0, V=0, N=1,
  Request = Requests.digest_1,
  Timestamp = Requests.timestamp_1,
  Data = Requests.associated_data,
  Signature = "dummy signature",
  TestReplica = 1,
  apply_RECEIVE_REQUEST(Request, Timestamp, TestReplica),
  apply_RECEIVE_PRE_PREPARE(V, N, Request, Data, Primary, Signature, TestReplica),
  apply_SEND_PREPARE(Request, V, N, TestReplica),
  assertion(\+pre_SEND_PREPARE(Request, V, N, TestReplica)),
  !.

:- end_tests(test_action_04_send_prepare).
