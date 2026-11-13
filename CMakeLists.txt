cmake_minimum_required(VERSION 3.22)
project(Riffhouse VERSION 0.1.6 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")

option(RIFFHOUSE_BUILD_PLUGIN "Build the standalone app and plugins" ON)
option(RIFFHOUSE_BUILD_TESTS "Build the offline test and render tools" ON)
set(RIFFHOUSE_ASIO_SDK_DIR "" CACHE PATH "Path to the Steinberg ASIO SDK (Windows only, enables ASIO)")

include(FetchContent)
set(FETCHCONTENT_QUIET OFF)

# JUCE (AGPLv3). One patch: Windows Audio counted the smallest buffer at each device's own
# Windows rate, so a guitar input set to 192 kHz held every PC at 12 ms buffers (cmake/patches).
FetchContent_Declare(juce
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.10
    GIT_SHALLOW ON
    PATCH_COMMAND ${CMAKE_COMMAND} -DPATCH=${CMAKE_CURRENT_SOURCE_DIR}/cmake/patches/juce-wasapi-buffer-sizes.patch
                  -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/ApplyPatch.cmake)
FetchContent_MakeAvailable(juce)

# Neural Amp Modeler core (MIT)
FetchContent_Declare(namcore
    GIT_REPOSITORY https://github.com/sdatkinson/NeuralAmpModelerCore.git
    GIT_TAG v0.6.0
    GIT_SHALLOW ON
    GIT_SUBMODULES "Dependencies/eigen"
    SOURCE_SUBDIR do-not-add)
FetchContent_MakeAvailable(namcore)

# Signalsmith Stretch (MIT) and the maths library it sits on (MIT): song speed and key, header only
FetchContent_Declare(signalsmith_stretch
    GIT_REPOSITORY https://github.com/Signalsmith-Audio/signalsmith-stretch.git
    GIT_TAG 1.4.0
    GIT_SHALLOW ON
    SOURCE_SUBDIR do-not-add)
FetchContent_Declare(signalsmith_linear
    GIT_REPOSITORY https://github.com/Signalsmith-Audio/linear.git
    GIT_TAG 0.6.6
    GIT_SHALLOW ON
    SOURCE_SUBDIR do-not-add)
FetchContent_MakeAvailable(signalsmith_stretch signalsmith_linear)
include(cmake/Lame.cmake)
include(cmake/Notices.cmake)
add_library(riffhouse_stretch INTERFACE)
target_include_directories(riffhouse_stretch SYSTEM INTERFACE
    "${signalsmith_stretch_SOURCE_DIR}/include"
    "${signalsmith_linear_SOURCE_DIR}/include")
if (MSVC)
    add_compile_options(/bigobj) # the stretcher is one very large template
endif()

file(GLOB NAM_SOURCES "${namcore_SOURCE_DIR}/NAM/*.cpp" "${namcore_SOURCE_DIR}/NAM/*/*.cpp")
add_library(nam_core OBJECT ${NAM_SOURCES})
target_include_directories(nam_core SYSTEM PUBLIC
    "${namcore_SOURCE_DIR}"
    "${namcore_SOURCE_DIR}/Dependencies/eigen"
    "${namcore_SOURCE_DIR}/Dependencies/nlohmann")
target_compile_definitions(nam_core PUBLIC NAM_SAMPLE_FLOAT NAM_ENABLE_A2_FAST)
if (MSVC)
    target_compile_definitions(nam_core PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN _USE_MATH_DEFINES)
    target_compile_options(nam_core PRIVATE /W0 /bigobj)
else()
    target_compile_options(nam_core PRIVATE -w)
endif()

add_subdirectory(src)
if (RIFFHOUSE_BUILD_TESTS)
    add_subdirectory(tests)
endif()
