cmake_minimum_required(VERSION 3.20)
project(simulnetx VERSION 0.4.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(simulnetx
    src/network.cxx
    src/node.cxx
    src/rng.cxx
    src/scenario.cxx
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
        tests/crash_test.cxx
        tests/determinism_test.cxx
        tests/directed_test.cxx
        tests/expectation_test.cxx
        tests/golden_test.cxx
        tests/network_test.cxx
        tests/partition_test.cxx
        tests/reflect_test.cxx
        tests/request_test.cxx
        tests/request_trace_test.cxx
        tests/rng_test.cxx
        tests/scenario_test.cxx
        tests/serializer_test.cxx
        tests/topology_test.cxx
        tests/trace_test.cxx
        tests/typed_test.cxx
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

option(SIMULNETX_BUILD_EXAMPLES "Build simulnetx examples" ON)

if(SIMULNETX_BUILD_EXAMPLES)
    add_executable(retry_under_loss examples/retry_under_loss.cxx)
    target_link_libraries(retry_under_loss PRIVATE simulnetx)

    if(MSVC)
        target_compile_options(retry_under_loss PRIVATE /W4 /WX)
    else()
        target_compile_options(retry_under_loss PRIVATE -Wall -Wextra -Werror)
    endif()

    add_executable(scenario_retry examples/scenario_retry.cxx)
    target_link_libraries(scenario_retry PRIVATE simulnetx)

    if(MSVC)
        target_compile_options(scenario_retry PRIVATE /W4 /WX)
    else()
        target_compile_options(scenario_retry PRIVATE -Wall -Wextra -Werror)
    endif()

    add_executable(failover examples/failover.cxx)
    target_link_libraries(failover PRIVATE simulnetx)

    if(MSVC)
        target_compile_options(failover PRIVATE /W4 /WX)
    else()
        target_compile_options(failover PRIVATE -Wall -Wextra -Werror)
    endif()

    add_executable(typed_ping_pong examples/typed_ping_pong.cxx)
    target_link_libraries(typed_ping_pong PRIVATE simulnetx)

    if(MSVC)
        target_compile_options(typed_ping_pong PRIVATE /W4 /WX)
    else()
        target_compile_options(typed_ping_pong PRIVATE -Wall -Wextra -Werror)
    endif()
endif()
