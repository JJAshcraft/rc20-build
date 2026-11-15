cmake_minimum_required(VERSION 3.22)

project(Patina VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS version")

option(PATINA_BUILD_TESTS "Build the offline DSP smoke test" OFF)

# JUCE is pulled in automatically. To use a local copy instead, pass
#   -DFETCHCONTENT_SOURCE_DIR_JUCE=/path/to/JUCE
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW ON)
FetchContent_MakeAvailable(JUCE)

set(PATINA_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND PATINA_FORMATS AU)
endif()

juce_add_plugin(Patina
    COMPANY_NAME "Patina Audio"
    BUNDLE_ID "com.patinaaudio.patina"
    PLUGIN_MANUFACTURER_CODE Ptna
    PLUGIN_CODE Ptn1
    FORMATS ${PATINA_FORMATS}
    PRODUCT_NAME "Patina"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Distortion
    AU_MAIN_TYPE kAudioUnitType_Effect)

target_sources(Patina PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_include_directories(Patina PRIVATE Source)

target_compile_definitions(Patina PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(Patina
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

if(PATINA_BUILD_TESTS)
    # Builds the processor straight into a console app so the DSP can be
    # exercised offline (no DAW needed).
    juce_add_console_app(PatinaSmokeTest PRODUCT_NAME "PatinaSmokeTest")
    target_sources(PatinaSmokeTest PRIVATE
        Tests/SmokeTest.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp)
    target_include_directories(PatinaSmokeTest PRIVATE Source)
    target_compile_definitions(PatinaSmokeTest PRIVATE
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 PATINA_SMOKE_TEST=1)
    target_link_libraries(PatinaSmokeTest PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_recommended_config_flags)
endif()
