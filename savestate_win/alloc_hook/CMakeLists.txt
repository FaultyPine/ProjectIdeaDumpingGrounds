cmake_minimum_required(VERSION 3.10)
project(alloc_hook)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable debug symbols
set(CMAKE_BUILD_TYPE Debug)

# Use vcpkg
set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")

# Find Detours package
find_path(DETOURS_INCLUDE_DIRS "detours/detours.h")
find_library(DETOURS_LIBRARY detours REQUIRED)

# DLL project
add_library(alloc_hook_dll SHARED
    src/dll/dllmain.cpp
    src/dll/hook.cpp
    src/dll/memory.cpp
)

target_include_directories(alloc_hook_dll PRIVATE
    ${CMAKE_SOURCE_DIR}/src/dll
    ${DETOURS_INCLUDE_DIRS}
)

target_link_libraries(alloc_hook_dll PRIVATE
    ${DETOURS_LIBRARY}
)

target_include_directories(alloc_hook_dll PRIVATE
    ${DETOURS_INCLUDE_DIRS}
)

# Injector executable
add_executable(injector
    src/injector/main.cpp
)

target_include_directories(injector PRIVATE
    ${CMAKE_SOURCE_DIR}/src/injector
)

target_link_libraries(injector PRIVATE
    ${DETOURS_LIBRARY})

target_include_directories(injector PRIVATE
    ${DETOURS_INCLUDE_DIRS})

# Test harness
add_executable(test_harness
    src/test/harness.cpp
)

target_link_libraries(test_harness PRIVATE
    ${DETOURS_LIBRARY}
)

target_include_directories(test_harness PRIVATE
    ${CMAKE_SOURCE_DIR}/src/test
)