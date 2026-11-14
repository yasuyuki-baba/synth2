# Keep the bundled reader independent of optional system compression/crypto
# libraries. RAR decompression and CRC/BLAKE2 verification are built in.
# Scope upstream's general-purpose options and build settings to this function.
function(synth2_add_rar)
    set(BUILD_SHARED_LIBS OFF)
    set(BUILD_TESTING OFF)
    # RAR reading needs no regex engine. AUTO would probe PCRE/libgcc on MSVC.
    set(POSIX_REGEX_LIB NONE)
    foreach(feature MBEDTLS NETTLE OPENSSL MD LIBB2 LZ4 LZO LZMA ZSTD ZLIB BZip2
            LIBXML2 BSDXML EXPAT WIN32_XMLLITE PCREPOSIX PCRE2POSIX LIBGCC CNG
            TAR CPIO CAT UNZIP XATTR ACL TEST COVERAGE INSTALL CLANG_TIDY WERROR)
        set(ENABLE_${feature} OFF)
    endforeach()
    set(DONT_FAIL_ON_CRC_ERROR OFF)
    add_subdirectory(third_party/libarchive EXCLUDE_FROM_ALL)
    set_target_properties(archive_static PROPERTIES
        POSITION_INDEPENDENT_CODE ON C_VISIBILITY_PRESET hidden)
    target_compile_definitions(archive_static INTERFACE LIBARCHIVE_STATIC)
    add_library(Synth2Rar ALIAS archive_static)
endfunction()
synth2_add_rar()
