:- consult("test_00_utils.pl").

:-
  ReplicaId0 = 0,
  ReplicaId1 = 1,
  ReplicaId2 = 2,
  ReplicaId3 = 3,
  Replica = 2,
  Cluster_size = 4,
  ViewId = 1,
  NewViewId is ViewId + 1,
  CheckpointIndex = 0, CheckpointDigest = "checkpoint_digest",
  Checkpoint = (CheckpointIndex, CheckpointDigest),
  Checkpoints = [Checkpoint],
  Prepared0 = [(1, "request_1_digest", ViewId), (3, "request_3_digest", ViewId)],
  Prepared1 = [(3, "request_3_digest", ViewId)],
  Prepared2 = [(3, "request_3_digest", ViewId)],
  Prepared3 = [(3, "request_3_digest", ViewId)],
  PrePrepared0 = [(1, "request_1_digest", ViewId), (2, "request_2_digest", ViewId), (3, "request_3_digest", ViewId)],
  PrePrepared1 = [(1, "request_1_digest", ViewId), (3, "request_3_digest", ViewId)],
  PrePrepared2 = [(1, "request_1_digest", ViewId), (3, "request_3_digest", ViewId)],
  PrePrepared3 = [(1, "request_1_digest", ViewId), (3, "request_3_digest", ViewId)],
  init(Replica, Cluster_size),
  foreach(
    between(0,3,Replica_id),
    set_view(Replica_id, NewViewId)
  ),
  %
  % recall definition:
  %  msg_log_add_view_change(Replica_id, Vc_digest,      V,         Hi, C,           Pi,       Qi,          Sender_id)
     msg_log_add_view_change(ReplicaId2, "vc_digest_0",  NewViewId, 0,  Checkpoints, Prepared0, PrePrepared0, ReplicaId0),
     msg_log_add_view_change(ReplicaId2, "vc_digest_1",  NewViewId, 0,  Checkpoints, Prepared1, PrePrepared1, ReplicaId1),
     msg_log_add_view_change(ReplicaId2, "vc_digest_2",  NewViewId, 0,  Checkpoints, Prepared2, PrePrepared2, ReplicaId2),
     msg_log_add_view_change(ReplicaId2, "vc_digest_3",  NewViewId, 0,  Checkpoints, Prepared3, PrePrepared3, ReplicaId3),
  %
  % recall definition:
  %        msg_log_add_view_change_ack(Replica_id, V,         Sender_id,  Vc_sender_id, Vc_digest)
  %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
  %  in the following, we populate VIEW-CHANGE-ACK messages.
  %  * VIEW-CHANGE-ACKs about VIEW-CHANGE message "vc_digest_0"
  %    * 0->0, ack sent to node 2 is skipped (don't send view-change to self)
  %        msg_log_add_view_change_ack(ReplicaId2, NewViewId, ReplicaId0, ReplicaId0,   "vc_digest_0"),
  %    * 0->1, ack sent to node 2
           msg_log_add_view_change_ack(ReplicaId2, NewViewId, ReplicaId1, ReplicaId0,   "vc_digest_0"),
  %    * 0->2, ack sent to node 2 is skipped (don't send ack to self)
  %        msg_log_add_view_change_ack(ReplicaId2, NewViewId, ReplicaId2, ReplicaId0,   "vc_digest_0"),
  %    * 0->3, ack sent to node 2
           msg_log_add_view_change_ack(ReplicaId2, NewViewId, ReplicaId3, ReplicaId0,   "vc_digest_0"),
  %  * VIEW-CHANGE-ACKs about VIEW-CHANGE message "vc_digest_1"
  %    * 1->0, ack sent to node 2
           msg_log_add_view_change_ack(ReplicaId2, NewViewId, ReplicaId0, ReplicaId1,   "vc_digest_1"),
  %    * 1->1, ack sent to node 2 is skipped (don't send view-change to self)
  %        msg_log_add_view_change_ack(ReplicaId2, NewViewId, ReplicaId1, ReplicaId1,   "vc_digest_1"),
  %    * 1->2, ack sent to node 2 is skipped (don't send ack to self)
  %        msg_log_add_view_change_ack(ReplicaId2, NewViewId, ReplicaId2, ReplicaId1,   "vc_digest_1"),
  %    * 1->3, ack sent to node 2
           msg_log_add_view_change_ack(ReplicaId2, NewViewId, ReplicaId3, ReplicaId1,   "vc_digest_1"),
  %  * VIEW-CHANGE-ACKs about VIEW-CHANGE message "vc_digest_2".
  %    * 2->0, ack sent to node 2
           msg_log_add_view_change_ack(ReplicaId2, NewViewId, ReplicaId0, ReplicaId2,   "vc_digest_2"),
  %    * 2->1, ack sent to node 2
           msg_log_add_view_change_ack(ReplicaId2, NewViewId, ReplicaId1, ReplicaId2,   "vc_digest_2"),
  %    * 2->2, ack sent to node 2 is skipped (don't send view-change to self)
  %        msg_log_add_view_change_ack(ReplicaId2, NewViewId, ReplicaId2, ReplicaId2,   "vc_digest_2"),
  %    * 2->3, ack sent to node 2
           msg_log_add_view_change_ack(ReplicaId2, NewViewId, ReplicaId3, ReplicaId2,   "vc_digest_2"),
  %  * VIEW-CHANGE-ACKs about VIEW-CHANGE message "vc_digest_3"
  %    * 3->0, ack sent to node 2
           msg_log_add_view_change_ack(ReplicaId2, NewViewId, ReplicaId0, ReplicaId3,   "vc_digest_3"),
  %    * 3->1, ack sent to node 2
           msg_log_add_view_change_ack(ReplicaId2, NewViewId, ReplicaId1, ReplicaId3,   "vc_digest_3"),
  %    * 3->2, ack sent to node 2 is skipped (don't send ack to self)
  %        msg_log_add_view_change_ack(ReplicaId2, NewViewId, ReplicaId2, ReplicaId3,   "vc_digest_3"),
  %    * 3->3, ack sent to node 2 is skipped (don't send view-change to self)
  %        msg_log_add_view_change_ack(ReplicaId2, NewViewId, ReplicaId3, ReplicaId3,   "vc_digest_3"),
  %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
  true.
