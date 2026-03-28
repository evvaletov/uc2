# CLI directory archival round-trip test.
# Creates a nested directory structure, archives it, extracts, and
# verifies that both directory hierarchy and file contents survive.

file(REMOVE_RECURSE "${TEST_DIR}")
file(MAKE_DIRECTORY "${TEST_DIR}/input/src/lib"
                    "${TEST_DIR}/input/src/include"
                    "${TEST_DIR}/input/docs"
                    "${TEST_DIR}/output")

# Files in root
file(WRITE "${TEST_DIR}/input/README.txt" "Project readme\n")
file(WRITE "${TEST_DIR}/input/Makefile" "all: build\n")

# Files in src/
file(WRITE "${TEST_DIR}/input/src/main.c" "#include <stdio.h>\nint main() { return 0; }\n")

# Files in src/lib/
file(WRITE "${TEST_DIR}/input/src/lib/util.c" "void util(void) {}\n")
file(WRITE "${TEST_DIR}/input/src/lib/util.h" "#pragma once\nvoid util(void);\n")

# Files in src/include/ (just a header)
file(WRITE "${TEST_DIR}/input/src/include/api.h" "#pragma once\n#define VERSION 1\n")

# Files in docs/
file(WRITE "${TEST_DIR}/input/docs/guide.txt" "User guide\n")

# Create archive from the whole tree (mix of files and dirs at top level)
execute_process(
    COMMAND "${UC2_CLI}" -w "${TEST_DIR}/test.uc2"
        "${TEST_DIR}/input/README.txt"
        "${TEST_DIR}/input/Makefile"
        "${TEST_DIR}/input/src"
        "${TEST_DIR}/input/docs"
    RESULT_VARIABLE RC
    ERROR_VARIABLE STDERR
)
if(NOT RC EQUAL 0)
    message(FATAL_ERROR "uc2 -w failed: ${RC}\n${STDERR}")
endif()
message(STATUS "Compression log:\n${STDERR}")

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

# Verify directory entries appear in listing
foreach(DIR "src/" "src/lib/" "src/include/" "docs/")
    string(FIND "${LISTING}" "${DIR}" HAS_DIR)
    if(HAS_DIR EQUAL -1)
        message(FATAL_ERROR "Missing directory '${DIR}' in listing")
    endif()
endforeach()

# Extract
execute_process(
    COMMAND "${UC2_CLI}" -d "${TEST_DIR}/output" "${TEST_DIR}/test.uc2"
    RESULT_VARIABLE RC
)
if(NOT RC EQUAL 0)
    message(FATAL_ERROR "uc2 extract failed: ${RC}")
endif()

# Verify all files
foreach(F
    README.txt
    Makefile
    src/main.c
    src/lib/util.c
    src/lib/util.h
    src/include/api.h
    docs/guide.txt
)
    file(READ "${TEST_DIR}/input/${F}" ORIGINAL)
    file(READ "${TEST_DIR}/output/${F}" EXTRACTED)
    if(NOT "${ORIGINAL}" STREQUAL "${EXTRACTED}")
        message(FATAL_ERROR "${F}: content mismatch after round-trip")
    endif()
endforeach()

# Verify directories exist
foreach(D src src/lib src/include docs)
    if(NOT IS_DIRECTORY "${TEST_DIR}/output/${D}")
        message(FATAL_ERROR "${D}: directory not created on extraction")
    endif()
endforeach()

message(STATUS "cli_dirs: all files and directories verified")
