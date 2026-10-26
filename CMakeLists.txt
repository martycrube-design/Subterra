cmake_minimum_required(VERSION 3.22)

# Must be set before project(): on macOS, project() fixes the deployment target, and an
# unset one makes Xcode target the build machine's own macOS version.
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")

project(Subterra VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(SUBTERRA_BUILD_TESTS "Build the Subterra test runner" ON)

# Use a local JUCE checkout with -DJUCE_DIR=/path/to/JUCE, otherwise fetch it.
set(JUCE_DIR "" CACHE PATH "Path to a local JUCE checkout")
if(JUCE_DIR)
    add_subdirectory(${JUCE_DIR} ${CMAKE_BINARY_DIR}/JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.15
        GIT_SHALLOW ON)
    FetchContent_MakeAvailable(JUCE)
endif()

set(SUBTERRA_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND SUBTERRA_FORMATS AU)
endif()

juce_add_plugin(Subterra
    COMPANY_NAME "Subterra Audio"
    BUNDLE_ID "com.subterraaudio.subterra"
    PLUGIN_MANUFACTURER_CODE Sbta
    PLUGIN_CODE Sbtr
    PRODUCT_NAME "Subterra"
    DESCRIPTION "Bass synthesizer pedal"
    ICON_BIG resources/icon.png
    FORMATS ${SUBTERRA_FORMATS}
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Fx Distortion
    AU_MAIN_TYPE kAudioUnitType_Effect
    COPY_PLUGIN_AFTER_BUILD FALSE)

juce_add_binary_data(SubterraData SOURCES
    resources/fonts/BebasNeue-Regular.ttf
    resources/fonts/Inter-Medium.ttf
    resources/fonts/Inter-SemiBold.ttf
    resources/fonts/Inter-Bold.ttf)

set(SUBTERRA_CORE_SOURCES
    source/PluginProcessor.cpp
    source/Parameters.cpp
    source/Presets.cpp)

target_sources(Subterra PRIVATE
    ${SUBTERRA_CORE_SOURCES}
    source/PluginEditor.cpp
    source/ui/SubterraLookAndFeel.cpp)

target_include_directories(Subterra PRIVATE source)

target_compile_definitions(Subterra PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(Subterra
    PRIVATE
        SubterraData
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

if(SUBTERRA_BUILD_TESTS)
    juce_add_console_app(SubterraTests PRODUCT_NAME "SubterraTests")
    target_sources(SubterraTests PRIVATE tests/SubterraTests.cpp ${SUBTERRA_CORE_SOURCES})
    target_include_directories(SubterraTests PRIVATE source)
    target_compile_definitions(SubterraTests PRIVATE
        SUBTERRA_HEADLESS=1
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_UNIT_TESTS=1)
    target_link_libraries(SubterraTests PRIVATE
        juce::juce_audio_processors
        juce::juce_dsp
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
endif()

option(SUBTERRA_BUILD_SNAPSHOT "Build the UI snapshot tool" OFF)
if(SUBTERRA_BUILD_SNAPSHOT)
    juce_add_gui_app(SubterraSnapshot PRODUCT_NAME "SubterraSnapshot")
    target_sources(SubterraSnapshot PRIVATE
        tools/Snapshot.cpp
        ${SUBTERRA_CORE_SOURCES}
        source/PluginEditor.cpp
        source/ui/SubterraLookAndFeel.cpp)
    target_include_directories(SubterraSnapshot PRIVATE source)
    target_compile_definitions(SubterraSnapshot PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_MODAL_LOOPS_PERMITTED=1
        JucePlugin_VersionString="${PROJECT_VERSION}")
    target_link_libraries(SubterraSnapshot PRIVATE
        SubterraData
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_recommended_config_flags)
endif()
