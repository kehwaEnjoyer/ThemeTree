cmake_minimum_required(VERSION 3.16)
project(ThemeTree VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Force -O3 release optimizations
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

if(MSVC)
    add_compile_options(/O2)
else()
    add_compile_options(-O3 -march=native -flto)
    add_link_options(-flto)
endif()

# Find dependencies (Eigen3)
find_package(OpenMP REQUIRED)
find_package(Eigen3 REQUIRED)

# Include header directory
include_directories(include)

# Output compiled binary strictly to bin/ directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

# Gather implementation sources
file(GLOB SOURCES "src/*.cpp")

# Build target
add_executable(themeTree ${SOURCES})

# Link libraries
target_link_libraries(themeTree PRIVATE Eigen3::Eigen OpenMP::OpenMP_CXX)

# Enable compiler warnings
if(NOT MSVC)
    target_compile_options(themeTree PRIVATE -Wall -Wextra -Wpedantic)
endif()