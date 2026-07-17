cmake_minimum_required(VERSION 3.20)
project(AnalysisLib VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Header-only library
add_library(analysis_lib INTERFACE)
add_library(NowAnalysis::analysis_lib ALIAS analysis_lib)

target_include_directories(analysis_lib INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Example
option(BUILD_EXAMPLES "Build examples" ON)
if(BUILD_EXAMPLES)
    add_executable(simple_analysis examples/simple_analysis.cpp)
    target_link_libraries(simple_analysis PRIVATE analysis_lib)
endif()
