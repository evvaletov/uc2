# Optional libarchive read-format plugin for UC2.
#
# Builds only when -DUC2_BUILD_LIBARCHIVE_PLUGIN=ON is passed and a
# system libarchive-devel is found.  See contrib/libarchive/README.md
# for the integration plan.

option(UC2_BUILD_LIBARCHIVE_PLUGIN
       "Build the libarchive read-format plugin (skeleton)" OFF)

if(NOT UC2_BUILD_LIBARCHIVE_PLUGIN)
    return()
endif()

find_package(LibArchive)
if(NOT LibArchive_FOUND)
    message(WARNING
        "UC2_BUILD_LIBARCHIVE_PLUGIN=ON but LibArchive headers were "
        "not found.  Install libarchive-devel (Fedora/RHEL) or "
        "libarchive-dev (Debian/Ubuntu), or set LibArchive_INCLUDE_DIR "
        "manually.  Skipping plugin build.")
    return()
endif()

add_library(uc2_libarchive STATIC archive_read_support_format_uc2.c)
target_link_libraries(uc2_libarchive PRIVATE uc2 ${LibArchive_LIBRARIES})
target_include_directories(uc2_libarchive
    PRIVATE ${LibArchive_INCLUDE_DIRS}
    PUBLIC  ${PROJECT_SOURCE_DIR}/lib/include)
target_compile_features(uc2_libarchive PRIVATE c_std_99)

message(STATUS "UC2: libarchive plugin build enabled (skeleton)")
