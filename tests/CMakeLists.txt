# Hash regression test: links the real scrypt^2 core (with the same per-arch
# assembly the miner uses) and locks the output against golden vectors.

set(_scrypt_core_sources
    ${CMAKE_SOURCE_DIR}/src/algo/scrypt.c
    ${CMAKE_SOURCE_DIR}/src/algo/sha2.c)

if(VRM_USE_ASM)
    if(VRM_ARCH_X86_64)
        list(APPEND _scrypt_core_sources
             ${CMAKE_SOURCE_DIR}/src/asm/sha2-x64.S
             ${CMAKE_SOURCE_DIR}/src/asm/scrypt-x64.S)
    elseif(VRM_ARCH_X86)
        list(APPEND _scrypt_core_sources
             ${CMAKE_SOURCE_DIR}/src/asm/sha2-x86.S
             ${CMAKE_SOURCE_DIR}/src/asm/scrypt-x86.S)
    elseif(VRM_ARCH_ARM)
        list(APPEND _scrypt_core_sources
             ${CMAKE_SOURCE_DIR}/src/asm/sha2-arm.S
             ${CMAKE_SOURCE_DIR}/src/asm/scrypt-arm.S)
    endif()
endif()

if(VRM_ARCH_AARCH64)
    list(APPEND _scrypt_core_sources ${CMAKE_SOURCE_DIR}/src/algo/scrypt-aarch64-neon.c)
endif()

set(_test_sources test_hash.c ${_scrypt_core_sources})

add_executable(test_hash ${_test_sources})

target_include_directories(test_hash PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR})

# scrypt.c/sha2.c include miner.h which pulls in curl/jansson/pthread headers;
# link the libs so their include directories are available.
target_link_libraries(test_hash PRIVATE
    CURL::libcurl
    Jansson::Jansson
    Threads::Threads)

if(NOT MSVC)
    target_compile_options(test_hash PRIVATE -fno-strict-aliasing -Wno-pointer-sign)
    if(WIN32)
        target_link_options(test_hash PRIVATE -Wl,--stack,10485760)
        target_link_libraries(test_hash PRIVATE ws2_32 crypt32)
    endif()
endif()

add_test(NAME hash_vectors COMMAND test_hash)

add_executable(test_scrypt_3way_equiv test_scrypt_3way_equiv.c ${_scrypt_core_sources})
target_include_directories(test_scrypt_3way_equiv PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(test_scrypt_3way_equiv PRIVATE
    CURL::libcurl
    Jansson::Jansson
    Threads::Threads)
if(NOT MSVC)
    target_compile_options(test_scrypt_3way_equiv PRIVATE -fno-strict-aliasing -Wno-pointer-sign)
    if(WIN32)
        target_link_options(test_scrypt_3way_equiv PRIVATE -Wl,--stack,10485760)
        target_link_libraries(test_scrypt_3way_equiv PRIVATE ws2_32 crypt32)
    endif()
endif()
add_test(NAME scrypt_3way_equiv COMMAND test_scrypt_3way_equiv)
