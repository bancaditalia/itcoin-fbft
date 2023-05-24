#!/usr/bin/env bash

# https://stackoverflow.com/questions/59895/how-can-i-get-the-directory-where-a-bash-script-is-located-from-within-the-scrip#246128
MY_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

cd "${MY_DIR}/../test_pl"
"${MY_DIR}/../build/usrlocal/bin/swipl" -t run_tests               \
    test_00_example.pl                                             \
    test_01_pbft_normal_operation_happy_path.pl                    \
    test_03_normal_operation_with_replica_on_previous_view.pl      \
    test_04_normal_operation_after_view_recovery.pl                \
    test_action_01_receive_request.pl                              \
    test_action_02_send_pre_prepare.pl                             \
    test_action_03_receive_pre_prepare.pl                          \
    test_action_04_send_prepare.pl                                 \
    test_action_05_receive_prepare.pl                              \
    test_action_06_send_commit.pl                                  \
    test_action_07_receive_commit.pl                               \
    test_02_pbft_view_change.pl                                    \
    test_action_12_send_view_change_00.pl                          \
    test_action_12_send_view_change_01.pl                          \
    test_action_13_receive_view_change.pl                          \
    test_action_15_send_new_view.pl                                \
    test_action_16_receive_new_view.pl                             \
    test_action_20_process_new_view.pl                             \
    test_aux_05_pre_prepared.pl                                    \
    test_aux_06_prepared.pl                                        \
    test_aux_07_committed.pl                                       \
    test_aux_08_correct_view_change.pl                             \
    test_aux_10_correct_nu.pl                                      \
    test_aux_11_correct_chi.pl                                     \
    test_aux_12_correct_new_view.pl                                \
    test_viewchange_01_empty.pl                                    \
    test_viewchange_02_with_prepared_messages.pl                   \
    test_viewchange_05_dead_primary_empty.pl                       \
    test_viewchange_05_dead_primary_prepared.pl
