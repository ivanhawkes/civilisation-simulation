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

# ─── Generated Game Data ───
# The YAML is edited as separate files under data/ and merged into the monolithic
# documents in data/core-config by data/build.sh, which needs yq (mikefarah v4).
set(CIVSIM_DATA_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/data")
set(CIVSIM_DATA_OUTPUT_DIR "${CIVSIM_DATA_SOURCE_DIR}/core-config")

set(CIVSIM_DATA_OUTPUTS
    "${CIVSIM_DATA_OUTPUT_DIR}/full.yaml"
    "${CIVSIM_DATA_OUTPUT_DIR}/simple.yaml"
    "${CIVSIM_DATA_OUTPUT_DIR}/ages.yaml"
    "${CIVSIM_DATA_OUTPUT_DIR}/categories.yaml"
)

find_program(YQ_EXECUTABLE NAMES yq)
find_program(BASH_PROGRAM NAMES bash)

if(NOT YQ_EXECUTABLE)
    message(WARNING
        "yq was not found: the generated YAML in data/core-config cannot be built "
        "and the test suite is disabled. Activate the development shell (direnv allow) "
        "or install mikefarah/yq v4.")
elseif(NOT BASH_PROGRAM)
    message(WARNING "bash was not found: data/build.sh cannot be run.")
else()
    execute_process(
        COMMAND "${YQ_EXECUTABLE}" --version
        OUTPUT_VARIABLE YQ_VERSION_STRING
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_STRIP_TRAILING_WHITESPACE
    )
    if(NOT YQ_VERSION_STRING MATCHES "mikefarah" AND NOT YQ_VERSION_STRING MATCHES "[ (]v?4[.0-9]*[ )]?$")
        message(WARNING
            "data/build.sh expects mikefarah/yq v4; found '${YQ_VERSION_STRING}'. "
            "The YAML merge may fail.")
    endif()

    # Only the source YAML, never the generated documents in data/core-config.
    file(GLOB_RECURSE CIVSIM_DATA_INPUTS CONFIGURE_DEPENDS
        "${CIVSIM_DATA_SOURCE_DIR}/*.yaml"
        "${CIVSIM_DATA_SOURCE_DIR}/*.yml"
    )
    list(FILTER CIVSIM_DATA_INPUTS EXCLUDE REGEX "/core-config/")

    add_custom_command(
        OUTPUT ${CIVSIM_DATA_OUTPUTS}
        COMMAND "${BASH_PROGRAM}" "${CIVSIM_DATA_SOURCE_DIR}/build.sh"
        DEPENDS "${CIVSIM_DATA_SOURCE_DIR}/build.sh" ${CIVSIM_DATA_INPUTS}
        WORKING_DIRECTORY "${CIVSIM_DATA_SOURCE_DIR}"
        COMMENT "Merging the source YAML into data/core-config"
        VERBATIM
    )

    add_custom_target(CivSimData DEPENDS ${CIVSIM_DATA_OUTPUTS})
endif()

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
    "${CIVSIM_CORE_DIR}/Private/Core/ProgressionManager.cpp"
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
if(BUILD_TESTING AND TARGET CivSimData)
    add_executable(CivSimCoreTests
        Tests/SimulationTests.cpp
    )

    target_link_libraries(CivSimCoreTests PRIVATE CivSimCore)

    # Apply configuration to the test binary executable target so GDB can parse its breakpoints!
    apply_project_compiler_flags(CivSimCoreTests)

    add_dependencies(CivSimCoreTests CivSimData)

    add_test(
        NAME CivSimCoreTests
        COMMAND CivSimCoreTests
            "${CIVSIM_DATA_OUTPUT_DIR}/ages.yaml"
            "${CIVSIM_DATA_OUTPUT_DIR}/categories.yaml"
            "${CIVSIM_DATA_OUTPUT_DIR}/full.yaml"
    )

    set_tests_properties(CivSimCoreTests PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    )
endif()
