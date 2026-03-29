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

# CLI directory archival round-trip test
add_test(NAME cli_dirs
    COMMAND ${CMAKE_COMMAND}
        -DUC2_CLI=$<TARGET_FILE:uc2-cli>
        -DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/cli_dirs_test
        -P ${CMAKE_CURRENT_SOURCE_DIR}/test_cli_dirs.cmake
)

add_executable(test_cdc src/test_cdc.c)
target_link_libraries(test_cdc PRIVATE uc2)
target_include_directories(test_cdc PRIVATE "${PROJECT_BINARY_DIR}/lib")
target_compile_features(test_cdc PRIVATE c_std_99)
add_test(NAME cdc COMMAND test_cdc)

add_executable(test_merkle src/test_merkle.c)
target_link_libraries(test_merkle PRIVATE uc2 m)
target_include_directories(test_merkle PRIVATE "${PROJECT_BINARY_DIR}/lib")
target_compile_features(test_merkle PRIVATE c_std_99)
add_test(NAME merkle COMMAND test_merkle)

# Cross-tool round-trip: UC2 v3 <-> original uc2pro.exe via DOSBox-X
add_test(NAME roundtrip_dosbox
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/scripts/roundtrip_dosbox.sh
        $<TARGET_FILE:uc2-cli>
        ${CMAKE_CURRENT_SOURCE_DIR}/../original/UC2_source/uc2pro.exe
        ${CMAKE_CURRENT_SOURCE_DIR}/corpus
)
set_tests_properties(roundtrip_dosbox PROPERTIES
    LABELS "dosbox"
    TIMEOUT 1200
)
