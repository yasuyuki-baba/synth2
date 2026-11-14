# Keep the bundled reader independent of optional system compression/crypto
# libraries. RAR/7z formats and CRC/BLAKE2 verification are provided by libarchive;
# liblzma supplies LZMA/LZMA2 decoding for 7z.
# Scope upstream's general-purpose options and build settings to this function.
function(synth2_add_archives)
    # Honor scoped settings even when upstream set(CACHE) supplies a default.
    set(CMAKE_POLICY_DEFAULT_CMP0126 NEW)
    set(BUILD_SHARED_LIBS OFF)
    set(BUILD_TESTING OFF)
    # RAR reading needs no regex engine. AUTO would probe PCRE/libgcc on MSVC.
    set(POSIX_REGEX_LIB NONE)
    set(XZ_THREADS no)
    set(XZ_ENCODERS "")
    foreach(feature NLS DOC TOOL_XZ TOOL_XZDEC TOOL_LZMADEC TOOL_LZMAINFO
            TOOL_SCRIPTS MICROLZMA_ENCODER MICROLZMA_DECODER LZIP_DECODER)
        set(XZ_${feature} OFF)
    endforeach()
    add_subdirectory(third_party/xz EXCLUDE_FROM_ALL)
    set_target_properties(liblzma PROPERTIES
        POSITION_INDEPENDENT_CODE ON C_VISIBILITY_PRESET hidden)
    target_include_directories(liblzma INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/third_party/xz/src/liblzma/api>)
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
    # Wire the vendored target explicitly: FindLibLZMA probes a built system
    # library and cannot try-link a target that is part of this build.
    target_compile_definitions(archive_static PRIVATE HAVE_LZMA_H=1 HAVE_LIBLZMA=1)
    target_link_libraries(archive_static PRIVATE liblzma)
    add_library(Synth2Archives ALIAS archive_static)
endfunction()
synth2_add_archives()
