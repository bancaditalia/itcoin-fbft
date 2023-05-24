:- consult("../specs/pbft-replica-engine.pl").
:- consult("test_00_utils.pl").

:- begin_tests(test_02_pbft_view_change).

build_config(Config) :-
  Config = _{
    req_digest_1: "request_digest_1",
    req_timestamp_1: 42,
    proposed_block: "BLOCK_1",
    viewchange1_d: "VC_digest_1",
    viewchange2_d: "VC_digest_2",
    viewchange3_d: "VC_digest_3",
    r0: 0, r1: 1, r2: 2, r3: 3,
    view0: 0,
    view1: 1,
    seqno0: 0,
    seqno1: 1,
    seqno2: 2,
    first_checkpoint_digest: "GENESIS_BLOCK_HASH",
    unknown_proposed_block: "",
    block_signature: "BLOCK_SIGNATURE",
    replica_own_signature: "SIG_OWN_REPLICA",
    sender_signature: _{
      0: "SIGNATURE_0",
      1: "SIGNATURE_1",
      2: "SIGNATURE_2",
      3: "SIGNATURE_3"
    },
    new_view_signature: "SIG_IN_NEW_VIEW"
  }.

expected_vc_digests(Config, Digests) :-
  % compute expected VC digests, used for assertions in send-new-view and process-new-view
  ExpectedH = 0,
  ExpectedC = "GENESIS_BLOCK_HASH",
  ExpectedPi = [[1, Config.req_digest_1, Config.view0]],
  ExpectedQi = [[1, Config.req_digest_1, Config.proposed_block, Config.view0]],
  NewV = Config.view1,
  digest_view_change(NewV, ExpectedH, ExpectedC, ExpectedPi, ExpectedQi, Config.r1, VC_digest_1),
  digest_view_change(NewV, ExpectedH, ExpectedC, ExpectedPi, ExpectedQi, Config.r2, VC_digest_2),
  digest_view_change(NewV, ExpectedH, ExpectedC, ExpectedPi, ExpectedQi, Config.r3, VC_digest_3),
  Digests = [VC_digest_1, VC_digest_2, VC_digest_3].

setup_test() :-
  init_all.

% Replica 0 and 1 receive two different Requests
test(test_view_change_01, [setup((setup_test(), build_config(Config)))]) :-
  effect_RECEIVE_REQUEST(Config.req_digest_1, Config.req_timestamp_1, 0),
  % Primary should be willing to send the Pre-prepare
  V = 0, N = 1,
  test_active_send_pre_prepare(
    0,[[Config.req_digest_1, V, N, 0]]
  ),
  test_active_send_pre_prepare(1,[]).

% Replica 0 Sends the Pre-Prepare
test(test_view_change_02, [setup(build_config(Config))]) :-
  V = 0, N = 1,
  effect_SEND_PRE_PREPARE(Config.req_digest_1, Config.proposed_block, V, N, 0),
  % primary no longer wants to send the pre-prepare
  test_active_send_pre_prepare(0, []).

% Replica 1, 2 and 3 Receive the Pre-Prepare
test(test_view_change_03, [setup(build_config(Config))]) :-
  Primary_id = 0,
  V = 0, N = 1,
  msg_out_clear_all(Primary_id),
  foreach(
    between(1,3,Replica_id),
    effect_RECEIVE_PRE_PREPARE(V, N, Config.req_digest_1, Config.proposed_block, Primary_id, Config.sender_signature.Primary_id, Replica_id)
  ),
  % Nothing should happen, since the replicas have not received the requests
  test_active_send_prepare(1, []).

% Replica 1, 2 and 3 also receive the piggybacked Request and get ready to send the Prepare
test(test_view_change_04, [setup(build_config(Config))]) :-
  V = 0, N = 1,
  % Replica 1,2,3 receive the request
  foreach(
    between(1,3,Replica_id),
    effect_RECEIVE_REQUEST(Config.req_digest_1, Config.req_timestamp_1, Replica_id)
  ),
  % Replica 1,2,3 should be willing to send the Prepare
  test_active_send_prepare(0, []),
  foreach(
    between(1,3,Replica_id),
    test_active_send_prepare(Replica_id, [[Config.req_digest_1, V, N]])
  ).

% Replica 1, 2 and 3 send the Prepare
test(test_view_change_05, [setup(build_config(Config))]) :-
  V = 0, N = 1,
  foreach(
    between(1,3,Replica_id),
    effect_SEND_PREPARE(Config.req_digest_1, V, N, Replica_id)
  ),
  % Message log of 1, 2, 3 should contain the Prepare
  foreach(
    between(1,3,Replica_id),
    assertion(msg_log_prepare(Replica_id, V, N, Config.req_digest_1, Replica_id, Config.replica_own_signature))
  ).

% Replica 0, 1, 2, 3 receive prepare and get prepared to send commit
test(test_view_change_06, [setup(build_config(Config))]) :-
  V = 0, N = 1,
  %replica 0 (primary) receives from all other replicas
  forall(
    between(1,3,Sender_id),
    effect_RECEIVE_PREPARE(V, N, Config.req_digest_1, Sender_id, Config.sender_signature.Sender_id, 0)
    ),
  test_active_send_commit(0, [[Config.req_digest_1, V, N]]),
  %replica 1,2,3 receive from other non-primary replicas
  forall(
    between(1,3,Replica_id),
    (
      forall(
        (between(1,3,Sender_id), Sender_id =\= Replica_id),
        effect_RECEIVE_PREPARE(V, N, Config.req_digest_1, Sender_id, Config.sender_signature.Sender_id, Replica_id)
        ),
      % each replica should now be ready to send commit
      test_active_send_commit(Replica_id, [[Config.req_digest_1, V, N]])
    )
  ).

% ...however, primary fails. Check that view-change is active
test(test_view_change_07, [setup(build_config(Config))]) :-
  timeout(0, Timeout),
  T = Config.req_timestamp_1 + Timeout + 1,
  set_synthetic_time(0, T),
  active_SEND_VIEW_CHANGE(1, Active_actions_1),
  active_SEND_VIEW_CHANGE(2, Active_actions_2),
  active_SEND_VIEW_CHANGE(3, Active_actions_3),
  view(1, V),
  NewV #= V + 1,
  assertion( Active_actions_1 == [[NewV]] ),
  assertion( Active_actions_2 == [[NewV]] ),
  assertion( Active_actions_3 == [[NewV]] ).


% send view-change messages
test(test_view_change_08, [setup(build_config(Config))]) :-
  view(Config.r1, V),
  NewV #= V + 1,
  effect_SEND_VIEW_CHANGE(NewV, Config.r1),
  effect_SEND_VIEW_CHANGE(NewV, Config.r2),
  effect_SEND_VIEW_CHANGE(NewV, Config.r3),
  % the expected highest sequence number of a checkpoint is 0 - just the first one
  ExpectedH = 0,
  % the expected list of checkpoints is only the genesis block
  ExpectedC = "GENESIS_BLOCK_HASH",
  % the expected list of prepared requests is made of only request 1
  ExpectedPi = [[1, Config.req_digest_1, V]],
  % the expected list of pre-prepared requests is made of only request 1
  ExpectedQi = [[1, Config.req_digest_1, Config.proposed_block, V]],
  % compute expected VC digests
  expected_vc_digests(Config, Digests),
  Digests = [Expected_D1, Expected_D2, _],

  % check msg_log_view_change is populated correctly for each replica
  assertion(msg_log_view_change(Config.r1, Expected_D1, NewV, ExpectedH, ExpectedC, ExpectedPi, ExpectedQi, Config.r1, Config.replica_own_signature)),
  assertion(msg_log_view_change(Config.r2, Expected_D2, NewV, ExpectedH, ExpectedC, ExpectedPi, ExpectedQi, Config.r2, Config.replica_own_signature)),
  assertion(msg_log_view_change(Config.r2, Expected_D2, NewV, ExpectedH, ExpectedC, ExpectedPi, ExpectedQi, Config.r2, Config.replica_own_signature)),

  % check msg_out_add_view_change is populated correctly for each replica
  assertion(msg_out_add_view_change(Config.r1, NewV, ExpectedH, ExpectedC, ExpectedPi, ExpectedQi)),
  assertion(msg_out_add_view_change(Config.r2, NewV, ExpectedH, ExpectedC, ExpectedPi, ExpectedQi)),
  assertion(msg_out_add_view_change(Config.r2, NewV, ExpectedH, ExpectedC, ExpectedPi, ExpectedQi)),

  % check all previous messages with lower view have been removed
  foreach(
    (between(1,3,Ri), OldView #>=0, OldView #< NewV),
    (
        assertion(\+ msg_log_pre_prepare(Ri, OldView, _, _, _, _, _)),
        assertion(\+ msg_log_prepare(Ri, OldView, _, _, _, _)),
        assertion(\+ msg_log_commit(Ri, OldView, _, _, _, _)),
        assertion(\+ msg_log_view_change(Ri, _, OldView, _, _, _, _, _, _)),
        assertion(\+ msg_log_new_view(Ri, OldView, _, _, _))
    )
  ).

% each replica receives view-change messages, and sends view-change-ack messages
test(test_view_change_09, [setup(build_config(Config))]) :-
    %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    % BEGIN ARRANGE SECTION
    % read attributes of view-change messages
    msg_out_view_change(Config.r1, V1, H1, C1, P1, Q1),
    msg_out_view_change(Config.r2, V2, H2, C2, P2, Q2),
    msg_out_view_change(Config.r3, V3, H3, C3, P3, Q3),
    % END ARRANGE SECTION
    %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

    %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    % BEGIN ACT SECTION
    Sig_1 = Config.sender_signature.1,
    Sig_2 = Config.sender_signature.2,
    Sig_3 = Config.sender_signature.3,
    % replica 1 receives view-change messages from 2 and 3
    effect_RECEIVE_VIEW_CHANGE(V2, H2, C2, P2, Q2, Config.r2, Sig_2, Config.r1),
    effect_RECEIVE_VIEW_CHANGE(V3, H3, C3, P3, Q3, Config.r3, Sig_3, Config.r1),

    % replica 2 receives view-change messages from 1 and 3
    effect_RECEIVE_VIEW_CHANGE(V1, H1, C1, P1, Q1, Config.r1, Sig_1, Config.r2),
    effect_RECEIVE_VIEW_CHANGE(V3, H3, C3, P3, Q3, Config.r3, Sig_3, Config.r2),

    % replica 3 receives view-change messages from 1 and 2
    effect_RECEIVE_VIEW_CHANGE(V1, H1, C1, P1, Q1, Config.r1, Sig_1, Config.r3),
    effect_RECEIVE_VIEW_CHANGE(V2, H2, C2, P2, Q2, Config.r2, Sig_2, Config.r3),
    % END ACT SECTION
    %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

    %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    % BEGIN ASSERTION SECTION
    % compute expected VC digests
    expected_vc_digests(Config, Digests),
    Digests = [Expected_D1, Expected_D2, Expected_D3],
    % check view-change messages have been added to the log of replica 1
    assertion(msg_log_view_change(Config.r1, Expected_D2, V2, H2, C2, P2, Q2, Config.r2, Sig_2)),
    assertion(msg_log_view_change(Config.r1, Expected_D3, V3, H3, C3, P3, Q3, Config.r3, Sig_3)),

    % check view-change messages have been added to the log of replica 2
    assertion(msg_log_view_change(Config.r2, Expected_D1, V1, H1, C1, P1, Q1, Config.r1, Sig_1)),
    assertion(msg_log_view_change(Config.r2, Expected_D3, V3, H3, C3, P3, Q3, Config.r3, Sig_3)),

    % check view-change messages have been added to the log of replica 3
    assertion(msg_log_view_change(Config.r3, Expected_D1, V1, H1, C1, P1, Q1, Config.r1, Sig_1)),
    assertion(msg_log_view_change(Config.r3, Expected_D2, V3, H3, C3, P3, Q3, Config.r2, Sig_2)).

    % END ASSERTION SECTION
    %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

% send new view message
test(test_view_change_11, [setup(build_config(Config))]) :-
    % a non-primary cannot send a new-view message
    \+ active_SEND_NEW_VIEW(Config.r2, [_Nu2, _Chi2]),
    \+ active_SEND_NEW_VIEW(Config.r3, [_Nu3, _Chi3]),
    % the primary can send a new-view message
    active_SEND_NEW_VIEW(Config.r1, [Nu, Chi]),
    !,
    % compute expected VC digests
    expected_vc_digests(Config, Digests),
    Digests = [Expected_D1, Expected_D2, Expected_D3],
    assertion(Nu == [[Config.r1, Expected_D1], [Config.r2, Expected_D2], [Config.r3, Expected_D3]]),
    nth0(0, Chi, SelectedCheckpoint),
    assertion(SelectedCheckpoint == [1, Config.req_digest_1, Config.proposed_block]),
    % primary sends the new-view message
    effect_SEND_NEW_VIEW(Nu, Chi, Config.r1),
    assert(msg_log_new_view(Config.r1, Config.view1, Nu, Chi, Config.replica_own_signature)),
    assert(msg_out_new_view(Config.r1, Config.view1, Nu, Chi)).

% non-primary replica receive new view messages
test(test_view_change_12, [setup(build_config(Config))]) :-
    primary(Config.view1, Config.r1),
    % replica 2 cannot receive a new-view message from itself
    \+ effect_RECEIVE_NEW_VIEW(Config.view1, _Nu2, _Chi2, Config.r2, _, Config.r2),
    % replica 2 cannot receive a new-view message from non-primary replica 3
    \+ effect_RECEIVE_NEW_VIEW(Config.view1, _Nu3, _Chi3, Config.r3, _, Config.r2),
    % get Nu and Chi sent by the primary in the previous test
    msg_out_new_view(Config.r1, Config.view1, Nu, Chi),
    !,
    % replica 2 receives new-view message from current primary 1
    Sig_1 = Config.sender_signature.1,
    effect_RECEIVE_NEW_VIEW(Config.view1, Nu, Chi, Config.r1, Sig_1, Config.r2),
    assertion(msg_log_new_view(Config.r2, Config.view1, Nu, Chi, Sig_1)),
    % replica 3 receives new-view message from current primary 1
    effect_RECEIVE_NEW_VIEW(Config.view1, Nu, Chi, Config.r1, Sig_1, Config.r3),
    assertion(msg_log_new_view(Config.r3, Config.view1, Nu, Chi, Sig_1)).

% replicas process new view messages
test(test_view_change_13, [setup(build_config(Config))]) :-
    Hi = Config.seqno0,
    % check active process-new-view at replica 1
    active_PROCESS_NEW_VIEW(Hi, Config.r1, Out_active_1),
    nth0(0, Out_active_1, [Nu1, Chi1]),
    % compute expected VC digests
    expected_vc_digests(Config, Digests),
    Digests = [Expected_D1, Expected_D2, Expected_D3],
    assertion(Nu1 == [[Config.r1, Expected_D1], [Config.r2, Expected_D2], [Config.r3, Expected_D3]]),
    assertion(nth0(0, Chi1, [1, Config.req_digest_1, Config.proposed_block])),
    % check active process-new-view at replica 2
    active_PROCESS_NEW_VIEW(Hi, Config.r2, Out_active_2),
    nth0(0, Out_active_2, [Nu2, Chi2]),
    assertion(Nu2 == [[Config.r1, Expected_D1], [Config.r2, Expected_D2], [Config.r3, Expected_D3]]),
    assertion(nth0(0, Chi2, [1, Config.req_digest_1, Config.proposed_block])),
    % check active process-new-view at replica 3
    active_PROCESS_NEW_VIEW(Hi, Config.r3, Out_active_3),
    nth0(0, Out_active_3, [Nu3, Chi3]),
    assertion(Nu3 == [[Config.r1, Expected_D1], [Config.r2, Expected_D2], [Config.r3, Expected_D3]]),
    assertion(nth0(0, Chi3, [1, Config.req_digest_1, Config.proposed_block])),

    NewHi = Config.seqno1,
    % apply process-new-view at replica 1
    effect_PROCESS_NEW_VIEW(NewHi, Chi1, Config.r1),
    assertion(msg_log_pre_prepare(Config.r1, Config.view1, Config.seqno1, Config.req_digest_1, Config.proposed_block, Config.r1, Config.new_view_signature)),
    % primary does not add messages to prepare
    assertion(\+ msg_log_prepare(Config.r1, Config.view1, Config.seqno1, Config.req_digest_1, Config.r1, Config.replica_own_signature)),
    % apply process-new-view at replica 2
    effect_PROCESS_NEW_VIEW(NewHi, Chi2, Config.r2),
    assertion(msg_log_pre_prepare(Config.r2, Config.view1, Config.seqno1, Config.req_digest_1, Config.proposed_block, Config.r1, Config.new_view_signature)),
    assertion(msg_log_prepare(Config.r2, Config.view1, Config.seqno1, Config.req_digest_1, Config.r2, Config.replica_own_signature)),
    assertion(msg_out_prepare(Config.r2, Config.view1, Config.seqno1, Config.req_digest_1)),
    % add out prepare
    % apply process-new-view at replica 3
    effect_PROCESS_NEW_VIEW(NewHi, Chi3, Config.r3),
    assertion(msg_log_pre_prepare(Config.r3, Config.view1, Config.seqno1, Config.req_digest_1, Config.proposed_block, Config.r1, Config.new_view_signature)),
    assertion(msg_log_prepare(Config.r3, Config.view1, Config.seqno1, Config.req_digest_1, Config.r3, Config.replica_own_signature)),
    assertion(msg_out_prepare(Config.r3, Config.view1, Config.seqno1, Config.req_digest_1)),

    % replicas other than the primary send prepare messages to every other replica
    nb_getval(cluster_size, Csize),
    LastReplicaId #= Csize - 1,
    Chis = [Chi1, Chi2, Chi3],
    forall(
        (between(2,LastReplicaId,Sender_id)),
        forall(
            (nth1(Sender_id, Chis, Chi_i), member([N, D, _], Chi_i), N #> 0),
            forall(
                (between(1, LastReplicaId, Replica_id), Replica_id #\= Sender_id),
                (view(Sender_id, V), effect_RECEIVE_PREPARE(V, N, D, Sender_id, Config.sender_signature.Sender_id, Replica_id))
            )
        )
    ).


% replicas are ready to send commit
test(test_view_change_14, [setup(build_config(Config))]) :-
  view(1, CurV),
  active_SEND_COMMIT(1, Output_active_1),
  assertion(member([Config.req_digest_1, CurV, Config.seqno1], Output_active_1)),
  active_SEND_COMMIT(2, Output_active_2),
  assertion(member([Config.req_digest_1, CurV, Config.seqno1], Output_active_2)),
  active_SEND_COMMIT(3, Output_active_3),
  assertion(member([Config.req_digest_1, CurV, Config.seqno1], Output_active_3)),
  !,
  foreach(
    between(1,3,Replica_id),
    effect_SEND_COMMIT(Config.view1, Config.seqno1, Config.block_signature, Replica_id)
  ).
  % replica shouldn't be prepared to send commit anymore
  % TODO: still appears commit of previous view; to be fixed
  %foreach(
  %  between(0, 3, Replica_id),
  %  test_active_send_commit(Replica_id, [])
  %).


% All replicas receive commit and get prepared to execute
test(test_view_change_15, [setup(build_config(Config))]) :-
  V = 1, N = 1,
  Sig_0 = Config.sender_signature.0,
  Sig_1 = Config.sender_signature.1,
  Sig_2 = Config.sender_signature.2,
  %replica 1
  effect_RECEIVE_COMMIT(V, N, Config.block_signature, 0, Sig_0, 1),
  effect_RECEIVE_COMMIT(V, N, Config.block_signature, 2, Sig_2, 1),
  active_EXECUTE(1, Active_execute_1),
  assertion( member([Config.req_digest_1, V, N], Active_execute_1) ),
  %replica 2
  effect_RECEIVE_COMMIT(V, N, Config.block_signature, 0, Sig_0, 2),
  effect_RECEIVE_COMMIT(V, N, Config.block_signature, 1, Sig_1, 2),
  active_EXECUTE(2, Active_execute_2),
  assertion( member([Config.req_digest_1, V, N], Active_execute_2) ),
  %replica 3
  effect_RECEIVE_COMMIT(V, N, Config.block_signature, 0, Sig_0, 3),
  effect_RECEIVE_COMMIT(V, N, Config.block_signature, 1, Sig_1, 3),
  active_EXECUTE(3, Active_execute_3),
  assertion( member([Config.req_digest_1, V, N], Active_execute_3) ).


:- end_tests(test_02_pbft_view_change).
