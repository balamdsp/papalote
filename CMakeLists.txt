cmake_minimum_required(VERSION 3.22)
project(Papalote VERSION 0.8.1)

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        9.0.1
)
FetchContent_MakeAvailable(JUCE)

add_subdirectory(libs/clap-juce-extensions EXCLUDE_FROM_ALL)

option(PAPALOTE_COPY_AFTER_BUILD "Copy built plugins into the system plugin folders" ON)

if (PAPALOTE_COPY_AFTER_BUILD)
    set_property(GLOBAL PROPERTY JUCE_COPY_PLUGIN_AFTER_BUILD TRUE)
endif()

juce_add_plugin(Papalote
    COMPANY_NAME "BalamDSP"
    PLUGIN_MANUFACTURER_CODE "Bala"
    PLUGIN_CODE "Papa"
    FORMATS Standalone VST3 AU
    PRODUCT_NAME "Papalote"
    VERSION ${PROJECT_VERSION} 
    PLUGIN_DESCRIPTION "Dirt Saturator"
    VST3_CATEGORIES "Distortion" "Fx"
    LV2URI "https://balamdsp.com/plugins/Papalote"
    AAX_CATEGORY "AAX_ePlugInCategory_Harmonic"
)

clap_juce_extensions_plugin(TARGET Papalote
    CLAP_ID "com.balamdsp.Papalote"
    CLAP_FEATURES "audio-effect")

juce_generate_juce_header(Papalote)

target_compile_definitions(Papalote
    PUBLIC
        JUCE_STRICT_REFCOUNTEDPOINTER=1
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_WIN_PER_MONITOR_DPI_AWARE=0
)

target_sources(Papalote
    PRIVATE
        source/CustomLookAndFeel.h
        source/SaturationCurves.h
        source/SaturationCurves.cpp
        source/TapeEmulation.cpp
        source/TapeEmulation.h
        source/PluginProcessor.h
        source/PluginProcessor.cpp
        source/PluginEditor.cpp
        source/PluginEditor.h
        source/PresetManager.h
        source/PresetManager.cpp
)

juce_add_binary_data(Papalote_BinaryData
    HEADER_NAME "BinaryData.h"
    NAMESPACE "BinaryData"
    SOURCES
        source/VT323-Regular.ttf
)

target_link_libraries(Papalote PRIVATE Papalote_BinaryData)

target_link_libraries(Papalote
    PRIVATE
        juce::juce_audio_basics
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_audio_plugin_client
        juce::juce_audio_processors
        juce::juce_audio_processors_headless
        juce::juce_audio_utils
        juce::juce_core
        juce::juce_data_structures
        juce::juce_dsp
        juce::juce_events
        juce::juce_graphics
        juce::juce_gui_basics
        juce::juce_gui_extra
        juce::juce_opengl
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

