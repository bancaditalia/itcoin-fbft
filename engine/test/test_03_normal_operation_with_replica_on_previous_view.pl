% Copyright (c) 2023 Bank of Italy
% Distributed under the GNU AGPLv3 software license, see the accompanying COPYING file.

:- consult("test_00_utils.pl").

:- begin_tests(test_03_normal_operation_with_replica_on_previous_view).

 % PRE-PREPARE
 test_pre_prepare(Request, Data, V, N, PrimaryId, TestReplicaId, Signature) :-
    apply_SEND_PRE_PREPARE(Request, Data, V, N, PrimaryId),
    forall(
        (between(0,2, ReplicaId), ReplicaId #\= PrimaryId),
        apply_RECEIVE_PRE_PREPARE(V, N, Request, Data, PrimaryId, Signature, ReplicaId)
    ),
    % Replica 3 doesn't receive pre-prepare because it is in a different view
    assertion(\+apply_RECEIVE_PRE_PREPARE(V, N, Request, Data, PrimaryId, Signature, TestReplicaId)).

% PREPARE
test_prepare(Request, V, N, PrimaryId, TestReplicaId, Signature) :-
    forall(
        (between(0,2, ReplicaId), ReplicaId #\= PrimaryId),
        apply_SEND_PREPARE(Request, V, N, ReplicaId)
    ),
    forall(
        (between(0,2, ReplicaId), ReplicaId #\= PrimaryId),
        apply_RECEIVE_PREPARE(V, N, Request, ReplicaId, Signature, PrimaryId)
    ),
    apply_RECEIVE_PREPARE(V, N, Request, 0, Signature, 2),
    apply_RECEIVE_PREPARE(V, N, Request, 2, Signature, 0),
    % replica 3 doesn't receive prepare from other non-primary replicas
    forall(
        (between(0,2, ReplicaId), ReplicaId #\= PrimaryId),
        assertion(\+ apply_RECEIVE_PREPARE(V, N, Request, ReplicaId, Signature, TestReplicaId))
    ).

% COMMIT
test_commit(Data, V, N, TestReplicaId, Signature) :-
    forall(
        between(0,2, ReplicaId),
        apply_SEND_COMMIT(V, N, Data, ReplicaId)
    ),
    forall(
        between(0,2, SenderId),
        forall(
            (between(0,2, ReceiverId), ReceiverId #\= SenderId),
            apply_RECEIVE_COMMIT(V, N, Data, SenderId, Signature, ReceiverId)
        )
    ),
    % replica 3 doesn't receive commit from other replicas but receives recovery message
    forall(
        between(0,2, ReplicaId),
        (
            apply_RECEIVE_COMMIT(V, N, Data, ReplicaId, Signature, TestReplicaId),
            assertion(\+msg_log_commit(TestReplicaId, _, _, _, ReplicaId, _)),
            assertion(msg_log_commit_view_recovery(TestReplicaId, V, N, SenderId))
        )
    ).

% ROAST INIT
test_roast_init(Request, V, N, TestReplicaId) :-
    % replica 3 is not ready for roast phase while other replicas are
    forall(
        between(0,2, ReplicaId),
        assertion(pre_ROAST_INIT(ReplicaId, Request, V, N))
    ),
    assertion(\+pre_ROAST_INIT(TestReplicaId, Request, V, N)).

% EXECUTE
test_execute(Request, V, N, TestReplicaId) :-
    % replica 3 is not ready while other replicas are
    forall(
        between(0,2, ReplicaId),
        assertion(pre_EXECUTE(Request, V, N, ReplicaId))
    ),
    assertion(\+pre_EXECUTE(Request, V, N, TestReplicaId)).

test(test_replica_on_previous_view_01) :-
    init_all,
    Request = "req_digest",
    Data = "block_1",
    Timestamp = 34,
    receive_req_all(Request, Timestamp),
    V = 1, N = 1,
    Signature = "dummy signature",
    % Replicas move to view 1 while replica 3 stays at view 0
    forall(
        between(0,2,ReplicaId),
        set_view(ReplicaId, V)
    ),
    PrimaryId = 1, TestReplicaId = 3,
    test_pre_prepare(Request, Data, V, N, PrimaryId, TestReplicaId, Signature),
    test_prepare(Request, V, N, PrimaryId, TestReplicaId, Signature),
    test_commit(Data, V, N, TestReplicaId, Signature),
    test_roast_init(Request, V, N, TestReplicaId).
    % test_execute(Request, V, N, TestReplicaId). % TODO: un-comment after adding ROAST phase


% This test case describes the case in which a non-primary replica crashes and then restarts.
% The replica has a view number that is lower with respect to the view number of other replicas
% Nevertheless the replica receives blocks from the network, meaning that the network
% is making progress on the blocks. The replica DOES NOT update its view number, it will be stuck
% and WILL NOT join other replica in the consensus algorithm.

test(test_replica_on_previous_view_02) :-
    Request = "req_digest",
    Data = "block_1",
    Timestamp = 34,
    V = 1, N = 1,
    Signature = "dummy signature",
    PrimaryId = 1, TestReplicaId = 3,
    % all replicas receive block
     forall(
        between(0,3,ReplicaId),
        apply_RECEIVE_BLOCK(ReplicaId, N, Timestamp, Data)
    ),
    Timestamp2 = 35,
    receive_req_all(Request, Timestamp2),
    % operations are tested again: replica 3 still fails on receiving pre-prepare, prepare and commit and is not prepared to execute
    N2 = 2,
    test_pre_prepare(Request, Data, V, N2, PrimaryId, TestReplicaId, Signature),
    test_prepare(Request, V, N2, PrimaryId, TestReplicaId, Signature),
    test_commit(Data, V, N2, TestReplicaId, Signature),
    test_roast_init(Request, V, N2, TestReplicaId).
    %test_execute(Request, V, N2, TestReplicaId). % TODO: un-comment after adding ROAST phase

:- end_tests(test_03_normal_operation_with_replica_on_previous_view).
