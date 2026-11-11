cmake_minimum_required(VERSION 3.22)

project(SafeSide VERSION 2.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# On Mac build a universal binary (Apple Silicon + Intel) that runs on macOS 11+
if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "" FORCE)
    if(NOT CMAKE_OSX_ARCHITECTURES)
        set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
    endif()
endif()

# ---- Mono-contract test of the DSP engine (pure C++, no JUCE) ---------------
add_executable(SafeSideContractTest tests/contract_test.cpp)
add_executable(SafeSideRender tests/render.cpp)
enable_testing()
add_test(NAME mono_contract COMMAND SafeSideContractTest)
option(SS_ONLY_TESTS "Configure only the DSP tests (no JUCE download)" OFF)
if(SS_ONLY_TESTS)
    return()
endif()

# ---- JUCE ------------------------------------------------------------------
# Use a local JUCE checkout if JUCE_DIR is given, otherwise download it.
if(DEFINED JUCE_DIR)
    add_subdirectory(${JUCE_DIR} JUCE EXCLUDE_FROM_ALL)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.9
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

# ---- Plugin ----------------------------------------------------------------
if(APPLE)
    set(SS_FORMATS VST3 AU)
    set(SS_COPY TRUE)      # installs into ~/Library/Audio/Plug-Ins after building
else()
    set(SS_FORMATS VST3)
    set(SS_COPY FALSE)
endif()

juce_add_plugin(SafeSide
    COMPANY_NAME                "BOBA"
    PLUGIN_MANUFACTURER_CODE    Boba
    PLUGIN_CODE                 SfS2
    PRODUCT_NAME                "SAFE-SIDE"
    DESCRIPTION                 "Mono-safe stereo widener with six width characters"
    FORMATS                     ${SS_FORMATS}
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     ${SS_COPY}
    VST3_CATEGORIES             Fx Spatial
    AU_MAIN_TYPE                kAudioUnitType_Effect
    BUNDLE_ID                   com.boba.safeside)

set(SS_SOURCES
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_sources(SafeSide PRIVATE ${SS_SOURCES})
target_include_directories(SafeSide PRIVATE Source)

# Figtree font (SIL Open Font License) embedded as binary data
juce_add_binary_data(SSBinaryData SOURCES
    Resources/fonts/Figtree-Regular.ttf
    Resources/fonts/Figtree-SemiBold.ttf
    Resources/fonts/Figtree-Bold.ttf
    Resources/fonts/Figtree-ExtraBold.ttf)
set_target_properties(SSBinaryData PROPERTIES POSITION_INDEPENDENT_CODE ON)

set(SS_DEFINITIONS
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

if(UNIX AND NOT APPLE)
    # keep the Linux build light (only used for testing)
    list(APPEND SS_DEFINITIONS JUCE_USE_XRANDR=0 JUCE_USE_XINERAMA=0 JUCE_USE_XCURSOR=0)
endif()

target_compile_definitions(SafeSide PUBLIC ${SS_DEFINITIONS})
if(MSVC)
    target_compile_options(SafeSide PRIVATE /utf-8)
endif()

target_link_libraries(SafeSide
    PRIVATE
        SSBinaryData
        juce::juce_audio_processors
        juce::juce_gui_basics
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# ---- UI snapshot (optional, renders the editor to a PNG) --------------------
option(SS_BUILD_SNAPSHOT "Build the UI snapshot tool" OFF)
if(SS_BUILD_SNAPSHOT)
    juce_add_console_app(SafeSideSnapshot PRODUCT_NAME "SafeSideSnapshot")
    target_sources(SafeSideSnapshot PRIVATE tests/ui_snapshot.cpp ${SS_SOURCES})
    target_include_directories(SafeSideSnapshot PRIVATE Source)
    target_compile_definitions(SafeSideSnapshot PRIVATE ${SS_DEFINITIONS}
        JUCE_STANDALONE_APPLICATION=1 JUCE_MODAL_LOOPS_PERMITTED=1
        JucePlugin_Name="SAFE-SIDE")
    target_link_libraries(SafeSideSnapshot PRIVATE
        SSBinaryData
        juce::juce_audio_processors juce::juce_gui_basics
        juce::juce_recommended_config_flags)
endif()

# ---- Host test (optional): loads the built VST3 like a DAW -------------------
option(SS_BUILD_HOSTTEST "Build the VST3 host test" OFF)
if(SS_BUILD_HOSTTEST)
    juce_add_console_app(SafeSideHostTest PRODUCT_NAME "SafeSideHostTest")
    target_sources(SafeSideHostTest PRIVATE tests/host_test.cpp)
    target_compile_definitions(SafeSideHostTest PRIVATE ${SS_DEFINITIONS}
        JUCE_PLUGINHOST_VST3=1 JUCE_STANDALONE_APPLICATION=1 JUCE_MODAL_LOOPS_PERMITTED=1)
    target_link_libraries(SafeSideHostTest PRIVATE
        juce::juce_audio_processors juce::juce_gui_basics juce::juce_recommended_config_flags)
endif()
