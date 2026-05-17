include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.17.0
)

FetchContent_Declare(
    abseil-cpp
    GIT_REPOSITORY https://github.com/abseil/abseil-cpp.git
    GIT_TAG 20260107.1
)

FetchContent_Declare(
    re2
    GIT_REPOSITORY https://github.com/google/re2.git
    GIT_TAG 2025-11-05
)

set(ABSL_PROPAGATE_CXX_STD ON  CACHE BOOL "" FORCE)
set(RE2_BUILD_TESTING       OFF CACHE BOOL "" FORCE)
set(RE2_BUILD_BENCHMARK     OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(googletest)

set(CMAKE_SKIP_INSTALL_RULES ON)
FetchContent_MakeAvailable(abseil-cpp re2)
set(CMAKE_SKIP_INSTALL_RULES OFF)
