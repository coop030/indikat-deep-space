cmake_minimum_required(VERSION 3.22)

project(IndikatDeepSpace VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE: either as a "JUCE" subfolder (git clone) or fetched automatically
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
    add_subdirectory(JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.9
        GIT_SHALLOW ON)
    FetchContent_MakeAvailable(JUCE)
endif()

juce_add_binary_data(IndikatFonts SOURCES
    Resources/Michroma-Regular.ttf
    Resources/Questrial-Regular.ttf
    Resources/ShareTechMono-Regular.ttf
    Resources/indikat-logo.svg)

set(DEEPSPACE_SOURCES
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/DeepSpaceReverb.cpp)

juce_add_plugin(IndikatDeepSpace
    COMPANY_NAME "INDIKAT"
    BUNDLE_ID com.indikat.deepspace
    PLUGIN_MANUFACTURER_CODE Indk
    PLUGIN_CODE Idsp
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "INDIKAT Deep Space"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Reverb)

target_sources(IndikatDeepSpace PRIVATE ${DEEPSPACE_SOURCES})

target_compile_definitions(IndikatDeepSpace PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(IndikatDeepSpace
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        IndikatFonts
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Offline test: renders every preset and checks levels and tail decay (cmake -DINDIKAT_TESTS=ON)
option(INDIKAT_TESTS "Build offline render test" OFF)
if(INDIKAT_TESTS)
    juce_add_console_app(IndikatRenderTest PRODUCT_NAME "IndikatRenderTest")
    target_sources(IndikatRenderTest PRIVATE Tests/RenderTest.cpp ${DEEPSPACE_SOURCES})
    target_compile_definitions(IndikatRenderTest PRIVATE
        JucePlugin_Name="INDIKAT Deep Space" JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(IndikatRenderTest PRIVATE juce::juce_audio_utils juce::juce_dsp IndikatFonts
        juce::juce_recommended_config_flags)
endif()
