# Locate jansson, exposing it as the imported target Jansson::Jansson.
#
# Resolution order:
#   1. jansson's own CMake package config (Jansson::Jansson / jansson::jansson)
#   2. pkg-config
#   3. manual find_path / find_library
#   4. FetchContent (download and build) as a last resort
#
# This replaces the previously vendored compat/jansson source tree.

if(TARGET Jansson::Jansson)
    return()
endif()

# 1) CMake package config
find_package(jansson CONFIG QUIET)
if(TARGET jansson::jansson)
    add_library(Jansson::Jansson ALIAS jansson::jansson)
    message(STATUS "veriumMiner: using jansson via CMake config package")
    return()
endif()

# 2) pkg-config
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(PC_JANSSON QUIET jansson)
endif()

# 3) manual discovery
find_path(JANSSON_INCLUDE_DIR
    NAMES jansson.h
    HINTS ${PC_JANSSON_INCLUDEDIR} ${PC_JANSSON_INCLUDE_DIRS})
find_library(JANSSON_LIBRARY
    NAMES jansson libjansson
    HINTS ${PC_JANSSON_LIBDIR} ${PC_JANSSON_LIBRARY_DIRS})

if(JANSSON_INCLUDE_DIR AND JANSSON_LIBRARY)
    add_library(Jansson::Jansson UNKNOWN IMPORTED)
    set_target_properties(Jansson::Jansson PROPERTIES
        IMPORTED_LOCATION "${JANSSON_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${JANSSON_INCLUDE_DIR}")
    message(STATUS "veriumMiner: using system jansson (${JANSSON_LIBRARY})")
    return()
endif()

# 4) FetchContent fallback
message(STATUS "veriumMiner: jansson not found, fetching from source")
include(FetchContent)
set(JANSSON_BUILD_DOCS    OFF CACHE BOOL "" FORCE)
set(JANSSON_EXAMPLES      OFF CACHE BOOL "" FORCE)
set(JANSSON_WITHOUT_TESTS ON  CACHE BOOL "" FORCE)
set(JANSSON_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(jansson
    GIT_REPOSITORY https://github.com/akheron/jansson.git
    GIT_TAG        v2.14)
# CMake 4+ no longer configures subprojects that declare cmake_minimum_required
# below 3.5 (jansson 2.14 still does). Allow the fetch path on CI/macOS brew-less
# release builds until jansson bumps its minimum.
if(CMAKE_VERSION VERSION_GREATER_EQUAL "4.0")
    set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
endif()
FetchContent_MakeAvailable(jansson)

# Fetched jansson 2.14 does not always export INTERFACE include dirs; our sources
# include <jansson.h> via miner.h (cpuminer + test_hash).
FetchContent_GetProperties(jansson)
if(jansson_SOURCE_DIR)
    set(_jansson_inc_dirs "")
    if(EXISTS "${jansson_SOURCE_DIR}/include/jansson.h")
        list(APPEND _jansson_inc_dirs "${jansson_SOURCE_DIR}/include")
    endif()
    if(EXISTS "${jansson_SOURCE_DIR}/src/jansson.h")
        list(APPEND _jansson_inc_dirs "${jansson_SOURCE_DIR}/src")
    endif()
    if(jansson_BINARY_DIR AND EXISTS "${jansson_BINARY_DIR}/jansson.h")
        list(APPEND _jansson_inc_dirs "${jansson_BINARY_DIR}")
    endif()
    foreach(_jt jansson jansson_static)
        if(TARGET ${_jt} AND _jansson_inc_dirs)
            target_include_directories(${_jt} INTERFACE ${_jansson_inc_dirs})
        endif()
    endforeach()
endif()

if(TARGET jansson::jansson)
    add_library(Jansson::Jansson ALIAS jansson::jansson)
elseif(TARGET jansson)
    add_library(Jansson::Jansson ALIAS jansson)
else()
    message(FATAL_ERROR "veriumMiner: unable to provide jansson")
endif()
