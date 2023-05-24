:- consult("../specs/pbft-replica-engine.pl").
:- consult("test_00_utils.pl").

:- begin_tests(test_aux_08_correct_view_change).

setup_test(ReqDigest, AssociatedData) :-
    init_all,
    ReqDigest = "req_digest_1",
    AssociatedData = "block_1".

test(test_aux_08_correct_view_change_00, [setup(setup_test(ReqDigest, AssociatedData))]) :-
    V = 0, N = 1, H = 0,
    P = [[N, ReqDigest, V]], Q = [[N, ReqDigest, AssociatedData, V]],
    NewV #= V + 1,
    assertion(correct_view_change(NewV, H, P, Q)).

test(test_aux_08_correct_view_change_01, [setup(setup_test(ReqDigest, AssociatedData))]) :-
    V = 0, N = 1, H = 0,
    P = [[N, ReqDigest, V]], Q = [[N, ReqDigest, AssociatedData, V]],
    NewV #= V,
    % using the same view doesn't work
    assertion(\+correct_view_change(NewV, H, P, Q)).

test(test_aux_08_correct_view_change_02, [setup(setup_test(ReqDigest, AssociatedData))]) :-
    V = 0, N = 1,
    P = [[N, ReqDigest, V]], Q = [[N, ReqDigest, AssociatedData, V]],
    NewV #= V + 1,
    H = N,
    % using the same H as N doesn't work
    assertion(\+correct_view_change(NewV, H, P, Q)).

% test empty P and Q
test(test_aux_08_correct_view_change_00, [setup(setup_test(_, _))]) :-
    V = 1, H = 0,
    P = [], Q = [],
    assertion(correct_view_change(V, H, P, Q)).

:- end_tests(test_aux_08_correct_view_change).
