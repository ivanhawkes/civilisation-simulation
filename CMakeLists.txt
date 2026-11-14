cmake_minimum_required(VERSION 3.28)

project(Manifest VERSION 0.0.1 DESCRIPTION "Civilisation trade simulation for Jettison" LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
# No C++ modules are used; keep compile commands directly usable by clang-tidy.
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# As in Jettison, a host consuming the library does not inherit its test build.
option(MANIFEST_BUILD_TESTS "Build Manifest's test suites" ${PROJECT_IS_TOP_LEVEL})
if(MANIFEST_BUILD_TESTS)
    include(CTest)
endif()
include(FetchContent)
option(MANIFEST_BUILD_TUI "Build the development terminal interface" ${PROJECT_IS_TOP_LEVEL})
option(MANIFEST_WERROR "Treat project warnings as errors" OFF)
option(MANIFEST_CCACHE "Compile through ccache when available" ON)
if(MANIFEST_CCACHE AND NOT CMAKE_CXX_COMPILER_LAUNCHER)
    find_program(MANIFEST_CCACHE_PROGRAM ccache)
    if(MANIFEST_CCACHE_PROGRAM)
        set(CMAKE_C_COMPILER_LAUNCHER "${MANIFEST_CCACHE_PROGRAM}")
        set(CMAKE_CXX_COMPILER_LAUNCHER "${MANIFEST_CCACHE_PROGRAM}")
    endif()
endif()

add_subdirectory(source/external)

# The engine owns the SDK versions and ABI settings. Link its warnings privately, so
# consumers inherit the API and dependencies, without inheriting our build policy.
add_library(manifest_warnings INTERFACE)
target_link_libraries(manifest_warnings INTERFACE jettison_warnings)
if(MANIFEST_WERROR)
    target_compile_options(manifest_warnings INTERFACE -Werror)
endif()

# ─── Generated Game Data ───
# The YAML is edited as separate files under data/ and merged into the monolithic
# documents in data/core-config by data/build.sh, which needs yq (mikefarah v4).
set(MANIFEST_DATA_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/data")
set(MANIFEST_DATA_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/data/core-config")

set(MANIFEST_DATA_OUTPUTS
    "${MANIFEST_DATA_OUTPUT_DIR}/full.yaml"
    "${MANIFEST_DATA_OUTPUT_DIR}/simple.yaml"
    "${MANIFEST_DATA_OUTPUT_DIR}/ages.yaml"
    "${MANIFEST_DATA_OUTPUT_DIR}/categories.yaml"
)

find_program(YQ_EXECUTABLE NAMES yq)
find_program(BASH_PROGRAM NAMES bash)

if(YQ_EXECUTABLE AND NOT BASH_PROGRAM)
    message(FATAL_ERROR "data/build.sh needs bash, and bash was not found.")
endif()

if(NOT YQ_EXECUTABLE AND MANIFEST_BUILD_TESTS AND BUILD_TESTING)
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
    # refer to each other by id - itemId, skillId, categoryId, ageId - so no anchor has
    # to survive the trip from one file to the next: any yq v4 builds the data. The
    # development shell pins v4.54.1, and that is the version this merge is tested with,
    # which is what the warning is about.
    set(MANIFEST_YQ_TESTED_VERSION "4.54.1")
    if(NOT YQ_VERSION_STRING MATCHES "${MANIFEST_YQ_TESTED_VERSION}")
        message(WARNING
            "The data merge is tested with mikefarah/yq v${MANIFEST_YQ_TESTED_VERSION}, the "
            "version the development shell pins; found '${YQ_VERSION_STRING}'. The merge is "
            "plain v4 syntax and the source files link by id, so a v4 of any version "
            "should build it. Use the devenv shell for the tested version, or configure "
            "with -DBUILD_TESTING=OFF.")
    endif()

    # Only the source YAML, never the generated documents in data/core-config.
    file(GLOB_RECURSE MANIFEST_DATA_INPUTS CONFIGURE_DEPENDS
        "${MANIFEST_DATA_SOURCE_DIR}/*.yaml"
        "${MANIFEST_DATA_SOURCE_DIR}/*.yml"
    )
    list(FILTER MANIFEST_DATA_INPUTS EXCLUDE REGEX "/core-config/")

    add_custom_command(
        OUTPUT ${MANIFEST_DATA_OUTPUTS}
        COMMAND "${BASH_PROGRAM}" "${MANIFEST_DATA_SOURCE_DIR}/build.sh" "${MANIFEST_DATA_OUTPUT_DIR}"
        DEPENDS "${MANIFEST_DATA_SOURCE_DIR}/build.sh" ${MANIFEST_DATA_INPUTS}
        WORKING_DIRECTORY "${MANIFEST_DATA_SOURCE_DIR}"
        COMMENT "Merging the source YAML into data/core-config"
        VERBATIM
    )

    add_custom_target(manifest_data DEPENDS ${MANIFEST_DATA_OUTPUTS})
endif()


include(cmake/ManifestModule.cmake)
set(MANIFEST_MOD_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/mods/manifest")
add_subdirectory(source)

# A mod is content, not a native shared library. Keep its virtual paths stable in
# both directory and pack form; the C++ systems remain a host-side library for now.
add_custom_target(manifest_mod
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${MANIFEST_MOD_DIRECTORY}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${MANIFEST_MOD_DIRECTORY}/manifest"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/mods/manifest/mod.yaml" "${MANIFEST_MOD_DIRECTORY}/mod.yaml"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${MANIFEST_DATA_OUTPUT_DIR}" "${MANIFEST_MOD_DIRECTORY}/manifest/core-config"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${MANIFEST_DATA_SOURCE_DIR}/universe" "${MANIFEST_MOD_DIRECTORY}/manifest/universe"
    VERBATIM)
if(TARGET manifest_data)
    add_dependencies(manifest_mod manifest_data)
else()
    # Packaging without yq must fail, rather than ship a directory missing its game data.
    add_custom_command(TARGET manifest_mod PRE_BUILD
        COMMAND ${CMAKE_COMMAND} -E echo "manifest_mod requires mikefarah/yq v4"
        COMMAND ${CMAKE_COMMAND} -E false)
endif()
jettison_add_pack(manifest_pack
    SOURCE "${MANIFEST_MOD_DIRECTORY}"
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/manifest.pack"
    DEPENDS manifest_mod)
