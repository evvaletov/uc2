# CLI round-trip test for files larger than the 64KB sliding window.
# Regression test for the window-edge bugs fixed 2026-06-11 (git-bug
# d747658): linear chunk loads crossing the circular-buffer edge
# corrupted archives at every level, and the rANS decoder could not
# flush more than one window of output.  Content must be varied, not
# uniform: all-zeros hides reordering corruption.

file(REMOVE_RECURSE "${TEST_DIR}")
file(MAKE_DIRECTORY "${TEST_DIR}/input")

# ~200KB of deterministic varied text (seeded string(RANDOM)), with a
# compressible refrain so both literal and match paths are exercised.
set(BIG "")
foreach(i RANGE 1 180)
    string(RANDOM LENGTH 1024 RANDOM_SEED ${i} CHUNK)
    string(APPEND BIG "${CHUNK}\nThe quick brown fox jumps over the lazy dog ${i}\n")
endforeach()
file(WRITE "${TEST_DIR}/input/bigfile.txt" "${BIG}")

foreach(LEVEL 5 6)
    set(WORK "${TEST_DIR}/L${LEVEL}")
    file(MAKE_DIRECTORY "${WORK}/output")

    execute_process(
        COMMAND "${UC2_CLI}" -q -w -L ${LEVEL} "${WORK}/big.uc2"
            "${TEST_DIR}/input/bigfile.txt"
        RESULT_VARIABLE RC
    )
    if(NOT RC EQUAL 0)
        message(FATAL_ERROR "uc2 -w -L ${LEVEL} failed: ${RC}")
    endif()

    execute_process(
        COMMAND "${UC2_CLI}" -q -d "${WORK}/output" "${WORK}/big.uc2"
        RESULT_VARIABLE RC
    )
    if(NOT RC EQUAL 0)
        message(FATAL_ERROR "uc2 -d failed at -L ${LEVEL}: ${RC}")
    endif()

    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E compare_files
            "${TEST_DIR}/input/bigfile.txt" "${WORK}/output/bigfile.txt"
        RESULT_VARIABLE RC
    )
    if(NOT RC EQUAL 0)
        message(FATAL_ERROR "bigfile round-trip mismatch at -L ${LEVEL}")
    endif()

    message(STATUS "bigfile round-trip OK at -L ${LEVEL}")
endforeach()
