# CLI master-block deduplication round-trip test.
# Creates multiple files with identical first 4096 bytes (triggering
# fingerprint grouping) and verifies they survive a create/extract cycle.

file(REMOVE_RECURSE "${TEST_DIR}")
file(MAKE_DIRECTORY "${TEST_DIR}/input" "${TEST_DIR}/output")

# Common header: 4500 bytes of repeated text (exceeds 4096-byte sample window)
string(REPEAT "Master-block deduplication test data header line.\n" 90 HEADER)

# variant_a.txt: header + unique tail A
string(REPEAT "AAAA unique tail for variant A, differs from B and C.\n" 60 TAIL_A)
file(WRITE "${TEST_DIR}/input/variant_a.txt" "${HEADER}${TAIL_A}")

# variant_b.txt: header + unique tail B
string(REPEAT "BBBB unique tail for variant B, differs from A and C.\n" 40 TAIL_B)
file(WRITE "${TEST_DIR}/input/variant_b.txt" "${HEADER}${TAIL_B}")

# variant_c.txt: header + unique tail C (largest → becomes master source)
string(REPEAT "CCCC unique tail for variant C, differs from A and B.\n" 100 TAIL_C)
file(WRITE "${TEST_DIR}/input/variant_c.txt" "${HEADER}${TAIL_C}")

# unrelated.txt: different header, should NOT be grouped
string(REPEAT "Completely different file unrelated to the master group.\n" 80 UNREL)
file(WRITE "${TEST_DIR}/input/unrelated.txt" "${UNREL}")

# small.txt: below MinMasterFile (< 1024 bytes), should use SuperMaster
file(WRITE "${TEST_DIR}/input/small.txt" "Short file, no custom master.\n")

# Create archive (capture stderr for master diagnostics)
execute_process(
    COMMAND "${UC2_CLI}" -w "${TEST_DIR}/test.uc2"
        "${TEST_DIR}/input/variant_a.txt"
        "${TEST_DIR}/input/variant_b.txt"
        "${TEST_DIR}/input/variant_c.txt"
        "${TEST_DIR}/input/unrelated.txt"
        "${TEST_DIR}/input/small.txt"
    RESULT_VARIABLE RC
    ERROR_VARIABLE STDERR
)
if(NOT RC EQUAL 0)
    message(FATAL_ERROR "uc2 -w failed: ${RC}")
endif()
message(STATUS "Compression log:\n${STDERR}")

# Verify a custom master was created (stderr should mention "master[2]")
string(FIND "${STDERR}" "master[2]" HAS_MASTER)
if(HAS_MASTER EQUAL -1)
    message(FATAL_ERROR "Expected custom master but none was created")
endif()

# Verify custom-master files are flagged in output
string(FIND "${STDERR}" "custom master" HAS_CM_TAG)
if(HAS_CM_TAG EQUAL -1)
    message(FATAL_ERROR "Expected '(custom master)' tag in compression output")
endif()

# List archive
execute_process(
    COMMAND "${UC2_CLI}" -l "${TEST_DIR}/test.uc2"
    OUTPUT_VARIABLE LISTING
    RESULT_VARIABLE RC
)
if(NOT RC EQUAL 0)
    message(FATAL_ERROR "uc2 -l failed: ${RC}")
endif()
message(STATUS "Archive listing:\n${LISTING}")

# Extract
execute_process(
    COMMAND "${UC2_CLI}" -d "${TEST_DIR}/output" "${TEST_DIR}/test.uc2"
    RESULT_VARIABLE RC
)
if(NOT RC EQUAL 0)
    message(FATAL_ERROR "uc2 extract failed: ${RC}")
endif()

# Verify each file
foreach(F variant_a.txt variant_b.txt variant_c.txt unrelated.txt small.txt)
    file(READ "${TEST_DIR}/input/${F}" ORIGINAL)
    file(READ "${TEST_DIR}/output/${F}" EXTRACTED)
    if(NOT "${ORIGINAL}" STREQUAL "${EXTRACTED}")
        message(FATAL_ERROR "${F}: content mismatch after round-trip")
    endif()
endforeach()

message(STATUS "cli_master: all files verified (custom master deduplication)")
