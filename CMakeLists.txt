cmake_minimum_required(VERSION 3.16)
project(ultc_system LANGUAGES C)

# ============================================================
# C 标准
# ============================================================
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# ============================================================
# 编译选项
# ============================================================
if(MSVC)
    add_compile_options(/W4 /utf-8)
else()
    add_compile_options(-Wall -Wextra -pedantic)
endif()

# ============================================================
# 静态库（所有模块实现）
# ============================================================
add_library(ultc_lib STATIC
    src/file_io.c
    src/ui.c
    src/platform.c
    src/auth.c
    src/material.c
    src/borrow.c
    src/inventory.c
    src/search.c
    src/stats.c
)
target_include_directories(ultc_lib PUBLIC include)

# ============================================================
# 主程序
# ============================================================
add_executable(ultc_system src/main.c)
target_link_libraries(ultc_system PRIVATE ultc_lib)

# ============================================================
# 测试程序
# ============================================================
add_executable(test_file_io tests/test_file_io.c)
target_link_libraries(test_file_io PRIVATE ultc_lib)

add_executable(test_auth tests/test_auth.c)
target_link_libraries(test_auth PRIVATE ultc_lib)

add_executable(test_material tests/test_material.c)
target_link_libraries(test_material PRIVATE ultc_lib)

add_executable(test_borrow tests/test_borrow.c)
target_link_libraries(test_borrow PRIVATE ultc_lib)

add_executable(test_inventory tests/test_inventory.c)
target_link_libraries(test_inventory PRIVATE ultc_lib)

add_executable(test_search tests/test_search.c)
target_link_libraries(test_search PRIVATE ultc_lib)

add_executable(test_stats tests/test_stats.c)
target_link_libraries(test_stats PRIVATE ultc_lib)

add_executable(test_integration tests/test_integration.c)
target_link_libraries(test_integration PRIVATE ultc_lib)

# ============================================================
# 后续版本测试 target 占位：
# add_executable(test_auth tests/test_auth.c)
# target_link_libraries(test_auth PRIVATE ultc_lib)
# add_executable(test_material tests/test_material.c)
# target_link_libraries(test_material PRIVATE ultc_lib)
# add_executable(test_borrow tests/test_borrow.c)
# target_link_libraries(test_borrow PRIVATE ultc_lib)
# add_executable(test_inventory tests/test_inventory.c)
# target_link_libraries(test_inventory PRIVATE ultc_lib)
# add_executable(test_search tests/test_search.c)
# target_link_libraries(test_search PRIVATE ultc_lib)
# add_executable(test_stats tests/test_stats.c)
# target_link_libraries(test_stats PRIVATE ultc_lib)
# ============================================================
