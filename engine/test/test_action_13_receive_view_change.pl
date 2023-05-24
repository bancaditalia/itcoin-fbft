% Copyright (c) 2023 Bank of Italy
% Distributed under the GNU AGPLv3 software license, see the accompanying COPYING file.

:- consult("test_00_utils.pl").

% Command to debug
% guitracer, trace.

%
% Test build Pi
%

:- begin_tests(test_action_13_receive_view_change).

% Test that we can receive a view change from another replica.
% Expected outcome: a new entry in view-change log and in view-change-ack out queue
test(test_receive_view_change_from_another_replica) :-
  init_all,
  Replica_id = 0,
  V = 1, H = 0, C = [], P = [], Q = [],
  Sender_id = 1,
  Sender_sig = "SIGNATURE_1",
  digest_view_change(V, H, C, P, Q, Sender_id, Vc_digest),
  apply_RECEIVE_VIEW_CHANGE(V, H, C, P, Q, Sender_id, Sender_sig, Replica_id),
  assertion(msg_log_view_change(Replica_id, Vc_digest, V, H, C, P, Q, Sender_id, Sender_sig)).

% Test that we *cannot* receive a view change from the same replica.
test(test_receive_view_change_from_same_replica_fails) :-
  init_all,
  Replica_id = 0,
  V = 1, H = 0, C = [], P = [], Q = [],
  Sender_id = Replica_id,
  Sender_sig = "SIGNATURE_0",
  \+ apply_RECEIVE_VIEW_CHANGE(V, H, C, P, Q, Sender_id, Sender_sig, Replica_id).

% Test that adding the same view-change message twice fails.
test(test_receive_same_view_change_twice_fails) :-
  init_all,
  Replica_id = 0,
  V = 1, H = 0, C = [], P = [], Q = [],
  Sender_id = 1,
  Sender_sig = "SIGNATURE_1",
  apply_RECEIVE_VIEW_CHANGE(V, H, C, P, Q, Sender_id, Sender_sig, Replica_id),
  % adding the same view-change message a second time fails
  \+ apply_RECEIVE_VIEW_CHANGE(V, H, C, P, Q, Sender_id, Sender_sig, Replica_id).


% Test that adding a view-change message with same primary key of another view-change message fails
test(test_receive_view_change_same_primary_keys_different_content) :-
  init_all,
  Replica_id = 0,
  V = 1, H = 0, C = [], P = [], Q = [],
  Sender_id = 1,
  Sender_sig = "SIGNATURE_1",
  apply_RECEIVE_VIEW_CHANGE(V, H, C, P, Q, Sender_id, Sender_sig, Replica_id),
  % keeping (Replica_id, V, Sender_id) the same, changing other fields make the view-change add to fail
  \+ apply_RECEIVE_VIEW_CHANGE(V, H + 1, C, P, Q, Sender_id, Sender_sig, Replica_id),
  \+ apply_RECEIVE_VIEW_CHANGE(V, H, ["checkpoint"], P, Q, Sender_id, Sender_sig, Replica_id),
  \+ apply_RECEIVE_VIEW_CHANGE(V, H, C, ["prepared"], Q, Sender_id, Sender_sig, Replica_id),
  \+ apply_RECEIVE_VIEW_CHANGE(V, H, C, P, ["preprepared"], Sender_id, Sender_sig, Replica_id).


:- end_tests(test_action_13_receive_view_change).
