# SPDX-License-Identifier: GPL-3.0-or-later OR BSD-2-Clause
find_package(Python REQUIRED COMPONENTS Interpreter Development.Module)
Python_add_library(_native MODULE WITH_SOABI native.c)
target_link_libraries(_native PRIVATE uc2)
target_compile_features(_native PRIVATE c_std_99)
set_target_properties(_native PROPERTIES C_VISIBILITY_PRESET hidden)
install(TARGETS _native
        LIBRARY DESTINATION uc2 COMPONENT python
        RUNTIME DESTINATION uc2 COMPONENT python)
