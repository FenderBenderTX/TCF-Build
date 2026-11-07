cmake_minimum_required(VERSION 3.22)
project(BBD_SCF04_Model VERSION 0.1.0)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Use a local JUCE checkout:  cmake -B build -DJUCE_DIR=/path/to/JUCE
# otherwise JUCE 8 is fetched from GitHub.
if(DEFINED JUCE_DIR)
  add_subdirectory(${JUCE_DIR} juce)
else()
  include(FetchContent)
  FetchContent_Declare(juce GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.4)
  FetchContent_MakeAvailable(juce)
endif()

juce_add_plugin(BBD_SCF04_Model
  COMPANY_NAME "Unofficial"
  PLUGIN_MANUFACTURER_CODE Unof
  PLUGIN_CODE Scf4
  FORMATS VST3 Standalone
  PRODUCT_NAME "BBD SCF04 Model"
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT FALSE
  NEEDS_MIDI_OUTPUT FALSE
  VST3_CATEGORIES Fx Modulation)

target_sources(BBD_SCF04_Model PRIVATE
  Source/PluginProcessor.cpp Source/PluginProcessor.h Source/ScfEngine.h)

target_compile_definitions(BBD_SCF04_Model PUBLIC
  JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(BBD_SCF04_Model
  PRIVATE juce::juce_audio_utils juce::juce_audio_processors
  PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags
         juce::juce_recommended_warning_flags)
