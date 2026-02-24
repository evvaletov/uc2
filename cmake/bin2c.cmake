# bin2c.cmake — Convert a binary file to a C array (MSVC fallback for .incbin).
# Usage: cmake -DINPUT=<file> -DOUTPUT=<file.c> -P bin2c.cmake

file(READ "${INPUT}" DATA HEX)
string(LENGTH "${DATA}" DATA_LEN)
math(EXPR NBYTES "${DATA_LEN} / 2")

set(OUT "/* Generated from super.bin -- do not edit */\n")
string(APPEND OUT "#include <stdint.h>\n\n")
string(APPEND OUT "const uint8_t uc2_supermaster_compressed[${NBYTES}] = {\n")

set(POS 0)
while(POS LESS DATA_LEN)
    math(EXPR LINE_END "${POS} + 32")
    if(LINE_END GREATER DATA_LEN)
        set(LINE_END ${DATA_LEN})
    endif()
    string(SUBSTRING "${DATA}" ${POS} 2 BYTE)
    string(APPEND OUT "    0x${BYTE}")
    math(EXPR POS "${POS} + 2")
    while(POS LESS LINE_END)
        string(SUBSTRING "${DATA}" ${POS} 2 BYTE)
        string(APPEND OUT ",0x${BYTE}")
        math(EXPR POS "${POS} + 2")
    endwhile()
    if(POS LESS DATA_LEN)
        string(APPEND OUT ",")
    endif()
    string(APPEND OUT "\n")
endwhile()

string(APPEND OUT "};\n\n")
string(APPEND OUT "/* uc2_supermaster_compressed_end: pointer to one past the last byte.\n")
string(APPEND OUT "   decompress.c declares this as extern u8[], so we define it as an\n")
string(APPEND OUT "   array at the correct address using a linker section. */\n")
string(APPEND OUT "const unsigned int uc2_supermaster_compressed_size = ${NBYTES};\n")

file(WRITE "${OUTPUT}" "${OUT}")
