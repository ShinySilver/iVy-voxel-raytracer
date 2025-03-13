cmake_minimum_required(VERSION 3.22)
project(iVy)

# Default build type is "Release" for increased performance
if (NOT CMAKE_BUILD_TYPE)
    message("Build Type not set, defaulting to Release")
    set(CMAKE_BUILD_TYPE Release)
endif()

#set(CMAKE_GENERATOR Ninja)
#set(CMAKE_MAKE_PROGRAM ninja)
#set(CMAKE_CXX_COMPILER /usr/bin/g++-13)

# CMake configuration
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_SCAN_FOR_MODULES)
set(BUILD_SHARED_LIBS OFF)
set(CPM_USE_NAMED_CACHE_DIRECTORIES ON)
set(CMAKE_SOURCE_DIR src/)

# Defining compilation flags
set(RELEASE_OPTIONS -O3 -s -flto=auto -march=native -mtune=native -fomit-frame-pointer -fmacro-prefix-map=${CMAKE_CURRENT_SOURCE_DIR}/=)
set(DEBUG_OPTIONS -Wall -Wextra -Wpedantic -Wformat=2 -Wtrampolines -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-function)

# Defining sources files
file(GLOB_RECURSE IVY_SOURCES src/**.cpp src/**.glsl libraries/**.cpp libraries/**.c)
file(GLOB_RECURSE IVY_TESTS tests/**.cpp)

# Downloading libraries from github
include(CPM.cmake)
CPMAddPackage(
        NAME glfw
        GITHUB_REPOSITORY glfw/glfw
        GIT_TAG 3.4
        OPTIONS "GLFW_BUILD_TESTS OFF" "GLFW_BUILD_EXAMPLES OFF" "GLFW_BUILD_DOCS OFF" "GLFW_LIBRARY_TYPE STATIC"
)
CPMAddPackage(
        NAME glm
        GITHUB_REPOSITORY g-truc/glm
        GIT_TAG 1.0.1
)
CPMAddPackage(
        NAME FastNoise2
        GITHUB_REPOSITORY Auburn/FastNoise2
        GIT_TAG v0.10.0-alpha
        OPTIONS "FASTNOISE2_NOISETOOL OFF"
)
CPMAddPackage(
        NAME imgui
        GITHUB_REPOSITORY ocornut/imgui
        GIT_TAG v1.91.0
)
CPMAddPackage(
        NAME googletest
        GITHUB_REPOSITORY google/googletest
        GIT_TAG v1.15.2
)

# Adding the imgui dependency
add_library(imgui STATIC ${imgui_SOURCE_DIR}/imgui.cpp ${imgui_SOURCE_DIR}/imgui_draw.cpp ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp)
target_compile_options(imgui PRIVATE $<$<CONFIG:Release>:${RELEASE_OPTIONS}> $<$<CONFIG:Debug>:${DEBUG_OPTIONS}>)
target_include_directories(imgui PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
target_link_libraries(imgui glfw)

# Building iVy
add_executable(iVy src/main.cpp ${IVY_SOURCES})
target_compile_options(iVy PRIVATE $<$<CONFIG:Release>:${RELEASE_OPTIONS}> $<$<CONFIG:Debug>:${DEBUG_OPTIONS}>)
target_include_directories(iVy PRIVATE libraries/glad_gl_core_43/include libraries/iVy_utils/include ${CMAKE_SOURCE_DIR})
target_link_libraries(iVy glfw glm FastNoise imgui)

# Building iVy_tests
include(GoogleTest)
list(FILTER IVY_SOURCES EXCLUDE REGEX "main.cpp")
add_executable(iVy_tests ${IVY_SOURCES} ${IVY_TESTS})
target_compile_options(iVy_tests PRIVATE $<$<CONFIG:Release>:${RELEASE_OPTIONS}> $<$<CONFIG:Debug>:${DEBUG_OPTIONS}>)
target_include_directories(iVy_tests PRIVATE libraries/glad_gl_core_43/include libraries/iVy_utils/include ${CMAKE_SOURCE_DIR})
target_link_libraries(iVy_tests PRIVATE glfw glm FastNoise imgui gtest_main)
gtest_discover_tests(iVy_tests)
