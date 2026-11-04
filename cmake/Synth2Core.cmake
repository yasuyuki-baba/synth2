find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_custom_target(Synth2NamingLint
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/lint_naming.py
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    VERBATIM)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang"
   AND NOT CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC"
   AND CMAKE_GENERATOR MATCHES "Ninja|Makefiles")
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
    set(synth2_header_audit_supported ON)
    add_custom_target(Synth2HeaderAudit
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/audit_dsp_headers.py
            --build-dir ${CMAKE_BINARY_DIR} --compiler ${CMAKE_CXX_COMPILER}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        VERBATIM)
endif()
set(core_generated_dir ${CMAKE_CURRENT_BINARY_DIR}/generated/core)
add_custom_command(OUTPUT ${core_generated_dir}/CoreResources.cpp ${core_generated_dir}/CoreResources.h
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/embed_core_resources.py
        ${CMAKE_CURRENT_SOURCE_DIR} ${core_generated_dir}
    DEPENDS tools/embed_core_resources.py resources/init.sy1 resources/parameter_names.json
    VERBATIM)
add_library(Synth2Resources STATIC ${core_generated_dir}/CoreResources.cpp)
target_include_directories(Synth2Resources PUBLIC ${core_generated_dir})
target_compile_features(Synth2Resources PRIVATE cxx_std_20)
set_target_properties(Synth2Resources PROPERTIES POSITION_INDEPENDENT_CODE ON CXX_EXTENSIONS OFF)
add_library(Synth2Zip STATIC third_party/miniz/miniz.c third_party/miniz/miniz_tdef.c
    third_party/miniz/miniz_tinfl.c third_party/miniz/miniz_zip.c)
target_include_directories(Synth2Zip PUBLIC third_party/miniz)
set_target_properties(Synth2Zip PROPERTIES POSITION_INDEPENDENT_CODE ON)
add_library(Synth2Xml STATIC third_party/tinyxml2/tinyxml2.cpp)
target_include_directories(Synth2Xml PUBLIC third_party/tinyxml2)
target_compile_features(Synth2Xml PUBLIC cxx_std_11)
set_target_properties(Synth2Xml PROPERTIES POSITION_INDEPENDENT_CODE ON CXX_EXTENSIONS OFF)
include(cmake/Synth1PatchCore.cmake)
add_library(Synth2Core STATIC
    projects/Synth2/src/SynthEngine.cpp projects/Synth2/src/VoiceDispatcher.cpp
    projects/Synth2/src/SynthVoice.cpp projects/Synth2/src/VoiceCorePool.cpp projects/Synth2/src/SynthDiagnostics.cpp projects/Synth2/src/dsp/OutputPipeline.cpp projects/Synth2/src/dsp/SharedModulationRuntime.cpp
    projects/Synth2/src/render/RenderPolicy.cpp projects/Synth2/src/patch/PatchLoader.cpp projects/Synth2/src/patch/PatchMapper.cpp projects/Synth2/src/patch/Synth1PatchMapper.cpp
    projects/Synth2/src/patch/PatchFormat.cpp
    projects/Synth2/src/host/ProcessorBridge.cpp projects/Synth2/src/host/PatchLibrary.cpp
    projects/Synth2/src/host/PatchCatalog.cpp)
target_include_directories(Synth2Core PUBLIC projects/Synth2/src projects/Synth2/src/host)
target_link_libraries(Synth2Core PUBLIC Synth1PatchCore PRIVATE Synth2Resources Synth2Zip Synth2Xml)
target_compile_features(Synth2Core PUBLIC cxx_std_20)
set_target_properties(Synth2Core PROPERTIES POSITION_INDEPENDENT_CODE ON CXX_EXTENSIONS OFF)
