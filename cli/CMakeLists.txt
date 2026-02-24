# uc2 command-line tool

add_executable(uc2-cli src/main.c)
set_target_properties(uc2-cli PROPERTIES OUTPUT_NAME uc2)

target_link_libraries(uc2-cli PRIVATE uc2)

# list.h is in the library's src directory
target_include_directories(uc2-cli PRIVATE
    "${PROJECT_SOURCE_DIR}/lib/src"
    "${PROJECT_BINARY_DIR}/lib"  # for configured uc2_version.h
)

target_compile_features(uc2-cli PRIVATE c_std_99)

if(WIN32)
    target_sources(uc2-cli PRIVATE src/compat/compat_win32.c)
    target_include_directories(uc2-cli PRIVATE src/compat/include)
    target_compile_definitions(uc2-cli PRIVATE
        g_err g_errx g_warn g_warnx g_vwarn g_vwarnx g_verr g_verrx
        g_getprogname g_setlinebuf g_fnmatch
        g_compat__utf8_console g_compat__wpath g_fopen
        g_access g_mkdir g_utime
    )
endif()
