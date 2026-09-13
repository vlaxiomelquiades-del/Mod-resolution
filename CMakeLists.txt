cmake_minimum_required(VERSION 3.22)

project(resscale LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT ANDROID)
  message(FATAL_ERROR
    "Configure com o toolchain do NDK, ex.: -DCMAKE_TOOLCHAIN_FILE=<ndk>/build/cmake/android.toolchain.cmake -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-28")
endif()

include(FetchContent)

# SDK oficial do LeviLaunchroid (mesma dependência usada pelos exemplos do
# próprio repositório). Fixe uma tag de release para builds reprodutíveis.
set(PRELOADER_ANDROID_REF "main" CACHE STRING
  "preloader-android tag/branch/commit used to build this mod")

FetchContent_Declare(
  preloader_android
  GIT_REPOSITORY https://github.com/LiteLDev/preloader-android.git
  GIT_TAG ${PRELOADER_ANDROID_REF})
FetchContent_MakeAvailable(preloader_android)

# Dependências transitivas exigidas por <pl/Config.hpp> (mesmos pins usados em
# examples/full-cpp-mod/CMakeLists.txt no repositório do launcher).
FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.11.3)

FetchContent_Declare(
  boost_pfr
  GIT_REPOSITORY https://github.com/boostorg/pfr.git
  GIT_TAG boost-1.88.0)

FetchContent_Declare(
  magic_enum
  GIT_REPOSITORY https://github.com/Neargye/magic_enum.git
  GIT_TAG v0.9.7)

set(FMT_DOC OFF CACHE BOOL "" FORCE)
set(FMT_TEST OFF CACHE BOOL "" FORCE)
set(FMT_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
  fmt
  GIT_REPOSITORY https://github.com/fmtlib/fmt.git
  GIT_TAG 11.2.0)

FetchContent_MakeAvailable(nlohmann_json boost_pfr magic_enum fmt)

add_library(resscale SHARED
  src/ResScaleMod.cpp)

set_target_properties(resscale PROPERTIES OUTPUT_NAME "resscale")

target_include_directories(resscale PRIVATE
  src)

find_library(LL_LOG_LIB log)

target_link_libraries(resscale PRIVATE
  preloader
  nlohmann_json::nlohmann_json
  Boost::pfr
  magic_enum::magic_enum
  fmt::fmt
  android
  vulkan
  ${LL_LOG_LIB})

target_compile_options(resscale PRIVATE
  $<$<COMPILE_LANG_AND_ID:CXX,Clang,GNU>:-Wall -Wextra -Wpedantic -fvisibility=hidden -fvisibility-inlines-hidden>)
