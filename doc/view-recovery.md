
Inizializzi le quattro repliche.

Passi tre repliche alla vista 1, lasciando la replica R3 alla vista 0.

Effettui un ciclo di normal operation sulle tre repliche e su R3.
Le 3 repliche arrivano a execute, mentre R3 ritorna false sia alle precondition, sia ai messaggi di RECEIVE_PRE_PREPARE(V=1 ... ), RECEIVE_PREPARE(V=1), RECEIVE_COMMIT(V=1).

Tutte le repliche ricevono il blocco, apply_RECEIVE_BLOCK(R3) ritornera true per tutte.

Effettui un secondo ciclo di normal operation sulle tre repliche e su R3.
Succede la stessa cosa di riga7.

<!-- Implementi questo codice sotto -->

msg_log_commit_view_recovery(Replica_id, V, N, Sender_id)
msg_log_commit_view_recovery(Replica_id, V, N, Sender_id)
checkpoint(Replica_id, N, Block_hash, Block_time)

apply_RECEIVE_COMMIT(Commit_v, Commit_n, Commit_associated_data, Sender_id, Sender_sig, Replica_id) :-
  Sender_id =\= Replica_id,
  if( in_wv(Commit_v, Commit_n, Replica_id) )
  {
    msg_log_add_commit(Replica_id, Commit_v, Commit_n, Commit_associated_data, Sender_id, Sender_sig);
  }
  else if( Commit_v > View_i )
  {
    msg_log_recovery_add_commit(Replica_id, Commit_v, Commit_n)
  }

pre_RECOVER_VIEW(Replice_id, V) :-
  V>View_i,
  active_view,
  msg_log_commit_recovery(Replica_id, V, N, Sender_id),
  len(distinct Sender_id) >= quorum,
  checkpoint(Replica_id, N, _, _),
  V is the max among the views reaching the quorum, so that we don't trigger view recovery for more than one view.

effect_RECOVER_VIEW(Replica_id, V) :-
  set_view(Replica_id, V),
  delete all msg_log_commit_recovery where view < V.

<!-- Effettui un secondo ciclo di normal operation sulle tre repliche e su R3. Tutte le repliche partecipano -->
