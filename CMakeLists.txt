cmake_minimum_required(VERSION 3.20)

project(
    flux
    VERSION 0.1.0
    DESCRIPTION "High-performance C++ order book and matching engine"
    LANGUAGES CXX
)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(CTest)

add_library(flux_core
    src/itch.cpp
    src/matching_engine.cpp
    src/order_book.cpp
)

target_compile_features(flux_core PUBLIC cxx_std_20)

target_include_directories(
    flux_core
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

add_executable(flux src/main.cpp)

target_link_libraries(flux PRIVATE flux_core)

add_executable(flux_bench benchmarks/order_book_bench.cpp)

target_link_libraries(flux_bench PRIVATE flux_core)

if(BUILD_TESTING)
    add_executable(flux_order_book_tests tests/order_book_tests.cpp)
    target_link_libraries(flux_order_book_tests PRIVATE flux_core)
    add_test(NAME flux_order_book_tests COMMAND flux_order_book_tests)

    add_executable(flux_itch_tests tests/itch_tests.cpp)
    target_link_libraries(flux_itch_tests PRIVATE flux_core)
    add_test(NAME flux_itch_tests COMMAND flux_itch_tests)
endif()

if(MSVC)
    target_compile_options(flux_core PRIVATE /W4 /permissive-)
    target_compile_options(flux PRIVATE /W4 /permissive-)
    target_compile_options(flux_bench PRIVATE /W4 /permissive-)
    if(BUILD_TESTING)
        target_compile_options(flux_order_book_tests PRIVATE /W4 /permissive-)
        target_compile_options(flux_itch_tests PRIVATE /W4 /permissive-)
    endif()
else()
    target_compile_options(flux_core PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(flux PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(flux_bench PRIVATE -Wall -Wextra -Wpedantic)
    if(BUILD_TESTING)
        target_compile_options(flux_order_book_tests PRIVATE -Wall -Wextra -Wpedantic)
        target_compile_options(flux_itch_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endif()
