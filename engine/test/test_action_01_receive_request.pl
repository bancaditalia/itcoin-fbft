% Copyright (c) 2023 Bank of Italy
% Distributed under the GNU AGPLv3 software license, see the accompanying COPYING file.

:- consult("test_00_utils.pl").

:- begin_tests(test_action_01_receive_request).

build_config(Config) :-
  Config = _{
    req_1 : "request_1",
    req_2 : "request_2",
    req_3 : "request_3",
    req_4 : "request_4",
    req_5 : "request_5",
    timestamp_0 : 33,
    timestamp_1 : 34,
    timestamp_2 : 35
  }.

% Initialize 4 replicas
setup_test() :-
  init_all.

test(test_receive_request_01, [setup((setup_test(), build_config(Config)))]) :-
  effect_RECEIVE_REQUEST(Config.req_1, Config.timestamp_1, 0),
  assertion( msg_log_request(0, Config.req_1, Config.timestamp_1)).

test(test_receive_request_02, [setup(build_config(Config))]) :-
  % last rep has not been set so we can keep on adding requests
  effect_RECEIVE_REQUEST(Config.req_2, Config.timestamp_1, 0),
  assertion( msg_log_request(0, Config.req_1, Config.timestamp_1) ),
  assertion( msg_log_request(0, Config.req_2, Config.timestamp_1) ).

test(test_receive_request_03, [setup(build_config(Config))]) :-
  set_last_rep(0, Config.timestamp_1),
  % request is not added to logs because timestamp equals last rep timestamp
  effect_RECEIVE_REQUEST(Config.req_3, Config.timestamp_1, 0),
  assertion( \+ msg_log_request(0, Config.req_3, Config.timestamp_1) ),
  % request is not added to logs because timestamp is lesser than last rep timestamp
  effect_RECEIVE_REQUEST(Config.req_4, Config.timestamp_0, 0),
  assertion( \+ msg_log_request(0, Config.req_4, Config.timestamp_0)).

test(test_receive_request_04, [setup(build_config(Config))]) :-
  % request is added to logs because timestamp is greater than last rep timestamp
  effect_RECEIVE_REQUEST(Config.req_5, Config.timestamp_2, 0),
  assertion( msg_log_request(0, Config.req_5, Config.timestamp_2) ).


:- end_tests(test_action_01_receive_request).
