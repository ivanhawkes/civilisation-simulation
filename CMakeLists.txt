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

if(YQ_EXECUTABLE AND NOT BASH_PROGRAM)
    message(FATAL_ERROR "data/build.sh needs bash, and bash was not found.")
endif()

if(NOT YQ_EXECUTABLE AND BUILD_TESTING)
    # A missing yq takes the generated documents and the data test suite with it.
    # Refuse the configure rather than configure a test suite that runs nothing.
    message(FATAL_ERROR
        "yq (mikefarah/yq v4) was not found: the generated YAML in data/core-config cannot "
        "be built and the data test suite cannot run. Activate the development shell "
        "(direnv allow) or install mikefarah/yq v4. Build without the tests with "
        "-DBUILD_TESTING=OFF.")
endif()

if(YQ_EXECUTABLE)
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

    # Every source file under data/ is a document yq loads on its own, and the files
    # refer to each other by uniqueId, so no anchor has to survive the trip from one file
    # to the next: any yq v4 builds the data. The development shell pins v4.54.1, and that
    # is the version this merge is tested with, which is what the warning is about.
    set(CIVSIM_YQ_TESTED_VERSION "4.54.1")
    if(NOT YQ_VERSION_STRING MATCHES "${CIVSIM_YQ_TESTED_VERSION}")
        message(WARNING
            "The data merge is tested with mikefarah/yq v${CIVSIM_YQ_TESTED_VERSION}, the "
            "version the development shell pins; found '${YQ_VERSION_STRING}'. The merge is "
            "plain v4 syntax and the source files link by uniqueId, so a v4 of any version "
            "should build it. Use the devenv shell for the tested version, or configure "
            "with -DBUILD_TESTING=OFF.")
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
    "${CIVSIM_CORE_DIR}/Private/Core/Universe.cpp"
    "${CIVSIM_CORE_DIR}/Private/Core/ProgressionManager.cpp"
    "${CIVSIM_CORE_DIR}/Private/Core/Simulation.cpp"
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
# The unit suites feed the loaders temporary documents they write themselves, so they
# need no data step. The data suite reads the generated documents and depends on the
# CivSimData target.
if(BUILD_TESTING)
    # Each suite is its own executable, with its own ctest entry.
    function(civsim_add_test SUITE_NAME)
        cmake_parse_arguments(PARSE_ARGV 1 TEST "" "" "SOURCES;ARGS")

        add_executable(${SUITE_NAME} ${TEST_SOURCES})

        target_link_libraries(${SUITE_NAME} PRIVATE CivSimCore)

        # Apply configuration to the test binary executable target so GDB can parse its breakpoints!
        apply_project_compiler_flags(${SUITE_NAME})

        target_include_directories(${SUITE_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/Tests")

        add_test(NAME ${SUITE_NAME} COMMAND ${SUITE_NAME} ${TEST_ARGS})
        set_tests_properties(${SUITE_NAME} PROPERTIES
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        )
    endfunction()

    civsim_add_test(ConfigTests SOURCES Tests/ConfigTests.cpp)
    civsim_add_test(UniverseTests SOURCES Tests/UniverseTests.cpp)
    civsim_add_test(ProgressionTests SOURCES Tests/ProgressionTests.cpp)
    civsim_add_test(SimulationTests SOURCES Tests/SimulationTests.cpp)

    if(TARGET CivSimData)
        # The data suite reads the generated documents, the way the game loads them.
        civsim_add_test(DataTests
            SOURCES Tests/DataTests.cpp
            ARGS
                "${CIVSIM_DATA_OUTPUT_DIR}/ages.yaml"
                "${CIVSIM_DATA_OUTPUT_DIR}/categories.yaml"
                "${CIVSIM_DATA_OUTPUT_DIR}/full.yaml"
                "${CIVSIM_DATA_OUTPUT_DIR}/simple.yaml"
                "${CIVSIM_DATA_SOURCE_DIR}/universe/solar-system-names.yaml"
        )

        add_dependencies(DataTests CivSimData)
    endif()
endif()
