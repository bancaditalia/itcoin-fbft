:- consult("../specs/pbft-replica-engine.pl").
:- consult("test_00_utils.pl").

:- begin_tests(test_01_pbft_normal_operation_happy_path).

build_config(Config) :-
  Config = _{
    req_digest_1: "REQUEST_1",
    req_digest_2: "REQUEST_2",
    req_timestamp_1: 34,
    req_timestamp_2: 35,
    associated_data: "DATA",
    value: "STATE_1", % state value and digest use the same value
    replica_own_signature: "SIG_OWN_REPLICA",
    sender_signature: _{ % sender signatures indexed by replica_id
      0: "SIGNATURE_0",
      1: "SIGNATURE_1",
      2: "SIGNATURE_2",
      3: "SIGNATURE_3"
    }
  }.

% Initialize 4 replicas
setup_test() :-
  init_all.

% Replica 0 and 1 receive two different Requests
test(test_happy_path_01, [setup((setup_test(), build_config(Config)))]) :-
  effect_RECEIVE_REQUEST(Config.req_digest_1, Config.req_timestamp_1, 0),
  effect_RECEIVE_REQUEST(Config.req_digest_2, Config.req_timestamp_2, 1),
  % Primary should be willing to send the Pre-prepare
  V = 0, N = 1,
  test_active_send_pre_prepare(
    0,[[Config.req_digest_1, V, N, 0]]
  ),
  test_active_send_pre_prepare(1,[]).

% Replica 0 Sends the Pre-Prepare
test(test_happy_path_02, [setup(build_config(Config))]) :-
  V = 0, N = 1,
  effect_SEND_PRE_PREPARE(Config.req_digest_1, Config.associated_data, V, N, 0),
  % primary no longer wants to send the pre-prepare
  test_active_send_pre_prepare(0, []).

% Replica 1, 2 and 3 Receive the Pre-Prepare
test(happy_path_03, [setup(build_config(Config))]) :-
  Primary_id = 0,
  Primary_signature = (Config.sender_signature).0,
  V = 0, N = 1,
  msg_out_clear_all(Primary_id),
  foreach(
    between(1,3,Replica_id),
    effect_RECEIVE_PRE_PREPARE(V, N, Config.req_digest_1, Config.associated_data, Primary_id, Primary_signature, Replica_id)
  ),
  % Nothing should happen, since the replicas have not received the requests
  test_active_send_prepare(1, []).

% Replica 1, 2 and 3 also receive the piggybacked Request and get ready to send the Prepare
test(test_happy_path_04, [setup(build_config(Config))]) :-
  V = 0, N = 1,
  % Replica 1,2,3 receive the request
  foreach(
    between(1,3,Replica_id),
    effect_RECEIVE_REQUEST(Config.req_digest_1, Config.req_timestamp_1, Replica_id)
  ),
  % Replica 1,2,3 should be willing to send the Prepare
  test_active_send_prepare(0, []),
  foreach(
    between(1,3,Replica_id),
    test_active_send_prepare(Replica_id, [[Config.req_digest_1, V, N]])
  ).

% Replica 1, 2 and 3 send the Prepare
test(test_happy_path_05, [setup(build_config(Config))]) :-
  V = 0, N = 1,
  foreach(
    between(1,3,Replica_id),
    effect_SEND_PREPARE(Config.req_digest_1, V, N, Replica_id)

  ),
  % Message log of 1, 2, 3 should contain the Prepare
  foreach(
    between(1,3,Replica_id),
    assertion(msg_log_prepare(Replica_id, V, N, Config.req_digest_1, Replica_id, Config.replica_own_signature))
  ).

% Replica 0, 1, 2, 3 receive prepare and get prepared to send commit
test(test_happy_path_06, [setup(build_config(Config))]) :-
  V = 0, N = 1,
  %replica 0 (primary) receives from all other replicas
  forall(
    between(1,3,Sender_id),
    effect_RECEIVE_PREPARE(V, N, Config.req_digest_1, Sender_id, Config.sender_signature.Sender_id, 0)
    ),
  test_active_send_commit(0, [[Config.req_digest_1, V, N]]),
  %replica 1,2,3 receive from other non-primary replicas
  forall(
    between(1,3,Replica_id),
    (
      forall(
        (between(1,3,Sender_id), Sender_id =\= Replica_id),
        effect_RECEIVE_PREPARE(V, N, Config.req_digest_1, Sender_id, Config.sender_signature.Sender_id, Replica_id)
        ),
      % each replica should now be ready to send commit
      test_active_send_commit(Replica_id, [[Config.req_digest_1, V, N]])
    )
  ).

% all replicas send commit
test(test_happy_path_07, [setup(build_config(Config))]) :-
  V = 0, N = 1,
  foreach(
    between(0,3,Replica_id),
    effect_SEND_COMMIT(V, N, Config.associated_data, Replica_id)
  ),
  % replica shouldn't be prepared to send commit anymore
  foreach(
    between(0, 3, Replica_id),
    test_active_send_commit(Replica_id, [])
  ).

% All replicas receive commit and get prepared to execute
test(test_happy_path_08, setup(build_config(Config))) :-
  V = 0, N = 1,
  %replica 0
  effect_RECEIVE_COMMIT(V, N, Config.associated_data, 1, (Config.sender_signature).1, 0),
  effect_RECEIVE_COMMIT(V, N, Config.associated_data, 2, (Config.sender_signature).2, 0),
  %effect_RECEIVE_PREPARE(V, N, Config.associated_data, 3, 0), % 3 out of 4 is enough
  active_EXECUTE(0, Active_execute_0),
  assertion( Active_execute_0 == [[Config.req_digest_1, V, N]]),
  %replica 1
  effect_RECEIVE_COMMIT(V, N, Config.associated_data, 0, (Config.sender_signature).0, 1),
  effect_RECEIVE_COMMIT(V, N, Config.associated_data, 2, (Config.sender_signature).2, 1),
  %effect_RECEIVE_PREPARE(V, N, Config.associated_data, 3, 1), % 3 out of 4 is enough
  active_EXECUTE(1, Active_execute_1),
  assertion( Active_execute_1 == [[Config.req_digest_1, V, N]]),
  %replica 2
  effect_RECEIVE_COMMIT(V, N, Config.associated_data, 0, (Config.sender_signature).0, 2),
  effect_RECEIVE_COMMIT(V, N, Config.associated_data, 1, (Config.sender_signature).1, 2),
  %effect_RECEIVE_PREPARE(V, N, Config.associated_data, 3, 2), % 3 out of 4 is enough
  active_EXECUTE(2, Active_execute_2),
  assertion( Active_execute_2 == [[Config.req_digest_1, V, N]]),
  %replica 3
  effect_RECEIVE_COMMIT(V, N, Config.associated_data, 0, (Config.sender_signature).0, 3),
  effect_RECEIVE_COMMIT(V, N, Config.associated_data, 1, (Config.sender_signature).1, 3),
  %effect_RECEIVE_PREPARE(V, N, Config.associated_data, 2, 3), % 3 out of 4 is enough
  active_EXECUTE(3, Active_execute_3),
  assertion( Active_execute_3 == [[Config.req_digest_1, V, N]]).

% all replicas execute
test(test_happy_path_09, setup(build_config(Config))) :-
  N = 1,
  foreach(
    between(0,3, Replica_id),
    (
      effect_EXECUTE(Config.req_digest_1, N, Config.value, Replica_id),
      assertion(last_exec(Replica_id, N)),
      assertion(last_rep(Replica_id, Config.req_timestamp_1)),
      assertion(val(Replica_id, Config.value))
    )
  ).

% collect garbage
test(test_happy_path_10, setup(build_config(Config))) :-
  foreach(
    between(0,3,Replica_id),
    collect_garbage(1,Replica_id)
    ),
  % logs are emptied
  assertion(\+msg_log_pre_prepare(_,_,_,_,_,_,_)),
  assertion(\+msg_log_prepare(_,_,_,_,_,_)),
  assertion(\+msg_log_commit(_,_,_,_,_,_)),
  % current checkpoints
  % checkpoint(_,N,_,_,_),
  % assertion(N == 1),
  % timestamp
  msg_log_request(_,_,T),
  assertion(T == Config.req_timestamp_2).

:- end_tests(test_01_pbft_normal_operation_happy_path).
