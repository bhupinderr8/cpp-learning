cmake_minimum_required(VERSION 3.20)
project(week01_cache_bench CXX)

# Enforce C++20 standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Threading library required for multi-threaded false-sharing benchmark
find_package(Threads REQUIRED)

# Common compiler warnings and flags
add_compile_options(-Wall -Wextra -Wpedantic -Wno-interference-size)

# Optional sanitizer flag (pass -DENABLE_SANITIZERS=ON to cmake)
option(ENABLE_SANITIZERS "Enable Address and UndefinedBehavior sanitizers" OFF)
if(ENABLE_SANITIZERS)
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer -g -O1)
    add_link_options(-fsanitize=address,undefined)
endif()

# -----------------------------------------------------------------------------
# Main Weekend Benchmark Target
# (Notice: no 'src/' prefix, file is in the root of the week folder)
# -----------------------------------------------------------------------------
add_executable(cache_bench
    cache_bench.cpp
)

target_link_libraries(cache_bench PRIVATE Threads::Threads)