cmake_minimum_required(VERSION 3.22)

project(BreachResonator VERSION 2.0.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Build universal macOS binaries" FORCE)

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.4
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

set(PLUGIN_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND PLUGIN_FORMATS AU)
endif()

if(DEFINED VST2_SDK_PATH AND EXISTS "${VST2_SDK_PATH}")
    juce_set_vst2_sdk_path("${VST2_SDK_PATH}")
    list(APPEND PLUGIN_FORMATS VST)
endif()

juce_add_plugin(BreachResonator
    COMPANY_NAME "BREACH Audio"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE Brch
    PLUGIN_CODE BrRs
    FORMATS ${PLUGIN_FORMATS}
    PRODUCT_NAME "BREACH Resonator"
)

juce_add_binary_data(BreachArtwork
    SOURCES Assets/BarlowCondensed-Regular.ttf Assets/Impact.svg
            Assets/ImpactProjectile.svg Assets/ImpactFracture.svg
            Assets/Gun0.svg Assets/Gun1.svg Assets/Gun2.svg Assets/Gun3.svg Assets/Gun4.svg
)

target_sources(BreachResonator
    PRIVATE
        Source/PluginProcessor.h
        Source/PluginProcessor.cpp
        Source/PluginEditor.h
        Source/PluginEditor.cpp
        Source/ImpactPanel.h
        Source/ImpactPanel.cpp
        Source/ImpactTelemetry.h
        Source/MangaLookAndFeel.h
        Source/MangaControls.h
        Source/SpatialDSP.h
        Source/NonlinearDSP.h
        Source/RingsStringDSP.h
        Source/FormantDSP.h
        Source/ModalDSP.h
        Source/BalanceDSP.h
        Source/DynamicsDSP.h
        Source/ThirdParty/Faust/FaustOtt.h
        Source/ReverbDSP.h
        Source/OutputEqDSP.h
        Source/OutputEqPanel.h
        Source/OutputEqPanel.cpp
        Source/RingTimeDSP.h
        Source/VoicePanels.h
        Source/VoicePanels.cpp
        Source/ThirdParty/STK/StkReverb.h
        Source/ThirdParty/Rings/RingsModalPrimitives.h
        Source/ThirdParty/STK/StkFormant.h
        Source/ThirdParty/Rings/RingsPrimitives.h
)

target_compile_definitions(BreachResonator
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_compile_options(BreachResonator
    PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/utf-8 /W3>
        $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wno-unused-parameter>
)

target_link_libraries(BreachResonator
    PRIVATE
        BreachArtwork
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

option(BREACH_BUILD_UI_TESTS "Build the UI and session compatibility smoke test" OFF)
if(BREACH_BUILD_UI_TESTS)
    add_executable(BreachUiSmoke Tests/UiSmoke.cpp Tests/ImpactPanelSmoke.cpp)
    target_include_directories(BreachUiSmoke PRIVATE Source $<TARGET_PROPERTY:BreachResonator,INCLUDE_DIRECTORIES>)
    target_compile_definitions(BreachUiSmoke PRIVATE $<TARGET_PROPERTY:BreachResonator,COMPILE_DEFINITIONS>)
    target_link_libraries(BreachUiSmoke PRIVATE BreachResonator BreachArtwork)
endif()

option(BREACH_BUILD_DSP_TESTS "Build DSP and session compatibility tests" OFF)
if(BREACH_BUILD_DSP_TESTS)
    enable_testing()
    add_executable(BreachSpatialTests Tests/SpatialDSPTests.cpp)
    target_include_directories(BreachSpatialTests PRIVATE Source)
    add_test(NAME SpatialDSP COMMAND BreachSpatialTests)
    add_executable(BreachSpatialAudition Tests/SpatialAudition.cpp)
    target_include_directories(BreachSpatialAudition PRIVATE Source $<TARGET_PROPERTY:BreachResonator,INCLUDE_DIRECTORIES>)
    target_compile_definitions(BreachSpatialAudition PRIVATE $<TARGET_PROPERTY:BreachResonator,COMPILE_DEFINITIONS>)
    target_link_libraries(BreachSpatialAudition PRIVATE BreachResonator BreachArtwork)
    add_executable(BreachSpatialIntegration Tests/SpatialIntegration.cpp)
    target_include_directories(BreachSpatialIntegration PRIVATE Source $<TARGET_PROPERTY:BreachResonator,INCLUDE_DIRECTORIES>)
    target_compile_definitions(BreachSpatialIntegration PRIVATE $<TARGET_PROPERTY:BreachResonator,COMPILE_DEFINITIONS>)
    target_link_libraries(BreachSpatialIntegration PRIVATE BreachResonator BreachArtwork)
    add_test(NAME SpatialSessionRecall COMMAND BreachSpatialIntegration --verify ${CMAKE_CURRENT_SOURCE_DIR}/Tests/Fixtures/v1_1_2)
endif()

if(BREACH_BUILD_DSP_TESTS)
    add_executable(BreachNonlinearAudition Tests/NonlinearAudition.cpp)
    target_include_directories(BreachNonlinearAudition PRIVATE Source $<TARGET_PROPERTY:BreachResonator,INCLUDE_DIRECTORIES>)
    target_compile_definitions(BreachNonlinearAudition PRIVATE $<TARGET_PROPERTY:BreachResonator,COMPILE_DEFINITIONS>)
    target_link_libraries(BreachNonlinearAudition PRIVATE BreachResonator BreachArtwork)
    add_executable(BreachNonlinearTests Tests/NonlinearDSPTests.cpp)
    target_include_directories(BreachNonlinearTests PRIVATE Source)
    add_test(NAME NonlinearDSP COMMAND BreachNonlinearTests)
    add_executable(BreachNonlinearIntegration Tests/NonlinearIntegration.cpp)
    target_include_directories(BreachNonlinearIntegration PRIVATE Source $<TARGET_PROPERTY:BreachResonator,INCLUDE_DIRECTORIES>)
    target_compile_definitions(BreachNonlinearIntegration PRIVATE $<TARGET_PROPERTY:BreachResonator,COMPILE_DEFINITIONS>)
    target_link_libraries(BreachNonlinearIntegration PRIVATE BreachResonator BreachArtwork)
    add_test(NAME NonlinearSessionRecall COMMAND BreachNonlinearIntegration --verify ${CMAKE_CURRENT_SOURCE_DIR}/Tests/Fixtures/spatial_stage1)
endif()

if(BREACH_BUILD_DSP_TESTS)
    add_executable(BreachRingsAudition Tests/RingsAudition.cpp)
    target_include_directories(BreachRingsAudition PRIVATE Source $<TARGET_PROPERTY:BreachResonator,INCLUDE_DIRECTORIES>)
    target_compile_definitions(BreachRingsAudition PRIVATE $<TARGET_PROPERTY:BreachResonator,COMPILE_DEFINITIONS>)
    target_link_libraries(BreachRingsAudition PRIVATE BreachResonator BreachArtwork)
    add_executable(BreachRingsTests Tests/RingsDSPTests.cpp)
    target_include_directories(BreachRingsTests PRIVATE Source)
    add_test(NAME RingsDSP COMMAND BreachRingsTests)
    add_executable(BreachRingsIntegration Tests/RingsIntegration.cpp)
    target_include_directories(BreachRingsIntegration PRIVATE Source $<TARGET_PROPERTY:BreachResonator,INCLUDE_DIRECTORIES>)
    target_compile_definitions(BreachRingsIntegration PRIVATE $<TARGET_PROPERTY:BreachResonator,COMPILE_DEFINITIONS>)
    target_link_libraries(BreachRingsIntegration PRIVATE BreachResonator BreachArtwork)
    add_test(NAME RingsSessionRecall COMMAND BreachRingsIntegration --verify ${CMAKE_CURRENT_SOURCE_DIR}/Tests/Fixtures/nonlinear_stage2)
endif()

# Keep third-party notices in distributed plugin/app artifacts.
foreach(format IN LISTS PLUGIN_FORMATS)
    set(breach_format_target "BreachResonator_${format}")
    if(TARGET ${breach_format_target})
        if(APPLE)
            juce_add_bundle_resources_directory(${breach_format_target} "${CMAKE_CURRENT_SOURCE_DIR}/Licenses")
        else()
            if(format STREQUAL "VST3")
                set(breach_license_destination "$<TARGET_FILE_DIR:${breach_format_target}>/../Resources/Licenses")
            else()
                set(breach_license_destination "$<TARGET_FILE_DIR:${breach_format_target}>/Licenses")
            endif()
            add_custom_command(TARGET ${breach_format_target} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E make_directory "${breach_license_destination}"
                COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/Licenses" "${breach_license_destination}"
                VERBATIM)
        endif()
    endif()
endforeach()

if(BREACH_BUILD_DSP_TESTS)
    add_executable(BreachFormantAudition Tests/FormantAudition.cpp)
    target_include_directories(BreachFormantAudition PRIVATE Source $<TARGET_PROPERTY:BreachResonator,INCLUDE_DIRECTORIES>)
    target_compile_definitions(BreachFormantAudition PRIVATE $<TARGET_PROPERTY:BreachResonator,COMPILE_DEFINITIONS>)
    target_link_libraries(BreachFormantAudition PRIVATE BreachResonator BreachArtwork)
    add_executable(BreachFormantTests Tests/FormantDSPTests.cpp)
    target_include_directories(BreachFormantTests PRIVATE Source)
    add_test(NAME FormantDSP COMMAND BreachFormantTests)
    add_executable(BreachFormantIntegration Tests/FormantIntegration.cpp)
    target_include_directories(BreachFormantIntegration PRIVATE Source $<TARGET_PROPERTY:BreachResonator,INCLUDE_DIRECTORIES>)
    target_compile_definitions(BreachFormantIntegration PRIVATE $<TARGET_PROPERTY:BreachResonator,COMPILE_DEFINITIONS>)
    target_link_libraries(BreachFormantIntegration PRIVATE BreachResonator BreachArtwork)
    add_test(NAME FormantSessionRecall COMMAND BreachFormantIntegration --verify ${CMAKE_CURRENT_SOURCE_DIR}/Tests/Fixtures/rings_stage3)
endif()

if(BREACH_BUILD_DSP_TESTS)
    add_executable(BreachModalAudition Tests/ModalAudition.cpp)
    target_include_directories(BreachModalAudition PRIVATE Source $<TARGET_PROPERTY:BreachResonator,INCLUDE_DIRECTORIES>)
    target_compile_definitions(BreachModalAudition PRIVATE $<TARGET_PROPERTY:BreachResonator,COMPILE_DEFINITIONS>)
    target_link_libraries(BreachModalAudition PRIVATE BreachResonator BreachArtwork)
    add_executable(BreachModalTests Tests/ModalDSPTests.cpp)
    target_include_directories(BreachModalTests PRIVATE Source)
    add_test(NAME ModalDSP COMMAND BreachModalTests)
    add_executable(BreachModalIntegration Tests/ModalIntegration.cpp)
    target_include_directories(BreachModalIntegration PRIVATE Source $<TARGET_PROPERTY:BreachResonator,INCLUDE_DIRECTORIES>)
    target_compile_definitions(BreachModalIntegration PRIVATE $<TARGET_PROPERTY:BreachResonator,COMPILE_DEFINITIONS>)
    target_link_libraries(BreachModalIntegration PRIVATE BreachResonator BreachArtwork)
    add_test(NAME ModalSessionRecall COMMAND BreachModalIntegration --verify ${CMAKE_CURRENT_SOURCE_DIR}/Tests/Fixtures/formant_stage4)
endif()

if(BREACH_BUILD_DSP_TESTS)
    add_executable(BreachBalanceTests Tests/BalanceDSPTests.cpp)
    target_include_directories(BreachBalanceTests PRIVATE Source)
    add_test(NAME BalanceDSP COMMAND BreachBalanceTests)
    add_executable(BreachBalanceIntegration Tests/BalanceIntegration.cpp)
    target_include_directories(BreachBalanceIntegration PRIVATE Source $<TARGET_PROPERTY:BreachResonator,INCLUDE_DIRECTORIES>)
    target_compile_definitions(BreachBalanceIntegration PRIVATE $<TARGET_PROPERTY:BreachResonator,COMPILE_DEFINITIONS>)
    target_link_libraries(BreachBalanceIntegration PRIVATE BreachResonator BreachArtwork)
    add_test(NAME BalanceSessionRecall COMMAND BreachBalanceIntegration --verify ${CMAKE_CURRENT_SOURCE_DIR}/Tests/Fixtures/modal_stage5)
    add_executable(BreachBalanceMeasure Tests/BalanceMeasure.cpp)
    target_include_directories(BreachBalanceMeasure PRIVATE Source $<TARGET_PROPERTY:BreachResonator,INCLUDE_DIRECTORIES>)
    target_compile_definitions(BreachBalanceMeasure PRIVATE $<TARGET_PROPERTY:BreachResonator,COMPILE_DEFINITIONS>)
    target_link_libraries(BreachBalanceMeasure PRIVATE BreachResonator BreachArtwork)
endif()

if(BREACH_BUILD_DSP_TESTS)
    add_executable(BreachDynamicsTests Tests/DynamicsDSPTests.cpp)
    target_include_directories(BreachDynamicsTests PRIVATE Source)
    add_test(NAME DynamicsDSP COMMAND BreachDynamicsTests)
    add_executable(BreachReverbTests Tests/ReverbDSPTests.cpp)
    target_include_directories(BreachReverbTests PRIVATE Source)
    add_test(NAME ReverbDSP COMMAND BreachReverbTests)
endif()

if(BREACH_BUILD_DSP_TESTS)
    add_executable(BreachDynamicsIntegration Tests/DynamicsIntegration.cpp)
    target_include_directories(BreachDynamicsIntegration PRIVATE Source $<TARGET_PROPERTY:BreachResonator,INCLUDE_DIRECTORIES>)
    target_compile_definitions(BreachDynamicsIntegration PRIVATE $<TARGET_PROPERTY:BreachResonator,COMPILE_DEFINITIONS>)
    target_link_libraries(BreachDynamicsIntegration PRIVATE BreachResonator BreachArtwork)
    add_test(NAME DynamicsSessionRecall COMMAND BreachDynamicsIntegration ${CMAKE_CURRENT_SOURCE_DIR}/Tests/Fixtures/balance_stage6)
endif()

if(BREACH_BUILD_DSP_TESTS)
    add_executable(BreachStage7Audition Tests/Stage7Audition.cpp)
    target_include_directories(BreachStage7Audition PRIVATE Source $<TARGET_PROPERTY:BreachResonator,INCLUDE_DIRECTORIES>)
    target_compile_definitions(BreachStage7Audition PRIVATE $<TARGET_PROPERTY:BreachResonator,COMPILE_DEFINITIONS>)
    target_link_libraries(BreachStage7Audition PRIVATE BreachResonator BreachArtwork)
endif()

if(BREACH_BUILD_DSP_TESTS)
    add_executable(BreachOutputEqTests Tests/OutputEqTests.cpp)
    target_include_directories(BreachOutputEqTests PRIVATE Source)
    add_test(NAME OutputEqDSP COMMAND BreachOutputEqTests)
    add_executable(BreachOutputEqIntegration Tests/OutputEqIntegration.cpp)
    target_include_directories(BreachOutputEqIntegration PRIVATE Source $<TARGET_PROPERTY:BreachResonator,INCLUDE_DIRECTORIES>)
    target_compile_definitions(BreachOutputEqIntegration PRIVATE $<TARGET_PROPERTY:BreachResonator,COMPILE_DEFINITIONS>)
    target_link_libraries(BreachOutputEqIntegration PRIVATE BreachResonator BreachArtwork)
    add_test(NAME OutputEqIntegration COMMAND BreachOutputEqIntegration)
endif()

if(BREACH_BUILD_DSP_TESTS)
    add_test(NAME Stage8SessionRecall COMMAND BreachBalanceIntegration --verify-stage8 ${CMAKE_CURRENT_SOURCE_DIR}/Tests/Fixtures/output_eq_stage8)
    add_executable(BreachRingTimeIntegration Tests/RingTimeIntegration.cpp)
    target_include_directories(BreachRingTimeIntegration PRIVATE Source $<TARGET_PROPERTY:BreachResonator,INCLUDE_DIRECTORIES>)
    target_compile_definitions(BreachRingTimeIntegration PRIVATE $<TARGET_PROPERTY:BreachResonator,COMPILE_DEFINITIONS>)
    target_link_libraries(BreachRingTimeIntegration PRIVATE BreachResonator BreachArtwork)
    add_test(NAME RingTimeIntegration COMMAND BreachRingTimeIntegration)
endif()
