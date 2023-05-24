:- use_module(library(persistency)).

:- dynamic view/2.
:- persistent(view(Replica_id:integer, V:integer)).

set_view(Replica_id, V):-
  retractall_view(Replica_id, _),
  assert_view(Replica_id, V).

:- debug(init_notx).

init_notx(Replica_id, Db_filename, Db_reset) :-
  debug(init_notx, "init_notx: Filename = ~w", [Db_filename]),
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
          debug(init_notx, "init_notx: Database do not reset", []),
          db_attach(Db_filename, [sync(flush)])
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
  true.
