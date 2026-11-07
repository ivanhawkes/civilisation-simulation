cmake_minimum_required(VERSION 3.24)

project(CivSimLibrary
    VERSION 0.0.1
    DESCRIPTION "Civilisation trade simulation"
    LANGUAGES CXX
)

include(FetchContent)
include(CTest)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Help Intellisense by exporting compile commands
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Keep the dependency version aligned with Scripts/FetchYamlCpp.sh.
set(YAML_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(YAML_CPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(YAML_CPP_BUILD_TOOLS OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    yaml-cpp
    GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
    GIT_TAG yaml-cpp-0.9.0
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(yaml-cpp)

FetchContent_Declare(
  ftxui
  GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI.git
  GIT_TAG v7.0.3
)
FetchContent_MakeAvailable(ftxui)

set(CIVSIM_CORE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Source/CivSimCore")

# ─── Macro for Shared Compiler Configuration ───
macro(apply_project_compiler_flags TARGET_NAME)
    target_compile_features(${TARGET_NAME} PUBLIC cxx_std_20)
    if(MSVC)
        target_compile_options(${TARGET_NAME} PRIVATE /W4 /permissive-)
    else()
        # Explicitly forces full warning sets AND keeps debug symbols tracking active
        target_compile_options(${TARGET_NAME} PRIVATE -Wall -Wextra -Wpedantic -g)
    endif()
endmacro()

# ─── Core Library Definition ───
add_library(CivSimCore STATIC
    "${CIVSIM_CORE_DIR}/Private/Core/SimulationConfig.cpp"
    "${CIVSIM_CORE_DIR}/Public/Core/ProgressionManager.h"
)

target_include_directories(CivSimCore
    PUBLIC
        "${CIVSIM_CORE_DIR}/Public"
)

target_link_libraries(CivSimCore
    PRIVATE
        yaml-cpp::yaml-cpp
)

# Apply configuration to the static library target
apply_project_compiler_flags(CivSimCore)

# ─── TUI Executable Definition ───
add_executable(CivSimCoreTui
    "${CIVSIM_CORE_DIR}/Private/Tui/CivSimTui.cpp"
    "${CIVSIM_CORE_DIR}/Public/Tui/CivSimTui.h"
)

target_link_libraries(CivSimCoreTui 
    PRIVATE CivSimCore ftxui::screen ftxui::component ftxui::dom)

apply_project_compiler_flags(CivSimCoreTui)

# ─── Test Suite Generation ───
if(BUILD_TESTING)
    add_executable(CivSimCoreTests
        Tests/SimulationTests.cpp
    )
    
    target_link_libraries(CivSimCoreTests PRIVATE CivSimCore)

    # Apply configuration to the test binary executable target so GDB can parse its breakpoints!
    apply_project_compiler_flags(CivSimCoreTests)

    add_test(
        NAME CivSimCoreTests
        COMMAND CivSimCoreTests
            "${CMAKE_CURRENT_SOURCE_DIR}/data/generated/full.yaml"
    )
endif()
