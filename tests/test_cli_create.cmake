# CLI create/extract round-trip test
# Creates test files, archives them, extracts, and verifies byte identity.

file(REMOVE_RECURSE "${TEST_DIR}")
file(MAKE_DIRECTORY "${TEST_DIR}/input" "${TEST_DIR}/output")

# Generate test files
file(WRITE "${TEST_DIR}/input/hello.txt" "Hello, UC2!\n")
file(WRITE "${TEST_DIR}/input/empty.dat" "")

string(REPEAT "The quick brown fox jumps over the lazy dog.\n" 100 REPEATED)
file(WRITE "${TEST_DIR}/input/repeated.txt" "${REPEATED}")

# Binary pattern (255 bytes, values 1..255 — avoid null which CMake can't handle)
foreach(i RANGE 1 255)
    string(ASCII ${i} CH)
    string(APPEND BINARY_DATA "${CH}")
endforeach()
file(WRITE "${TEST_DIR}/input/binary.dat" "${BINARY_DATA}")

# Create archive
execute_process(
    COMMAND "${UC2_CLI}" -w "${TEST_DIR}/test.uc2"
        "${TEST_DIR}/input/hello.txt"
        "${TEST_DIR}/input/empty.dat"
        "${TEST_DIR}/input/repeated.txt"
        "${TEST_DIR}/input/binary.dat"
    RESULT_VARIABLE RC
)
if(NOT RC EQUAL 0)
    message(FATAL_ERROR "uc2 -w failed: ${RC}")
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
foreach(F hello.txt empty.dat repeated.txt binary.dat)
    file(READ "${TEST_DIR}/input/${F}" ORIGINAL)
    file(READ "${TEST_DIR}/output/${F}" EXTRACTED)
    if(NOT "${ORIGINAL}" STREQUAL "${EXTRACTED}")
        message(FATAL_ERROR "${F}: content mismatch after round-trip")
    endif()
endforeach()

message(STATUS "cli_create: all files verified")
