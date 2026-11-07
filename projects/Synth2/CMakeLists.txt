# Adapted from iPlug2OOS/TemplateProject at 9b214c1121a0da1e8db14b86a565c942a3bfea7e.
project(Synth2 VERSION ${SYNTH2_NUMERIC_VERSION} LANGUAGES C CXX)
include_directories("${SYNTH2_VERSION_DIR}")
if(NOT WIN32 AND NOT APPLE AND NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(FATAL_ERROR "Synth2 iPlug2 targets require Windows, macOS or Linux. Use Synth2Core for portable validation.")
endif()
set(IPLUG2_DIR "" CACHE PATH "Path to the pinned iPlug2 SDK checkout")
if(NOT EXISTS "${IPLUG2_DIR}/iPlug2.cmake")
    message(FATAL_ERROR "Initialize the SDK with: git submodule update --init iPlug2, or set IPLUG2_DIR to an external checkout; see docs/desktop-build.md")
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND
   NOT EXISTS "${IPLUG2_DIR}/IGraphics/Platforms/IGraphicsLinux.cpp")
    message(FATAL_ERROR "Linux requires the pinned iPlug2 fork; see dependencies.lock.json and docs/desktop-build.md")
endif()

# APP is selected explicitly; the default formats are the platform plugins.
set(synth2_iplug_default_formats VST3 CLAP)
if(APPLE)
    list(APPEND synth2_iplug_default_formats AU)
endif()
set(SYNTH2_IPLUG2_FORMATS "${synth2_iplug_default_formats}" CACHE STRING "iPlug2 formats (APP for standalone UI validation)")
set(synth2_iplug_formats ${SYNTH2_IPLUG2_FORMATS})
if(APPLE AND "APP" IN_LIST synth2_iplug_formats)
    # The SDK includes the SWELL resources and compiles the XIB into the bundle.
    # A missing XIB otherwise produces an app that cannot initialize its menu.
    foreach(resource Synth2-macOS-Info.plist.in Synth2-macOS-MainMenu.xib main.rc_mac_dlg main.rc_mac_menu)
        if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/resources/${resource}")
            message(FATAL_ERROR "The macOS standalone requires resources/${resource}")
        endif()
    endforeach()
    set(CMAKE_OBJCXX_STANDARD 20)
    set(CMAKE_OBJCXX_STANDARD_REQUIRED ON)
    set(CMAKE_OBJCXX_EXTENSIONS OFF)
endif()
if(NOT synth2_iplug_formats)
    message(FATAL_ERROR "Select at least one iPlug2 format")
endif()
set(synth2_editor_sources
    Synth2Editor.cpp
    editor/EditorKeyboardFocus.cpp
    editor/EditorControls.cpp
    editor/Synth2LibraryEditor.cpp)
if(APPLE)
    enable_language(OBJCXX)
    set(CMAKE_OBJCXX_STANDARD 20)
    set(CMAKE_OBJCXX_STANDARD_REQUIRED ON)
    set_source_files_properties(editor/EditorKeyboardFocus.cpp PROPERTIES LANGUAGE OBJCXX)
endif()
include(${CMAKE_CURRENT_LIST_DIR}/EditorFonts.cmake)
if(MINGW AND SYNTH2_IPLUG2_FORMATS STREQUAL "APP")
    include(${CMAKE_CURRENT_LIST_DIR}/MinGWStandalone.cmake)
    return()
endif()

# Build artifacts stay in the build tree unless explicitly enabled by the user.
option(IPLUG_DEPLOY_PLUGINS "Deploy iPlug2 plugins to system directories" OFF)
set(IPLUG2_CXX_STANDARD 20 CACHE STRING "C++ standard shared with Synth2Core")
# Synth2 uses IGraphics, not the SDK's optional IWebViewControl extra. The
# pinned SDK otherwise configures WIL/WebView2 downloads on every Windows build,
# even when that extra is not linked. Satisfy its target guard without enabling
# the unused feature or altering the pinned SDK checkout.
if(NOT TARGET iPlug2::Extras::IWebViewControl)
    add_library(iPlug2::Extras::IWebViewControl INTERFACE IMPORTED)
endif()
include("${IPLUG2_DIR}/iPlug2.cmake")
find_package(iPlug2 REQUIRED)
if(TARGET iPlug2_Extras_IWebViewControl_obj)
    set_target_properties(iPlug2_Extras_IWebViewControl_obj PROPERTIES EXCLUDE_FROM_ALL TRUE)
endif()
foreach(format IN LISTS synth2_iplug_formats)
    if(format STREQUAL "VST3" AND NOT IPLUG2_VST3_SUPPORTED)
        message(FATAL_ERROR "The VST3 SDK is required for the selected format")
    elseif(format STREQUAL "CLAP" AND NOT IPLUG2_CLAP_SUPPORTED)
        message(FATAL_ERROR "The CLAP SDKs are required for the selected format")
    endif()
endforeach()

iplug_add_plugin(Synth2
    SOURCES Synth2.cpp ${synth2_editor_sources} Synth2.h config.h
    FORMATS ${synth2_iplug_formats}
    UI IGRAPHICS
    DEFINES _USE_MATH_DEFINES
    LINK Synth2Core Synth2EditorFonts
)

# Native formats share the Synth2 binary name. MSVC derives both the import
# library and .exp sidecar from it, so parallel links need separate directories.
if(WIN32)
    foreach(target Synth2-vst3 Synth2-clap Synth2-app)
        if(TARGET ${target})
            set_target_properties(${target} PROPERTIES
                ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/link/${target}/$<CONFIG>")
        endif()
    endforeach()
endif()

# Keep Xcode's per-configuration bundle paths aligned with the SDK resource
# hooks and the candidate packager, which both consume build/out directly.
if(APPLE)
    foreach(target Synth2-vst3 Synth2-clap Synth2-au Synth2-app)
        if(TARGET ${target})
            foreach(configuration DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
                set_target_properties(${target} PROPERTIES
                    LIBRARY_OUTPUT_DIRECTORY_${configuration} "${CMAKE_BINARY_DIR}/out"
                    RUNTIME_OUTPUT_DIRECTORY_${configuration} "${CMAKE_BINARY_DIR}/out")
            endforeach()
        endif()
    endforeach()
endif()

if(APPLE)
    foreach(pair "vst3|VST3" "clap|CLAP" "au|AU" "app|macOS")
        string(REPLACE "|" ";" fields "${pair}")
        list(GET fields 0 suffix)
        list(GET fields 1 format)
        if(TARGET Synth2-${suffix})
            set_target_properties(Synth2-${suffix} PROPERTIES MACOSX_BUNDLE_INFO_PLIST
                "${SYNTH2_VERSION_DIR}/Synth2-${format}-Info.plist")
        endif()
    endforeach()
endif()
