#
# veriumMiner - modern CMake build
#
# A multi-threaded CPU miner for Verium (scrypt-squared / "VeriHash").
# Replaces the legacy autotools + Visual Studio build.
#
cmake_minimum_required(VERSION 3.16)

project(veriumminer
        VERSION 1.4.15
        DESCRIPTION "Multi-threaded CPU miner for Verium (scrypt^2)"
        LANGUAGES C CXX)

# ---------------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------------
option(USE_ASM        "Use hand-written assembly cores where available" ON)
option(MARCH_NATIVE   "Build with -march=native (single-machine builds)" OFF)
option(ENABLE_LTO     "Enable link-time optimization for Release builds"  ON)
option(BUILD_TESTS    "Build the hash regression tests"                   ON)
# MinGW Windows builds default to static so cpuminer.exe runs without MSYS DLLs.
set(_VRM_STATIC_BUILD_DEFAULT OFF)
if(WIN32 AND NOT MSVC)
    set(_VRM_STATIC_BUILD_DEFAULT ON)
endif()
option(STATIC_BUILD   "Statically link runtime/dependencies for distribution" ${_VRM_STATIC_BUILD_DEFAULT})
option(WERROR         "Treat compiler warnings as errors (CI gate)"            OFF)
option(ENABLE_ARM_CRYPTO "Experimental ARMv8 SHA2 crypto for PBKDF2 on aarch64" OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(CheckIncludeFile)
include(CheckSymbolExists)
include(CheckCSourceCompiles)

# ---------------------------------------------------------------------------
# Target architecture detection
# ---------------------------------------------------------------------------
string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" _vrm_arch)
set(VRM_ARCH_X86_64  FALSE)
set(VRM_ARCH_X86     FALSE)
set(VRM_ARCH_AARCH64 FALSE)
set(VRM_ARCH_ARM     FALSE)

if(_vrm_arch MATCHES "^(x86_64|amd64|x64)$")
    set(VRM_ARCH_X86_64 TRUE)
elseif(_vrm_arch MATCHES "^(i[3-6]86|x86)$")
    set(VRM_ARCH_X86 TRUE)
elseif(_vrm_arch MATCHES "^(aarch64|arm64)$")
    set(VRM_ARCH_AARCH64 TRUE)
elseif(_vrm_arch MATCHES "arm")
    set(VRM_ARCH_ARM TRUE)
endif()

message(STATUS "veriumMiner: target arch '${CMAKE_SYSTEM_PROCESSOR}' "
        "(x86_64=${VRM_ARCH_X86_64} x86=${VRM_ARCH_X86} "
        "aarch64=${VRM_ARCH_AARCH64} arm=${VRM_ARCH_ARM})")

# Assembly: x86/x86-64/32-bit ARM use GAS .S files. aarch64 uses scrypt-aarch64-neon.c.
# Intel macOS (x86_64) may use x86-64 assembly; Apple Silicon arm64 does not link .S scrypt.
set(VRM_USE_ASM FALSE)
if(USE_ASM AND (VRM_ARCH_X86_64 OR VRM_ARCH_X86 OR VRM_ARCH_ARM))
    if(NOT APPLE OR VRM_ARCH_X86_64)
        set(VRM_USE_ASM TRUE)
        enable_language(ASM)
    endif()
endif()

# ---------------------------------------------------------------------------
# Feature checks -> cpuminer-config.h
# ---------------------------------------------------------------------------
check_include_file("alloca.h"      HAVE_ALLOCA_H)
check_include_file("inttypes.h"    HAVE_INTTYPES_H)
check_include_file("stdint.h"      HAVE_STDINT_H)
check_include_file("stdlib.h"      HAVE_STDLIB_H)
check_include_file("strings.h"     HAVE_STRINGS_H)
check_include_file("string.h"      HAVE_STRING_H)
check_include_file("syslog.h"      HAVE_SYSLOG_H)
check_include_file("sys/endian.h"  HAVE_SYS_ENDIAN_H)
check_include_file("sys/param.h"   HAVE_SYS_PARAM_H)
check_include_file("sys/sysctl.h"  HAVE_SYS_SYSCTL_H)
check_include_file("sys/types.h"   HAVE_SYS_TYPES_H)
check_include_file("unistd.h"      HAVE_UNISTD_H)

set(STDC_HEADERS 1)
check_symbol_exists(alloca "alloca.h"  HAVE_ALLOCA_VIA_ALLOCA_H)
check_symbol_exists(alloca "stdlib.h"  HAVE_ALLOCA_VIA_STDLIB_H)
if(HAVE_ALLOCA_VIA_ALLOCA_H OR HAVE_ALLOCA_VIA_STDLIB_H OR MSVC)
    set(HAVE_ALLOCA 1)
endif()

check_symbol_exists(getopt_long "getopt.h" HAVE_GETOPT_LONG)

# BSD endian helpers: define HAVE_DECL_* to 1 only when the system provides
# them (otherwise miner.h supplies portable fallbacks).
foreach(_sym be32dec be32enc le16dec le16enc le32dec le32enc)
    string(TOUPPER "${_sym}" _SYM)
    if(HAVE_SYS_ENDIAN_H)
        check_symbol_exists(${_sym} "sys/endian.h" _have_${_sym})
    else()
        set(_have_${_sym} FALSE)
    endif()
    if(_have_${_sym})
        set(HAVE_DECL_${_SYM} 1)
    else()
        set(HAVE_DECL_${_SYM} 0)
    endif()
endforeach()

# 128-bit integer support
check_c_source_compiles("int main(void){ static __uint128_t i = 100; return (int)i; }"
                        USE_INT128)

# SIMD assembly capability probes (x86-64 only), mirroring the old configure.
set(USE_AVX  FALSE)
set(USE_AVX2 FALSE)
set(USE_XOP  FALSE)
if(VRM_USE_ASM AND VRM_ARCH_X86_64)
    include(CheckCCompilerFlag)
    check_c_compiler_flag("-mavx"  _vrm_compiler_mavx)
    check_c_compiler_flag("-mavx2" _vrm_compiler_mavx2)
    if(_vrm_compiler_mavx)
        set(USE_AVX TRUE)
    endif()
    if(_vrm_compiler_mavx2)
        set(USE_AVX  TRUE)
        set(USE_AVX2 TRUE)
    endif()
    if(USE_AVX)
        set(_vrm_saved_probe_flags "${CMAKE_REQUIRED_FLAGS}")
        set(CMAKE_REQUIRED_FLAGS "-mavx")
        check_c_source_compiles("int main(void){ asm(\"vprotd $7, %xmm0, %xmm1\"); return 0; }" USE_XOP)
        set(CMAKE_REQUIRED_FLAGS "${_vrm_saved_probe_flags}")
    endif()
endif()

# Distinct from the USE_ASM CMake option: option() leaves USE_ASM in the cache,
# so #cmakedefine would wrongly #define USE_ASM on Apple/aarch64 where no .S
# objects are linked.
if(VRM_USE_ASM)
    set(VRM_HAVE_ASM_FOR_CONFIG 1)
else()
    set(VRM_HAVE_ASM_FOR_CONFIG 0)
endif()

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/cpuminer-config.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/cpuminer-config.h"
    @ONLY)

# ---------------------------------------------------------------------------
# Version string: single-sourced from project() and refined with `git describe`
# so release binaries report the exact commit/tag they were built from.
# ---------------------------------------------------------------------------
find_package(Git QUIET)
set(VRM_GIT_REV "")
if(Git_FOUND AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.git")
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" describe --tags --dirty --always
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        OUTPUT_VARIABLE VRM_GIT_REV
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET)
endif()
if(NOT VRM_GIT_REV)
    set(VRM_GIT_REV "v${PROJECT_VERSION}")
endif()
message(STATUS "veriumMiner: version ${PROJECT_VERSION} (${VRM_GIT_REV})")

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/version.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/version.h"
    @ONLY)

# ---------------------------------------------------------------------------
# Dependencies
# ---------------------------------------------------------------------------
find_package(CURL REQUIRED)

set(CMAKE_THREAD_PREFER_PTHREAD ON)
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/FindOrFetchJansson.cmake)

# ---------------------------------------------------------------------------
# Sources
# ---------------------------------------------------------------------------
# Clang on macOS Intel mishandles GAS .macro in x86_64 .S files; fireworm
# runs nomacro.pl to rewrite macros as C preprocessor #defines before assembly.
set(VRM_ASM_X64_SHA2 "${CMAKE_CURRENT_SOURCE_DIR}/src/asm/sha2-x64.S")
set(VRM_ASM_X64_SCRYPT "${CMAKE_CURRENT_SOURCE_DIR}/src/asm/scrypt-x64.S")
if(APPLE AND VRM_ARCH_X86_64 AND VRM_USE_ASM)
    find_package(Perl REQUIRED)
    set(_vrm_gen_asm_dir "${CMAKE_CURRENT_BINARY_DIR}/generated-asm")
    file(MAKE_DIRECTORY "${_vrm_gen_asm_dir}")
    foreach(_vrm_asm_in scrypt-x64.S sha2-x64.S)
        execute_process(
            COMMAND "${PERL_EXECUTABLE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/contrib/nomacro.pl"
                "${CMAKE_CURRENT_SOURCE_DIR}/src/asm/${_vrm_asm_in}"
                "${_vrm_gen_asm_dir}/${_vrm_asm_in}"
            RESULT_VARIABLE _vrm_nomacro_rc
            ERROR_VARIABLE _vrm_nomacro_err
            ERROR_STRIP_TRAILING_WHITESPACE)
        if(NOT _vrm_nomacro_rc EQUAL 0)
            message(FATAL_ERROR
                "nomacro.pl failed on ${_vrm_asm_in}: ${_vrm_nomacro_err}")
        endif()
    endforeach()
    set(VRM_ASM_X64_SHA2 "${_vrm_gen_asm_dir}/sha2-x64.S")
    set(VRM_ASM_X64_SCRYPT "${_vrm_gen_asm_dir}/scrypt-x64.S")
    message(STATUS "veriumMiner: macOS x86_64 assembly via nomacro.pl")
endif()

# Consensus hashing core: one static library linked by cpuminer and regression tests
# so per-arch assembly (incl. nomacro output) is compiled exactly once.
set(VRM_CORE_SOURCES
    src/algo/scrypt.c
    src/algo/sha2.c)

if(VRM_USE_ASM)
    if(VRM_ARCH_X86_64)
        list(APPEND VRM_CORE_SOURCES ${VRM_ASM_X64_SHA2} ${VRM_ASM_X64_SCRYPT})
    elseif(VRM_ARCH_X86)
        list(APPEND VRM_CORE_SOURCES src/asm/sha2-x86.S src/asm/scrypt-x86.S)
    elseif(VRM_ARCH_ARM)
        list(APPEND VRM_CORE_SOURCES src/asm/sha2-arm.S src/asm/scrypt-arm.S)
    endif()
endif()

if(VRM_ARCH_AARCH64)
    list(APPEND VRM_CORE_SOURCES src/algo/scrypt-aarch64-neon.c)
endif()

add_library(vrm_scrypt_core STATIC ${VRM_CORE_SOURCES})
target_include_directories(vrm_scrypt_core PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_BINARY_DIR}")
target_link_libraries(vrm_scrypt_core PUBLIC
    CURL::libcurl
    Jansson::Jansson)

if(NOT MSVC)
    target_compile_options(vrm_scrypt_core PRIVATE
        -fno-strict-aliasing
        $<$<COMPILE_LANGUAGE:C>:-Wno-pointer-sign>
        $<$<COMPILE_LANGUAGE:C>:-Wall>)
endif()

set(VRM_SOURCES
    src/cpu-miner.c
    src/util.c
    src/api.c
    src/sysinfos.c
    src/topo.c
    src/stats.c
    src/logfmt.c
    src/pools.c
    src/onboard.c
    src/sha1.c
    src/uint256.cpp)

# getopt_long fallback for platforms that lack it (some Windows toolchains).
if(NOT HAVE_GETOPT_LONG)
    list(APPEND VRM_SOURCES compat/getopt/getopt_long.c)
endif()

if(WIN32)
    list(APPEND VRM_SOURCES compat/winansi.c compat/win_largepages.c)
endif()

add_executable(cpuminer ${VRM_SOURCES})
target_link_libraries(cpuminer PRIVATE vrm_scrypt_core)

if(ENABLE_ARM_CRYPTO AND VRM_ARCH_AARCH64)
    include(CheckCCompilerFlag)
    check_c_compiler_flag("-march=armv8-a+crypto" _vrm_arm_crypto)
    if(_vrm_arm_crypto)
        target_compile_options(cpuminer PRIVATE
            $<$<COMPILE_LANGUAGE:C>:-march=armv8-a+crypto>)
        target_compile_definitions(cpuminer PRIVATE VRM_HAVE_ARM_CRYPTO=1)
        message(STATUS "veriumMiner: ENABLE_ARM_CRYPTO (SHA2 extension point; scrypt NEON unchanged)")
    else()
        message(WARNING "veriumMiner: ENABLE_ARM_CRYPTO requested but compiler lacks armv8-a+crypto")
    endif()
endif()

target_include_directories(cpuminer PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"          # for "compat/winansi.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include"  # core headers
    "${CMAKE_CURRENT_BINARY_DIR}")         # generated cpuminer-config.h
if(NOT HAVE_GETOPT_LONG)
    target_include_directories(cpuminer PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/compat/getopt")
endif()

# For a static Windows distribution we link curl and its transitive
# dependencies from their .a archives (via pkg-config) instead of the shared
# import library, so the resulting cpuminer.exe has no MinGW DLL dependencies.
# The default (dynamic) build is unchanged.
if(STATIC_BUILD AND WIN32 AND NOT MSVC)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(PC_CURL REQUIRED libcurl)
    target_include_directories(cpuminer PRIVATE ${PC_CURL_STATIC_INCLUDE_DIRS})
    target_link_directories(cpuminer PRIVATE ${PC_CURL_STATIC_LIBRARY_DIRS})
    target_link_libraries(cpuminer PRIVATE
        ${PC_CURL_STATIC_LIBRARIES}
        jansson
        Threads::Threads)
else()
    target_link_libraries(cpuminer PRIVATE
        CURL::libcurl
        Jansson::Jansson
        Threads::Threads)
endif()

if(WIN32)
    target_link_libraries(cpuminer PRIVATE ws2_32 crypt32 gdi32 winmm)

    # Embed version info, icon, and application manifest so Windows and AV
    # scanners see a labeled Verium miner (not a nameless cpuminer-multi blob).
    set(VRM_FILE_VERSION_COMMA
        "${PROJECT_VERSION_MAJOR},${PROJECT_VERSION_MINOR},${PROJECT_VERSION_PATCH}")
    set(VRM_VERSION_STRING "${PROJECT_VERSION}")
    set(VRM_MANIFEST_VERSION "${PROJECT_VERSION}.0")
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/res/veriumminer.manifest.in"
        "${CMAKE_CURRENT_BINARY_DIR}/veriumminer.manifest"
        @ONLY)
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/res/cpuminer.rc.in"
        "${CMAKE_CURRENT_BINARY_DIR}/cpuminer.rc"
        @ONLY)
    target_sources(cpuminer PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/cpuminer.rc")
    set_source_files_properties("${CMAKE_CURRENT_BINARY_DIR}/cpuminer.rc"
        PROPERTIES COMPILE_FLAGS
        "-I \"${CMAKE_CURRENT_SOURCE_DIR}/res\" -I \"${CMAKE_CURRENT_BINARY_DIR}\"")
endif()

# ---------------------------------------------------------------------------
# Compiler/optimization flags
# ---------------------------------------------------------------------------
if(MSVC)
    target_compile_options(cpuminer PRIVATE /W3)
else()
    target_compile_options(cpuminer PRIVATE
        -fno-strict-aliasing
        $<$<COMPILE_LANGUAGE:C>:-Wno-pointer-sign>
        $<$<COMPILE_LANGUAGE:C>:-Wall>)
    if(MARCH_NATIVE)
        target_compile_options(cpuminer PRIVATE -march=native)
    endif()
    if(WIN32)
        # larger stack for the scrypt scratchpad handling
        target_link_options(cpuminer PRIVATE -Wl,--stack,10485760)
    endif()

    # Strip symbols from Release builds for smaller distributables (keep on
    # Linux/Windows; Apple's ld handles stripping differently and warns on -s).
    if(NOT APPLE)
        target_link_options(cpuminer PRIVATE $<$<CONFIG:Release>:-s>)
    endif()

    if(WERROR)
        target_compile_options(cpuminer PRIVATE -Werror)
    endif()

    # The vendored SHA1 multi-block loop trips a known GCC -Wstringop-overread
    # false positive that only surfaces when LTO inlines it into api.c. Build
    # this one file without LTO and silence the check there; -Werror stays in
    # force for the rest of the tree.
    if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
        set_source_files_properties(src/sha1.c PROPERTIES
            COMPILE_OPTIONS "-fno-lto;-Wno-stringop-overread")
    endif()

    # Static distribution build: avoid runtime DLL/.so dependencies so the
    # released binary runs on a clean machine without extra packages.
    if(STATIC_BUILD)
        target_compile_definitions(cpuminer PRIVATE CURL_STATICLIB)
        if(WIN32)
            target_link_options(cpuminer PRIVATE
                -static -static-libgcc -static-libstdc++)
        else()
            target_link_options(cpuminer PRIVATE
                -static-libgcc -static-libstdc++)
        endif()
    endif()
endif()

if(ENABLE_LTO)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT _ipo_ok OUTPUT _ipo_msg)
    if(_ipo_ok)
        set_property(TARGET cpuminer PROPERTY
                     INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
    else()
        message(STATUS "veriumMiner: LTO not supported: ${_ipo_msg}")
    endif()
endif()

install(TARGETS cpuminer RUNTIME DESTINATION bin)

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

find_program(NPM_EXECUTABLE npm)
if(NPM_EXECUTABLE AND EXISTS "${CMAKE_SOURCE_DIR}/package.json")
    add_custom_target(format-check
        COMMAND ${NPM_EXECUTABLE} run format:check
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Checking formatting with Prettier"
    )
    add_dependencies(cpuminer format-check)
endif()

message(STATUS "veriumMiner: ASM=${VRM_USE_ASM} AVX=${USE_AVX} AVX2=${USE_AVX2} "
        "XOP=${USE_XOP} INT128=${USE_INT128} getopt_long=${HAVE_GETOPT_LONG}")
