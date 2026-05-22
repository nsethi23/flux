cmake_minimum_required(VERSION 3.14)
project(Flux VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(flux src/main.cpp src/order_book.cpp)

target_include_directories(flux PRIVATE include)

find_package(GTest REQUIRED)

add_executable(flux_tests tests/order_book_test.cpp src/order_book.cpp)
target_include_directories(flux_tests PRIVATE include)
target_link_libraries(flux_tests GTest::GTest GTest::Main)

find_package(benchmark REQUIRED)

add_executable(flux_bench benchmarks/bench_order_book.cpp src/order_book.cpp)
target_include_directories(flux_bench PRIVATE include)
target_link_libraries(flux_bench benchmark::benchmark)

add_executable(flux_latency benchmarks/latency.cpp src/order_book.cpp)
target_include_directories(flux_latency PRIVATE include)

add_executable(flux_parser src/main_parser.cpp src/itch_parser.cpp src/order_book.cpp)
target_include_directories(flux_parser PRIVATE include)

include_directories(/opt/homebrew/include)

set(CMAKE_BUILD_TYPE Release)
set(CMAKE_CXX_FLAGS_RELEASE "-O3")