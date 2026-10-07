cmake_minimum_required(VERSION 3.22)

project(RecklessMuse VERSION 1.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")

option(RM_COPY_AFTER_BUILD "Copy the plug-ins to ~/Library/Audio/Plug-Ins after building" OFF)
option(RM_BUILD_TESTS "Build the RecklessMuse test runner" ON)

add_subdirectory(external/JUCE)

# ---------------------------------------------------------------------------
# Factory presets (one XML file per preset, embedded as binary data)
# ---------------------------------------------------------------------------
file(GLOB RM_PRESET_FILES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Presets/*.xml")
juce_add_binary_data(RecklessMusePresets
    HEADER_NAME PresetData.h
    NAMESPACE PresetData
    SOURCES ${RM_PRESET_FILES})
set_target_properties(RecklessMusePresets PROPERTIES POSITION_INDEPENDENT_CODE ON)

set(RM_ENGINE_SOURCES
    Source/Params.cpp
    Source/engine/Voice.cpp
    Source/engine/TimbreEngine.cpp
    Source/engine/SynthEngine.cpp
    Source/engine/NoteGenerator.cpp
    Source/presets/PresetManager.cpp)

set(RM_COMMON_DEFINITIONS
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

# ---------------------------------------------------------------------------
# Plug-in
# ---------------------------------------------------------------------------
juce_add_plugin(RecklessMuse
    COMPANY_NAME "Reckless Audio"
    COMPANY_WEBSITE "https://github.com/RecklessBoise/RecklessMuse"
    BUNDLE_ID com.recklessaudio.recklessmuse
    PLUGIN_MANUFACTURER_CODE Rklb
    PLUGIN_CODE RMus
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "RecklessMuse"
    DESCRIPTION "8-voice bitimbral analog-modeled polysynth"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD ${RM_COPY_AFTER_BUILD}
    VST3_CATEGORIES Instrument Synth
    AU_MAIN_TYPE kAudioUnitType_MusicDevice)

target_sources(RecklessMuse PRIVATE
    ${RM_ENGINE_SOURCES}
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/ui/MuseLookAndFeel.cpp
    Source/ui/Controls.cpp
    Source/ui/Display.cpp
    Source/ui/MainPanel.cpp
    Source/ui/MatrixOverlay.cpp)

target_include_directories(RecklessMuse PRIVATE Source)
target_compile_definitions(RecklessMuse PUBLIC ${RM_COMMON_DEFINITIONS})

target_link_libraries(RecklessMuse
    PRIVATE
        RecklessMusePresets
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# ---------------------------------------------------------------------------
# Tests (DSP sanity checks + render of every factory preset)
# ---------------------------------------------------------------------------
if(RM_BUILD_TESTS)
    juce_add_console_app(RecklessMuseTests PRODUCT_NAME "RecklessMuseTests")
    target_sources(RecklessMuseTests PRIVATE ${RM_ENGINE_SOURCES} tests/TestMain.cpp)
    target_include_directories(RecklessMuseTests PRIVATE Source)
    target_compile_definitions(RecklessMuseTests PRIVATE ${RM_COMMON_DEFINITIONS})
    target_link_libraries(RecklessMuseTests
        PRIVATE
            RecklessMusePresets
            juce::juce_audio_processors
            juce::juce_dsp
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags)
endif()

# ---------------------------------------------------------------------------
# UI snapshot tool (renders the editor to PNG, used for docs and layout checks)
# ---------------------------------------------------------------------------
if(RM_BUILD_TESTS)
    juce_add_console_app(RecklessMuseSnapshot PRODUCT_NAME "RecklessMuseSnapshot")
    target_sources(RecklessMuseSnapshot PRIVATE
        ${RM_ENGINE_SOURCES}
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/ui/MuseLookAndFeel.cpp
        Source/ui/Controls.cpp
        Source/ui/Display.cpp
        Source/ui/MainPanel.cpp
        Source/ui/MatrixOverlay.cpp
        tests/Snapshot.cpp)
    target_include_directories(RecklessMuseSnapshot PRIVATE Source)
    target_compile_definitions(RecklessMuseSnapshot PRIVATE ${RM_COMMON_DEFINITIONS}
        JucePlugin_Name="RecklessMuse" JUCE_STANDALONE_APPLICATION=1)
    target_link_libraries(RecklessMuseSnapshot
        PRIVATE
            RecklessMusePresets
            juce::juce_audio_utils
            juce::juce_dsp
        PUBLIC
            juce::juce_recommended_config_flags)
endif()
