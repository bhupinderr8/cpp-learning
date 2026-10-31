cmake_minimum_required(VERSION 3.20)
project(week02_aligned_arena CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Threads REQUIRED)

add_compile_options(-Wall -Wextra -Wpedantic)

option(ENABLE_SANITIZERS "Enable Address and UndefinedBehavior sanitizers" OFF)
if(ENABLE_SANITIZERS)
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer -g -O1)
    add_link_options(-fsanitize=address,undefined)
endif()

add_executable(aligned_arena_bench
    aligned_arena.cpp
    aligned_arena_bench.cpp
)

target_link_libraries(aligned_arena_bench PRIVATE Threads::Threads)
