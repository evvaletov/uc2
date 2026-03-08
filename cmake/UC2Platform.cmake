# Platform detection and compiler flags for UC2

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wno-unused-parameter)
    if(DJGPP)
        # DJGPP needs gnu99 for PATH_MAX and other POSIX extensions
        add_compile_options(-std=gnu99)
    endif()
elseif(MSVC)
    add_compile_options(/W3)
endif()
