cmake_minimum_required(VERSION 3.22)
project(Papalote VERSION 0.7)

# Check for environment variable first, then fallback to variable passed via CLI (-DPAPALOTE_JUCE_PATH=...)
if(DEFINED ENV{PAPALOTE_JUCE_PATH} AND NOT DEFINED PAPALOTE_JUCE_PATH)
    set(PAPALOTE_JUCE_PATH "$ENV{PAPALOTE_JUCE_PATH}" CACHE PATH "Path to local JUCE installation")
else()
    set(PAPALOTE_JUCE_PATH "" CACHE PATH "Path to local JUCE installation")
endif()

# Fetch JUCE automatically if no valid local path is provided or if the path doesn't exist
if(NOT PAPALOTE_JUCE_PATH OR NOT EXISTS "${PAPALOTE_JUCE_PATH}")
    message(STATUS "Local JUCE not specified or not found -- fetching JUCE 9.0.1 via FetchContent")
    
    include(FetchContent)
    FetchContent_Declare(
        JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        9.0.1
    )
    FetchContent_MakeAvailable(JUCE)
else()
    message(STATUS "Using local JUCE from: ${PAPALOTE_JUCE_PATH}")
    add_subdirectory("${PAPALOTE_JUCE_PATH}" juce)
endif()

if (EXISTS "${PAPALOTE_JUCE_PATH}")
    add_subdirectory("${PAPALOTE_JUCE_PATH}" juce_build)
else()
    message(STATUS "PAPALOTE_JUCE_PATH '${PAPALOTE_JUCE_PATH}' not found -- fetching JUCE 9.0.1")
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 9.0.1
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

# Copy the built VST3 into the system plugin folder after every successful
# build. JUCE reads these as INHERITED target properties during
# juce_add_plugin, so they must be set at GLOBAL scope BEFORE that call.
# CI disables this with -DPAPALOTE_COPY_AFTER_BUILD=OFF.
option(PAPALOTE_COPY_AFTER_BUILD "Copy built plugins into the system plugin folders" ON)

if (PAPALOTE_COPY_AFTER_BUILD)
    set_property(GLOBAL PROPERTY JUCE_COPY_PLUGIN_AFTER_BUILD TRUE)
    set_property(GLOBAL PROPERTY JUCE_VST3_COPY_DIR "C:/Program Files/Common Files/VST3/BalamDSP")
endif()

juce_add_plugin(Papalote
    COMPANY_NAME "BalamDSP"
    PLUGIN_MANUFACTURER_CODE "Bala"
    PLUGIN_CODE "Papa"
    FORMATS Standalone VST3 # JUCE has no native CLAP support
    PRODUCT_NAME "Papalote"
    VERSION ${PROJECT_VERSION} # single source of truth: project(VERSION) above
    PLUGIN_DESCRIPTION "Dirt Saturator"
    VST3_CATEGORIES "Distortion" "Fx"
    LV2URI "https://balamdsp.com/plugins/Papalote"
    AAX_CATEGORY "8192"
)

juce_generate_juce_header(Papalote)

target_compile_definitions(Papalote
    PUBLIC
        JUCE_STRICT_REFCOUNTEDPOINTER=1
        JUCE_VST3_CAN_REPLACE_VST2=0
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
