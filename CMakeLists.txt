cmake_minimum_required(VERSION 3.20)
project(simulnetx VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(simulnetx
    src/network.cxx
    src/node.cxx
    src/rng.cxx
)
target_include_directories(simulnetx PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

if(MSVC)
    target_compile_options(simulnetx PRIVATE /W4 /WX)
else()
    target_compile_options(simulnetx PRIVATE -Wall -Wextra -Werror)
endif()

option(SIMULNETX_BUILD_TESTS "Build simulnetx tests" ON)

if(SIMULNETX_BUILD_TESTS)
    include(FetchContent)
    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
    )
    # For Windows: prevent overriding the parent project's compiler/linker settings.
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    enable_testing()

    add_executable(simulnetx_tests
        tests/determinism_test.cxx
        tests/expectation_test.cxx
        tests/golden_test.cxx
        tests/network_test.cxx
        tests/partition_test.cxx
        tests/rng_test.cxx
    )
    target_link_libraries(simulnetx_tests PRIVATE simulnetx GTest::gtest_main)

    if(MSVC)
        target_compile_options(simulnetx_tests PRIVATE /W4 /WX)
    else()
        target_compile_options(simulnetx_tests PRIVATE -Wall -Wextra -Werror)
    endif()

    include(GoogleTest)
    gtest_discover_tests(simulnetx_tests)
endif()
