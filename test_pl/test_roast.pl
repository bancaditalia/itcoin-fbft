:- consult(test_00_utils).

:- use_module(library(lists)).

% product of elements of a list
prod(X,Y, Product) :- 
  Product is X * Y.

product_list(List, Prod) :- foldl(prod, List, 1, Prod).



% roast_crypto_state(Replica_id, State)
:- dynamic roast_crypto_state/2.

:-
  assert(roast_crypto_state(0, 0)),
  assert(roast_crypto_state(1, 0)),
  assert(roast_crypto_state(2, 0)),
  assert(roast_crypto_state(3, 0)).

% config parameters
group_order(97).
shift(100).
% for simplicity, the two hash values will be fixed
hash_non(15).
hash_sig(27).
% secret keys sk (Replica_id, Sk_i) and pub_key for 3-of-4 signatures
secret_key(0, 54).
secret_key(1, 94).
secret_key(2, 14).
secret_key(3, -186).
secret_key(-106).
public_key(88).

% group of integers modulo P will be used for the test
% there is no State_i since it will be equal to the pre-signature share, because we are doing operations in Z_p
roast_crypto_pre_round_test(Replica_id, Pre_signature_share) :-
  % generate integers D, E in range [0, P-1] with P prime and fixed seeds
  group_order(P),
  shift(S),
  debug(roast_crypto_pre_round, "test_pl: Replica_id = ~w", [Replica_id]),
  % using P = 9 and seeds equal to Replica_id and Replica_id + 100 respectively, gets the following values for D and E:
  % 0 -> (24, 32),
  % 1 -> (59, 92),
  % 2 -> (43, 22),
  % 3 -> (86, 76)
  set_random(seed(Replica_id)),
  D_i is random(P),
  NewSeed is Replica_id + S,
  set_random(seed(NewSeed)),
  E_i is random(P),
  Pre_signature_share = (D_i,E_i).

:- debug(roast_crypto_pre_round).

% Aggregates the pre-signature shares
% Pre_signatures_shares is a list of pairs (D_i,E_i) with i in a set of signers T
roast_crypto_pre_sig_aggregate_test(Replica_id, Pre_signature_shares, Pre_signature) :-
  debug(roast_crypto_pre_sig_aggregate, "test_pl: Replica_id = ~w, Pre_signature_shares = ~w", [Replica_id, Pre_signature_shares]),
  group_order(P),
  findall(D_i, member((D_i,_), Pre_signature_shares), Ds),
  findall(E_i, member((_,E_i), Pre_signature_shares), Es),
  sum_list(Ds, D_sum),
  sum_list(Es, E_sum),
  D is D_sum mod P,
  E is E_sum mod P,
  Pre_signature = (D,E).

:- debug(roast_crypto_pre_sig_aggregate).

% calculates the signature share
% Signers is a list of integers (each integer is a replica index)
% State_i is a pair (D_i, E_i) and is the same as Pre_signature_share
% Pre_signature is a pair (D,E) and is not used since the hash is fixed
roast_crypto_sign_round_test(Replica_id, Signers, State_i, Pre_signature, Signature_share) :-
  debug(roast_crypto_sign_round, "test_pl: Replica_id = ~w, Signers = ~w, State_i = ~w, Pre_signature = ~w", [Replica_id, Signers, State_i, Pre_signature]),
  hash_non(B),
  hash_sig(C),
  State_i = (D_i, E_i),
  lagrange(Signers, Replica_id, Lambda_i),
  secret_key(Replica_id, Sk_i),
  Signature_share is D_i + B * E_i + C * Lambda_i * Sk_i.

:- debug(roast_crypto_sign_round).

% calculates lagrange coefficient
% Signers is a list of integers (each integer is a replica index)
lagrange(Signers, Replica_id, Lambda_i) :-
  I is Replica_id + 1,
  maplist(plus(1), Signers, T),
  convlist(lagrange_partial(I), T, Lambda_list),
  product_list(Lambda_list, Lambda_i).
% internal function needed to calculate lagrange coefficient
lagrange_partial(I, J, Out) :-
  I =\= J,
  Out is J / (J - I).

% Share validation, must not fail
% Signers is a list of integers (each integer is a replica index),
% Pre_signature is a pair (D,E) but is not used since the hash is fixed
% Pre_signature_share is a pair (D_i, E_i),
% Signature_share is an integer
roast_crypto_share_val_test(Replica_id, Signers, Pre_signature, Pre_signature_share, Signature_share) :-
  debug(roast_crypto_share_val, "test_pl: Replica_id = ~w, Signers = ~w, Pre_signature = ~w ,Pre_signature_share = ~w, Signature_share = ~w", [Replica_id, Signers, Pre_signature, Pre_signature_share, Signature_share]),
  group_order(P),
  hash_non(B),
  hash_sig(C),
  lagrange(Signers, Replica_id, Lambda_i),
  Pre_signature_share = (D_i, E_i),
  Sigma_i = floor(Signature_share),
  secret_key(Replica_id, Sk_i),
  Left_side is Sigma_i mod P,
  Right_side is floor(D_i + B * E_i + C * Lambda_i * Sk_i) mod P,
  Left_side =:= Right_side.

:- debug(roast_crypto_share_val).

% Pre_signature is a pair (D,E)
% Signature_shares is a list of integers
roast_crypto_sign_aggregate_test(Replica_id, Pre_signature, Signature_shares, Signature) :-
  debug(roast_crypto_sign_aggregate, "test_pl: Replica_id = ~w, Pre_signature = ~w, Signature_shares = ~w", [Replica_id, Pre_signature, Signature_shares]),
  hash_non(B),
  group_order(P),
  Pre_signature = (D, E),
  R is (D + B * E) mod P,
  sum_list(Signature_shares, S),
  Signature = (R,S).

:- debug(roast_crypto_sign_aggregate).


:-
  init_all,
  assertz(
    (
    roast_crypto_pre_sig_aggregate(Replica_id, Pre_signature_shares, Pre_signature) :-
      roast_crypto_pre_sig_aggregate_test(Replica_id, Pre_signature_shares, Pre_signature)
    )
  ),
  set_signature_algorithm("ROAST"),
  set_synthetic_time_all(61).

:-
  Req_digest = "req0",
  Req_timestamp = 60,
  receive_req_all(Req_digest, Req_timestamp).

:-
  Req_digest = "req0",
  Proposed_block = "block0",
  V = 0,
  N = 1,
  pre_prepare_all(Req_digest, Proposed_block, V, N).

:-
  Req_digest = "req0",
  V = 0,
  N = 1,
  prepare_all(Req_digest, V, N).

:-
  V = 0,
  N = 1,
  apply_SEND_COMMIT(V, N, "PRESIGNATURE_FROM_0", 0),
  apply_RECEIVE_COMMIT(V, N, "PRESIGNATURE_FROM_1", 1, "SIG_FALSE", 0),
  apply_RECEIVE_COMMIT(V, N, "PRESIGNATURE_FROM_2", 2, "SIG_FALSE", 0),
  true.

:- pre_ROAST_INIT(0, Req_digest, V, N),
  apply_ROAST_INIT(0, V, N, Req_digest).

:-
  Replica_id = 0,
  msg_out_roast_pre_signature(Replica_id, Signers, Pre_signature),
  foreach(
    member(S, Signers),
    (
      Signature_share = "Signature_share",
      Next_pre_signature_share = "Next_pre_signature_share",
      apply_RECEIVE_PRE_SIGNATURE(S, Signers, Pre_signature, Signature_share, Next_pre_signature_share)
    )
  ).

:- roast_print_all.

:- Sig_shares = [],
  Sig_share = "hello",
  append(Sig_shares, [Sig_share], New_sig_share),
  writeln(New_sig_share),
  Sig_share_2 = "hello",
  append(New_sig_share, [Sig_share_2], New_sig_share_2),
  proper_length(New_sig_share_2, Num_sig_shares),
  writeln(Num_sig_shares),
  writeln(New_sig_share_2).

:- true, true, true ->
  writeln("The whole is true"),
  false,
  writeln("The whole is again true")
  ;
  writeln("The whole is false").