:- consult("test_00_utils.pl").

:- begin_tests(test_viewchange_01_empty).

% We set up a network of 4 replicas where a view-change is triggered
% but the primary participates in the view-change protocol.
% in this case, there are no checkpoints (except the initial, default checkpoint)
% and empty list of pre-prepared/prepared messages.
setup_viewchange() :-
  init_all,
  ReplicaId0 = 0, Signature0 = "signature_0",
  ReplicaId1 = 1, Signature1 = "signature_1",
  ReplicaId2 = 2, Signature2 = "signature_2",
  ReplicaId3 = 3, Signature3 = "signature_3",
  Replica = 2,
  ViewId = 1,
  NewViewId is ViewId + 1,
  % select first checkpoint at test replica
  checkpoint(Replica, CheckpointIndex, CheckpointDigest, _),
  Checkpoint = (CheckpointIndex, CheckpointDigest),
  Checkpoints = [Checkpoint],
  Prepared = [],
  PrePrepared = [],
  foreach(
    between(0,3,Replica_id),
    set_view(Replica_id, NewViewId)
  ),
  %
  % recall definition:
  %  msg_log_add_view_change(Replica_id, Vc_digest,      V,         Hi, C,           Pi,       Qi,          Sender_id,  Sender_signature)
     msg_log_add_view_change(ReplicaId2, "vc_digest_0",  NewViewId, 0,  Checkpoints, Prepared, PrePrepared, ReplicaId0, Signature0),
     msg_log_add_view_change(ReplicaId2, "vc_digest_1",  NewViewId, 0,  Checkpoints, Prepared, PrePrepared, ReplicaId1, Signature1),
     msg_log_add_view_change(ReplicaId2, "vc_digest_2",  NewViewId, 0,  Checkpoints, Prepared, PrePrepared, ReplicaId2, Signature2),
     msg_log_add_view_change(ReplicaId2, "vc_digest_3",  NewViewId, 0,  Checkpoints, Prepared, PrePrepared, ReplicaId3, Signature3),
  true.


test(main, [ setup(setup_viewchange()) ]) :-
  correct_nu_once(Nu, 2), correct_chi(Chi, Nu, 2),
  assertion(Chi == []).

:- end_tests(test_viewchange_01_empty).
