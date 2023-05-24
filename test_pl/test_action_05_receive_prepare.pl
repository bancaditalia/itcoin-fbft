:- consult("../specs/pbft-replica-engine.pl").
:- consult("test_00_utils.pl").

:- begin_tests(test_action_05_receive_prepare).


% replica does not receive prepare from primary
test(test_receive_prepare_01) :-
  init_all,
  Primary = 0, V=0, N=1,
  Request = "req_digest",
  Signature = "dummy signature",
  TestReplica = 1,
  \+apply_RECEIVE_PREPARE(V, N, Request, Primary, Signature, TestReplica).

% replica receives prepare from non-primary
test(test_receive_prepare_02) :-
  init_all,
  NonPrimary = 2, V=0, N=1,
  Request = "req_digest",
  Signature = "dummy signature",
  TestReplica = 1,
  apply_RECEIVE_PREPARE(V, N, Request, NonPrimary, Signature, TestReplica),
  assertion(msg_log_prepare(TestReplica, V, N, Request, NonPrimary, Signature)).

:- end_tests(test_action_05_receive_prepare).
