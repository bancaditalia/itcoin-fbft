:- consult("test_00_utils.pl").

:- begin_tests(test_aux_11_correct_chi).

setup_test(V) :-
  init_all,
  V = 1,
  foreach(
    between(0,3,ReplicaId),
    set_view(ReplicaId, V)
  ).

build_config(Config) :-
  Config = _{
    r0: 0,
    r1: 1,
    r2: 2,
    r3: 3,
    vcd0: "vc_digest_0",
    vcd1: "vc_digest_1",
    vcd2: "vc_digest_2",
    vcd3: "vc_digest_3",
    sig0: "signature_0",
    sig1: "signature_1",
    sig2: "signature_2",
    sig3: "signature_3",
    req_digest: "req_digest_1",
    associated_data: "block_1"
  }.

test(test_aux_11_correct_chi_00, [setup((setup_test(V),build_config(Config)))]) :-
  init_all,
  TestReplicaId = 2,
  C = "",
  % empty set of prepared and pre-prepared requests
  P = [], Q = [],
  % highest seq number of checkpoint is 0 (just the initial checkpoint)
  H = 0,
  % add view-change messages
  msg_log_add_view_change(TestReplicaId, Config.vcd0,  V, H,  C, P, Q, Config.r0, Config.sig0),
  msg_log_add_view_change(TestReplicaId, Config.vcd1,  V, H,  C, P, Q, Config.r1, Config.sig1),
  msg_log_add_view_change(TestReplicaId, Config.vcd2,  V, H,  C, P, Q, Config.r2, Config.sig2),
  msg_log_add_view_change(TestReplicaId, Config.vcd3,  V, H,  C, P, Q, Config.r3, Config.sig3),
  % Nu and Chi set
  Nu = [[Config.r0, Config.vcd0], [Config.r1, Config.vcd1], [Config.r2, Config.vcd2], [Config.r3, Config.vcd3]],
  % Chi is empty since P is empty
  Chi = [],
  assertion(correct_chi(Chi, Nu,TestReplicaId)).

test(test_aux_11_correct_chi_01, [setup((setup_test(V),build_config(Config)))]) :-
  init_all,
  TestReplicaId = 2,
  C = "",
  N = 1,
  % empty set of prepared and pre-prepared requests
  P = [[N, Config.req_digest, V]], Q = [[N, Config.req_digest, Config.associated_data, V]],
  % highest seq number of checkpoint is 0 (just the initial checkpoint)
  H = 0,
  % add view-change messages
  % it is enough that there is at least one VC message with non-empty P and one with non-empty Q
  msg_log_add_view_change(TestReplicaId, Config.vcd0,  V, H,  C, P, [], Config.r0, Config.sig0),
  msg_log_add_view_change(TestReplicaId, Config.vcd1,  V, H,  C, [], Q, Config.r1, Config.sig1),
  msg_log_add_view_change(TestReplicaId, Config.vcd2,  V, H,  C, [], [], Config.r2, Config.sig2),
  msg_log_add_view_change(TestReplicaId, Config.vcd3,  V, H,  C, [], [], Config.r3, Config.sig3),
  % Nu and Chi set
  Nu = [[Config.r0, Config.vcd0], [Config.r1, Config.vcd1], [Config.r2, Config.vcd2], [Config.r3, Config.vcd3]],
  Chi = [[N, Config.req_digest, Config.associated_data]],
  assertion(correct_chi(Chi, Nu,TestReplicaId)).

:- end_tests(test_aux_11_correct_chi).
