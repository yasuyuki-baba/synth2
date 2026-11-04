# Project-maintained portable patch core; see projects/Synth2/src/patch/core/README.md for provenance.
# Build without access to the private upstream repository or its Windows host.
set(synth1probe_core_dir "${CMAKE_CURRENT_LIST_DIR}/../projects/Synth2/src/patch/core")
add_library(Synth1PatchCore STATIC
    ${synth1probe_core_dir}/Synth1NativeLoading.cpp
    ${synth1probe_core_dir}/Synth1NativeLoadingStrategy.cpp
    ${synth1probe_core_dir}/Synth1ParameterDescriptor.cpp
    ${synth1probe_core_dir}/Synth1PatchParser.cpp
    ${synth1probe_core_dir}/Synth1PatchSemantics.cpp
    ${synth1probe_core_dir}/Synth1PatchFamilyRawAdaptation.cpp
    ${synth1probe_core_dir}/Synth1PatchDocumentIo.cpp
    ${synth1probe_core_dir}/Synth1PatchIoCodec.cpp
    ${synth1probe_core_dir}/Synth1PatchIo.cpp
)
target_include_directories(Synth1PatchCore PUBLIC ${synth1probe_core_dir})
target_compile_features(Synth1PatchCore PUBLIC cxx_std_20)
set_target_properties(Synth1PatchCore PROPERTIES POSITION_INDEPENDENT_CODE ON CXX_EXTENSIONS OFF)
