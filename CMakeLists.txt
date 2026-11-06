cmake_minimum_required(VERSION 3.22)
project(PaffettiDubChords VERSION 0.2.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE: cartella passata da fuori (-DJUCE_DIR=...) oppure ./JUCE
if(NOT DEFINED JUCE_DIR)
    set(JUCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/JUCE)
endif()
add_subdirectory(${JUCE_DIR} JUCE)

option(PDC_BUILD_PLUGIN "Compila il plugin" ON)
option(PDC_BUILD_TESTS  "Compila il banco di prova (solo Linux)" OFF)

juce_add_binary_data(PDCBinary SOURCES
    Resources/Fonts/PressStart2P-Regular.ttf
    Resources/Fonts/VT323-Regular.ttf)

set(PDC_SOURCES
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Library.cpp)

set(PDC_DEFS
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

set(PDC_LIBS
    PDCBinary
    juce::juce_audio_processors
    juce::juce_audio_formats
    juce::juce_dsp
    juce::juce_gui_extra
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)

if(PDC_BUILD_PLUGIN)
    juce_add_plugin(PaffettiDubChords
        COMPANY_NAME "Black Loops"
        COMPANY_WEBSITE "https://blackloops.bandcamp.com"
        BUNDLE_ID com.blackloops.paffettidubchords
        PLUGIN_MANUFACTURER_CODE Blkl
        PLUGIN_CODE Pdch
        FORMATS VST3 AU
        PRODUCT_NAME "Paffetti Dub Chords"
        IS_SYNTH TRUE
        NEEDS_MIDI_INPUT TRUE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        COPY_PLUGIN_AFTER_BUILD FALSE
        AU_MAIN_TYPE kAudioUnitType_MusicDevice
        VST3_CATEGORIES Instrument Sampler)
    target_sources(PaffettiDubChords PRIVATE ${PDC_SOURCES})
    target_compile_definitions(PaffettiDubChords PUBLIC ${PDC_DEFS})
    target_link_libraries(PaffettiDubChords PRIVATE ${PDC_LIBS})
endif()

if(PDC_BUILD_TESTS)
    juce_add_console_app(PDCTest PRODUCT_NAME "PDCTest")
    target_sources(PDCTest PRIVATE ${PDC_SOURCES} Tests/TestMain.cpp)
    target_compile_definitions(PDCTest PRIVATE ${PDC_DEFS} JUCE_ALSA=0 JUCE_JACK=0
        JucePlugin_Name="Paffetti Dub Chords" JucePlugin_IsSynth=1 JucePlugin_WantsMidiInput=1
        JucePlugin_ProducesMidiOutput=0 JucePlugin_IsMidiEffect=0 JucePlugin_VersionString="0.1.0")
    target_link_libraries(PDCTest PRIVATE ${PDC_LIBS})
endif()
