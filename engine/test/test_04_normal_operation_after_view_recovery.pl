% Copyright (c) 2023 Bank of Italy
% Distributed under the GNU AGPLv3 software license, see the accompanying COPYING file.

:- consult("test_00_utils.pl").

:- begin_tests(test_04_normal_operation_after_view_recovery).

 % PRE-PREPARE
 test_pre_prepare_before_recovery(Request, Data, V, N, PrimaryId, TestReplicaId, Signature) :-
    apply_SEND_PRE_PREPARE(Request, Data, V, N, PrimaryId),
    forall(
        (between(0,2, ReplicaId), ReplicaId #\= PrimaryId),
        apply_RECEIVE_PRE_PREPARE(V, N, Request, Data, PrimaryId, Signature, ReplicaId)
    ),
    % Replica 3 doesn't receive pre-prepare because it is in a different view
    assertion(\+apply_RECEIVE_PRE_PREPARE(V, N, Request, Data, PrimaryId, Signature, TestReplicaId)).

test_pre_prepare_after_recovery(Request, Data, V, N, PrimaryId, TestReplicaId, Signature) :-
    apply_SEND_PRE_PREPARE(Request, Data, V, N, PrimaryId),
    forall(
        (between(0,2, ReplicaId), ReplicaId #\= PrimaryId),
        apply_RECEIVE_PRE_PREPARE(V, N, Request, Data, PrimaryId, Signature, ReplicaId)
    ),
    % Replica 3 receives pre-prepare after view recovery
    assertion(apply_RECEIVE_PRE_PREPARE(V, N, Request, Data, PrimaryId, Signature, TestReplicaId)).

% PREPARE
test_prepare_before_recovery(Request, V, N, PrimaryId, TestReplicaId, Signature) :-
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

test_prepare_after_recovery(Request, V, N, PrimaryId, TestReplicaId, Signature) :-
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
    % replica 3 receives prepare from other non-primary replicas after view recovery
    forall(
        (between(0,2, ReplicaId), ReplicaId #\= PrimaryId),
        assertion(apply_RECEIVE_PREPARE(V, N, Request, ReplicaId, Signature, TestReplicaId))
    ).

% COMMIT
test_commit_before_recovery(Data, V, N, TestReplicaId, Signature) :-
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

test_commit_after_recovery(Data, V, N, TestReplicaId, Signature) :-
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
    % replica 3 recives commit from other replicas after view recovery
    forall(
        between(0,2, ReplicaId),
        assertion(apply_RECEIVE_COMMIT(V, N, Data, ReplicaId, Signature, TestReplicaId))
    ).

% ROAST INIT
test_roast_init_before_recovery(Request, V, N, TestReplicaId) :-
    % replica 3 is not ready while other replicas are
    forall(
        between(0,2, ReplicaId),
        assertion(pre_ROAST_INIT(ReplicaId, Request, V, N))
    ),
    assertion(\+pre_ROAST_INIT(TestReplicaId, Request, V, N)).

test_roast_init_after_recovery(Request, V, N) :-
    % all replicas are ready (including 3) after view recovery
    forall(
        between(0,3, ReplicaId),
        assertion(pre_ROAST_INIT(ReplicaId, Request, V, N))
    ).

% EXECUTE
test_execute_before_recovery(Request, V, N, TestReplicaId) :-
    % replica 3 is not ready while other replicas are
    forall(
        between(0,2, ReplicaId),
        assertion(pre_EXECUTE(Request, V, N, ReplicaId))
    ),
    assertion(\+pre_EXECUTE(Request, V, N, TestReplicaId)).

test_execute_after_recovery(Request, V, N) :-
    % all replicas are ready (including 3) after view recovery
    forall(
        between(0,3, ReplicaId),
        assertion(pre_EXECUTE(Request, V, N, ReplicaId))
    ).

% This test case describes the case in which a non-primary replica crashes and then restarts.
% The replica has a view number that is lower with respect to the view number of other replicas
% Nevertheless the replica receives blocks from the network, meaning that the network
% is making progress on the blocks. The replica updates its view number and joins
% other replica in the consensus algorithm.

test(test_replica_after_view_recovery) :-
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
    % replica 3 doesn't receive messages or execute
    test_pre_prepare_before_recovery(Request, Data, V, N, PrimaryId, TestReplicaId, Signature),
    test_prepare_before_recovery(Request, V, N, PrimaryId, TestReplicaId, Signature),
    test_commit_before_recovery(Data, V, N, TestReplicaId, Signature),
    test_roast_init_before_recovery(Request, V, N, TestReplicaId),
    % test_execute_before_recovery(Request, V, N, TestReplicaId), TODO: un-comment after adding ROAST phase
    % all replicas receive block
    forall(
        between(0,3,ReplicaId),
        apply_RECEIVE_BLOCK(ReplicaId, N, Timestamp, Data)
    ),
    Timestamp2 = 35,
    receive_req_all(Request, Timestamp2),
    % Replica 3 is ready to recover view
    pre_RECOVER_VIEW(TestReplicaId, V),
    assertion(V == 1),
    % View is recovered for replica 3
    apply_RECOVER_VIEW(TestReplicaId, V),
    % Replica 3 is not ready to recover view anymore
    assertion(\+pre_RECOVER_VIEW(TestReplicaId, V)),
    N2 = 2,
    test_pre_prepare_after_recovery(Request, Data, V, N2, PrimaryId, TestReplicaId, Signature),
    test_prepare_after_recovery(Request, V, N2, PrimaryId, TestReplicaId, Signature),
    test_commit_after_recovery(Data, V, N2, TestReplicaId, Signature),
    test_roast_init_after_recovery(Request, V, N2).
    % test_execute_after_recovery(Request, V, N2). TODO: un-comment after adding ROAST phase



:- end_tests(test_04_normal_operation_after_view_recovery).
