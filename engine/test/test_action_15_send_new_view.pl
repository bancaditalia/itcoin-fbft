% Copyright (c) 2023 Bank of Italy
% Distributed under the GNU AGPLv3 software license, see the accompanying COPYING file.

:- consult("test_00_utils.pl").

% Command to debug
% guitracer, trace.

%
% Test build Pi
%

:- begin_tests(test_action_15_send_new_view).

% set up a view-change scenario:
%  - old view is 0, new view is 1 -> new primary is 1
%  - no prepared/preprepared requests, only the first checkpoint
setup_viewchange() :-
  init_all,
  V = 1,
  % replica ids
  R0 = 0, R1 = 1, R2= 2, R3 = 3,
  NewPrimary = R1,
  % select first checkpoint at primary
  checkpoint(NewPrimary, _, CheckpointDigest, _),
  C = [CheckpointDigest],
  % empty set of prepared and pre-prepared requests
  P = [], Q = [],
  % highest seq number of checkpoint is 0 (just the initial checkpoint)
  H = 0,
  % view-change message digests
  digest_view_change(V, H, C, P, Q, R0, VCD0),
  digest_view_change(V, H, C, P, Q, R1, VCD1),
  digest_view_change(V, H, C, P, Q, R2, VCD2),
  digest_view_change(V, H, C, P, Q, R3, VCD3),
  % signatures
  Sig0 = "SIGNATURE_0",
  Sig1 = "SIGNATURE_1",
  Sig2 = "SIGNATURE_2",
  Sig3 = "SIGNATURE_3",
  % set new view for every replica
  foreach(
    between(0,3,Replica_id),
    set_view(Replica_id, V)
  ),
  % add view-change messages
  msg_log_add_view_change(NewPrimary, VCD0,  V, H,  C, P, Q, R0, Sig0),
  msg_log_add_view_change(NewPrimary, VCD1,  V, H,  C, P, Q, R1, Sig1),
  msg_log_add_view_change(NewPrimary, VCD2,  V, H,  C, P, Q, R2, Sig2),
  msg_log_add_view_change(NewPrimary, VCD3,  V, H,  C, P, Q, R3, Sig3),
  true.

% test that Chi is empty when P is
test(test_send_new_view, [ setup(setup_viewchange()) ]) :-
    NewPrimary = 1,
    active_SEND_NEW_VIEW(NewPrimary, [_, Chi]),
    assertion(Chi == []).

% test that "apply_SEND_NEW_VIEW" adds the new-view message both in log and in out queue,
%  and makes correct_new_view true
test(test_send_new_view, [ setup(setup_viewchange()) ]) :-
    NewPrimary = 1,
    view(NewPrimary, V),
    active_SEND_NEW_VIEW(NewPrimary, [Nu, Chi]),
    apply_SEND_NEW_VIEW(Nu, Chi, NewPrimary),
    msg_log_new_view(Replica_id, V, Nu, Chi, _),
    msg_out_new_view(Replica_id, V, Nu, Chi),
    correct_new_view(Chi, Nu, NewPrimary),
    !.

% test that "active_SEND_NEW_VIEW" fails when a new-view is already sent
test(test_send_new_view_fails_when_new_view_already_sent, [ setup(setup_viewchange()) ]) :-
    NewPrimary = 1,
    active_SEND_NEW_VIEW(NewPrimary, [OldNu, OldChi]),
    !,
    msg_log_add_new_view(NewPrimary, NewPrimary, OldNu, OldChi, "dummy signature"),
    \+ active_SEND_NEW_VIEW(NewPrimary, _),
    !.

% test that "active_SEND_NEW_VIEW" fails when the sender is not the new primary
test(test_send_new_view_fails_when_sender_is_not_the_primary) :-
    init_all,
    V = 2,
    R0 = 0, R1 = 1, R2= 2, R3 = 3,
    NotPrimary = R1,
    % select first checkpoint
    checkpoint(NotPrimary, CheckpointIndex, CheckpointDigest, _),
    C = [(CheckpointIndex, CheckpointDigest)],
    % empty set of prepared and pre-prepared requests
    P = [], Q = [],
    % highest seq number of checkpoint is 0 (just the initial checkpoint)
    H = 0,
    % signatures
    Sig0 = "SIGNATURE_0",
    Sig1 = "SIGNATURE_1",
    Sig2 = "SIGNATURE_2",
    Sig3 = "SIGNATURE_3",
    % view-change message digests
    VCD0 = "vc_digest_0", VCD1 = "vc_digest_1", VCD2 = "vc_digest_2", VCD3 = "vc_digest_3",
    % set new view for every replica
    foreach(
      between(0,3,Replica_id),
      set_view(Replica_id, V)
    ),
    % add view-change messages
    msg_log_add_view_change(NotPrimary, VCD0,  V, H,  C, P, Q, R0, Sig0),
    msg_log_add_view_change(NotPrimary, VCD1,  V, H,  C, P, Q, R1, Sig1),
    msg_log_add_view_change(NotPrimary, VCD2,  V, H,  C, P, Q, R2, Sig2),
    msg_log_add_view_change(NotPrimary, VCD3,  V, H,  C, P, Q, R3, Sig3),
    correct_nu_once(Nu, NotPrimary), correct_chi(Chi, Nu, NotPrimary),
    \+ active_SEND_NEW_VIEW(NotPrimary, [Nu, Chi]),
    !.

:- end_tests(test_action_15_send_new_view).
