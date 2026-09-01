cmake_minimum_required(VERSION 3.22)

project(OrchHarp VERSION 0.1.0)

# Path to your JUCE folder.
add_subdirectory("C:/JUCE/JUCE" JUCE)

juce_add_plugin(OrchHarp
    COMPANY_NAME "JohnPascu"
    PLUGIN_MANUFACTURER_CODE Jpas
    PLUGIN_CODE Ohrp

    FORMATS VST3

    PRODUCT_NAME "OrchHarp"
    PLUGIN_NAME "OrchHarp"
    BUNDLE_ID "com.johnpascu.OrchHarp"

    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT TRUE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_REPLACES_VST2 FALSE

    COPY_PLUGIN_AFTER_BUILD FALSE
)

juce_generate_juce_header(OrchHarp)

target_sources(OrchHarp
    PRIVATE
        Source/OrchHarpProcessor.cpp
        Source/OrchHarpEditor.cpp
        Source/OrchHarpPedalLogic.cpp
)

target_compile_definitions(OrchHarp
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(OrchHarp
    PRIVATE
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_gui_extra

    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

if (TARGET OrchHarp_VST3)
    target_compile_definitions(OrchHarp_VST3
        PRIVATE
            JUCE_VST3_CAN_REPLACE_VST2=0
    )
endif()

# Developer-only verification target for the pure pedal / string / governor logic.
juce_add_console_app(OrchHarpPedalLogicCheck
    PRODUCT_NAME "OrchHarpPedalLogicCheck"
)

juce_generate_juce_header(OrchHarpPedalLogicCheck)

target_sources(OrchHarpPedalLogicCheck
    PRIVATE
        Tools/OrchHarpPedalLogicCheck.cpp
        Source/OrchHarpPedalLogic.cpp
)

target_link_libraries(OrchHarpPedalLogicCheck
    PRIVATE
        juce::juce_core

    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

target_include_directories(OrchHarpPedalLogicCheck
    PRIVATE
        Source
)
