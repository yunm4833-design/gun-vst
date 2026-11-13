cmake_minimum_required(VERSION 3.22)
project(BreachResonator VERSION 2.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(PLUGIN_FORMATS AU VST3 Standalone)
if(DEFINED VST2_SDK_PATH AND EXISTS "${VST2_SDK_PATH}")
    list(APPEND PLUGIN_FORMATS VST)
endif()

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(JUCE)

if(DEFINED VST2_SDK_PATH AND EXISTS "${VST2_SDK_PATH}")
    juce_set_vst2_sdk_path("${VST2_SDK_PATH}")
endif()

juce_add_plugin(BreachResonator
    COMPANY_NAME "KineticAudio"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    PLUGIN_MANUFACTURER_CODE Kntc
    PLUGIN_CODE Brch
    FORMATS ${PLUGIN_FORMATS}
    PRODUCT_NAME "BREACH Resonator")

target_sources(BreachResonator
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp)

target_compile_definitions(BreachResonator
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(BreachResonator
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
