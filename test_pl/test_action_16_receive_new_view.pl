:- consult("../specs/pbft-replica-engine.pl").
:- consult("test_00_utils.pl").

% Command to debug
% guitracer, trace.

%
% Test build Pi
%

:- begin_tests(test_action_16_receive_new_view).

setup_test(V, Nu, Chi, Signatures) :-
  init_all,
  V = 1,
  foreach(
    between(0,3,Replica_id),
    set_view(Replica_id, V)
  ),
  % dummy Nu and Chi sets
  Nu = [(0, "vc_digest_0"), (1, "vc_digest_1"), (2, "vc_digest_2")],
  Chi = [(0, "GENESIS_BLOCK_HASH", "DATA")],
  % signatures
  Signatures = _{
    0 : "SIGNATURE_0",
    1 : "SIGNATURE_1",
    2 : "SIGNATURE_2",
    3 : "SIGNATURE_3"
  }.


% Test that a non-primary replica can receive a new-view message from the (new) primary.
% Expected outcome: a new entry in new-view log
test(test_receive_new_view_from_primary, [ setup(setup_test(V, Nu, Chi, Signatures)) ]) :-
  Replica_id = 2,
  primary(V, PrimaryId),
  apply_RECEIVE_NEW_VIEW(V, Nu, Chi, PrimaryId, Signatures.PrimaryId, Replica_id),
  assertion(msg_log_new_view(Replica_id, V, Nu, Chi, Signatures.PrimaryId)).

% Test that receiving the same new-view message fails.
test(test_receive_same_new_view_fails, [ setup(setup_test(V, Nu, Chi, Signatures)) ]) :-
  Replica_id = 2,
  primary(V, PrimaryId),
  apply_RECEIVE_NEW_VIEW(V, Nu, Chi, PrimaryId, Signatures.PrimaryId, Replica_id),
  % the second time we try to receive the same new-view message the predicate has no effect
  findall([Replica_id1, View_i1, Nu1, Chi1, Sig1], msg_log_new_view(Replica_id1, View_i1, Nu1, Chi1, Sig1), OldNewViews),
  apply_RECEIVE_NEW_VIEW(V, Nu, Chi, PrimaryId, Signatures.PrimaryId, Replica_id),
  findall([Replica_id2, View_i2, Nu2, Chi2, Sig2], msg_log_new_view(Replica_id2, View_i2, Nu2, Chi2, Sig2), NewNewViews),
  assertion(OldNewViews == NewNewViews).


% Test that receiving the new-view message from a non-primary replica fails.
test(test_receive_new_view_from_non_primary_fails, [ setup(setup_test(V, Nu, Chi, Signatures)) ]) :-
  Replica_id = 2,
  primary(V, PrimaryId),
  NonPrimaryId #\= PrimaryId,
  0 #< NonPrimaryId, NonPrimaryId #< 3,
  assertion(\+ apply_RECEIVE_NEW_VIEW(V, Nu, Chi, NonPrimaryId, Signatures.NonPrimaryId, Replica_id)).

% Test that receiving the new-view message of a newer view fails.
test(test_receive_new_view_from_newer_view_fails, [ setup(setup_test(V, Nu, Chi, Signatures)) ]) :-
  Replica_id = 2,
  primary(V, PrimaryId),
  NewView = V + 1,
  assertion(\+ apply_RECEIVE_NEW_VIEW(NewView, Nu, Chi, PrimaryId, Signatures.PrimaryId, Replica_id)).


:- end_tests(test_action_16_receive_new_view).
