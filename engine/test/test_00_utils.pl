% Copyright (c) 2023 Bank of Italy
% Distributed under the GNU AGPLv3 software license, see the accompanying COPYING file.

:- consult("../fbft-replica-engine.pl").

% auxiliary predicates to test preconditions
active_SEND_PRE_PREPARE(Replica_id, Out_active) :-
  findall([Req_digest, V, N, Replica_id],
    pre_SEND_PRE_PREPARE(Req_digest, V, N, Replica_id),
    Out_active).

active_SEND_PREPARE(Replica_id, Out_active) :-
  findall([Req_digest, V, N],
    pre_SEND_PREPARE(Req_digest, V, N, Replica_id),
    Out_active).

active_SEND_COMMIT(Replica_id, Result) :-
  findall([Req_digest, V, N],
    pre_SEND_COMMIT(Req_digest, V, N, Replica_id),
    Result).

active_ROAST_INIT(Replica_id, Result) :-
  findall([Req_digest, V, N],
    pre_ROAST_INIT(Replica_id, Req_digest, V, N),
    Result).

active_EXECUTE(Replica_id, Result) :-
  findall([Req_digest, V, N],
    pre_EXECUTE(Req_digest, V, N, Replica_id),
    Result).

active_SEND_VIEW_CHANGE(Replica_id, Out_active) :-
  findall([V],
    pre_SEND_VIEW_CHANGE(V, Replica_id),
    Out_active).

active_SEND_NEW_VIEW(Replica_id, Out_active) :-
  pre_SEND_NEW_VIEW(Nu, Chi, Replica_id),
  Out_active = [Nu, Chi].

active_PROCESS_NEW_VIEW(Hi, Replica_id, Out_active) :-
  findall([Nu, Chi],
    pre_PROCESS_NEW_VIEW(Hi, Nu, Chi, Replica_id),
    Out_active).

test_active_send_pre_prepare(Replica_id, Expected_output) :-
  active_SEND_PRE_PREPARE(Replica_id, Output),
  assertion(Output == Expected_output).
test_active_send_prepare(Replica_id, Expected_output) :-
  active_SEND_PREPARE(Replica_id, Output),
  assertion(Output == Expected_output).
test_active_send_commit(Replica_id, Expected_output) :-
  active_SEND_COMMIT(Replica_id, Output),
  assertion(Output == Expected_output).
test_active_execute(Replica_id, Expected_output) :-
  active_EXECUTE(Replica_id, Output),
  assertion(Output == Expected_output).

init_all :-
  Cluster_size = 4,
  Initial_block_height = 0,
  Initial_block_hash = "GENESIS_BLOCK_HASH",
  Initial_block_timestamp = 0,
  Genesis_block_timestamp = 0,
  Target_block_time = 60,
  Db_filename = "file",
  Db_reset = true,
  foreach(
    between(0,3,Replica_id),
    init(Replica_id, Cluster_size, Initial_block_height, Initial_block_hash, Initial_block_timestamp, Genesis_block_timestamp, Target_block_time, Db_filename, Db_reset)
  ).

receive_req_all(Req_digest, Req_timestamp) :-
  foreach(
    between(0,3, Replica_id),
    effect_RECEIVE_REQUEST(Req_digest, Req_timestamp, Replica_id)
  ).

pre_prepare_all(Req_digest, Proposed_block, V, N) :-
  apply_SEND_PRE_PREPARE(Req_digest, Proposed_block, V, N, 0),
  foreach(
    between(1,3,Replica_id),
    (
      apply_RECEIVE_PRE_PREPARE(V, N, Req_digest, Proposed_block, 0, "SIG_FALSE", Replica_id)
    )
  ).

prepare_all(Req_digest, V, N) :-
  foreach(
    between(1,3,Replica_id),
    (
      apply_SEND_PREPARE(Req_digest, V, N, Replica_id)
    )
  ),
  foreach(
    between(0,3,Replica_id),
    (
      foreach(
        between(1,3,Sender_id),
        (
          Sender_id #\=Replica_id
          ->
          apply_RECEIVE_PREPARE(V, N, Req_digest, Sender_id, "SIG_FALSE", Replica_id)
          ;
          true
        )
      )
    )
  ).

set_synthetic_time_all(Synthetic_time) :-
  foreach(
    between(0,3,Replica_id),
    set_synthetic_time(Replica_id, Synthetic_time)
  ).