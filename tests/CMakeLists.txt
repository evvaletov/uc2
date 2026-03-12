# UC2 tests

add_executable(test_identify src/test_identify.c)
target_link_libraries(test_identify PRIVATE uc2)
target_include_directories(test_identify PRIVATE "${PROJECT_BINARY_DIR}/lib")
target_compile_features(test_identify PRIVATE c_std_99)
add_test(NAME identify COMMAND test_identify)

add_executable(test_extract src/test_extract.c)
target_link_libraries(test_extract PRIVATE uc2)
target_include_directories(test_extract PRIVATE "${PROJECT_BINARY_DIR}/lib")
target_compile_features(test_extract PRIVATE c_std_99)
add_test(NAME extract COMMAND test_extract
    "${CMAKE_CURRENT_SOURCE_DIR}/archives"
    "${CMAKE_CURRENT_SOURCE_DIR}/corpus"
)

add_executable(test_roundtrip src/test_roundtrip.c)
target_link_libraries(test_roundtrip PRIVATE uc2)
target_include_directories(test_roundtrip PRIVATE "${PROJECT_BINARY_DIR}/lib")
target_compile_features(test_roundtrip PRIVATE c_std_99)
add_test(NAME roundtrip COMMAND test_roundtrip)

# CLI create/extract round-trip test
add_test(NAME cli_create
    COMMAND ${CMAKE_COMMAND}
        -DUC2_CLI=$<TARGET_FILE:uc2-cli>
        -DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/cli_test
        -P ${CMAKE_CURRENT_SOURCE_DIR}/test_cli_create.cmake
)

# CLI master-block deduplication round-trip test
add_test(NAME cli_master
    COMMAND ${CMAKE_COMMAND}
        -DUC2_CLI=$<TARGET_FILE:uc2-cli>
        -DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/cli_master_test
        -P ${CMAKE_CURRENT_SOURCE_DIR}/test_cli_master.cmake
)
