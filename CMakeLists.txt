cmake_minimum_required(VERSION 3.22)

# --- macOS build settings (must be set before project()) ---
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Build universal binary")
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS deployment target")

project(FriendlitronicsAmp VERSION 0.8.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- JUCE ---------------------------------------------------------------
# Use a local checkout if there is one (faster, and handy when hacking on
# JUCE itself); otherwise fetch the pinned release, so a fresh clone of this
# repository builds with nothing but CMake and a compiler.
set(AMPSIM_JUCE_TAG "7.0.12" CACHE STRING "JUCE release tag to fetch")

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
    message(STATUS "AmpSim: using local JUCE at ./JUCE")
    add_subdirectory(JUCE)
else()
    message(STATUS "AmpSim: fetching JUCE ${AMPSIM_JUCE_TAG}")
    include(FetchContent)
    # JUCE 7.0.12 predates the macOS 15 SDK, which removed an API it calls, so
    # a stock checkout will not compile with Xcode 16+. The patch re-declares
    # the symbol; see patches/ for the details.
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        ${AMPSIM_JUCE_TAG}
        GIT_SHALLOW    TRUE
        PATCH_COMMAND  ${CMAKE_COMMAND}
                       -DPATCH_FILE=${CMAKE_CURRENT_SOURCE_DIR}/patches/juce-7.0.12-macos15.patch
                       -DWORK_DIR=<SOURCE_DIR>
                       -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/ApplyPatch.cmake)
    FetchContent_MakeAvailable(JUCE)
endif()

# Installing into ~/Library on every build is what you want at a desk and not
# what you want on a build machine.
if(DEFINED ENV{CI})
    set(AMPSIM_COPY_AFTER_BUILD FALSE)
else()
    set(AMPSIM_COPY_AFTER_BUILD TRUE)
endif()

juce_add_plugin(FriendlitronicsAmp
    COMPANY_NAME            "Friendlitronics"
    BUNDLE_ID               "dev.elegancia.friendlitronicsamp"
    PLUGIN_MANUFACTURER_CODE Frnd
    PLUGIN_CODE             Amp1
    FORMATS                 AU VST3 Standalone
    PRODUCT_NAME            "Friendlitronics Amp"
    IS_SYNTH                FALSE
    NEEDS_MIDI_INPUT        FALSE
    NEEDS_MIDI_OUTPUT       FALSE
    IS_MIDI_EFFECT          FALSE
    COPY_PLUGIN_AFTER_BUILD ${AMPSIM_COPY_AFTER_BUILD})

juce_generate_juce_header(FriendlitronicsAmp)

# DSP modules are header-only (Source/dsp/*.h); only the plugin shell and the
# custom look-and-feel need translation units.
target_sources(FriendlitronicsAmp PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/ui/AmpLookAndFeel.cpp)

target_compile_definitions(FriendlitronicsAmp PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(FriendlitronicsAmp PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)

target_compile_options(FriendlitronicsAmp PRIVATE
    $<$<CXX_COMPILER_ID:AppleClang,Clang,GNU>:-Wall -Wextra>)
