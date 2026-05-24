include(FetchContent)

option(
    COLUMNAR_USE_TCMALLOC
    "Use google/tcmalloc for Linux x86-64 executables"
    ON
)

set(
    COLUMNAR_TCMALLOC_GIT_TAG
    "master"
    CACHE STRING "google/tcmalloc git ref"
)

set(
    COLUMNAR_TCMALLOC_BAZEL_VERSION
    "8.4.2"
    CACHE STRING "Bazel version used by Bazelisk to build google/tcmalloc"
)

add_library(columnar_tcmalloc INTERFACE)

if(NOT COLUMNAR_USE_TCMALLOC)
    message(STATUS "google/tcmalloc: disabled")
    return()
endif()

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(FATAL_ERROR
        "google/tcmalloc is enabled, but this project currently supports only Linux x86-64. "
        "Reconfigure with -DCOLUMNAR_USE_TCMALLOC=OFF to bypass allocator integration."
    )
endif()

if(NOT CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|amd64|AMD64)$")
    message(FATAL_ERROR
        "google/tcmalloc is enabled, but this project currently supports only Linux x86-64. "
        "Detected processor: ${CMAKE_SYSTEM_PROCESSOR}. "
        "Reconfigure with -DCOLUMNAR_USE_TCMALLOC=OFF to bypass allocator integration."
    )
endif()

find_program(COLUMNAR_BAZELISK_EXECUTABLE bazelisk)

if(NOT COLUMNAR_BAZELISK_EXECUTABLE)
    message(FATAL_ERROR
        "google/tcmalloc requires Bazelisk because upstream CMake integration is not reliable "
        "and system Bazel versions can break TCMalloc's Bazel dependency graph. "
        "Install Bazelisk or reconfigure with -DCOLUMNAR_USE_TCMALLOC=OFF."
    )
endif()

FetchContent_Declare(
    google_tcmalloc
    GIT_REPOSITORY https://github.com/google/tcmalloc.git
    GIT_TAG ${COLUMNAR_TCMALLOC_GIT_TAG}
    GIT_SHALLOW TRUE
)

# FetchContent_MakeAvailable is not usable here because tcmalloc is built via
# Bazel, not CMake. CMP0169 allows the deprecated direct FetchContent_Populate call.
cmake_policy(SET CMP0169 OLD)
FetchContent_Populate(google_tcmalloc)

set(COLUMNAR_TCMALLOC_BRIDGE_DIR
    ${google_tcmalloc_SOURCE_DIR}/cmake_bridge
)

file(MAKE_DIRECTORY ${COLUMNAR_TCMALLOC_BRIDGE_DIR})
file(WRITE ${COLUMNAR_TCMALLOC_BRIDGE_DIR}/BUILD.bazel
"load(\"@rules_cc//cc:cc_binary.bzl\", \"cc_binary\")

cc_binary(
    name = \"columnar_tcmalloc\",
    linkshared = True,
    linkstatic = True,
    linkopts = [\"-Wl,-soname,libcolumnar_tcmalloc.so\"],
    deps = [\"//tcmalloc\"],
)
"
)

set(COLUMNAR_TCMALLOC_OUTPUT_DIR
    ${CMAKE_BINARY_DIR}/_deps/google_tcmalloc-artifacts
)

set(COLUMNAR_TCMALLOC_LIBRARY
    ${COLUMNAR_TCMALLOC_OUTPUT_DIR}/libcolumnar_tcmalloc.so
)

set(COLUMNAR_TCMALLOC_BAZEL_LIBRARY
    ${google_tcmalloc_BINARY_DIR}/bazel-bin/cmake_bridge/libcolumnar_tcmalloc.so
)

add_custom_command(
    OUTPUT ${COLUMNAR_TCMALLOC_LIBRARY}
    COMMAND
        ${CMAKE_COMMAND}
        -E
        make_directory
        ${COLUMNAR_TCMALLOC_OUTPUT_DIR}
    COMMAND
        ${CMAKE_COMMAND}
        -E
        env
        USE_BAZEL_VERSION=${COLUMNAR_TCMALLOC_BAZEL_VERSION}
        ${COLUMNAR_BAZELISK_EXECUTABLE}
        build
        --compilation_mode=opt
        --symlink_prefix=${google_tcmalloc_BINARY_DIR}/bazel-
        //cmake_bridge:columnar_tcmalloc
    COMMAND
        ${CMAKE_COMMAND}
        -E
        copy_if_different
        ${COLUMNAR_TCMALLOC_BAZEL_LIBRARY}
        ${COLUMNAR_TCMALLOC_LIBRARY}
    WORKING_DIRECTORY ${google_tcmalloc_SOURCE_DIR}
    COMMENT "Building google/tcmalloc with Bazelisk ${COLUMNAR_TCMALLOC_BAZEL_VERSION}"
    VERBATIM
)

add_custom_target(
    columnar_build_tcmalloc
    DEPENDS ${COLUMNAR_TCMALLOC_LIBRARY}
)

add_library(columnar_google_tcmalloc SHARED IMPORTED GLOBAL)
add_dependencies(columnar_google_tcmalloc columnar_build_tcmalloc)

set_target_properties(
    columnar_google_tcmalloc
    PROPERTIES
    IMPORTED_LOCATION ${COLUMNAR_TCMALLOC_LIBRARY}
)

target_link_libraries(
    columnar_tcmalloc
    INTERFACE
    -Wl,--no-as-needed
    columnar_google_tcmalloc
    -Wl,--as-needed
)

target_link_options(
    columnar_tcmalloc
    INTERFACE
    -Wl,-rpath,${COLUMNAR_TCMALLOC_OUTPUT_DIR}
)

message(STATUS "google/tcmalloc: enabled via Bazelisk ${COLUMNAR_TCMALLOC_BAZEL_VERSION}")
