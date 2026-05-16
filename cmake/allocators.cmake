include(FetchContent)

option(
    COLUMNAR_USE_TCMALLOC
    "Use google/tcmalloc for Linux x86-64 executables"
    ON
)

set(
    COLUMNAR_TCMALLOC_GIT_TAG
    "master"
    CACHE STRING "Pinned google/tcmalloc git ref"
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

FetchContent_Declare(
    google_tcmalloc
    GIT_REPOSITORY https://github.com/google/tcmalloc.git
    GIT_TAG ${COLUMNAR_TCMALLOC_GIT_TAG}
    GIT_SHALLOW TRUE
)

set(COLUMNAR_PREV_BUILD_TESTING ${BUILD_TESTING})
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(google_tcmalloc)
set(BUILD_TESTING ${COLUMNAR_PREV_BUILD_TESTING} CACHE BOOL "" FORCE)

if(TARGET tcmalloc)
    target_link_libraries(columnar_tcmalloc INTERFACE tcmalloc)
elseif(TARGET tcmalloc::tcmalloc)
    target_link_libraries(columnar_tcmalloc INTERFACE tcmalloc::tcmalloc)
else()
    message(FATAL_ERROR
        "google/tcmalloc was fetched, but no known CMake target was exported. "
        "Check the fetched google/tcmalloc CMake targets and update cmake/allocators.cmake."
    )
endif()

message(STATUS "google/tcmalloc: enabled")
