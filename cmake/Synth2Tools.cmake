# Public Python tests render with Probe and inspect WAVs with CompareWaves.
# Keep these test dependencies available even when optional tools are disabled.
add_executable(Synth2Probe tools/synth2_probe.cpp)
target_link_libraries(Synth2Probe PRIVATE Synth2Core)
add_executable(CompareWaves tools/compare_waves.cpp)
target_include_directories(CompareWaves PRIVATE projects/Synth2/src)
set(synth2_tool_targets Synth2Probe CompareWaves)

if(SYNTH2_BUILD_TOOLS)
    add_executable(Synth2LoadingProbe tools/synth2_loading_probe.cpp)
    target_link_libraries(Synth2LoadingProbe PRIVATE Synth2Core)
    add_executable(ExportEditorLayout tools/export_editor_layout.cpp)
    target_link_libraries(ExportEditorLayout PRIVATE Synth2Core)
    add_executable(ExportBridgeBaseline tools/export_bridge_baseline.cpp)
    target_link_libraries(ExportBridgeBaseline PRIVATE Synth2Core)
    add_executable(Synth2HostBenchmark tools/synth2_host_benchmark.cpp)
    target_link_libraries(Synth2HostBenchmark PRIVATE Synth2Core)
    list(APPEND synth2_tool_targets Synth2LoadingProbe ExportEditorLayout
        ExportBridgeBaseline Synth2HostBenchmark)
endif()
foreach(target IN LISTS synth2_tool_targets)
    target_compile_features(${target} PRIVATE cxx_std_20)
    set_target_properties(${target} PROPERTIES CXX_EXTENSIONS OFF)
endforeach()
