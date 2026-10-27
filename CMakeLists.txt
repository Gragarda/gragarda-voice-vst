cmake_minimum_required(VERSION 3.24)
project(gragarda-voice-vst VERSION 0.2.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
if(APPLE)
  set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "")
  set(CMAKE_OSX_DEPLOYMENT_TARGET 12.0 CACHE STRING "")
endif()

# Coeur DSP (submodule) : gv_dsp + ${DFN_MODEL}
add_subdirectory(core)

# JUCE 8 (AGPLv3 ou commercial) : ce projet est GPL-2.0-or-later, plugin distribue donc sous AGPLv3
include(FetchContent)
FetchContent_Declare(juce GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.10 GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(juce)

juce_add_plugin(GragardaVoice
  COMPANY_NAME Gragarda
  COMPANY_WEBSITE "https://voice.gragarda.fr"
  BUNDLE_ID fr.gragarda.voice.vst
  PLUGIN_MANUFACTURER_CODE Grgd
  PLUGIN_CODE GrVo
  PRODUCT_NAME "Gragarda Voice"
  FORMATS VST3 Standalone
  VST3_CATEGORIES Fx Dynamics
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT FALSE)

target_sources(GragardaVoice PRIVATE src/PluginProcessor.cpp)
# Traductions : copie des .ini du filtre OBS (data/locale de gragarda-voice), embarquees
file(GLOB LOCALES locale/*.ini)
juce_add_binary_data(GVLocales SOURCES ${LOCALES})
target_compile_definitions(GragardaVoice PUBLIC
  JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0)
target_link_libraries(GragardaVoice PRIVATE gv_dsp GVLocales juce::juce_audio_utils
  PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
if(UNIX AND NOT APPLE)
  # symboles rnnoise_* / df_* non exportes (conflit avec un autre plugin qui embarque RNNoise)
  target_link_options(GragardaVoice PRIVATE -Wl,-z,noexecstack -Wl,--exclude-libs,ALL)
endif()

# Modele DeepFilterNet + licences dans le bundle : Gragarda Voice.vst3/Contents/Resources
set(VST3_BUNDLE "$<GENEX_EVAL:$<TARGET_PROPERTY:GragardaVoice_VST3,JUCE_PLUGIN_ARTEFACT_FILE>>")
add_custom_command(TARGET GragardaVoice_VST3 POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory "${VST3_BUNDLE}/Contents/Resources"
  COMMAND ${CMAKE_COMMAND} -E copy "${DFN_MODEL}" "${VST3_BUNDLE}/Contents/Resources/"
  COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/LICENSE" "${VST3_BUNDLE}/Contents/Resources/gragarda-voice-GPL-2.0.txt"
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/core/licenses" "${VST3_BUNDLE}/Contents/Resources/licenses")
# Standalone : memes ressources, dans Contents/Resources de l'app (macOS) ou Resources/ a cote de l'exe
set(SA_APP "$<GENEX_EVAL:$<TARGET_PROPERTY:GragardaVoice_Standalone,JUCE_PLUGIN_ARTEFACT_FILE>>")
if(APPLE)
  set(SA_RES "${SA_APP}/Contents/Resources")
else()
  set(SA_RES "$<TARGET_FILE_DIR:GragardaVoice_Standalone>/Resources")
endif()
add_custom_command(TARGET GragardaVoice_Standalone POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory "${SA_RES}"
  COMMAND ${CMAKE_COMMAND} -E copy "${DFN_MODEL}" "${SA_RES}/"
  COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/LICENSE" "${SA_RES}/gragarda-voice-GPL-2.0.txt"
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/core/licenses" "${SA_RES}/licenses")
if(APPLE)
  # ressources ajoutees apres la signature ad hoc de JUCE : re-signer
  add_custom_command(TARGET GragardaVoice_VST3 POST_BUILD COMMAND codesign --force --deep --sign - "${VST3_BUNDLE}")
  add_custom_command(TARGET GragardaVoice_Standalone POST_BUILD COMMAND codesign --force --deep --sign - "${SA_APP}")
endif()
