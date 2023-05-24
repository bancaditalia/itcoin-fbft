# Copyright (c) 2023 Bank of Italy
# Distributed under the GNU AGPLv3 software license, see the accompanying COPYING file.

set(THIS_DIR ${ROOT_DIR}/engine)

#
# Prolog fbft engine
#

file(MAKE_DIRECTORY ${GENERATED_OUTPUT_DIR}/prolog_fbft_engine)
set (GEN_PROLOG_FBFT_ENGINE_OUTPUT ${GENERATED_OUTPUT_DIR}/prolog_fbft_engine/resource_db_mem.h)
add_custom_command(
    OUTPUT ${GEN_PROLOG_FBFT_ENGINE_OUTPUT}
    COMMAND ${LIB_SWI_PROLOG_SWIPL} -f none -F none -g true -t "\"consult(['${THIS_DIR}/fbft-replica-engine.pl']),qsave_program('resource_db_mem.bin',[goal=version,toplevel=prolog,init_file=none])\""
    COMMAND xxd -i resource_db_mem.bin > ${GEN_PROLOG_FBFT_ENGINE_OUTPUT}
    COMMAND rm resource_db_mem.bin
    WORKING_DIRECTORY ${GENERATED_OUTPUT_DIR}/prolog_fbft_engine
    DEPENDS ${THIS_DIR}/fbft-replica-engine.pl SWI-Prolog
)
add_custom_target (generated_engine DEPENDS ${GEN_PROLOG_FBFT_ENGINE_OUTPUT})

#
# Prolog fbft engine tests
#

set (PL_TEST_SOURCE_FILES
  test/test_00_example.pl
  test/test_01_fbft_normal_operation_happy_path.pl
  test/test_03_normal_operation_with_replica_on_previous_view.pl
  test/test_04_normal_operation_after_view_recovery.pl
  test/test_action_01_receive_request.pl
  test/test_action_02_send_pre_prepare.pl
  test/test_action_03_receive_pre_prepare.pl
  test/test_action_04_send_prepare.pl
  test/test_action_05_receive_prepare.pl
  test/test_action_06_send_commit.pl
  test/test_action_07_receive_commit.pl
  test/test_02_fbft_view_change.pl
  test/test_action_12_send_view_change_00.pl
  test/test_action_12_send_view_change_01.pl
  test/test_action_13_receive_view_change.pl
  test/test_action_15_send_new_view.pl
  test/test_action_16_receive_new_view.pl
  test/test_action_20_process_new_view.pl
  test/test_aux_05_pre_prepared.pl
  test/test_aux_06_prepared.pl
  test/test_aux_07_committed.pl
  test/test_aux_08_correct_view_change.pl
  test/test_aux_10_correct_nu.pl
  test/test_aux_11_correct_chi.pl
  test/test_aux_12_correct_new_view.pl
  test/test_viewchange_01_empty.pl
  test/test_viewchange_02_with_prepared_messages.pl
  test/test_viewchange_05_dead_primary_empty.pl
  test/test_viewchange_05_dead_primary_prepared.pl
)

foreach(pl_test_src ${PL_TEST_SOURCE_FILES})
  # Extract the filename without an extension (NAME_WE)
  get_filename_component(test_name ${pl_test_src} NAME_WE)

  add_test(NAME ${test_name}
      WORKING_DIRECTORY ${THIS_DIR}
      COMMAND ${LIB_SWI_PROLOG_SWIPL} -t run_tests  ${pl_test_src} )
endforeach(pl_test_src)
