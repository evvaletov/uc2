# Round-trip test for the libarchive UC2 read plugin: the uc2 CLI
# creates archives (Huffman and rANS), then test_libarchive_uc2 reads
# them back through libarchive's public API and verifies every byte.

file(REMOVE_RECURSE "${TEST_DIR}")
file(MAKE_DIRECTORY "${TEST_DIR}/input/subdir")

file(WRITE "${TEST_DIR}/input/hello.txt" "Hello from libarchive!\n")
string(REPEAT "The quick brown fox jumps over the lazy dog.\n" 200 REPEATED)
file(WRITE "${TEST_DIR}/input/repeated.txt" "${REPEATED}")
string(RANDOM LENGTH 8192 RANDOM_SEED 99 BLOB)
file(WRITE "${TEST_DIR}/input/blob.dat" "${BLOB}")
file(WRITE "${TEST_DIR}/input/subdir/nested_long_file_name.txt"
     "nested content with a long name\n")
file(WRITE "${TEST_DIR}/input/empty.dat" "")

foreach(LEVEL 4 6)
    set(ARCHIVE "${TEST_DIR}/la${LEVEL}.uc2")
    execute_process(
        COMMAND "${UC2_CLI}" -q -w -L ${LEVEL} "${ARCHIVE}"
            hello.txt repeated.txt blob.dat empty.dat subdir
        WORKING_DIRECTORY "${TEST_DIR}/input"
        RESULT_VARIABLE RC
    )
    if(NOT RC EQUAL 0)
        message(FATAL_ERROR "uc2 -w -L ${LEVEL} failed: ${RC}")
    endif()

    execute_process(
        COMMAND "${LA_TEST}" "${ARCHIVE}" "${TEST_DIR}/input"
        RESULT_VARIABLE RC
        OUTPUT_VARIABLE OUT
        ERROR_VARIABLE OUT
    )
    message(STATUS "L${LEVEL}: ${OUT}")
    if(NOT RC EQUAL 0)
        message(FATAL_ERROR "libarchive round-trip failed at -L ${LEVEL}")
    endif()
endforeach()
