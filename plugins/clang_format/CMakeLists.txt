# Example LazyCMake plugin: clang-format auto-formatter.
# Build separately: mkdir -p build && cd build && cmake .. && make
# Then copy liblazycmake_clang_format.so to ~/.config/lazycmake/plugins/

cmake_minimum_required(VERSION 3.21)
project(lazycmake-clang-format LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(lazycmake_clang_format SHARED plugin.cpp)

target_include_directories(lazycmake_clang_format
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../../include
)

# When installed system-wide, use find_package instead:
# find_package(LazyCMake REQUIRED)
# target_link_libraries(lazycmake_clang_format PRIVATE lazycmake::lazycmake_core)
