% Copyright (c) 2023 Bank of Italy
% Distributed under the GNU AGPLv3 software license, see the accompanying COPYING file.

:- consult("test_00_utils.pl").

:- begin_tests(test_action_07_receive_commit).

% replica is not ready to send commit before receiving pre-prepare
test(test_receive_commit) :-
    init_all,
    Sender = 2, V=0, N=1,
    Data = "block_1",
    Signature = "dummy signature",
    TestReplica = 1,
    apply_RECEIVE_COMMIT(V, N, Data, Sender, Signature, TestReplica),
    assertion(msg_log_commit(TestReplica, V, N, Data, Sender, Signature)).


:- end_tests(test_action_07_receive_commit).
