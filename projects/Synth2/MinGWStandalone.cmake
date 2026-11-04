# Development-only Windows APP build using the same SDK host/editor and bridge.
# The pinned SDK's general CMake entry also configures unrelated WebView/VST SDKs
# and uses MSVC library names. Keep this narrowly scoped to MinGW APP validation.
set(ip ${IPLUG2_DIR}/IPlug)
set(ig ${IPLUG2_DIR}/IGraphics)
set(wdl ${IPLUG2_DIR}/WDL)
set(deps ${IPLUG2_DIR}/Dependencies)
set(app_sources)
foreach(name APIBase Parameter Paths PluginBase Processor Timer)
    list(APPEND app_sources ${ip}/IPlug${name}.cpp)
endforeach()
foreach(name IPlugAPP IPlugAPP_dialog IPlugAPP_host IPlugAPP_main)
    list(APPEND app_sources ${ip}/APP/${name}.cpp)
endforeach()
foreach(name IControl IGraphics IGraphicsEditorDelegate)
    list(APPEND app_sources ${ig}/${name}.cpp)
endforeach()
foreach(name IControls IPopupMenuControl ITextEntryControl)
    list(APPEND app_sources ${ig}/Controls/${name}.cpp)
endforeach()
foreach(name asio asiodrivers asiolist iasiothiscallresolver)
    list(APPEND app_sources ${deps}/IPlug/RTAudio/include/${name}.cpp)
endforeach()
add_executable(Synth2-app WIN32
    Synth2.cpp Synth2Editor.cpp resources/main.rc
    ${app_sources} ${ig}/Platforms/IGraphicsWin.cpp ${wdl}/win32_utf8.c
    ${deps}/IPlug/RTAudio/RtAudio.cpp ${deps}/IPlug/RTMidi/RtMidi.cpp)
target_include_directories(Synth2-app PRIVATE
    . resources ${ip} ${ip}/APP ${ip}/Extras ${wdl} ${wdl}/libpng ${wdl}/zlib
    ${ig} ${ig}/Controls ${ig}/Drawing ${ig}/Platforms ${ig}/Extras
    ${deps}/IGraphics/STB ${deps}/IGraphics/NanoVG/src ${deps}/IGraphics/NanoSVG/src
    ${deps}/IGraphics/glad_GL2/include ${deps}/IGraphics/glad_GL2/src
    ${deps}/IPlug/RTAudio ${deps}/IPlug/RTAudio/include ${deps}/IPlug/RTMidi)
target_compile_definitions(Synth2-app PRIVATE
    APP_API IPLUG_EDITOR=1 IPLUG_DSP=1 IGRAPHICS_NANOVG IGRAPHICS_GL2
    _USE_MATH_DEFINES NOMINMAX __WINDOWS_DS__ __WINDOWS_MM__ __WINDOWS_ASIO__)
target_compile_options(Synth2-app PRIVATE
    $<$<COMPILE_LANGUAGE:CXX>:-Wno-multichar;-Wno-deprecated-declarations>
)
# RC generators do not propagate target_compile_options.
set_source_files_properties(resources/main.rc PROPERTIES
    COMPILE_FLAGS "--preprocessor=${CMAKE_C_COMPILER} --preprocessor-arg=-E --preprocessor-arg=-xc --preprocessor-arg=-DRC_INVOKED")
target_link_libraries(Synth2-app PRIVATE Synth2Core Synth2EditorFonts
    shlwapi comctl32 wininet dsound winmm opengl32 gdi32 comdlg32 ole32 uuid dwmapi)
set_target_properties(Synth2-app PROPERTIES CXX_EXTENSIONS OFF OUTPUT_NAME Synth2
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/out)
