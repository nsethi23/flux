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

if(BUILD_TESTING)
    add_executable(flux_tests tests/order_book_tests.cpp)
    target_link_libraries(flux_tests PRIVATE flux_core)
    add_test(NAME flux_tests COMMAND flux_tests)
endif()

if(MSVC)
    target_compile_options(flux_core PRIVATE /W4 /permissive-)
    target_compile_options(flux PRIVATE /W4 /permissive-)
    if(BUILD_TESTING)
        target_compile_options(flux_tests PRIVATE /W4 /permissive-)
    endif()
else()
    target_compile_options(flux_core PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(flux PRIVATE -Wall -Wextra -Wpedantic)
    if(BUILD_TESTING)
        target_compile_options(flux_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endif()
