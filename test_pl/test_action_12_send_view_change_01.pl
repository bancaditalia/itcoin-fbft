%
% Test the increase of the timeout
%

:- consult("../specs/pbft-replica-engine.pl").
:- consult("test_00_utils.pl").

:- begin_tests(test_action_12_send_view_change_01).

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

test(test_send_view_change_01_00, [setup((setup(), build_config(Config)))]) :-
  receive_req_all(Config.req_digest_1, Config.timestamp),
  pre_prepare_all(Config.req_digest_1, Config.proposed_block_1, Config.view0, Config.seqno1),
  forall(
    between(0,3,Replica_id),
    set_synthetic_time(Replica_id, Config.timestamp)
  ),
  active_SEND_VIEW_CHANGE(0, Active_0),
  active_SEND_VIEW_CHANGE(1, Active_1),
  active_SEND_VIEW_CHANGE(2, Active_2),
  active_SEND_VIEW_CHANGE(3, Active_3),
  assertion( Active_0 == [] ),
  assertion( Active_1 == [] ),
  assertion( Active_2 == [] ),
  assertion( Active_3 == [] ),
  !.

test(test_send_view_change_01_01, [setup(build_config(Config))]) :-
  V = Config.view4, N = Config.seqno1,
  forall(
    between(0,3,Replica_id),
    set_view(Replica_id, V)
  ),
  apply_RECEIVE_PRE_PREPARE(V, N, Config.req_digest_1, Config.proposed_block_1, 0, Config.sender_signature.0, 2),
  timeout(0, Timeout),
  T = Config.timestamp + Timeout + 1,
  forall(
    between(0,3,Replica_id),
    set_synthetic_time(Replica_id, T)
  ),
  active_SEND_VIEW_CHANGE(0, Active_actions_0),
  active_SEND_VIEW_CHANGE(1, Active_actions_1),
  active_SEND_VIEW_CHANGE(2, Active_actions_2),
  active_SEND_VIEW_CHANGE(3, Active_actions_3),
  NewV = Config.view5,
  assertion( Active_actions_0 == [[NewV]] ),
  assertion( Active_actions_1 == [[NewV]] ),
  assertion( Active_actions_2 == [[NewV]] ),
  assertion( Active_actions_3 == [[NewV]] ),
  !.

:- end_tests(test_action_12_send_view_change_01).
