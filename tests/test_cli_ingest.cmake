# CLI --ingest / --ingest-restore round-trip test (git-bug 59bec0d).
# Ingests a byte stream into a real UC2 v3 archive, verifies it lists and
# restores byte-for-byte, and that duplicate content is deduplicated.

file(REMOVE_RECURSE "${TEST_DIR}")
file(MAKE_DIRECTORY "${TEST_DIR}")

# A 256 KiB stream that is a 32-byte unit repeated.  The stream is fully
# periodic with period 32, so every CDC boundary lands at the same phase
# and the interior chunks are byte-identical -- guaranteeing dedup, not
# just compressibility.
set(UNIT "UC2-ingest_dedup_test_0123456789")   # exactly 32 bytes
string(REPEAT "${UNIT}" 8192 STREAM)           # 256 KiB
file(WRITE "${TEST_DIR}/stream.bin" "${STREAM}")
file(SIZE "${TEST_DIR}/stream.bin" STREAM_SIZE)

# Ingest (stdin -> archive); capture the stats line from stderr.
execute_process(
    COMMAND "${UC2_CLI}" --ingest "${TEST_DIR}/ing.uc2"
    INPUT_FILE "${TEST_DIR}/stream.bin"
    ERROR_VARIABLE INGEST_MSG
    RESULT_VARIABLE RC
)
if(NOT RC EQUAL 0)
    message(FATAL_ERROR "uc2 --ingest failed: ${RC}")
endif()
message(STATUS "Ingest: ${INGEST_MSG}")

# Dedup must actually happen: the stats line must report a non-zero
# deduplicated-chunk count.
if(NOT INGEST_MSG MATCHES "([0-9]+) deduped")
    message(FATAL_ERROR "ingest stats line missing dedup count:\n${INGEST_MSG}")
endif()
if(NOT CMAKE_MATCH_1 GREATER 0)
    message(FATAL_ERROR "expected dedup on periodic input, got 0 deduped")
endif()

# The output must be a real UC2 archive (uc2 -l lists it)
execute_process(
    COMMAND "${UC2_CLI}" -l "${TEST_DIR}/ing.uc2"
    OUTPUT_VARIABLE LISTING
    RESULT_VARIABLE RC
)
if(NOT RC EQUAL 0)
    message(FATAL_ERROR "uc2 -l on ingest archive failed: ${RC}")
endif()
if(NOT LISTING MATCHES "part-0")
    message(FATAL_ERROR "ingest archive listing has no part entries:\n${LISTING}")
endif()
message(STATUS "Ingest listing:\n${LISTING}")

# Dedup: 16 identical blocks must compress to well under half the input
file(SIZE "${TEST_DIR}/ing.uc2" ARCHIVE_SIZE)
math(EXPR HALF "${STREAM_SIZE} / 2")
if(NOT ARCHIVE_SIZE LESS HALF)
    message(FATAL_ERROR
        "no dedup: archive ${ARCHIVE_SIZE} B >= half of input ${STREAM_SIZE} B")
endif()
message(STATUS "Dedup: ${STREAM_SIZE} B stream -> ${ARCHIVE_SIZE} B archive")

# Restore (archive -> stdout) and verify byte identity
execute_process(
    COMMAND "${UC2_CLI}" --ingest-restore "${TEST_DIR}/ing.uc2"
    OUTPUT_FILE "${TEST_DIR}/restored.bin"
    RESULT_VARIABLE RC
)
if(NOT RC EQUAL 0)
    message(FATAL_ERROR "uc2 --ingest-restore failed: ${RC}")
endif()
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E compare_files
        "${TEST_DIR}/stream.bin" "${TEST_DIR}/restored.bin"
    RESULT_VARIABLE RC
)
if(NOT RC EQUAL 0)
    message(FATAL_ERROR "restored stream differs from the original")
endif()

# Extract to disk and confirm the parts concatenate back to the stream
file(MAKE_DIRECTORY "${TEST_DIR}/out")
execute_process(
    COMMAND "${UC2_CLI}" -d "${TEST_DIR}/out" "${TEST_DIR}/ing.uc2"
    RESULT_VARIABLE RC
)
if(NOT RC EQUAL 0)
    message(FATAL_ERROR "uc2 extract of ingest archive failed: ${RC}")
endif()
file(GLOB PARTS "${TEST_DIR}/out/part-*")
list(SORT PARTS)
set(CAT "")
foreach(P ${PARTS})
    file(READ "${P}" CHUNK)
    string(APPEND CAT "${CHUNK}")
endforeach()
file(WRITE "${TEST_DIR}/cat.bin" "${CAT}")
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E compare_files
        "${TEST_DIR}/stream.bin" "${TEST_DIR}/cat.bin"
    RESULT_VARIABLE RC
)
if(NOT RC EQUAL 0)
    message(FATAL_ERROR "concatenated parts differ from the original stream")
endif()

# Empty stream must produce a valid archive that restores to nothing
file(WRITE "${TEST_DIR}/empty.bin" "")
execute_process(
    COMMAND "${UC2_CLI}" --ingest "${TEST_DIR}/empty.uc2"
    INPUT_FILE "${TEST_DIR}/empty.bin"
    RESULT_VARIABLE RC
)
if(NOT RC EQUAL 0)
    message(FATAL_ERROR "uc2 --ingest of empty stream failed: ${RC}")
endif()
execute_process(
    COMMAND "${UC2_CLI}" --ingest-restore "${TEST_DIR}/empty.uc2"
    OUTPUT_FILE "${TEST_DIR}/empty.out"
    RESULT_VARIABLE RC
)
if(NOT RC EQUAL 0)
    message(FATAL_ERROR "uc2 --ingest-restore of empty archive failed: ${RC}")
endif()
file(SIZE "${TEST_DIR}/empty.out" EMPTY_SIZE)
if(NOT EMPTY_SIZE EQUAL 0)
    message(FATAL_ERROR "empty stream restored to ${EMPTY_SIZE} bytes, expected 0")
endif()

message(STATUS "cli_ingest: round-trip, dedup, extract, and empty-stream all verified")
