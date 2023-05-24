% Run the test file with:
%   swipl -g run_tests -t halt test_viewchange_05_dead_primary.pl
% more details on the Prolog unit test framework: https://www.swi-prolog.org/pldoc/package/plunit.html
:- consult("../specs/pbft-replica-engine.pl").
:- consult("test_00_utils.pl").

requests(Req_digest) :-
  Req_digest = "REQUEST_DIGEST_BLOCK_1",
  Req_timestamp = 10,
  receive_req_all(Req_digest, Req_timestamp).

pre_prepare(Proposed_block_hex) :-
  Proposed_block_hex = "PROPOSED_BLOCK_HEX",
  pre_SEND_PRE_PREPARE(Req_digest, V, N, 0),
  effect_SEND_PRE_PREPARE(Req_digest, Proposed_block_hex, V, N, 0),
  msg_out_clear_all(0),
  pre_prepare_all(Req_digest, Proposed_block_hex, V, N).

prepares() :-
  pre_SEND_PREPARE(Req_digest, V, N, 1),
  pre_SEND_PREPARE(Req_digest, V, N, 2),
  pre_SEND_PREPARE(Req_digest, V, N, 3),
  effect_SEND_PREPARE(Req_digest, V, N, 1),
  msg_out_clear_all(1),
  effect_SEND_PREPARE(Req_digest, V, N, 2),
  msg_out_clear_all(2),
  effect_RECEIVE_PREPARE(V, N, Req_digest, 1, "SIGNATURE_1", 2),
  effect_RECEIVE_PREPARE(V, N, Req_digest, 2, "SIGNATURE_2", 1).

test_01_setup(Req_digest, Proposed_block_hex) :-
  init_all,
  requests(Req_digest),
  pre_prepare(Proposed_block_hex),
  prepares,
  set_synthetic_time_all(21).

% needed at the beginning of each test-unit
:- begin_tests(test_viewchange_05_dead_primary_prepared).

test(test_01, [ setup(test_01_setup(Req_digest, Proposed_block_hex)) ]) :-
  writeln("\nInitially no replica has SEND_VIEW_CHANGE active"),
  foreach( between(0,3,Replica_id), (
    aggregate_all(count, pre_SEND_VIEW_CHANGE(_, Replica_id), 0),
    assertion( active_view(Replica_id) )
  )),

  writeln("Advancing synthetic time to the VIEW CHANGE timeout"),
  foreach( between(0,3,Replica_id), (
    set_synthetic_time(Replica_id, 41)
  )),

  writeln("Now all Replica are willing to initiate VIEW CHANGE"),
  foreach( between(0,3,Replica_id), (
    aggregate_all(count, pre_SEND_VIEW_CHANGE(_, Replica_id), 1),
    pre_SEND_VIEW_CHANGE(1, Replica_id)
  )),

  writeln("Apply SEND_VIEW_CHANGE at R1, R2 and R3"),
  foreach( between(1,3,Replica_id), (
    apply_SEND_VIEW_CHANGE(1, Replica_id)
  )),
  assertion( \+ active_view(1) ),

  writeln("R1, R2 and R3 receive each other VIEW_CHANGE"),
  foreach( between(1,3,Replica_id), (
    foreach( msg_out_view_change(Sender_id, V, H, C, P, Q), (
      Replica_id \= Sender_id -> (
        apply_RECEIVE_VIEW_CHANGE(V, H, C, P, Q, Sender_id, "dummy_signature", Replica_id)
      ); true
    ))
  )),

  once(
    pre_SEND_NEW_VIEW(Nu_snv, Chi_snv, 1)
  ),
  apply_SEND_NEW_VIEW(Nu_snv, Chi_snv, 1),
  assertion( \+ active_view(1) ),
  assertion( \+ pre_SEND_NEW_VIEW(_, _, 1) ),

  writeln("R2 and R3 receive R1 NEW_VIEW"),
  foreach( between(2,3,Replica_id), (
    msg_out_new_view(1, Nv_V, Nv_Nu, Nv_Chi),
    apply_RECEIVE_NEW_VIEW(Nv_V, Nv_Nu, Nv_Chi, 1, "dummy_signature", Replica_id)
  )),

  writeln("R1, R2 and R3 PROCESS_NEW_VIEW"),
  Hi = 0,
  foreach( between(1,3,Replica_id), (
    pre_PROCESS_NEW_VIEW(Hi, _, Chi_pnv, Replica_id),
    apply_PROCESS_NEW_VIEW(Hi, Chi_pnv, Replica_id)
  )),

  foreach( between(1,3,Replica_id), (
    assertion(active_view(Replica_id)),
    msg_log_pre_prepare(Replica_id, V, N, Req_digest, Block_hex, 1, _),
    assertion( Block_hex == Proposed_block_hex ),
    assertion( V == 1 ),
    assertion( N == 1 )
  )),

  foreach( between(2,3,Replica_id), (
    msg_log_prepare(Replica_id, V, N, Req_digest, Replica_id,_),
    msg_out_prepare(Replica_id, V, N, Req_digest),
    assertion( V == 1 ),
    assertion( N == 1 )
  )),

  % print_all_dynamics,
  !.

:- end_tests(test_viewchange_05_dead_primary_prepared).
