% Copyright (c) 2023 Bank of Italy
% Distributed under the GNU AGPLv3 software license, see the accompanying COPYING file.

:- consult("test_00_utils.pl").

% Command to debug
% guitracer, trace.

:- begin_tests(test_action_12_send_view_change_00).

build_config(Config) :-
  Config = _{
    req_digest_1: "REQUEST_1",
    req_digest_2: "REQUEST_2",
    proposed_block_1 : "BLOCK_1",
    proposed_block_2 : "BLOCK_2",
    timestamp: 34,
    view0: 0,
    view1: 1,
    view4: 4,
    view5: 5,
    seqno1: 1,
    sender_signature: _{
      0: "SIGNATURE 0",
      1: "SIGNATURE_1",
      2: "SIGNATURE_2",
      3: "SIGNATURE_3"
    }
  }.

setup() :-
  init_all.

%
% Test build Pi
%

test(test_send_view_change_00_01, [setup((setup(), build_config(Config)))]) :-
  receive_req_all(Config.req_digest_1, Config.timestamp),
  pre_prepare_all(Config.req_digest_1, Config.proposed_block_1, Config.view0, Config.seqno1),
  update_view_change_Pi(2),
  findall([N,M,V], view_change_Pi(2, N, M, V), Pi),
  assertion( Pi == []),
  !.

test(test_send_view_change_00_02, [setup((setup(), build_config(Config)))]) :-
  V = Config.view0, N = Config.seqno1,
  receive_req_all(Config.req_digest_1, Config.timestamp),
  pre_prepare_all(Config.req_digest_1, Config.proposed_block_1, V, N),
  Replica_id = 2,
  apply_SEND_PREPARE(Config.req_digest_1, V, N, Replica_id),
  apply_RECEIVE_PREPARE(V, N, Config.req_digest_1, 3, Config.sender_signature.3, Replica_id),
  apply_RECEIVE_PREPARE(V, N, Config.req_digest_1, 1, Config.sender_signature.1, Replica_id),
  update_view_change_Pi(Replica_id),
  findall([N1,M,V1], view_change_Pi(Replica_id, N1, M, V1), Pi),
  assertion( Pi == [[N, Config.req_digest_1, V]]),
  !.

test(test_send_view_change_00_03,[setup((setup(), build_config(Config)))]) :-
  V = Config.view0, N = Config.seqno1,
  receive_req_all(Config.req_digest_1, Config.timestamp),
  pre_prepare_all(Config.req_digest_1, Config.proposed_block_1, V, N),
  % We move all replica to VIEW4, and we prepare same message at different view
  NewV = Config.view4,
  forall(
    between(0,3,Replica_id),
    set_view(Replica_id, NewV)
  ),
  apply_RECEIVE_PRE_PREPARE(NewV, N, Config.req_digest_1, Config.proposed_block_1, 0, Config.sender_signature.0, 2),
  apply_SEND_PREPARE(Config.req_digest_1, NewV, N, 2),
  apply_RECEIVE_PREPARE(NewV, N, Config.req_digest_1, 1, Config.sender_signature.1, 2),
  apply_RECEIVE_PREPARE(NewV, N, Config.req_digest_1, 3, Config.sender_signature.3, 2),
  update_view_change_Pi(2),
  findall([N1,M,V1], view_change_Pi(2, N1, M, V1), Pi),
  assertion( Pi == [[N,Config.req_digest_1,NewV]]),
  !.


test(test_send_view_change_00_04, [setup((setup(), build_config(Config)))]) :-
  V = Config.view0, N = Config.seqno1,
  receive_req_all(Config.req_digest_1, Config.timestamp),
  pre_prepare_all(Config.req_digest_1, Config.proposed_block_1, V, N),
  % We move all replica to VIEW4, and we prepare different message at different view
  NewV = Config.view4,
  forall(
    between(0,3,Replica_id),
    set_view(Replica_id, NewV)
  ),
  receive_req_all(Config.req_digest_2, Config.timestamp),
  apply_RECEIVE_PRE_PREPARE(NewV, N, Config.req_digest_2, Config.proposed_block_2, 0, Config.sender_signature.0, 2),
  apply_SEND_PREPARE(Config.req_digest_2, NewV, N, 2),
  apply_RECEIVE_PREPARE(NewV, N, Config.req_digest_2, 1, Config.sender_signature.1, 2),
  apply_RECEIVE_PREPARE(NewV, N, Config.req_digest_2, 3, Config.sender_signature.3, 2),
  update_view_change_Pi(2),
  findall([N1,M1,V1], view_change_Pi(2, N1, M1, V1), Pi),
  update_view_change_Qi(2),
  findall([N2,M2,B,V2], view_change_Qi(2, N2, M2, B, V2), Qi),
  assertion( Pi == [[N,Config.req_digest_2,NewV]]),
  assertion( Qi == [[N,Config.req_digest_1,Config.proposed_block_1, V], [N,Config.req_digest_2,Config.proposed_block_2, NewV]] ),
  !.

%
% Test build Qi
%

test(test_send_view_change_00_05, [setup((setup(),build_config(Config)))]) :-
  V = Config.view0, N = Config.seqno1,
  receive_req_all(Config.req_digest_1, Config.timestamp),
  pre_prepare_all(Config.req_digest_1, Config.proposed_block_1, V, N),
  % We move all replica to VIEW4 but don't get a new pre-prepare
  NewV = Config.view4,
  forall(
    between(0,3,Replica_id),
    set_view(Replica_id, NewV)
  ),
  update_view_change_Qi(2),
  findall([N1,M,B,V1], view_change_Qi(2, N1, M, B, V1), Qi),
  assertion( Qi == [[N,Config.req_digest_1,Config.proposed_block_1,V]]),
  !.

test(test_send_view_change_00_06, [setup((setup(),build_config(Config)))]) :-
  V = Config.view0, N = Config.seqno1,
  receive_req_all(Config.req_digest_1, Config.timestamp),
  pre_prepare_all(Config.req_digest_1, Config.proposed_block_1, V, N),
  % We move all replica to VIEW4 and get a new pre-prepare
  NewV = Config.view4,
  forall(
    between(0,3,Replica_id),
    set_view(Replica_id, NewV)
  ),apply_RECEIVE_PRE_PREPARE(NewV, N, Config.req_digest_1, Config.proposed_block_1, 0, Config.sender_signature.0, 2),
  update_view_change_Qi(2),
  findall([N1,M,B,V1], view_change_Qi(2, N1, M, B, V1), Qi),
  assertion( Qi == [[N,Config.req_digest_1,Config.proposed_block_1,NewV]] ),
  !.

%
% Test apply send view change
%

% We check that a msg_out_view_change is created in the message out buffer
test(test_send_view_change_00_07, [setup((setup(),build_config(Config)))]) :-
  V = Config.view0, N = Config.seqno1,
  receive_req_all(Config.req_digest_1, Config.timestamp),
  apply_RECEIVE_PRE_PREPARE(V, N, Config.req_digest_1, Config.proposed_block_1, 0, Config.sender_signature.0, 2), % V = 0, N = 1, Sender_id = 0
  apply_SEND_PREPARE(Config.req_digest_1, V, N, 2),
  apply_RECEIVE_PREPARE(V, N, Config.req_digest_1, 1, Config.sender_signature.1, 2),
  apply_RECEIVE_PREPARE(V, N, Config.req_digest_1, 3, Config.sender_signature.3, 2),
  NewV = Config.view1,
  apply_SEND_VIEW_CHANGE(NewV, 2),
  ExpectedH = 0,
  ExpectedC = "GENESIS_BLOCK_HASH",
  ExpectedPi = [[1, Config.req_digest_1, Config.view0]],
  ExpectedQi = [[1, Config.req_digest_1, Config.proposed_block_1, Config.view0]],
  assertion( msg_out_view_change(2, NewV, ExpectedH, ExpectedC, ExpectedPi, ExpectedQi) ),
  !.


% Check that messages with lower view are deleted
test(test_send_view_change_00_08, [setup((setup(),build_config(Config)))]) :-
  V = Config.view0, N = Config.seqno1,
  receive_req_all(Config.req_digest_1, Config.timestamp),
  apply_RECEIVE_PRE_PREPARE(V, N, Config.req_digest_1, Config.proposed_block_1, 0, Config.sender_signature.0, 2), % V = 0, N = 1, Sender_id = 0
  assertion( msg_log_pre_prepare(2, V, N, Config.req_digest_1, Config.proposed_block_1, 0, Config.sender_signature.0) ),
  V2 = Config.view4,
  forall(
    between(0,3,Replica_id),
    set_view(Replica_id, V2)
  ),
  apply_RECEIVE_PRE_PREPARE(V2, N, Config.req_digest_1, Config.proposed_block_1, 0, Config.sender_signature.0, 2), % V = 4, N = 1, Sender_id = 0
  apply_SEND_PREPARE(Config.req_digest_1, V2, N, 2),
  apply_RECEIVE_PREPARE(V2, N, Config.req_digest_1, 1, Config.sender_signature.1, 2),
  apply_RECEIVE_PREPARE(V2, N, Config.req_digest_1, 3, Config.sender_signature.3, 2),
  V3 = Config.view5,
  apply_SEND_VIEW_CHANGE(V3, 2),
  ExpectedH = 0,
  ExpectedC = "GENESIS_BLOCK_HASH",
  ExpectedPi = [[1, Config.req_digest_1, Config.view4]],
  ExpectedQi = [[1, Config.req_digest_1, Config.proposed_block_1, Config.view4]],
  assertion( msg_out_view_change(2, V3, ExpectedH, ExpectedC, ExpectedPi, ExpectedQi) ),
  assertion( \+ msg_log_pre_prepare(2, V, N, Config.req_digest_1, Config.proposed_block_1, 0, Config.sender_signature.0)),
  !.

:- end_tests(test_action_12_send_view_change_00).
