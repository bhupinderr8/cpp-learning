cmake_minimum_required(VERSION 3.20)
project(week01_cache_bench CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# -O3 is essential for cache benchmarks so register spills don't mask cache latency
add_compile_options(-O3 -Wall -Wextra -Werror -march=native)

find_package(Threads REQUIRED)

add_executable(cache_bench src/cache_bench.cpp)
target_include_directories(cache_bench PRIVATE include)
target_link_libraries(cache_bench PRIVATE Threads::Threads)