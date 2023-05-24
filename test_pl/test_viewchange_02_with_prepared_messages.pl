:- consult("test_00_utils.pl").

:- begin_tests(test_viewchange_02_with_prepared_messages).


% We set up a network of 4 replicas where a view-change is triggered
% but the primary participates in the view-change protocol.
% In this case, we have only the initial checkpoint
% and a partial list of pre-prepared/prepared messages.
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
  checkpoint(Replica, _, CheckpointDigest, _),
  Checkpoint = CheckpointDigest,
  AssociatedData = "block_1",
  Prepared0 = [[1, "request_1_digest", ViewId], [3, "request_3_digest", ViewId]],
  Prepared1 = [[3, "request_3_digest", ViewId]],
  Prepared2 = [[3, "request_3_digest", ViewId]],
  Prepared3 = [[3, "request_3_digest", ViewId]],
  PrePrepared0 = [[1, "request_1_digest", AssociatedData, ViewId], [2, "request_2_digest", AssociatedData, ViewId], [3, "request_3_digest", AssociatedData, ViewId]],
  PrePrepared1 = [[1, "request_1_digest", AssociatedData, ViewId], [3, "request_3_digest", AssociatedData, ViewId]],
  PrePrepared2 = [[1, "request_1_digest", AssociatedData, ViewId], [3, "request_3_digest", AssociatedData, ViewId]],
  PrePrepared3 = [[1, "request_1_digest", AssociatedData, ViewId], [3, "request_3_digest", AssociatedData, ViewId]],
  foreach(
    between(0,3,Replica_id),
    set_view(Replica_id, NewViewId)
  ),
  %
  % recall definition:
  %  msg_log_add_view_change(Replica_id, Vc_digest,      V,         Hi, C,          Pi,        Qi,           Sender_id,  Sender_signature)
     msg_log_add_view_change(ReplicaId2, "vc_digest_0",  NewViewId, 0,  Checkpoint, Prepared0, PrePrepared0, ReplicaId0, Signature0),
     msg_log_add_view_change(ReplicaId2, "vc_digest_1",  NewViewId, 0,  Checkpoint, Prepared1, PrePrepared1, ReplicaId1, Signature1),
     msg_log_add_view_change(ReplicaId2, "vc_digest_2",  NewViewId, 0,  Checkpoint, Prepared2, PrePrepared2, ReplicaId2, Signature2),
     msg_log_add_view_change(ReplicaId2, "vc_digest_3",  NewViewId, 0,  Checkpoint, Prepared3, PrePrepared3, ReplicaId3, Signature3),
  true.


test(main, [ setup(setup_viewchange()) ]) :-
  correct_nu_once(Nu, 2), correct_chi(Chi, Nu, 2),

  % require that for seqno 1 "request_1" is selected
  assertion(member([1, "request_1_digest", _], Chi)),

  % require that for seqno 2 the "null" is selected
  assertion(member([2, "null", _], Chi)),

  % require that for seqno 3 the "request_3" is selected
  assertion(member([3, "request_3_digest", _], Chi)).

:- end_tests(test_viewchange_02_with_prepared_messages).
