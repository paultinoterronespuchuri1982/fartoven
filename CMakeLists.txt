cmake_minimum_required(VERSION 3.22)

project(FartovenDrums VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.4
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(FartovenDrums
    COMPANY_NAME "FARTOVEN"
    PLUGIN_MANUFACTURER_CODE Frtv
    PLUGIN_CODE Frdr
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "FARTOVEN DRUMS"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    PLUGIN_NAME "FARTOVEN DRUMS"
    COPY_PLUGIN_AFTER_BUILD FALSE
)

juce_generate_juce_header(FartovenDrums)

target_sources(FartovenDrums PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/DrumChannelDSP.h
    Source/DrumChannel.h
    Source/DrumChannel.cpp
)

target_link_libraries(FartovenDrums PRIVATE
    juce::juce_audio_basics
    juce::juce_audio_processors
    juce::juce_dsp
    juce::juce_audio_utils
    juce::juce_gui_basics
    juce::juce_gui_extra
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)

# Pruebas del motor DSP: ejecutable de consola sin interfaz gráfica.
juce_add_console_app(FartovenDrumsTests PRODUCT_NAME "FARTOVEN DRUMS Tests")

target_sources(FartovenDrumsTests PRIVATE
    Tests/main.cpp
    Tests/DSPTests.cpp
    Source/DrumChannel.cpp
)

target_link_libraries(FartovenDrumsTests PRIVATE
    juce::juce_core
    juce::juce_dsp
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)

enable_testing()
add_test (NAME DrumChannelDSP COMMAND FartovenDrumsTests)
