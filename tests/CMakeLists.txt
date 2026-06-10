# Hash regression test: links the real scrypt^2 core (with the same per-arch
# assembly the miner uses) and locks the output against golden vectors.

add_executable(test_hash test_hash.c)
target_link_libraries(test_hash PRIVATE vrm_scrypt_core Threads::Threads)

target_include_directories(test_hash PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR})

if(NOT MSVC)
    target_compile_options(test_hash PRIVATE -fno-strict-aliasing -Wno-pointer-sign)
    if(WIN32)
        target_link_options(test_hash PRIVATE -Wl,--stack,10485760)
        target_link_libraries(test_hash PRIVATE ws2_32 crypt32)
    endif()
endif()

add_test(NAME hash_vectors COMMAND test_hash)

add_executable(test_scrypt_3way_equiv test_scrypt_3way_equiv.c)
target_link_libraries(test_scrypt_3way_equiv PRIVATE vrm_scrypt_core Threads::Threads)

target_include_directories(test_scrypt_3way_equiv PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR})

if(NOT MSVC)
    target_compile_options(test_scrypt_3way_equiv PRIVATE -fno-strict-aliasing -Wno-pointer-sign)
    if(WIN32)
        target_link_options(test_scrypt_3way_equiv PRIVATE -Wl,--stack,10485760)
        target_link_libraries(test_scrypt_3way_equiv PRIVATE ws2_32 crypt32)
    endif()
endif()

add_test(NAME scrypt_3way_equiv COMMAND test_scrypt_3way_equiv)
