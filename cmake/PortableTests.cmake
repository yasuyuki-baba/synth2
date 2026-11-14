include(FetchContent)
FetchContent_Declare(googletest GIT_REPOSITORY https://github.com/google/googletest.git GIT_TAG v1.15.2)
if(MSVC)
    # Match the parent runtime: portable MSVC builds default to /MD; the native
    # SDK selects /MT. Avoid mixing the two in the test executable.
    if(NOT DEFINED CMAKE_MSVC_RUNTIME_LIBRARY OR CMAKE_MSVC_RUNTIME_LIBRARY MATCHES "DLL")
        set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    else()
        set(gtest_force_shared_crt OFF CACHE BOOL "" FORCE)
    endif()
endif()
FetchContent_MakeAvailable(googletest)
add_executable(Synth2PatchTests
    tests/RenderPolicyTest.cpp
    tests/StateRepresentationTest.cpp
    tests/BankBoundaryTest.cpp
    tests/HistoryQueueBehaviorTest.cpp
    tests/PatchImportWorkspaceTest.cpp
    tests/PlaybackBehaviorTest.cpp
    tests/ModulationBehaviorTest.cpp
    tests/LfoPhasePrecisionTest.cpp
    tests/OversamplingTest.cpp
    tests/OscillatorReadTest.cpp
    tests/PwmPrecisionTest.cpp
    tests/FmPhaseCorrectionTest.cpp
    tests/EditorKeyboardTest.cpp
    tests/EditorInteractionTest.cpp
    tests/DspBasicsTest.cpp
    tests/BoundedPrecisionTest.cpp
    tests/ContinuousLevelTest.cpp
    tests/OutputPipelinePathTest.cpp
    tests/EffectsBehaviorTest.cpp
    tests/GeneratedInputTest.cpp
    tests/LoadingBehaviorTest.cpp
    tests/PatchFormatTest.cpp
    tests/PerformanceBehaviorTest.cpp
    tests/ChorusPathTest.cpp
    tests/DelayPathTest.cpp
    tests/EffectLifecycleTest.cpp
    tests/ProcessorBridgeTest.cpp
    tests/ProcessorContractsTest.cpp
    tests/ProcessorStateTest.cpp
    tests/HostLifecycleTest.cpp
    tests/PatchCatalogTest.cpp
    tests/PresetFilesTest.cpp
    tests/PatchLibraryTest.cpp
    tests/RarBankTest.cpp
    tests/EditorLayoutTest.cpp
    tests/ArpeggiatorBehaviorTest.cpp
    tests/FilterEnvelopePrecisionTest.cpp
    tests/FilterPathTest.cpp
    tests/FilterBehaviorTest.cpp
    tests/OutputTonePrecisionTest.cpp
    tests/EqualizerBehaviorTest.cpp
)
target_link_libraries(Synth2PatchTests PRIVATE Synth2Core Synth2Zip Synth2Xml Synth2Resources GTest::gtest_main)
target_include_directories(Synth2PatchTests PRIVATE projects/Synth2/src tests)
add_test(NAME PatchCompatibilityTests COMMAND Synth2PatchTests)
set_tests_properties(PatchCompatibilityTests PROPERTIES LABELS "portable;cpp")

target_compile_definitions(Synth2PatchTests PRIVATE SYNTH2_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")

find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_test(NAME NamingLint
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/lint_naming.py)
add_test(NAME NamingLintTests
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/test_lint_naming.py -v)
set_tests_properties(NamingLint NamingLintTests PROPERTIES LABELS "portable;python;lint" TIMEOUT 60)
if(synth2_header_audit_supported)
    add_test(NAME HeaderBoundaryAudit
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/audit_dsp_headers.py
            --build-dir ${CMAKE_BINARY_DIR} --compiler ${CMAKE_CXX_COMPILER})
    set_tests_properties(HeaderBoundaryAudit PROPERTIES LABELS "portable;cpp;lint" TIMEOUT 120)
endif()
add_test(
    NAME GainRegressionTests
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/test_gain_regression.py -v
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
set_tests_properties(GainRegressionTests PROPERTIES
    ENVIRONMENT "SYNTH2_PROBE_BIN=$<TARGET_FILE:Synth2Probe>"
    TIMEOUT 120
)
add_test(NAME WaveComparisonTests
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/test_compare_waves.py -v)
set_tests_properties(WaveComparisonTests PROPERTIES
    ENVIRONMENT "SYNTH2_COMPARE_BIN=$<TARGET_FILE:CompareWaves>;SYNTH2_PROBE_BIN=$<TARGET_FILE:Synth2Probe>"
    TIMEOUT 60)


add_test(NAME MeasurementToolsTests
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/test_measurement_common.py -v)
set_tests_properties(MeasurementToolsTests PROPERTIES TIMEOUT 60)

add_test(NAME BuildToolsTests
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/test_build_tools.py -v)
set_tests_properties(BuildToolsTests PROPERTIES LABELS "portable;python" TIMEOUT 60)

# Stable aggregate names preserve existing ctest -R invocations.
set_tests_properties(GainRegressionTests WaveComparisonTests MeasurementToolsTests
    PROPERTIES LABELS "portable;python")
set_tests_properties(PatchCompatibilityTests PROPERTIES LABELS "portable;cpp")

# Heap interception is isolated so it cannot affect the ordinary test executable.
add_executable(Synth2RealtimeTests tests/RealtimeBehaviorTest.cpp tests/support/HeapActivity.cpp)
target_include_directories(Synth2RealtimeTests PRIVATE tests)
target_link_libraries(Synth2RealtimeTests PRIVATE Synth2Core GTest::gtest_main)
add_test(NAME RealtimeBehaviorTests COMMAND Synth2RealtimeTests)
set_tests_properties(RealtimeBehaviorTests PROPERTIES LABELS "portable;cpp;realtime" TIMEOUT 120)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_definitions(Synth2RealtimeTests PRIVATE SYNTH2_TEST_WRAP_C_HEAP=1)
    target_link_options(Synth2RealtimeTests PRIVATE
        -Wl,--wrap=malloc -Wl,--wrap=calloc -Wl,--wrap=realloc -Wl,--wrap=free)
endif()

set_target_properties(Synth2PatchTests Synth2RealtimeTests PROPERTIES CXX_EXTENSIONS OFF)

add_test(NAME RenderDiagnosticsTests
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/test_render_diagnostics.py $<TARGET_FILE:Synth2Probe>)
set_tests_properties(RenderDiagnosticsTests PROPERTIES LABELS "portable;python" TIMEOUT 30)

add_test(NAME BuildVersionTests
    COMMAND ${CMAKE_COMMAND} -E env "CMAKE_COMMAND=${CMAKE_COMMAND}"
        ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/test_build_version.py -v)
set_tests_properties(BuildVersionTests PROPERTIES LABELS "portable;python" TIMEOUT 60)
