cmake_minimum_required(VERSION 3.20)
project(simulnetx VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(simulnetx
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
