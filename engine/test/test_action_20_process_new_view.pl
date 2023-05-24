% Copyright (c) 2023 Bank of Italy
% Distributed under the GNU AGPLv3 software license, see the accompanying COPYING file.

:- consult("test_00_utils.pl").

% Command to debug
% guitracer, trace.

%
% Test build Pi
%

:- begin_tests(test_action_20_process_new_view).

setup_test(TestReplicaId, Nu, Chi, H) :-
  init_all,
  V = 1,
  R0 = 0, R1 = 1, R2 = 2, R3 = 3,
  TestReplicaId = R2,
  checkpoint(TestReplicaId, _, CheckpointDigest, _),
  ReqDigest = "request_1_digest", Request1SeqNo = 1, AssociatedData = "BLOCK_1",
  msg_log_add_request(TestReplicaId, ReqDigest, 1),
  C = CheckpointDigest,
  % empty set of prepared and pre-prepared requests
  P = [[Request1SeqNo, ReqDigest, V]], Q = [[Request1SeqNo, ReqDigest, AssociatedData, V]],
  % highest seq number of checkpoint is 0 (just the initial checkpoint)
  H = 0,
  % view-change message digests
  VCD0 = "vc_digest_0", VCD1 = "vc_digest_1", VCD2 = "vc_digest_2", VCD3 = "vc_digest_3",
  % signatures
  Sig0 = "SIGNATURE_0", Sig1 = "SIGNATURE_1", Sig2 = "SIGNATURE_2", Sig3 = "SIGNATURE_3",
  % set new view for every replica
  foreach(
    between(0,3,Replica_id),
    set_view(Replica_id, V)
  ),
  % add view-change messages
  msg_log_add_view_change(TestReplicaId, VCD0,  V, H,  C, P, Q, R0, Sig0),
  msg_log_add_view_change(TestReplicaId, VCD1,  V, H,  C, P, Q, R1, Sig1),
  msg_log_add_view_change(TestReplicaId, VCD2,  V, H,  C, P, Q, R2, Sig2),
  msg_log_add_view_change(TestReplicaId, VCD3,  V, H,  C, P, Q, R3, Sig3),
  % dummy Nu and Chi sets
  Nu = [[0, VCD0], [1, VCD1], [2, VCD2], [3, VCD3]],
  % compute Chi
  Chi = [[Request1SeqNo, ReqDigest, AssociatedData]],
  set_active_view(TestReplicaId, false),
  msg_log_add_new_view(TestReplicaId, V, Nu, Chi, Sig1).


% test that the precondition are satisfied for the chosen Nu and Chi
test(test_pre_process_new_view, [ setup(setup_test(TestReplicaId, Nu, Chi, H)) ]) :-
  pre_PROCESS_NEW_VIEW(H, Nu, Chi, TestReplicaId),
  !.

% test that the effects of PROCESS_NEW_VIEW are correct.
/* test(test_apply_process_new_view, [ setup(setup_test(TestReplicaId, _Nu, Chi, H)) ]) :-
  apply_PROCESS_NEW_VIEW(H, Chi, TestReplicaId),
  !,
  view(TestReplicaId, V),
  % assert that request_1 is present in preprepare log
  assertion(msg_log_pre_prepare(TestReplicaId, V, 1, "request_1_digest", _, _, _)),
  % assert that request_1 is present in prepare log
  assertion(msg_log_prepare(TestReplicaId, V, 1, "request_1_digest", TestReplicaId, _)).*/

:- end_tests(test_action_20_process_new_view).
