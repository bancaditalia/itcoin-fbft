
set (APPS_MODULE_PATH "${PROJECT_SOURCE_DIR}/apps")

# ------------------------------------------------------------------------------
# main
# ------------------------------------------------------------------------------
set(APP_MAIN_NAME main)
add_executable(${APP_MAIN_NAME}
    main.cpp
    ../src/transport/zcomm.cpp
)
target_link_libraries(${APP_MAIN_NAME}
    PRIVATE
    ${LIB_ITCOIN_PBFT}
    ${THIRDPARTY_LIBRARIES}
    ${SQLite3_LIBRARIES}
    ${OPENSSL_LIBRARIES}
    ${CURL_LIBRARIES}
    zmq # TODO: use find_package(ZeroMQ REQUIRED) instead
    Threads::Threads
)
target_include_directories(${APP_MAIN_NAME}
    PRIVATE
    ${THIRDPARTY_INCLUDE_PATH}
    ${LIB_ITCOIN_PBFT_INCLUDE_PATH}
    ${GENERATED_INCLUDE_DIR}
)


# ------------------------------------------------------------------------------
# Test targets
# ------------------------------------------------------------------------------
set(APP_MAIN_TEST_NAME main-test)
add_executable(${APP_MAIN_TEST_NAME} main-test.cpp)
target_link_libraries(${APP_MAIN_TEST_NAME}
    ${LIB_ITCOIN_PBFT}
    ${THIRDPARTY_LIBRARIES}
    ${SQLite3_LIBRARIES}
    ${OPENSSL_LIBRARIES}
    ${CURL_LIBRARIES}
    Threads::Threads
)
target_include_directories(${APP_MAIN_TEST_NAME}
    PRIVATE
    ${THIRDPARTY_INCLUDE_PATH}
    ${LIB_ITCOIN_PBFT_INCLUDE_PATH}
    ${GENERATED_INCLUDE_DIR}
)

foreach(test_src ${TEST_SOURCE_FILES})
    # Extract the filename without an extension (NAME_WE)
    get_filename_component(test_name ${test_src} NAME_WE)

    # Finally add it to test execution -
    # Notice the WORKING_DIRECTORY and COMMAND
    add_test(NAME ${test_name}
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
        COMMAND ${CMAKE_CURRENT_BINARY_DIR}/${APP_MAIN_TEST_NAME} --run_test=${test_name} )

    set_property(TEST ${test_name} PROPERTY ENVIRONMENT "LD_LIBRARY_PATH=${LD_LIBRARY_PATH}:${USR_LIB_DIR}")
endforeach(test_src)

add_subdirectory(frost)
