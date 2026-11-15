cmake_minimum_required(VERSION 3.22)

project(VideoPlayerPlugin VERSION 0.3.0)

# Compile pour Apple Silicon ET Intel (FL Studio Mac tourne en natif sur les deux)
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "" FORCE)
set(CMAKE_CXX_STANDARD 17)

# Nécessaire pour le petit fichier spécifique Mac (NativeTweaks.mm)
if(APPLE)
    enable_language(OBJCXX)
endif()

# Télécharge JUCE automatiquement (nécessite Internet au premier build)
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(VideoPlayerPlugin
    COMPANY_NAME "MonStudio"
    PLUGIN_MANUFACTURER_CODE Msto
    PLUGIN_CODE Vpl1
    PRODUCT_NAME "Video Player"
    FORMATS VST3 AU Standalone
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    NEEDS_WEB_BROWSER TRUE
    VST3_CATEGORIES Fx
    AU_MAIN_TYPE kAudioUnitType_Effect
    # Copie automatiquement le plugin dans ~/Library/Audio/Plug-Ins après le build
    COPY_PLUGIN_AFTER_BUILD TRUE)

target_sources(VideoPlayerPlugin PRIVATE
    PluginProcessor.cpp
    PluginProcessor.h
    PluginEditor.cpp
    PluginEditor.h
    Theme.h
    Recents.h
    NativeTweaks.h
    NativeTweaks.mm)

target_compile_definitions(VideoPlayerPlugin PUBLIC
    JUCE_WEB_BROWSER=1
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(VideoPlayerPlugin
    PRIVATE
        juce::juce_audio_utils
        juce::juce_video
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
