%% Implements a PBFT I/O Automata from [1] using Constraint Logic Programming over Finite Domains - CLP(FD)

% For each I/O Automata action, the following prolog predicates are defined:
% - pre_ACTION: backtracks the values for which the action should be executed
%   The pre_ACTION is not defined for actions of type RECEIVE_MESSAGE because these are always active
% - apply_ACTION: updates the automata state
% - effect_ACTION: is the transaction version of apply_ACTION, in case it fails, temp changes are rolled back.
%   effect is separated from apply, because the non transactional version is easier to debug.

% Some simplifications and optimization have been applied with respect to [1], in particular:
% - The REQUEST handle a single operation, REPLY messages are omitted
% - Checkpoint messages are not present
% - The VIEW_CHANGE_ACK message is not present, since public key encription is used
% - The Chi in the NEW_VIEW only contains entries for prepared messages

% References
% [1] https://pmg.csail.mit.edu/papers/bft-tocs.pdf

/*
 * Utilities and imports
 */

% CLP(FD) library, see https://github.com/triska/clpfd for examples
:- use_module(library(clpfd)).

% list library, see https://www.swi-prolog.org/pldoc/man?section=lists
:- use_module(library(lists)).

% sha library
:- use_module(library(sha)).

% json library
:- use_module(library(http/json)).

% persistence library
:- use_module(library(persistency)).

% subsets([1,2,3], X). Generates all the subsets of given set.
% taken from: https://stackoverflow.com/questions/4912869/subsets-in-prolog
% WARN: should be used with once() when not backtracking
%   OR subset from library(lists) should be used instead in that case
subsets([], []).
subsets([E|Tail], [E|NTail]):-
  subsets(Tail, NTail).
subsets([_|Tail], NTail):-
  subsets(Tail, NTail).

% This helper function is used to filter out correct Nu that are too small for
% the evaluation of the correct-chi predicate.
filter_subsets_by_size(Max_nu, Sub, MinSize) :-
  subsets(Max_nu, Sub),
  proper_length(Sub, SubSize),
  SubSize>=MinSize.

% Assert a fact only once, avoiding duplicates, taken from: https://stackoverflow.com/questions/10437395/prolog-how-to-assert-make-a-database-only-once
assertz_once(Fact) :- ( Fact, !; assertz(Fact) ).

% Prints all the dynamic predicates, it is useful for debugging
print_all_dynamics :-
  nb_getval(request_buffer_len, L),
  nb_getval(cluster_size, Cluster_size),
  nb_getval(target_block_time, Target_block_time),
  nb_getval(genesis_block_timestamp, Genesis_block_timestamp),
  nb_getval(signature_algorithm, Sig_algo),
  writef(":- constants.\n \nRequest_buffer_len=%w \nCluster_size=%w \nGenesis_block_timestamp=%w \nTarget_block_time=%w \nSig_algo=%w \n\n", [L, Cluster_size, Genesis_block_timestamp, Target_block_time, Sig_algo]),
  listing(synthetic_time/2),
  listing(view/2),
  listing(active_view/1),
  listing(timeout/2),
  listing(last_exec/2),
  listing(last_rep/2),
  listing(val/2),
  listing(seqno/2),
  listing(checkpoint/4),
  listing(msg_log_request/3),
  listing(msg_log_pre_prepare/7),
  listing(msg_log_prepare/6),
  listing(msg_log_commit/6),
  listing(msg_log_commit_view_recovery/4),
  listing(msg_log_view_change/9),
  listing(msg_log_new_view/5),
  listing(msg_out_pre_prepare/5),
  listing(msg_out_prepare/4),
  listing(msg_out_commit/4),
  listing(msg_out_view_change/6),
  listing(msg_out_new_view/4),
  listing(view_change_Pi/4),
  listing(view_change_Qi/5),
  (
    Sig_algo == "ROAST"
    -> roast_print_all
    ;
    true
  ).

/*
 * Digest predicates
 */

% computes the hash function of a string
digest_from_string(String, Digest) :-
  sha_hash(String, Hash, []),
  hash_atom(Hash, Digest_atom),
  atom_string(Digest_atom, Digest).

% computes digest for each message type
digest_pre_prepare(V, N, Req_digest, Associated_ppp_data, Digest) :-
  Message = pre_prepare{view: V, n: N, request_digest: Req_digest, data: Associated_ppp_data},
  atom_json_dict(Json_string, Message, [as(string), tag(name)]),
  digest_from_string(Json_string, Digest).

digest_prepare(V, N, Req_digest, Sender_id, Digest) :-
  Message = prepare{view: V, n: N, request_digest: Req_digest, sender_id: Sender_id},
  atom_json_dict(Json_string, Message, [as(string), tag(name)]),
  digest_from_string(Json_string, Digest).

digest_commit(V, N, Associated_commit_data, Sender_id, Digest) :-
  debug(digest, "digest_commit: Associated_data = ~w", [Associated_commit_data]),
  Message = commit{v: V, n: N, data: Associated_commit_data, sender_id: Sender_id},
  atom_json_dict(Json_string, Message, [as(string), tag(name)]),
  digest_from_string(Json_string, Digest).

digest_view_change(V, Hi, C, Pi, Qi, Sender_id, Digest) :-
  Message = view_change{view: V, hi: Hi, c: C, pi: Pi, qi: Qi, sender_id: Sender_id},
  atom_json_dict(Json_string, Message, [as(string), tag(name)]),
  digest_from_string(Json_string, Digest).

digest_new_view(V, Nu, Chi, Digest) :-
  Message = new_view{view: V, nu: Nu, chi: Chi},
  atom_json_dict(Json_string, Message, [as(string), tag(name)]),
  digest_from_string(Json_string, Digest).

/*
 * Support for Synthetic Time
 * if synthetic_time(Replica_id, St) is not false, then its value is returned and can be used as synthetic time.
 * if synthetic_time(Replica_id, St) is false, get_synthetic_time returns the system time.
 * NB: In the future it could return the bitcoin "Network-adjusted time",
 * i.e. the median of the timestamps returned by all nodes connected to you.
 */

% synthetic_time(Replica_id, Synthetic_time).
% Synthetic_time: number, containing the current time for specified replica
:- dynamic synthetic_time/2.

get_synthetic_time(Replica_id, T) :-
  synthetic_time(Replica_id, St),
  (
    St==false ->
    (get_time(T));
    (T is St)
  ).

set_synthetic_time(Replica_id, T) :-
  retractall(synthetic_time(Replica_id, _)),
  assertz_once(synthetic_time(Replica_id, T)).

/*
 * STATE: 1 - Val
 */

% val(Replica_id, State).
% State: string, containing the digest of the latest state value, e.g. the latest block hash
:- dynamic val/2.

get_val(Replica_id, Out_val) :-
  val(Replica_id, Out_val).

set_val(Replica_id, State_val) :-
  retractall(val(Replica_id,_)),
  assertz_once(val(Replica_id, State_val)).

/*
 * STATE: 2, 3 - Last rep and Last rep timestamp
 */

% last_rep(Replica_id, Last_rep_t).
% Last_rep_t: number, containing the timestamp of the latest executed or checkpointed request
:- dynamic last_rep/2.

get_last_rep(Replica_id, Last_rep_t) :-
  last_rep(Replica_id, Last_rep_t).

set_last_rep(Replica_id, Last_rep_t) :-
  retractall(last_rep(Replica_id, _)),
  assertz(last_rep(Replica_id, Last_rep_t)).

/*
 * STATE: 4 - Checkpoints
 */

% checkpoint(Replica_id, N, State_val, Last_rep_t)
% N: number, containing the sequence number of the latest checkpointed request, e.g. the latest block height
% State_val: string, containing the digest of the latest checkpointed request, e.g. the latest block hash
% Last_rep_t: number, containing the timestamp of the latest checkpointed request
:- dynamic checkpoint/4.

add_checkpoint(Replica_id, N, State_val, Last_rep_t) :-
  assertz_once(checkpoint(Replica_id, N, State_val, Last_rep_t)).

/*
 * STATE: 5 and 6 - Message Log and Out buffer
 */

% msg_log_request(Replica_id, Req_digest, Req_timestamp)
% Req_digest: string, containing the digest of the latest checkpointed request, e.g. the latest block hash
% Req_timestamp: number, containing the timestamp of the request
:- dynamic msg_log_request/3.

msg_log_add_request(Replica_id, Req_digest, Req_timestamp) :-
  assertz_once(msg_log_request(Replica_id, Req_digest, Req_timestamp)).

% Returns the timestamp of the latest known request. This is used in the Replica code to automatically generate future requests
get_latest_request_time(Replica_id, Max_t) :-
  findall(T,
    % Either a request is in the message log
    msg_log_request(Replica_id, _, T);
    % Or the request has been checkpointed and request deleted
    get_last_rep(Replica_id,T),
  Out_t),
  max_list(Out_t, Max_t).

% msg_log_pre_prepare(Replica_id, V, N, Req_digest, Associated_pre_prepare_data, Sender_id, Sender_sig)
% Associated_pre_prepare_data: string, contains a value sent by the primary in the PRE_PREPARE message, e.g the proposed block.
:- dynamic msg_log_pre_prepare/7.
% msg_out_pre_prepare(Replica_id, V, N, Req_digest, Associated_pre_prepare_data), with Sender_id = Replica_id
:- dynamic msg_out_pre_prepare/5.

msg_log_add_pre_prepare(Replica_id, V, N, Req_digest, Associated_pre_prepare_data, Sender_id, Sender_sig) :-
  assertz_once(msg_log_pre_prepare(Replica_id, V, N, Req_digest, Associated_pre_prepare_data, Sender_id, Sender_sig)).

msg_out_add_pre_prepare(Replica_id, V, N, Req_digest, Associated_pre_prepare_data) :-
  assertz_once(msg_out_pre_prepare(Replica_id, V, N, Req_digest, Associated_pre_prepare_data)).

% msg_log_prepare(Replica_id, V, N, Req_digest, Sender_id, Sender_sig)
:- dynamic msg_log_prepare/6.
% msg_log_prepare(Replica_id, V, N, Req_digest), with Sender_id = Replica_id
:- dynamic msg_out_prepare/4.

msg_log_add_prepare(Replica_id, V, N, Req_digest, Sender_id, Sender_sig) :-
  assertz_once(msg_log_prepare(Replica_id, V, N, Req_digest, Sender_id, Sender_sig)).

msg_out_add_prepare(Replica_id, V, N, Req_digest) :-
  assertz_once(msg_out_prepare(Replica_id, V, N, Req_digest)).

% msg_log_commit(Replica_id, V, N, Associated_commit_data, Sender_id, Sender_sig)
:- dynamic msg_log_commit/6.
% msg_out_commit(Replica_id, V, N, Associated_commit_data), with Sender_id = Replica_id
:- dynamic msg_out_commit/4.

msg_log_add_commit(Replica_id, V, N, Associated_commit_data, Sender_id, Sender_sig) :-
  assertz_once(msg_log_commit(Replica_id, V, N, Associated_commit_data, Sender_id, Sender_sig)).

msg_out_add_commit(Replica_id, V, N, Associated_commit_data) :-
  assertz_once(msg_out_commit(Replica_id, V, N, Associated_commit_data)).

% msg_log_commit_view_recovery(Replica_id, V, N, Sender_id)
:- dynamic msg_log_commit_view_recovery/4.

msg_log_add_commit_view_recovery(Replica_id, V, N, Sender_id) :-
  assertz_once(msg_log_commit_view_recovery(Replica_id, V, N, Sender_id)).

% msg_log_view_change(Replica_id, Vc_digest, V, Hi, C, Pi, Qi, Sender_id, Sender_sig)
% Vc_digest: string, the digest of the view change message, stored mainly for debug purpose, i.e. it's easier to link content of
% a NEW_VIEW Nu with respective view change messages
% V: number, the view number
% Hi: the low watermark (height) of the latest known checkpoint
% C: string, the latest known checkpoint digest, since all checkpoints are stable by definition
% P: list of lists, a set of prepare messages for each request that prepared at Replica_id with a sequence number higher than Hi. See view_change_Pi
% Q: list of lists, a set of pre_prepare messages for each request that prepared at Replica_id with sequence number higher than Hi. See view_change_Qi
:- dynamic msg_log_view_change/9.
:- dynamic msg_out_view_change/6.

msg_log_add_view_change(Replica_id, Vc_digest, V, Hi, C, P, Q, Sender_id, Sender_sig) :-
  assertz_once(msg_log_view_change(Replica_id, Vc_digest, V, Hi, C, P, Q, Sender_id, Sender_sig)).

msg_out_add_view_change(Replica_id, V, Hi, C, P, Q) :-
  assertz_once(msg_out_view_change(Replica_id, V, Hi, C, P, Q)).

% msg_log_new_view(Replica_id, V, Nu, Chi, Sender_sig)
% Nu: list of lists, containing the viewchange message digests received by the primary plus the viewchange
%   message for 1 the primary sent (or would have sent)
% Chi: list of lists, a set of pre-prepare messages in the form [N, Req_digest, Associated_pre_prepare_data] sent by the primary
% The Sender_id is omitted, since it is always the primary
:- dynamic msg_log_new_view/5.
:- dynamic msg_out_new_view/4.

msg_log_add_new_view(Replica_id, V, Nu, Chi, Sender_sig) :-
  assertz_once( msg_log_new_view(Replica_id, V, Nu, Chi, Sender_sig)).

msg_out_add_new_view(Replica_id, V, Nu, Chi) :-
  assertz_once( msg_out_new_view(Replica_id, V, Nu, Chi)).

% Cleaning up utilities for the message log and message output buffer

msg_log_clear_where_view_less_than(Replica_id, V) :-
  foreach( msg_log_pre_prepare(Replica_id, V1, T3, T4, T5, T6, T7),
    ( V1<V -> retractall(msg_log_pre_prepare(Replica_id, V1, T3, T4, T5, T6, T7)); true )
  ),
  foreach( msg_log_prepare(Replica_id, V1, T3, T4, T5, T6),
    ( V1<V -> retractall(msg_log_prepare(Replica_id, V1, T3, T4, T5, T6)); true)
  ),
  foreach( msg_log_commit(Replica_id, V1, T3, T4, T5, T6),
    ( V1<V -> retractall(msg_log_commit(Replica_id, V1, T3, T4, T5, T6)); true)
  ),
  foreach( msg_log_commit_view_recovery(Replica_id, V1, T3, T4),
    ( V1<V -> retractall(msg_log_commit(Replica_id, V1, T3, T4)); true)
  ),
  foreach( msg_log_view_change(Replica_id, T2, V1, T4, T5, T6, T7, T8, T9),
    ( V1<V -> retractall(msg_log_view_change(Replica_id, T2, V1, T4, T5, T6, T7, T8, T9)); true)
  ),
  foreach( msg_log_new_view(Replica_id, V1, T3, T4, T5),
    ( V1<V -> retractall(msg_log_new_view(Replica_id, V1, T3, T4, T5)); true)
  ).

msg_log_clear_all(Replica_id) :-
  retractall( msg_log_request(Replica_id,_,_) ),
  retractall( msg_log_pre_prepare(Replica_id, _, _, _, _, _, _) ),
  retractall( msg_log_prepare(Replica_id, _, _, _, _, _) ),
  retractall( msg_log_commit(Replica_id, _, _, _, _, _) ),
  retractall( msg_log_commit_view_recovery(Replica_id, _, _, _)),
  retractall( msg_log_view_change(Replica_id, _, _, _, _, _, _, _, _) ),
  retractall( msg_log_new_view(Replica_id, _, _, _, _) ).

msg_out_clear_all(Replica_id) :-
  retractall( msg_out_pre_prepare(Replica_id,_,_,_,_) ),
  retractall( msg_out_prepare(Replica_id,_,_,_) ),
  retractall( msg_out_commit(Replica_id,_,_,_) ),
  retractall( msg_out_view_change(Replica_id, _, _, _, _, _) ),
  retractall( msg_out_new_view(Replica_id,_,_,_) ),
  (
    nb_getval(signature_algorithm, "ROAST")
    ->
    (
      retractall( msg_out_roast_pre_signature(Replica_id,_,_) ),
      retractall( msg_out_roast_signature_share(Replica_id,_,_) )
    )
    ;
    true
  ).
/*
 * STATE: 7 - Pi, set of prepared messages in the form (N, Req_digest, V)
 */

% view_change_Pi(Replica_id, N, Req_digest, V)
:- dynamic view_change_Pi/4.

update_view_change_Pi(Replica_id) :-
  debug(update_view_change_Pi, "update_view_change_Pi: Retractall for Replica_id = ~w", [Replica_id]),
  retractall(view_change_Pi(Replica_id, _, _, _)),
  foreach(
    (
      V2 #> V1,
      prepared(M, V1, N, Replica_id),
      \+ prepared(_, V2, N, Replica_id)
    ),
    (
      debug(update_view_change_Pi, "update_view_change_Pi: Replica_id = ~w, N = ~w, M = ~w, V1 = ~w", [Replica_id, N, M, V1]),
      assertz_once(view_change_Pi(Replica_id, N, M, V1))
    )
  ).

/*
 * STATE: 8 - Qi, set of pre-prepared messages in the form (N, Req_digest, Associated_pre_prepare_data, V)
 */

% view_change_Qi(Replica_id, N, Req_digest, Associated_pre_prepare_data, V)
:- dynamic view_change_Qi/5.

update_view_change_Qi(Replica_id) :-
  retractall(view_change_Qi(Replica_id, _, _, _, _)),
  foreach(
    (
      V2 #> V1,
      pre_prepared(M, V1, N, Replica_id),
      \+ pre_prepared(M, V2, N, Replica_id),
      primary(V1, P),
      msg_log_pre_prepare(Replica_id, V1, N, M, Associated_pre_prepare_data, P, _)
    ),
    assertz_once(view_change_Qi(Replica_id, N, M, Associated_pre_prepare_data, V1))
  ).

/*
 * STATE: 9 - View
 */

% The view change timeout
:- dynamic timeout/2.
% The view number is defined as a persistent attribute
:- persistent(view(replica_id:integer, view:integer)).

set_timeout(Replica_id, V) :-
  retractall(timeout(Replica_id, _)),
  nb_getval(target_block_time, Target_block_time),
  Base_timeout is Target_block_time/2,
  % Exponentially increase timeout on view change
  % This allows replica to sync on the view number
  Timeout is 2**V*Base_timeout,
  assertz_once(timeout(Replica_id, Timeout)).

set_view(Replica_id, V) :-
  retractall_view(Replica_id, _),
  assert_view(Replica_id, V),
  set_timeout(Replica_id, V).

/*
 * STATE: 10 - Active view
 */

% Active view is a boolean predicate, that is true when the Replica is in normal operation, otherwise false
:- dynamic active_view/1.

set_active_view(Replica_id, Av) :-
  retractall(active_view(Replica_id)),
  (
    Av -> assertz_once(active_view(Replica_id));
    true
  ).

/*
 * STATE: 11 - last exec
 */

% last_exec(Replica_id, Last_exec).
% Last_exec: number, containing the sequence number of the latest executed or checkpointed request
:- dynamic last_exec/2.

set_last_exec(Replica_id, Last_exec):-
  retractall(last_exec(Replica_id,_)),
  assertz_once(last_exec(Replica_id,Last_exec)).

/*
 * STATE: 12 - Sequence number
 */

:- dynamic seqno/2.

set_seqno(Replica_id, N) :-
  retractall(seqno(Replica_id, _)),
  assertz_once(seqno(Replica_id, N)).

/*
 * STATE: 13 - Hi
 */

get_h(Replica_id, Out_hi) :-
  findall(N,checkpoint(Replica_id,N,_,_),R), min_list(R,Out_hi).

/*
 * Auxiliary functions
 */

% 0 (this numbering represents the order in which auxiliary functions appear in [1])
% Byzantine quorum definition
quorum(Out_quorum) :-
  nb_getval(cluster_size, Csize),
  F is floor((Csize-1)/3),
  Out_quorum is 2*F+1.

% The prepare quorum is one less than the byzantine quorum, since the pre_prepare message sent by the primary
% is counted separately
quorum_prepare(Out_quorum) :-
  quorum(Byzantine_quorum),
  Out_quorum is Byzantine_quorum-1.

% 1
% Message tag is not implemented, we already separate messages in the log by tag

% 2
primary(V, Out_primary) :-
  nb_getval(cluster_size, Csize),
  Out_primary #= V mod Csize.

% 3
in_w(N, Replica_id) :-
  nb_getval(request_buffer_len, L),
  get_h(Replica_id, Hi),
  N - Hi #=< L,
  N - Hi #> 0.

% 4
in_wv(V, N, Replica_id) :-
  in_w(N, Replica_id),
  view(Replica_id, View_i),
  V #= View_i.

% 5
pre_prepared(Req_digest, V, N, Replica_id) :-
  view_change_Qi(Replica_id, N, Req_digest, _, V);
  primary(V, Primary_id),
  msg_log_request(Replica_id, Req_digest, _),
  msg_log_pre_prepare(Replica_id, V, N, Req_digest, _, Primary_id, _).

% 6
prepared(Req_digest, V, N, Replica_id) :-
  view_change_Pi(Replica_id, N, Req_digest, V);
  quorum_prepare(Quorum),
  pre_prepared(Req_digest, V, N, Replica_id),
  findall(K, msg_log_prepare(Replica_id, V, N, Req_digest, K, _), Prepare_msgs),
  proper_length(Prepare_msgs, Num_prepare_msgs), Num_prepare_msgs>=Quorum.

% 7
committed(Req_digest, V, N, Replica_id) :-
  prepared(Req_digest, V, N, Replica_id),
  quorum(Quorum),
  findall(K, msg_log_commit(Replica_id, V, N, _, K, _), Commit_msgs),
  proper_length(Commit_msgs, Num_commit_msgs), Num_commit_msgs>=Quorum.

% 8
correct_view_change(M_v, M_h, M_P, M_Q) :-
  nb_getval(request_buffer_len, L),
  foreach( member([N, _, V1], M_P), ( V1 < M_v,  N > M_h, N =< M_h + L ) ),
  foreach( member([N, _, _, V1], M_Q), ( V1 < M_v,  N > M_h, N =< M_h + L ) ).

% 9
% view(m)
% Implemented in msg_log_clear_where_view_less_than

% 10 - Correct Nu
correct_nu_element(J, D, Replica_id) :-
  view(Replica_id, View_i),
  primary(View_i, Primary_i),
  msg_log_view_change(Replica_id, D, V, _, _, _, _, J, _),
  once(
  (
    V #= View_i,
    Replica_id #\= Primary_i
  );
  true
  ).

% This function returns the largest Nu set present in the message_log
correct_nu_once(Nu, Replica_id) :-
  quorum(Quorum),
  findall([J,D], correct_nu_element(J,D,Replica_id), UnsortedNu),
  proper_length(UnsortedNu, NuSize),
  NuSize>=Quorum,
  sort(UnsortedNu, Nu).

% This function returns all the possible Nu sets that can be present in the message_log
correct_nu_all(Nu, Replica_id) :-
  quorum(Quorum),
  findall([J,D], correct_nu_element(J,D,Replica_id), Max_nu),
  filter_subsets_by_size(Max_nu, UnsortedNu, Quorum),
  sort(UnsortedNu, Nu).

nu_vset_i(Nu, Replica_id, V, Hi, C, Pi, Qi, Sender_id) :-
  findall(D, member([_,D], Nu), Vc_digests),
  member(Vc_digest, Vc_digests),
  msg_log_view_change(Replica_id, Vc_digest, V, Hi, C, Pi, Qi, Sender_id, _).

% 11 - Correct Chi

correct_chi(Chi, Nu, Replica_id) :-
  quorum(Quorum),
  debug(correct_chi_3, "Nu = ~w", [Nu]),
  findall(
    [Replica_id, V, Hi, C, P, Q, Sender_id],
    nu_vset_i(Nu, Replica_id, V, Hi, C, P, Q, Sender_id),
    ViewChangeMsgs
  ),
  proper_length(ViewChangeMsgs, ViewChangeMsgsLen), ViewChangeMsgsLen>=Quorum,
  debug(correct_chi_3, "ViewChangeMsgs = ~w", [ViewChangeMsgs]),
  % PART 2
  % Min_s is the sequence number of the latest stable checkpoint in Nu
  findall(Hi, member([_, _, Hi, _, P, _, _], ViewChangeMsgs), AllHis),
  debug(correct_chi_3, "All sequence numbers of the latest stable checkpoint in Nu = ~w", [AllHis]),
  min_list(AllHis,Min_s),
  debug(correct_chi_3, "Min_s = ~w", [Min_s]),
  % Max_s is the highest sequence number in a prepare message in Nu.
  findall(P, member([_, _, _, _, P, _, _], ViewChangeMsgs), AllPsAux),
  debug(correct_chi_3, "AllPsAux (before flatten) = ~w", [AllPsAux]),
  findall(X, (member(P,AllPsAux),member(X,P)), AllPs),
  % flatten(AllPsAux, AllPs),
  debug(correct_chi_3, "All Prepare messages in Nu = ~w", [AllPs]),
  findall(N, member([N, _, _], AllPs), AllPreparedNs),
  debug(correct_chi_3, "All sequence numbers in prepared messages in Nu = ~w",[AllPreparedNs]),
  once( max_list(AllPreparedNs,Max_s); Max_s is Min_s),
  debug(correct_chi_3, "Max_s = ~w", [Max_s]),
  % Compute set of Pre_prepare messages, append them in chi
  % The primary creates a new pre-prepare message
  % for each sequence number between Min_s and Max_s
  BetweenStart is Min_s + 1,
  findall(
    [N, Req_digest, Associated_pre_prepare_data],
    (
    between(BetweenStart, Max_s, N),
    once(
      (
        % CASE1: there is at least one set in the component of some view-change
        % message in with sequence number N
        member([_, _, _, _, P, _, _], ViewChangeMsgs),
        member([_, _, _, _, _, Q, _], ViewChangeMsgs),
        member([N, Req_digest, V], P),
        member([N, Req_digest, Associated_pre_prepare_data, V], Q),
        % In the first case, the primary creates a new message PRE-PREPARE,
        % where Req_digest is the request digest in the pre-prepare message for
        % sequence number N with the highest view number in Nu
        forall(
          member([N, Req_digest_prime, V_prime], P),
          (
            V_prime < V;
            (V_prime =:= V, Req_digest_prime = Req_digest)
          )
        )
        % The OR below allows for once/1 advance to next line when CASE1 is false
        ;
        % CASE2 (this should never happen): there is no such set
        Req_digest = "null"
      )
    )
    ),
    Chi
  ),
  debug(correct_chi_3, "Returning Chi = ~w", [Chi]),
  true.

% 12 - correct new view
correct_new_view(Chi, Nu, Replica_id) :-
  view(Replica_id, View_i),
  msg_log_new_view(Replica_id, View_i, Nu, Chi, _),
  correct_nu_all(Nu, Replica_id),
  correct_chi(Chi, Nu, Replica_id).

/*
 * Actions
 * They are numbered according to how they appear on page 455 of [1]
 */

% 0. INIT REPLICA STATE

% The _notx version of each effect method is useful for debugging purposes.
init_notx(Replica_id, Cluster_size,
    Initial_block_height, Initial_block_hash, Initial_block_timestamp,
    Genesis_block_timestamp, Target_block_time,
    Db_filename, Db_reset) :-
  % Global config values that are the same for all replicas
  nb_setval(request_buffer_len, 1),
  nb_setval(cluster_size, Cluster_size),
  nb_setval(target_block_time, Target_block_time),
  nb_setval(genesis_block_timestamp, Genesis_block_timestamp),
  % Signature algorithm
  nb_setval(signature_algorithm, "NAIVE"),
  % Synthetic time
  set_synthetic_time(Replica_id, false),
  % State val (initial value)
  set_val(Replica_id, Initial_block_hash),
  % TODO: last_rep is still a bit WiP
  retractall( last_rep(Replica_id,_,_,_)),
  set_last_rep(Replica_id, Initial_block_timestamp),
  % Checkpoints
  retractall( checkpoint(Replica_id,_,_,_) ),
  assertz_once( checkpoint(Replica_id, Initial_block_height, Initial_block_hash, Initial_block_timestamp) ),
  % Message Log
  msg_log_clear_all(Replica_id),
  % Message Out buffer
  msg_out_clear_all(Replica_id),
  % Pi and Qi
  retractall( view_change_Pi(Replica_id, _, _, _) ),
  retractall( view_change_Qi(Replica_id, _, _, _, _) ),
  % Other scalar state variables
  set_active_view(Replica_id, true),
  % Handles the persistence
  (exists_file(Db_filename) ->
    (
      debug(init_notx, "init_notx: Database file exists", []),
      (Db_reset ->
        (
          debug(init_notx, "init_notx: Database reset required", []),
          delete_file(Db_filename),
          db_attach(Db_filename, [sync(flush)]),
          set_view(Replica_id, 0)
        );
        (
          db_attach(Db_filename, [sync(flush)]),
          view(Replica_id, V),
          set_timeout(Replica_id, V),
          debug(init_notx, "init_notx: Database do not reset, starting at view=~w", [V])
        )
      )
    );
    % File does not exist
    (
      debug(init_notx, "init_notx: Database file does not exist", []),
      db_attach(Db_filename, [sync(flush)]),
      set_view(Replica_id, 0)
    )
  ),
  set_seqno(Replica_id, Initial_block_height),
  set_last_exec(Replica_id, Initial_block_height).

init(Replica_id, Cluster_size,
  Initial_block_height, Initial_block_hash, Initial_block_timestamp,
  Genesis_block_timestamp, Target_block_time, Db_filename, Db_reset) :-
  transaction( init_notx(Replica_id, Cluster_size,
    Initial_block_height, Initial_block_hash, Initial_block_timestamp,
    Genesis_block_timestamp, Target_block_time, Db_filename, Db_reset) ).

% 1. RECEIVE-REQUEST

apply_RECEIVE_REQUEST(Req_digest, Req_timestamp, Replica_id) :-
  get_last_rep(Replica_id, Last_rep_t),
  (
    % TODO: add effect on output log
    Req_timestamp =< Last_rep_t -> true;
    Req_timestamp > Last_rep_t, msg_log_add_request(Replica_id, Req_digest, Req_timestamp)
  ).

effect_RECEIVE_REQUEST(Req_digest, Req_timestamp, Replica_id) :-
  transaction( apply_RECEIVE_REQUEST(Req_digest, Req_timestamp, Replica_id) ).

% 2. SEND-PRE-PREPARE

pre_SEND_PRE_PREPARE(Req_digest, V, N, Replica_id) :-
  view(Replica_id, View_i),
  debug(pre_SEND_PRE_PREPARE, "pre_SEND_PRE_PREPARE: View_i=~w", [View_i]),
  active_view(Replica_id),
  seqno(Replica_id, Seqno_i),
  debug(pre_SEND_PRE_PREPARE, "pre_SEND_PRE_PREPARE: View_i=~w Seqno_i=~w", [View_i, Seqno_i]),
  primary(View_i, P), P #= Replica_id,
  Seqno_i #= N - 1,
  debug(pre_SEND_PRE_PREPARE, "pre_SEND_PRE_PREPARE: N=~w, if it stops here this is likely due to RECEIVE_BLOCK not triggered, e.g. IBD.", [N]),
  in_wv(V,N,Replica_id),
  debug(pre_SEND_PRE_PREPARE, "pre_SEND_PRE_PREPARE: V=~w", [V]),
  msg_log_request(Replica_id, Req_digest, Req_timestamp),
  debug(pre_SEND_PRE_PREPARE, "pre_SEND_PRE_PREPARE: Req_digest=~w Req_timestamp=~w N=~w V=~w", [Req_digest, Req_timestamp, N, V]),
  % Ensure that a pre_prepare is sent only for the minimum req_timestamp request not having a pre_prepare
  % This enforces the creation of one block at a time
  findall(T, (
    msg_log_request(Replica_id, D, T),
    \+ msg_log_pre_prepare(Replica_id, _, _, D, _, Replica_id, _),
    get_synthetic_time(Replica_id, Current_time),
    T =< Current_time
  ), Out_t), min_list(Out_t, Req_timestamp),
  debug(pre_SEND_PRE_PREPARE, "pre_SEND_PRE_PREPARE: Req_digest=~w Req_timestamp=~w N=~w V=~w", [Req_digest, Req_timestamp, N, V]),
  % With integers, we can use CLP/FD for the "NOT EXISTS pre-prepare with a different N" clause
  N1 #\= N, \+ msg_log_pre_prepare(Replica_id, V, N1, Req_digest, _, Replica_id, _).

apply_SEND_PRE_PREPARE(Req_digest, Associated_pre_prepare_data, V, N, Replica_id) :-
  set_seqno(Replica_id, N),
  msg_out_add_pre_prepare(Replica_id, V, N, Req_digest, Associated_pre_prepare_data),
  msg_log_add_pre_prepare(Replica_id, V, N, Req_digest, Associated_pre_prepare_data, Replica_id, "SIG_OWN_REPLICA").

effect_SEND_PRE_PREPARE(Req_digest, Associated_pre_prepare_data, V, N, Replica_id) :-
  transaction( apply_SEND_PRE_PREPARE(Req_digest, Associated_pre_prepare_data, V, N, Replica_id) ).

% 3. RECEIVE-PRE-PREPARE

apply_RECEIVE_PRE_PREPARE(Ppp_v, Ppp_n, Ppp_req_digest, Ppp_associated_data, Sender_id, Sender_sig, Replica_id) :-
  Sender_id =\= Replica_id,
  view(Replica_id, View_i), primary(View_i, P), P =:= Sender_id,
  in_wv(Ppp_v, Ppp_n, Replica_id),
  active_view(Replica_id),
  % With strings we express the NOT EXISTS pre-prepare with different digest as:
  % either it does not exist OR it exists with the same value
  (
    \+ msg_log_pre_prepare(Replica_id, Ppp_v, Ppp_n, _, _, Sender_id, _);
    msg_log_pre_prepare(Replica_id, Ppp_v, Ppp_n, Ppp_req_digest, _, Sender_id, _)
  ),
  msg_log_add_pre_prepare(Replica_id, Ppp_v, Ppp_n, Ppp_req_digest, Ppp_associated_data, Sender_id, Sender_sig).

effect_RECEIVE_PRE_PREPARE(Ppp_v, Ppp_n, Ppp_req_digest, Ppp_associated_data, Sender_id, Sender_sig, Replica_id) :-
  transaction( apply_RECEIVE_PRE_PREPARE(Ppp_v, Ppp_n, Ppp_req_digest, Ppp_associated_data, Sender_id, Sender_sig, Replica_id) ).

% 4. SEND-PREPARE

pre_SEND_PREPARE(Req_digest, V, N, Replica_id) :-
  view(Replica_id, View_i),
  primary(View_i, P), P #\= Replica_id,
  % TODO: check in_wv, in_wv is not in Castro spec, but prevents replica which have started a view change, to continue sending messages of previous views.
  in_wv(V,N,Replica_id),
  pre_prepared(Req_digest, V, N, Replica_id),
  \+ msg_log_prepare(Replica_id, V, N, Req_digest, Replica_id, _).

apply_SEND_PREPARE(Req_digest, V, N, Replica_id) :-
  msg_out_add_prepare(Replica_id, V, N, Req_digest),
  msg_log_add_prepare(Replica_id, V, N, Req_digest, Replica_id, "SIG_OWN_REPLICA").

effect_SEND_PREPARE(Req_digest, V, N, Replica_id) :-
  transaction( apply_SEND_PREPARE(Req_digest, V, N, Replica_id) ).

% 5. RECEIVE-PREPARE (WIP)

apply_RECEIVE_PREPARE(V, N, Req_digest, Sender_id, Sender_sig, Replica_id) :-
  Sender_id #\= Replica_id,
  view(Replica_id, View_i),
  primary(View_i, P), P #\= Sender_id,
  in_wv(V, N, Replica_id),
  msg_log_add_prepare(Replica_id, V, N, Req_digest, Sender_id, Sender_sig).

effect_RECEIVE_PREPARE(V, N, Req_digest, Sender_id, Sender_sig, Replica_id) :-
  transaction( apply_RECEIVE_PREPARE(V, N, Req_digest, Sender_id, Sender_sig, Replica_id)).

% 6. SEND-COMMIT

pre_SEND_COMMIT(Req_digest, V, N, Replica_id) :-
  % TODO: check in_wv. It is not in Castro spec, but prevents replica which have started a view change, to continue sending messages of previous views.
  in_wv(V,N,Replica_id),
  prepared(Req_digest, V, N, Replica_id),
  \+ msg_log_commit(Replica_id, V, N, _, Replica_id, _).

apply_SEND_COMMIT(V, N, Associated_commit_data, Replica_id) :-
  msg_out_add_commit(Replica_id, V, N, Associated_commit_data),
  msg_log_add_commit(Replica_id, V, N, Associated_commit_data, Replica_id, "SIG_OWN_REPLICA").

effect_SEND_COMMIT(V, N, Associated_commit_data, Replica_id) :-
  transaction( apply_SEND_COMMIT(V, N, Associated_commit_data, Replica_id)).

% 7. RECEIVE-COMMIT

apply_RECEIVE_COMMIT(Commit_v, Commit_n, Commit_associated_data, Sender_id, Sender_sig, Replica_id) :-
  Sender_id =\= Replica_id,
  ( % if
    in_wv(Commit_v, Commit_n, Replica_id)
    -> % then
    (
      msg_log_add_commit(Replica_id, Commit_v, Commit_n, Commit_associated_data, Sender_id, Sender_sig),
      ( % if we receive a commit during an active ROAST session, we are coordinating as primary, and it's not final, we apply_RECEIVE_SIG_SHARE
        roast_active(Replica_id, Commit_v, Commit_n, _),
        primary(Commit_v, Replica_id),
        \+ roast_final_signature_session(Replica_id, _, _, _, _)
        -> % then
        apply_RECEIVE_SIG_SHARE(Replica_id, Sender_id, false, Commit_associated_data)
        ; % else
        true
      )
    )
    ; % else
    view(Replica_id, V),
    Commit_v > V,
    debug(apply_RECEIVE_COMMIT, "apply_RECEIVE_COMMIT: Adding to View Recovery for Replica_id=~w Commit_v=~w Commmit_n=~w Sender_id=~w", [Replica_id, Commit_v, Commit_n, Sender_id]),
    msg_log_add_commit_view_recovery(Replica_id, Commit_v, Commit_n, Sender_id)
  ).

effect_RECEIVE_COMMIT(Commit_v, Commit_n, Commit_associated_data, Sender_id, Sender_sig, Replica_id) :-
  transaction( apply_RECEIVE_COMMIT(Commit_v, Commit_n, Commit_associated_data, Sender_id, Sender_sig, Replica_id)).

% 7B. RECOVER-VIEW

% TODO: here Replica_id and V inverted: for all actions, we always have Replica_id as the last argument.
% This is due to the way actions are specified in CastroLiskov2002, nevertheless Replica_id first seems much more intuitive.
pre_RECOVER_VIEW(Replica_id, V) :-
  view(Replica_id, View_i),
  active_view(Replica_id),
  quorum(Quorum),
  findall(V1, (
    checkpoint(Replica_id, N, _, _),
    msg_log_commit_view_recovery(Replica_id, V1, N, _),
    V1>View_i,
    findall(Sender_id, msg_log_commit_view_recovery(Replica_id, V1, N, Sender_id), Recovery_msgs),
    proper_length(Recovery_msgs, Len),
    Len >= Quorum
  ), V_list),
  debug(pre_RECOVER_VIEW, "pre_RECOVER_VIEW: Replica_id=~w V_list=~w", [Replica_id, V_list]),
  max_list(V_list, V).

apply_RECOVER_VIEW(Replica_id, V) :-
  set_view(Replica_id, V),
  msg_log_clear_where_view_less_than(Replica_id, V).

effect_RECOVER_VIEW(Replica_id, V) :-
  transaction(apply_RECOVER_VIEW(Replica_id, V)).

% 8. EXECUTE

pre_EXECUTE(Req_digest, V, N, Replica_id) :-
  nb_getval(signature_algorithm, Sig_algo),
  debug(pre_EXECUTE, "pre_EXECUTE: Replica_id=~w Sig_algo=~w", [Replica_id, Sig_algo]),
  last_exec(Replica_id, Last_exec),
  debug(pre_EXECUTE, "pre_EXECUTE: Replica_id=~w Last_exec=~w", [Replica_id, Last_exec]),
  Last_exec #= N - 1,
  debug(pre_EXECUTE, "pre_EXECUTE: Replica_id=~w N=~w", [Replica_id, N]),
  committed(Req_digest, V, N, Replica_id),
  debug(pre_EXECUTE, "pre_EXECUTE: Replica_id=~w V=~w Req_digest=~w", [Replica_id, V, Req_digest]),
  ( % if
    Sig_algo == "ROAST"
    -> % then
    primary(V, Replica_id), % only the primary executes in ROAST
    roast_final_signature_session(Replica_id, _, _, _, _)
    ; % else
    debug(pre_EXECUTE, "pre_EXECUTE: Replica_id=~w V=~w N=~w Req_digest=~w", [Replica_id, V, N, Req_digest])
  ).

apply_EXECUTE(Req_digest, N, State_val, Replica_id) :-
  ( % if
    msg_log_request(Replica_id, Req_digest, Req_timestamp),
    get_last_rep(Replica_id, Last_rep_t),
    Req_timestamp > Last_rep_t
    -> % then
    (
      debug(apply_EXECUTE, "apply_EXECUTE set: last_exec=~w last_rep=~w val=~w", [N, Req_timestamp, State_val]),
      set_last_exec(Replica_id, N),
      set_last_rep(Replica_id, Req_timestamp),
      set_val(Replica_id, State_val)
    )
    ; % else
    true
  ).
  % Here we assume that K=1, so we always move forward the checkpoint
  % But, we do not add to log and send checkpoint messages, this happens via the participants network

effect_EXECUTE(Req_digest, N, State_val, Replica_id) :-
  transaction( apply_EXECUTE(Req_digest, N, State_val, Replica_id)).

% 11. COLLECT GARBAGE

collect_garbage(N, Replica_id) :-
  get_last_rep(Replica_id, Last_rep_t),
  debug(collect_garbage, "collect_garbage: N=~w Replica_id=~w Last_rep_t=~w", [N, Replica_id, Last_rep_t]),
  foreach(
    (
      N1 #>= 0, N1 #< N,
      indomain(N1)
    ),
    (
      debug(collect_garbage, "collect_garbage: N1=~w", [N1]),
      retractall(checkpoint(Replica_id,N1,_,_)),
      retractall(msg_log_pre_prepare(Replica_id,_,N1,_,_,_,_)),
      retractall(msg_log_prepare(Replica_id,_,N1,_,_,_)),
      retractall(msg_log_commit(Replica_id,_,N1,_,_,_)),
      retractall(view_change_Pi(Replica_id,N1,_,_,_)),
      retractall(view_change_Qi(Replica_id,N1,_,_,_,_))
    )
  ),
  retractall(msg_log_pre_prepare(Replica_id,_,N,_,_,_,_)),
  retractall(msg_log_prepare(Replica_id,_,N,_,_,_)),
  retractall(msg_log_commit(Replica_id,_,N,_,_,_)),
  retractall(view_change_Pi(Replica_id,N,_,_)),
  retractall(view_change_Qi(Replica_id,N,_,_,_)),
  foreach( msg_log_request(Replica_id, T2, T),
    ( (T =< Last_rep_t) -> retractall(msg_log_request(Replica_id, T2, T)); true)
  ),
  ( % if
    nb_getval(signature_algorithm, "ROAST")
    -> % then
    roast_collect_garbage(Replica_id)
    ; % else
    true
  ).

% 12. SEND VIEW-CHANGE

pre_SEND_VIEW_CHANGE(V, Replica_id) :-
  debug(pre_SEND_VIEW_CHANGE, "pre_SEND_VIEW_CHANGE: Replica_id=~w", [Replica_id]),
  timeout(Replica_id, Timeout),
  %% primary of current view does not send VIEW-CHANGE
  debug(pre_SEND_VIEW_CHANGE, "pre_SEND_VIEW_CHANGE: Replica_id=~w, Timeout=~w", [Replica_id, Timeout]),
  view(Replica_id, View_i),
  debug(pre_SEND_VIEW_CHANGE, "pre_SEND_VIEW_CHANGE: Replica_id=~w, View_i=~w", [Replica_id, V]),
  %primary(View_i, P), P #\= Replica_id,
  %% primary of the next view does not send VIEW-CHANGE
  V #= View_i + 1,
  %primary(V, P1), P1 #\= Replica_id,
  % TODO: How we check the timeout, using the Req_timestamp, has to be confirmed
  % i.e. Can we trust Req_timestamp ?
  msg_log_request(Replica_id, _, Req_timestamp),
  get_last_rep(Replica_id, Out_last_rep_t),
  Out_last_rep_t < Req_timestamp,
  get_synthetic_time(Replica_id, T), Current_timestamp is T, % round(T),
  debug(pre_SEND_VIEW_CHANGE, "pre_SEND_VIEW_CHANGE: Replica_id=~w, Timeout=~w, V=~w, Req_t=~w, Cur_t=~w", [Replica_id, Timeout, V, Req_timestamp, Current_timestamp]),
  Current_timestamp - Req_timestamp > Timeout.

apply_SEND_VIEW_CHANGE(V, Replica_id) :-
  debug(apply_SEND_VIEW_CHANGE, "apply_SEND_VIEW_CHANGE: V=~w Replica_id=~w", [V, Replica_id]),
  set_view(Replica_id, V),
  set_active_view(Replica_id, false),
  set_roast_active(Replica_id, false, false, false),
  % Build Pi
  update_view_change_Pi(Replica_id),
  findall([N1,Req_digest1,V1], view_change_Pi(Replica_id, N1, Req_digest1, V1), Pi),
  debug(apply_SEND_VIEW_CHANGE, "apply_SEND_VIEW_CHANGE: Pi=~w", [Pi]),
  % Build Qi
  update_view_change_Qi(Replica_id),
  findall([N2,Req_digest2, Associated_pre_prepare_data2, V2], view_change_Qi(Replica_id, N2, Req_digest2, Associated_pre_prepare_data2, V2), Qi),
  debug(apply_SEND_VIEW_CHANGE, "apply_SEND_VIEW_CHANGE: Qi=~w", [Qi]),
  % Build Hi
  get_h(Replica_id, Hi),
  debug(apply_SEND_VIEW_CHANGE, "apply_SEND_VIEW_CHANGE: Hi=~w", [Hi]),
  % Build C
  checkpoint(Replica_id, Hi, C, _),
  debug(apply_SEND_VIEW_CHANGE, "apply_SEND_VIEW_CHANGE: C=~w", [C]),
  % Append message to out buffer
  msg_out_add_view_change(Replica_id, V, Hi, C, Pi, Qi),
  % Append message to log
  digest_view_change(V, Hi, C, Pi, Qi, Replica_id, Vc_digest),
  msg_log_add_view_change(Replica_id, Vc_digest, V, Hi, C, Pi, Qi, Replica_id, "SIG_OWN_REPLICA"),
  % Removing all messages with lower view.
  msg_log_clear_where_view_less_than(Replica_id, V).

effect_SEND_VIEW_CHANGE(V, Replica_id) :-
  transaction( apply_SEND_VIEW_CHANGE(V, Replica_id) ).

% 13. RECEIVE VIEW-CHANGE

apply_RECEIVE_VIEW_CHANGE(V, Hi, C, Pi, Qi, Sender_id, Sender_sig, Replica_id) :-
  Sender_id =\= Replica_id,
  view(Replica_id, View_i), V>=View_i,
  correct_view_change(V, Hi, Pi, Qi),
  % Does not exist a view change with same V and Sender_id, but different V, H, C, P, Q
  \+ msg_log_view_change(Replica_id, _, V, _, _, _, _, Sender_id, _),
  digest_view_change(V, Hi, C, Pi, Qi, Sender_id, Vc_digest),
  msg_log_add_view_change(Replica_id, Vc_digest, V, Hi, C, Pi, Qi, Sender_id, Sender_sig).

effect_RECEIVE_VIEW_CHANGE(V, Hi, C, Pi, Qi, Sender_id, Sender_sig, Replica_id) :-
  transaction( apply_RECEIVE_VIEW_CHANGE(V, Hi, C, Pi, Qi, Sender_id, Sender_sig, Replica_id) ).

% 15. SEND NEW-VIEW

pre_SEND_NEW_VIEW(Nu, Chi, Replica_id) :-
  view(Replica_id, View_i),
  primary(View_i, P), P #= Replica_id,
  correct_nu_once(Nu, Replica_id),
  correct_chi(Chi, Nu, Replica_id),
  \+ msg_log_new_view(Replica_id, View_i, _, _, _).

apply_SEND_NEW_VIEW(Nu, Chi, Replica_id) :-
  view(Replica_id, View_i),
  msg_log_add_new_view(Replica_id, View_i, Nu, Chi, "SIG_OWN_REPLICA"),
  msg_out_add_new_view(Replica_id, View_i, Nu, Chi).

effect_SEND_NEW_VIEW(Nu, Chi, Replica_id) :-
  transaction( apply_SEND_NEW_VIEW(Nu, Chi, Replica_id)).

% 16 RECEIVE NEW-VIEW

apply_RECEIVE_NEW_VIEW(V, Nu, Chi, Sender_id, Sender_sig, Replica_id) :-
  Sender_id =\= Replica_id,
  view(Replica_id, View_i),
  primary(V, Primary),
  V > 0, V =< View_i,
  Sender_id =:= Primary,
  % does not exist a new-view with same Nu or Chi
  (msg_log_new_view(Replica_id, View_i, Nu, Chi, _) -> true; msg_log_add_new_view(Replica_id, V, Nu, Chi, Sender_sig)).

effect_RECEIVE_NEW_VIEW(Nv_v, Nv_nu, Nv_chi, Sender_id, Sender_sig, Replica_id) :-
  transaction( apply_RECEIVE_NEW_VIEW(Nv_v, Nv_nu, Nv_chi, Sender_id, Sender_sig, Replica_id)).

% 19. RECEIVE BLOCK has some similarities with RECEIVE STATE so it's placed here

apply_RECEIVE_BLOCK(Replica_id, Block_height, Block_timestamp, Block_hash) :-
  debug(apply_RECEIVE_BLOCK, "apply_RECEIVE_BLOCK: Replica_id=~w Block_h=~w Block_t=~w Block_hash=~w", [Replica_id, Block_height, Block_timestamp, Block_hash]),
  msg_log_request(Replica_id, Req_digest, Block_timestamp),
  apply_EXECUTE(Req_digest, Block_height, Block_hash, Replica_id),
  add_checkpoint(Replica_id, Block_height, Block_hash, Block_timestamp),
  collect_garbage(Block_height, Replica_id).

effect_RECEIVE_BLOCK(Replica_id, Block_height, Block_timestamp, Block_hash) :-
  transaction( apply_RECEIVE_BLOCK(Replica_id, Block_height, Block_timestamp, Block_hash)).

% 20. PROCESS NEW-VIEW

pre_PROCESS_NEW_VIEW(Hi, Nu, Chi, Replica_id) :-
  \+active_view(Replica_id),
  correct_new_view(Chi, Nu, Replica_id),
  % get Hi, the minimum checkpoint sequence number
  get_h(Replica_id, Hi),
  % Ensure that the new view will start at least at Hi
  findall(
    [Replica_id, V, Hi, C, P, Q, Sender_id],
    nu_vset_i(Nu, Replica_id, V, Hi, C, P, Q, Sender_id),
    ViewChangeMsgs
  ),
  findall(Vc_hi, member([_, _, Vc_hi, _, P, _, _], ViewChangeMsgs), All_Vc_his), min_list(All_Vc_his, Min_s),
  Hi >= Min_s,
  % Ensure that I know all the checkpoints in the view change messages I received
  % I.e. I am in sync with the network
  findall(C, member([_, _, _, C, _, _, _], ViewChangeMsgs), AllCs),
  forall(member(C, AllCs), checkpoint(Replica_id, _, C, _)),
  % Ensure that minimum sequence in Chi is Min_s + 1
  findall(ChiId, member([ChiId, _, _], Chi), ChiIds),
  once((
    min_list(ChiIds, MinChiId), MinChiId is Min_s+1; %OR
    length(ChiIds, 0)
  )),
  % Check that we have request in the log
  forall(member([_, D, _], Chi), (D=="null"; msg_log_request(Replica_id, D, _))),
  true.

apply_PROCESS_NEW_VIEW(Hi, Chi, Replica_id) :-
  primary(V, Primary),
  set_active_view(Replica_id, true),
  view(Replica_id, V),
  % Populate pre-prepare message log. Assuming the sender is the current primary.
  foreach((member([N,D,B], Chi)), msg_log_add_pre_prepare(Replica_id, V, N, D, B, Primary, "SIG_IN_NEW_VIEW")),
  % set sequence number to the maximum
  findall(SeqNumber, msg_log_pre_prepare(Replica_id, V, SeqNumber, _, _, _, _), SeqNumbers),
  ( \+ length(SeqNumbers, 0) ->
    (
      max_list(SeqNumbers, MaxSeqNumber),
      set_seqno(Replica_id, MaxSeqNumber)
    );
    set_seqno(Replica_id, Hi)
  ),
  % if we are not the primary node, populate prepare message log
  (Primary \== Replica_id ->
    % sender id not known at this point
    (
        foreach((member([N,D,_], Chi)),
            msg_log_add_prepare(Replica_id, V, N, D, Replica_id, "SIG_OWN_REPLICA")),
        foreach((member([N,D,_], Chi)),
            msg_out_add_prepare(Replica_id, V, N, D))
    );
    true).

effect_PROCESS_NEW_VIEW(Hi, Chi, Replica_id) :-
  transaction( apply_PROCESS_NEW_VIEW(Hi, Chi, Replica_id)).

%
% Additional code for the implementation of 5-FBFT with ROAST
%

% Globally enable ROAST signature algorithm, using Sig_algo="ROAST"

set_signature_algorithm(Sig_algo) :-
  nb_setval(signature_algorithm, Sig_algo).

% ROAST state variables

% roast_active(Replica_id, V, N, Req_digest)
:- dynamic roast_active/4.

set_roast_active(Replica_id, V, N, Req_digest) :-
  retractall(roast_active(Replica_id, _, _, _)),
  ( % if
    \+(V==false),
    \+(N==false),
    \+(Req_digest==false)
    -> % then
    assertz_once(roast_active(Replica_id, V, N, Req_digest))
    ; % else
    true
  ).

% roast_responsive_signer(Replica_id, Signer)
:- dynamic roast_responsive_signer/2.

roast_mark_responsive(Replica_id, Signer) :-
  assertz_once(roast_responsive_signer(Replica_id, Signer)).

roast_clear_responsive_signers(Replica_id) :-
  retractall( roast_responsive_signer(Replica_id,_) ).

% roast_malicious_signer(Replica_id, Signer)
:- dynamic roast_malicious_signer/2.

roast_mark_malicious(Replica_id, Signer) :-
  assertz_once(roast_malicious_signer(Replica_id, Signer)).

% roast_latest_pre_signature_share(Replica_id, Signer_id, Pre_signature_share)
:- dynamic roast_latest_pre_signature_share/3.

set_latest_pre_signature_share(Replica_id, Signer_id, Pre_signature_share) :-
  retractall(roast_latest_pre_signature_share(Replica_id, Signer_id, _)),
  assertz_once(roast_latest_pre_signature_share(Replica_id, Signer_id, Pre_signature_share)).

% roast_session_counter(Replica_id, Counter)
:- dynamic roast_session_counter/2.

set_roast_session_counter(Replica_id, Counter) :-
  retractall(roast_session_counter(Replica_id, _)),
  assertz_once(roast_session_counter(Replica_id, Counter)).

% roast_signers_sid(Replica_id, Signer, Session_id)
:- dynamic roast_signers_sid/3.

set_roast_signers_sid(Replica_id, Signer, Session_id) :-
  retractall(roast_signers_sid(Replica_id, Signer, _)),
  assertz_once(roast_signers_sid(Replica_id, Signer, Session_id)).

% roast_signature_session(Replica_id, Session_id, Signers, Pre_signature, Signature_shares)
:- dynamic roast_signature_session/5.

set_roast_signature_session(Replica_id, Session_id, Signers, Pre_signature, Signature_shares) :-
  retractall(roast_signature_session(Replica_id, Session_id, _, _, _)),
  assertz_once(roast_signature_session(Replica_id, Session_id, Signers, Pre_signature, Signature_shares)).

roast_final_signature_session(Replica_id, Session_id, Signers, Pre_signature, Signature_shares) :-
  roast_active(Replica_id, _, _, _),
  roast_signature_session(Replica_id, Session_id, Signers, Pre_signature, Signature_shares),
  sig_quorum(Sig_threshold),
  proper_length(Signature_shares, Sig_threshold).

% msg_out_roast_pre_signature(Replica_id, Signers, Pre_signature)
% Signers: list of integers, replica_id of the proposed signers
% Pre_signature: string, the ROAST pre_signature calculated from public commitments
:- dynamic msg_out_roast_pre_signature/3.

msg_out_add_pre_signature(Replica_id, Signers, Pre_signature) :-
  assertz_once(msg_out_roast_pre_signature(Replica_id, Signers, Pre_signature)).

% msg_out_roast_signature_share(Replica_id, Signers, Signature_share, Next_pre_signature_share)
% Signature_share: string, the ROAST signature share of the signer sending the message
% Next_pre_signature_share: string, the ROAST pre_signature share of the signer sending the message
:- dynamic msg_out_roast_signature_share/3.

msg_out_add_signature_share(Replica_id, Signature_share, Next_pre_signature_share) :-
  assertz_once(msg_out_roast_signature_share(Replica_id, Signature_share, Next_pre_signature_share)).

% ROAST

% computes the digest for the pre_signature message
digest_roast_pre_signature(Signers, Pre_signature, Sender_id, Digest) :-
  Message = roast_pre_signature{signers: Signers, pre_signature: Pre_signature, sender_id: Sender_id},
  atom_json_dict(Json_string, Message, [as(string), tag(name)]),
  digest_from_string(Json_string, Digest).

% computes the digest for the signature_share message
digest_roast_signature_share(Signature_share, Next_pre_signature_share, Sender_id, Digest) :-
  Message = roast_signature_share{signature_share: Signature_share, next_pre_signature_share: Next_pre_signature_share, sender_id: Sender_id},
  atom_json_dict(Json_string, Message, [as(string), tag(name)]),
  digest_from_string(Json_string, Digest).

% ROAST auxiliary functions

sig_quorum(Out_quorum) :-
  nb_getval(cluster_size, Csize),
  F is floor((Csize-1)/3),
  Out_quorum is F+1.

roast_collect_garbage(Replica_id) :-
  retractall( roast_active(Replica_id, _, _, _) ),
  retractall( roast_responsive_signer(Replica_id,_) ),
  retractall( roast_malicious_signer(Replica_id,_) ),
  retractall( roast_latest_pre_signature_share(Replica_id,_,_) ),
  retractall( roast_session_counter(Replica_id,_) ),
  retractall( roast_signers_sid(Replica_id,_,_) ),
  retractall( roast_signature_session(Replica_id,_,_,_,_) ),
  retractall( msg_out_roast_pre_signature(Replica_id,_,_) ),
  retractall( msg_out_roast_signature_share(Replica_id,_,_) ).

roast_print_all :-
  listing(roast_active/4),
  listing(roast_responsive_signer/2),
  listing(roast_malicious_signer/2),
  listing(roast_latest_pre_signature_share/3),
  listing(roast_session_counter/2),
  listing(roast_signers_sid/3),
  listing(roast_signature_session/5),
  listing(msg_out_roast_pre_signature/3),
  listing(msg_out_roast_signature_share/3),
  true.

roast_debug :-
  debug(apply_RECEIVE_PRE_SIGNATURE),
  debug(apply_RECEIVE_SIG_SHARE),
  debug(apply_ROAST_INIT).

% Dynamic predicates for dynamic rules.
% roast_crypto_pre_sig_aggregate(Replica_id, Pre_signature_shares, Pre_signature)
:- dynamic roast_crypto_pre_sig_aggregate/3.

% ROAST actions

% ROAST_INIT, which includes a SEND_FIRST_PRE_SIGNATURE when primary

pre_ROAST_INIT(Replica_id, Req_digest, V, N) :-
  % Ensure we are using the ROAST signature algorithm
  nb_getval(signature_algorithm, "ROAST"),
  % Commitments proposal are sent when a request is committed but not executed
  last_exec(Replica_id, Last_exec),
  Last_exec #= N - 1,
  committed(Req_digest, V, N, Replica_id),
  % And we do not have already a ROAST session in place
  \+ roast_active(Replica_id, _, _, _),
  % And we do not have already gathered enough signature shares
  \+ roast_final_signature_session(Replica_id, _, _, _, _),
  true.

apply_ROAST_INIT(Replica_id, V, N, Req_digest) :-
  debug(apply_ROAST_INIT, "apply_ROAST_INIT: Replica_id = ~w, V = ~w, N = ~w", [Replica_id, V, N]),
  ( % if
    primary(V, Replica_id)
    -> % then,
    (
      debug(apply_ROAST_INIT, "apply_ROAST_INIT: Replica_id = ~w is primary, initiating ROAST", [Replica_id]),
      set_roast_active(Replica_id, V, N, Req_digest),
      set_roast_session_counter(Replica_id, 0),
      foreach(
        msg_log_commit(Replica_id, V, N, Associated_commit_data, Sender_id, _),
        % Initially all signers send (null, ro_i) to the coordinator. This is done via commit messages
        apply_RECEIVE_SIG_SHARE(Replica_id, Sender_id, false, Associated_commit_data)
      )
    )
    ; % else
    (
      debug(apply_ROAST_INIT, "apply_ROAST_INIT: Replica_id = ~w initiating ROAST", [Replica_id]),
      set_roast_active(Replica_id, V, N, Req_digest)
    )
  ).

effect_ROAST_INIT(Replica_id, V, N, Req_digest) :-
  transaction(apply_ROAST_INIT(Replica_id, V, N, Req_digest)).

% RECEIVE_PRE_SIGNATURE

% This action is executed by the signer (primary and backups), upon receipt of (ro, R) from the coordinator
% ro is a pre_signature, R is a set of signers

apply_RECEIVE_PRE_SIGNATURE(Replica_id, Signers, Pre_signature, Signature_share, Next_pre_signature_share) :-
  roast_active(Replica_id, _, _, _),
  debug(apply_RECEIVE_PRE_SIGNATURE, "apply_RECEIVE_PRE_SIGNATURE: Replica_id ~w received signers ~w and presignature ~w", [Replica_id, Signers, Pre_signature]),
  % Always send the signature share upon receive of pre_signature_share
  msg_out_add_signature_share(Replica_id, Signature_share, Next_pre_signature_share),
  true.

effect_RECEIVE_PRE_SIGNATURE(Replica_id, Signers, Pre_signature, Signature_share, Next_pre_signature_share) :-
  transaction(apply_RECEIVE_PRE_SIGNATURE(Replica_id, Signers, Pre_signature, Signature_share, Next_pre_signature_share)).

% RECEIVE_SIG_SHARE

% This action is executed by the coordinator (primary), upon receipt of (sigma_i, ro_i) from a signer
% sigma_i is a signature share, ro_i is a pre_signature_share

apply_RECEIVE_SIG_SHARE(Replica_id, Signer_id, Signature_share, Next_pre_signature_share) :-
  roast_active(Replica_id, V, _, _),
  \+ roast_final_signature_session(Replica_id, _, _, _, _),
  primary(V, Replica_id),
  % Here goes the algorithm of the primary
  sig_quorum(Sig_threshold),
  debug(apply_RECEIVE_SIG_SHARE, "apply_RECEIVE_SIG_SHARE: Coordinator ~w using threshold ~w", [Replica_id, Sig_threshold]),
  ( % if in R then: MarkMalicious(i) and break
    roast_responsive_signer(Replica_id, Signer_id)
    -> % then
    (
      debug(apply_RECEIVE_SIG_SHARE, "apply_RECEIVE_SIG_SHARE: Coordinator ~w receives unsolicited reply from Signer ~w", [Replica_id, Signer_id]),
      roast_mark_malicious(Replica_id, Signer_id)
    )
    ; % else do nothing
    true
  ),
  ( % if SID[i] is not null
    roast_signers_sid(Replica_id, Signer_id, Session_id)
    -> % then
    (
      % sid <- SID[i]
      debug(apply_RECEIVE_SIG_SHARE, "apply_RECEIVE_SIG_SHARE: Coordinator ~w receives message from signer ~w, which is in session ~w", [Replica_id, Signer_id, Session_id]),
      roast_signature_session(Replica_id, Session_id, Session_signers, Session_pre_sig, Session_sig_shares),
      debug(apply_RECEIVE_SIG_SHARE, "apply_RECEIVE_SIG_SHARE: Lookup signers ~w and aggregate presignature ~w of session ~w with shares ~w", [Session_signers, Session_pre_sig, Session_id, Session_sig_shares]),
      roast_latest_pre_signature_share(Replica_id, Signer_id, Pre_signature_share),
      debug(apply_RECEIVE_SIG_SHARE, "apply_RECEIVE_SIG_SHARE: Lookup presignature share of signer ~w = ~w (this may be omitted)", [Signer_id, Pre_signature_share]),
      % TODO: if not share val PK, then mark malicious, currently omitted
      debug(apply_RECEIVE_SIG_SHARE, "apply_RECEIVE_SIG_SHARE: Store valid signature share of signer ~w = ~w", [Signer_id, Signature_share]),
      append(Session_sig_shares, [Signature_share], New_session_sig_shares),
      set_roast_signature_session(Replica_id, Session_id, Session_signers, Session_pre_sig, New_session_sig_shares),
      debug(apply_RECEIVE_SIG_SHARE, "apply_RECEIVE_SIG_SHARE: New_session_sig_shares = ~w", [New_session_sig_shares]),
      proper_length(New_session_sig_shares, Num_sig_shares),
      ( % if
        Num_sig_shares==Sig_threshold
        -> % then
        % Here we can execute!
        debug(apply_RECEIVE_SIG_SHARE, "apply_RECEIVE_SIG_SHARE: We have ~w signature share, aggregate and execute!!", [Num_sig_shares])
        ; % else do nothing
        debug(apply_RECEIVE_SIG_SHARE, "apply_RECEIVE_SIG_SHARE: We have ~w signature share out of ~w, do nothing", [Num_sig_shares, Sig_threshold])
      )
    )
    ; % else
    true
  ),
  ( % if roast is still active, because it may have been disabled in case Num_sig_shares==Sig_threshold
    \+ roast_final_signature_session(Replica_id, _, _, _, _)
    -> % then
    (
      debug(apply_RECEIVE_SIG_SHARE, "apply_RECEIVE_SIG_SHARE: Coordinator ~w stores received presignature share from Signer ~w", [Replica_id, Signer_id]),
      set_latest_pre_signature_share(Replica_id, Signer_id, Next_pre_signature_share),
      debug(apply_RECEIVE_SIG_SHARE, "apply_RECEIVE_SIG_SHARE: Mark ~w as responsive", [Signer_id]),
      roast_mark_responsive(Replica_id, Signer_id),
      debug(apply_RECEIVE_SIG_SHARE, "apply_RECEIVE_SIG_SHARE: Sig quorum is ~w", [Sig_threshold]),
      findall(S, roast_responsive_signer(Replica_id, S), Responsive_signers),
      debug(apply_RECEIVE_SIG_SHARE, "apply_RECEIVE_SIG_SHARE: Responsive signers are ~w", [Responsive_signers]),
      proper_length(Responsive_signers, Num_responsive_signers),
      ( % if
      Num_responsive_signers==Sig_threshold
      -> % then
      (
        debug(apply_RECEIVE_SIG_SHARE, "apply_RECEIVE_SIG_SHARE: We now have ~w responsive signers", [Num_responsive_signers]),
        roast_session_counter(Replica_id, Old_sid_counter),
        Sid_counter is Old_sid_counter+1,
        set_roast_session_counter(Replica_id, Sid_counter),
        debug(apply_RECEIVE_SIG_SHARE, "apply_RECEIVE_SIG_SHARE: Initiate a new session with them, id = ~w", [Sid_counter]),
        findall(Presig, (member(I, Responsive_signers), roast_latest_pre_signature_share(Replica_id, I, Presig)), Presig_shares),
        debug(apply_RECEIVE_SIG_SHARE, "apply_RECEIVE_SIG_SHARE: Lookup presignature shares = ~w", [Presig_shares]),
        roast_crypto_pre_sig_aggregate(Replica_id, Presig_shares, Pre_signature),
        debug(apply_RECEIVE_SIG_SHARE, "apply_RECEIVE_SIG_SHARE: Build the presignature = ~w", [Pre_signature]),
        foreach(
          member(I, Responsive_signers),
          (
            debug(apply_RECEIVE_SIG_SHARE, "apply_RECEIVE_SIG_SHARE: Remember that the session of ~w is ~w", [I, Sid_counter]),
            set_roast_signers_sid(Replica_id, I, Sid_counter)
          )
        ),
        debug(apply_RECEIVE_SIG_SHARE, "apply_RECEIVE_SIG_SHARE: Send the presignature to the signers", []),
        msg_out_add_pre_signature(Replica_id, Responsive_signers, Pre_signature),
        debug(apply_RECEIVE_SIG_SHARE, "apply_RECEIVE_SIG_SHARE: Remember the signers ~w and presignature ~w of session ~w", [Responsive_signers, Pre_signature, Sid_counter]),
        set_roast_signature_session(Replica_id, Sid_counter, Responsive_signers, Pre_signature, []),
        debug(apply_RECEIVE_SIG_SHARE, "apply_RECEIVE_SIG_SHARE: Mark signers as pending again", []),
        roast_clear_responsive_signers(Replica_id),
        true
      )
      ; % else
      true
      )
    )
    ; % else
    debug(apply_RECEIVE_SIG_SHARE, "apply_RECEIVE_SIG_SHARE: Roast has already terminated, exiting...", [])
  ).

effect_RECEIVE_SIG_SHARE(Replica_id, Signer_id, Signature_share, Next_pre_signature_share) :-
  transaction(apply_RECEIVE_SIG_SHARE(Replica_id, Signer_id, Signature_share, Next_pre_signature_share) ).
