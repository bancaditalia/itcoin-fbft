% Run the test file with:
%   swipl -g run_tests -t halt test_viewchange_05_dead_primary.pl
% more details on the Prolog unit test framework: https://www.swi-prolog.org/pldoc/package/plunit.html
:- consult("../specs/pbft-replica-engine.pl").
:- consult("test_00_utils.pl").

% :- debug(correct_chi_3).

test_01_setup(Req_digest) :-
  init_all,
  set_synthetic_time_all(0),
  Req_digest = "REQUEST_DIGEST_BLOCK_1",
  Req_timestamp = 10,
  receive_req_all(Req_digest, Req_timestamp).

% needed at the beginning of each test-unit
:- begin_tests(test_viewchange_05_dead_primary_empty).

test(test_01, [ setup(test_01_setup(Req_digest)) ]) :-
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
  aggregate_all(count, pre_SEND_NEW_VIEW(_, _, 1), Count_SEND_NEW_VIEW),
  assertion( Count_SEND_NEW_VIEW == 1 ),

  pre_SEND_NEW_VIEW(Nu_snv, Chi_snv, 1),
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

  assertion((
    active_view(1),
    active_view(2),
    active_view(3)
  )),
  assertion( (
    pre_SEND_PRE_PREPARE(Req_digest, V, N, 1),
    V == 1,
    N == 1
  )),

  % print_all_dynamics,
  !.

:- end_tests(test_viewchange_05_dead_primary_empty).
