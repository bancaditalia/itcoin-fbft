% Copyright (c) 2023 Bank of Italy
% Distributed under the GNU AGPLv3 software license, see the accompanying COPYING file.

:- consult("test_00_utils.pl").

:- begin_tests(test_action_06_send_commit).

setup_test(Config) :-
    init_all,
    Config = _{
        request: "req_digest",
        timestamp: 34,
        associated_data: "block_1"
    },
    receive_req_all(Config.request, Config.timestamp).

% replica is not ready to send commit before receiving pre-prepare
test(test_send_commit_01, [setup(setup_test(Config))]) :-
  V=0, N=1,
  TestReplica = 1,
  Signature = "dummy signature",
  apply_RECEIVE_PREPARE(V, N, Config.request, 2, Signature, TestReplica),
  apply_RECEIVE_PREPARE(V, N, Config.request, 3, Signature, TestReplica),
  assertion(\+pre_SEND_COMMIT(Config.request, V, N, TestReplica)).

% replica is not ready to send commit if prepare quorum is not met
test(test_send_commit_03, [setup(setup_test(Config))]) :-
    V=0, N=1,
    Primary = 0,
    TestReplica = 1,
    Signature = "dummy signature",
    apply_RECEIVE_PRE_PREPARE(V, N, Config.request, Config.associated_data, Primary, Signature, TestReplica),
    apply_RECEIVE_PREPARE(V, N, Config.request, 2, Signature, TestReplica),
    assertion(\+pre_SEND_COMMIT(Config.request, V, N, TestReplica)),
    !.

% replica is ready to send commit
test(test_send_commit_04, [setup(setup_test(Config))]) :-
    V=0, N=1,
    Primary = 0,
    TestReplica = 1,
    Signature = "dummy signature",
    apply_RECEIVE_PRE_PREPARE(V, N, Config.request, Config.associated_data, Primary, Signature, TestReplica),
    apply_RECEIVE_PREPARE(V, N, Config.request, 2, Signature, TestReplica),
    apply_RECEIVE_PREPARE(V, N, Config.request, 3, Signature, TestReplica),
    assertion(pre_SEND_COMMIT(Config.request, V, N, TestReplica)),
    !.

% commit added to logs after being sent
test(test_send_commit_05, [setup(setup_test(Config))]) :-
    V=0, N=1,
    TestReplica = 1,
    apply_SEND_COMMIT(V, N, Config.associated_data, TestReplica),
    assertion(msg_log_commit(TestReplica, V, N, Config.associated_data, TestReplica, _)).

% replica is no longer ready to send commit after sending one
test(test_send_commit_06, [setup(setup_test(Config))]) :-
    V=0, N=1,
    Primary = 0,
    TestReplica = 1,
    Signature = "dummy signature",
    apply_RECEIVE_PRE_PREPARE(V, N, Config.request, Config.associated_data, Primary, Signature, TestReplica),
    apply_SEND_PREPARE(Config.request, V, N, TestReplica),
    apply_RECEIVE_PREPARE(V, N, Config.request, 2, Signature, TestReplica),
    apply_RECEIVE_PREPARE(V, N, Config.request, 3, Signature, TestReplica),
    apply_SEND_COMMIT(V, N, Config.associated_data,TestReplica),
    assertion(\+pre_SEND_COMMIT(Config.request, V, N, TestReplica)),
    !.

:- end_tests(test_action_06_send_commit).
