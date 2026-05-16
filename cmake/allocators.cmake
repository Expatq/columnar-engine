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

find_program(COLUMNAR_BAZEL_EXECUTABLE bazel)

if(NOT COLUMNAR_BAZEL_EXECUTABLE)
    message(FATAL_ERROR
        "google/tcmalloc requires Bazel because upstream CMake integration is not reliable. "
        "Install Bazel or reconfigure with -DCOLUMNAR_USE_TCMALLOC=OFF."
    )
endif()

FetchContent_Declare(
    google_tcmalloc
    GIT_REPOSITORY https://github.com/google/tcmalloc.git
    GIT_TAG ${COLUMNAR_TCMALLOC_GIT_TAG}
    GIT_SHALLOW TRUE
)

FetchContent_Populate(google_tcmalloc)

set(COLUMNAR_TCMALLOC_LIBRARY
    ${google_tcmalloc_SOURCE_DIR}/bazel-bin/tcmalloc/libtcmalloc.a
)

add_custom_command(
    OUTPUT ${COLUMNAR_TCMALLOC_LIBRARY}
    COMMAND
        ${COLUMNAR_BAZEL_EXECUTABLE}
        build
        --compilation_mode=opt
        //tcmalloc
    WORKING_DIRECTORY ${google_tcmalloc_SOURCE_DIR}
    COMMENT "Building google/tcmalloc with Bazel"
    VERBATIM
)

add_custom_target(
    columnar_build_tcmalloc
    DEPENDS ${COLUMNAR_TCMALLOC_LIBRARY}
)

add_library(columnar_google_tcmalloc STATIC IMPORTED GLOBAL)
add_dependencies(columnar_google_tcmalloc columnar_build_tcmalloc)

set_target_properties(
    columnar_google_tcmalloc
    PROPERTIES
    IMPORTED_LOCATION ${COLUMNAR_TCMALLOC_LIBRARY}
)

target_link_libraries(
    columnar_tcmalloc
    INTERFACE
    columnar_google_tcmalloc
)

message(STATUS "google/tcmalloc: enabled via Bazel")
