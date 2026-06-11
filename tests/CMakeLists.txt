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

# CLI >64KB round-trip test (sliding-window edge regression, git-bug d747658)
add_test(NAME cli_bigfile
    COMMAND ${CMAKE_COMMAND}
        -DUC2_CLI=$<TARGET_FILE:uc2-cli>
        -DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/cli_bigfile_test
        -P ${CMAKE_CURRENT_SOURCE_DIR}/test_cli_bigfile.cmake
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
if(WIN32)
    target_link_libraries(test_merkle PRIVATE uc2)
else()
    target_link_libraries(test_merkle PRIVATE uc2 m)
endif()
target_include_directories(test_merkle PRIVATE "${PROJECT_BINARY_DIR}/lib")
target_compile_features(test_merkle PRIVATE c_std_99)
add_test(NAME merkle COMMAND test_merkle)

add_executable(test_blockstore src/test_blockstore.c)
target_link_libraries(test_blockstore PRIVATE uc2)
target_include_directories(test_blockstore PRIVATE "${PROJECT_BINARY_DIR}/lib")
target_compile_features(test_blockstore PRIVATE c_std_99)
add_test(NAME blockstore COMMAND test_blockstore)

add_executable(test_simhash src/test_simhash.c)
target_link_libraries(test_simhash PRIVATE uc2)
target_include_directories(test_simhash PRIVATE "${PROJECT_BINARY_DIR}/lib")
target_compile_features(test_simhash PRIVATE c_std_99)
add_test(NAME simhash COMMAND test_simhash)

add_executable(test_delta src/test_delta.c)
target_link_libraries(test_delta PRIVATE uc2)
target_include_directories(test_delta PRIVATE "${PROJECT_BINARY_DIR}/lib")
target_compile_features(test_delta PRIVATE c_std_99)
add_test(NAME delta COMMAND test_delta)

add_executable(test_rans src/test_rans.c)
if(WIN32)
    target_link_libraries(test_rans PRIVATE uc2)
else()
    target_link_libraries(test_rans PRIVATE uc2 m)
endif()
target_include_directories(test_rans PRIVATE "${PROJECT_BINARY_DIR}/lib")
target_compile_features(test_rans PRIVATE c_std_99)
add_test(NAME rans COMMAND test_rans)

add_executable(test_dict src/test_dict.c)
target_link_libraries(test_dict PRIVATE uc2)
target_include_directories(test_dict PRIVATE "${PROJECT_BINARY_DIR}/lib")
target_compile_features(test_dict PRIVATE c_std_99)
add_test(NAME dict COMMAND test_dict)

add_executable(test_preprocess src/test_preprocess.c)
target_link_libraries(test_preprocess PRIVATE uc2)
target_include_directories(test_preprocess PRIVATE "${PROJECT_BINARY_DIR}/lib")
target_compile_features(test_preprocess PRIVATE c_std_99)
add_test(NAME preprocess COMMAND test_preprocess)

add_executable(test_lz4 src/test_lz4.c)
target_link_libraries(test_lz4 PRIVATE uc2)
target_include_directories(test_lz4 PRIVATE "${PROJECT_BINARY_DIR}/lib")
target_compile_features(test_lz4 PRIVATE c_std_99)
add_test(NAME lz4 COMMAND test_lz4)

add_executable(test_blake3 src/test_blake3.c)
target_link_libraries(test_blake3 PRIVATE uc2)
target_include_directories(test_blake3 PRIVATE "${PROJECT_BINARY_DIR}/lib")
target_compile_features(test_blake3 PRIVATE c_std_99)
add_test(NAME blake3 COMMAND test_blake3)

add_executable(test_sha256 src/test_sha256.c)
target_link_libraries(test_sha256 PRIVATE uc2)
target_include_directories(test_sha256 PRIVATE "${PROJECT_BINARY_DIR}/lib")
target_compile_features(test_sha256 PRIVATE c_std_99)
add_test(NAME sha256 COMMAND test_sha256)

add_executable(test_ots src/test_ots.c)
target_link_libraries(test_ots PRIVATE uc2)
target_include_directories(test_ots PRIVATE "${PROJECT_BINARY_DIR}/lib")
target_compile_features(test_ots PRIVATE c_std_99)
add_test(NAME ots COMMAND test_ots)

add_executable(test_ingest src/test_ingest.c)
target_link_libraries(test_ingest PRIVATE uc2)
target_include_directories(test_ingest PRIVATE "${PROJECT_BINARY_DIR}/lib")
target_compile_features(test_ingest PRIVATE c_std_99)
add_test(NAME ingest COMMAND test_ingest)

# Optional cross-check: validates uc2 .ots output against the python-opentimestamps
# reference parser.  Skipped (return code 77) when opentimestamps is not installed.
find_package(Python3 COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND)
    add_test(NAME ots_cross_check
        COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/scripts/cross_check_ots.py
            $<TARGET_FILE:uc2-cli>
            ${CMAKE_CURRENT_BINARY_DIR}/ots_cross_check
    )
    set_tests_properties(ots_cross_check PROPERTIES
        SKIP_RETURN_CODE 77
        LABELS "optional"
    )
endif()

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
