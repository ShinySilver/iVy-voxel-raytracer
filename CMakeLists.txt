cmake_minimum_required(VERSION 3.22)
project(iVy)

if (${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
    message(FATAL_ERROR "In-source builds not allowed. Please make a new directory (called a build directory) and run CMake from there. You may need to remove CMakeCache.txt.")
endif ()

if (NOT CMAKE_BUILD_TYPE)
    message("Build Type not set, defaulting to Release")
    set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_GENERATOR Ninja)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_COMPILER /usr/bin/g++-13)
set(CMAKE_SCAN_FOR_MODULES)
set(BUILD_SHARED_LIBS OFF)
set(CPM_USE_NAMED_CACHE_DIRECTORIES ON)
set(CMAKE_SOURCE_DIR src/)
file(GLOB_RECURSE IVY_TEST_FILES tests/**.cpp)
file(GLOB_RECURSE IVY_SRC_FILES src/**.cpp src/**.glsl)
add_executable(iVy src/main.cpp ${IVY_SRC_FILES})
list(FILTER IVY_SRC_FILES EXCLUDE REGEX "main.cpp")
add_executable(iVy_tests ${IVY_SRC_FILES} ${IVY_TEST_FILES})

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
)
CPMAddPackage(
        NAME glad
        GITHUB_REPOSITORY Dav1dde/glad
        GIT_TAG v2.0.6
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

# Generating glad target with gl core 4.3
set(GLAD_SOURCES_DIR ${glad_SOURCE_DIR})
include(${glad_SOURCE_DIR}/cmake/GladConfig.cmake)
glad_add_library(glad_gl_core_43 STATIC API gl:core=4.3)

# Creating the imgui target
add_library(imgui STATIC ${imgui_SOURCE_DIR}/imgui.cpp ${imgui_SOURCE_DIR}/imgui_draw.cpp ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp)
target_include_directories(imgui PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)

# Enabling IPO if supported
include(CheckIPOSupported)
check_ipo_supported(RESULT supported OUTPUT error)
if(supported)
    set_property(TARGET iVy PROPERTY INTERPROCEDURAL_OPTIMIZATION <$<CONFIG:Release>:TRUE><$<CONFIG:Debug>:FALSE>)
endif()

# Optionally enabling libasan in debug builds
#target_link_options(iVy PRIVATE $<$<CONFIG:Debug>:-fsanitize=address,undefined>)

# Enabling optimization for release builds and more warnings in debugs builds
target_compile_options(iVy PRIVATE $<$<CONFIG:Release>:-O3 -s -flto>
        $<$<CONFIG:Debug>:-Wall -Wextra -Wpedantic -Wformat=2 -Wtrampolines -Werror -Woverloaded-virtual -Wno-unused-parameter -Wno-unused-variable>)

# Building iVy
target_link_libraries(iVy glad_gl_core_43 glfw glm FastNoise2 imgui)

# Building iVy_tests
include(GoogleTest)
target_include_directories(iVy_tests PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(iVy_tests glad_gl_core_43 glfw glm FastNoise2 imgui gtest_main)
gtest_discover_tests(iVy_tests)
