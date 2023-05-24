# SWI-prolog

We use prolog as a language that allows to solve logical equalities and inequalities.

In particular, Annex of [1] defines PBFT algorithm as I/O Automata, i.e. a State and a set of Actions (see page 449).

* https://pmg.csail.mit.edu/papers/bft-tocs.pdf

Each Action has a Signature, a Precondition, and an Effect. When the Precondition is true, the action is a candidate to be executed,
and when an action is executed, the Effect is applied to the Automata State.

We see the precondition as a logical inequality over a finite domain, in which:

- The current State represents the parameters of the inequality, i.e. the constants; these are, for example view_i, seqno_i.
- The variables in the signature of the Action represent the unknowns of the inequality, e.g. n, v.

The solutions of the inequality represent the set of active actions that are candidate to be executed.

We solve logical inequantions using a library written in Prolog for the SWI-Prolog implementation, called "CLP(FD): Constraint Logic Programming over Finite Domains":

* https://www.swi-prolog.org/pldoc/man?section=clpfd

Source code of the library can be found on Github, at the SWI-Prolog repo:

* https://github.com/SWI-Prolog/swipl-devel

C++ bindings for SWI prolog can be found on Github too:

* https://github.com/SWI-Prolog/packages-cpp

Instructions for SWI prolog installation on ubuntu. Header files are installed in `/usr/lib/swi-prolog/include`.

* https://www.swi-prolog.org/build/PPA.html

The SWI prolog has an active community on discourse blog.

* https://swi-prolog.discourse.group

This is a repo with a lot of C++ prolog code, that can be used as an inspiration for best practices.

* https://github.com/CapelliC/loqt

These are some interesting stackoverflow discussions, and CapelliC is very active also here.

* https://stackoverflow.com/questions/24642031/list-processing-in-prolog-using-a-c-interface
* https://stackoverflow.com/questions/28054166/integrate-prolog-in-c-program
* https://stackoverflow.com/users/874024/capellic

This is the Prolog manual with instructions on the embedding of prolog code in other applications, and the linking with SWI prolog engine.

* https://www.swi-prolog.org/pldoc/man?section=embedded
* https://www.swi-prolog.org/pldoc/man?section=plld

Other implementations of CLP(FD) exist for proprietary versions of prolog, such as SICStus Prolog.

* https://github.com/triska/clpz
