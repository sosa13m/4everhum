cmake_minimum_required(VERSION 3.22)

project(FourEverHum VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(FOUREVERHUM_BUILD_PLUGIN "Compilar el plugin (requiere JUCE)" ON)
option(FOUREVERHUM_BUILD_TESTS  "Compilar los tests del DSP"          ON)
option(FOUREVERHUM_BUILD_TOOLS  "Compilar la herramienta hum2midi"    ON)

# ---------------------------------------------------------------------------
# Libreria DSP pura (sin JUCE): deteccion de pitch + segmentacion en notas.
# Se mantiene separada para poder testearla sin abrir un DAW.
# ---------------------------------------------------------------------------
add_library(fourever_dsp STATIC
    Source/dsp/PitchTracker.cpp
    Source/dsp/NoteSegmenter.cpp
    Source/dsp/Analyzer.cpp)
target_include_directories(fourever_dsp PUBLIC Source)
if(MSVC)
    target_compile_options(fourever_dsp PRIVATE /W4)
else()
    target_compile_options(fourever_dsp PRIVATE -Wall -Wextra)
endif()

if(FOUREVERHUM_BUILD_TESTS)
    enable_testing()
    add_executable(dsp_tests tests/test_dsp.cpp)
    target_link_libraries(dsp_tests PRIVATE fourever_dsp)
    add_test(NAME dsp_tests COMMAND dsp_tests)
endif()

if(NOT FOUREVERHUM_BUILD_PLUGIN)
    return()
endif()

# ---------------------------------------------------------------------------
# JUCE: usa una copia local si se pasa -DJUCE_DIR=..., si no la descarga.
# ---------------------------------------------------------------------------
set(JUCE_DIR "" CACHE PATH "Ruta a una copia local de JUCE (opcional)")

if(JUCE_DIR)
    add_subdirectory(${JUCE_DIR} ${CMAKE_BINARY_DIR}/juce EXCLUDE_FROM_ALL)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.15
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

set(FOUREVERHUM_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND FOUREVERHUM_FORMATS AU)
endif()

juce_add_plugin(FourEverHum
    COMPANY_NAME                  "4EVERTOOLS"
    COMPANY_WEBSITE               "https://4evertools.com"
    BUNDLE_ID                     "com.4evertools.4everhum"
    PLUGIN_MANUFACTURER_CODE      Fevr
    PLUGIN_CODE                   Fhum
    PRODUCT_NAME                  "4EVERHUM"
    FORMATS                       ${FOUREVERHUM_FORMATS}
    IS_SYNTH                      FALSE
    NEEDS_MIDI_INPUT              FALSE
    NEEDS_MIDI_OUTPUT             FALSE
    IS_MIDI_EFFECT                FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS   FALSE
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT    "4EVERHUM necesita el microfono para convertir tu voz en MIDI."
    VST3_CATEGORIES               Fx Analyzer
    AU_MAIN_TYPE                  kAudioUnitType_Effect
    COPY_PLUGIN_AFTER_BUILD       FALSE)

# Fuentes y logo embebidos en el plugin (identidad visual SOTANO 4EVER).
juce_add_binary_data(FourEverHumAssets SOURCES
    Resources/fonts/Anton-Regular.ttf
    Resources/fonts/SpecialElite-Regular.ttf
    Resources/fonts/SpaceMono-Regular.ttf
    Resources/fonts/SpaceMono-Bold.ttf
    Resources/images/logo-4ever.png)

target_sources(FourEverHum PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/ui/NotePreview.cpp
    Source/ui/Theme.cpp
    Source/ui/HumLookAndFeel.cpp
    Source/midi/MidiExporter.cpp)

target_compile_definitions(FourEverHum PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

# Los textos de la UI van en UTF-8 (tildes y enies).
if(MSVC)
    target_compile_options(FourEverHum PRIVATE /utf-8)
endif()

target_link_libraries(FourEverHum
    PRIVATE
        fourever_dsp
        FourEverHumAssets
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

if(FOUREVERHUM_BUILD_TESTS)
    juce_add_console_app(midi_export_test PRODUCT_NAME "midi_export_test")
    target_sources(midi_export_test PRIVATE tests/test_midi_export.cpp Source/midi/MidiExporter.cpp)
    target_compile_definitions(midi_export_test PRIVATE JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0)
    target_link_libraries(midi_export_test PRIVATE fourever_dsp juce::juce_audio_basics
                                                   juce::juce_recommended_config_flags)
    add_test(NAME midi_export_test COMMAND midi_export_test)
endif()

# ---------------------------------------------------------------------------
# hum2midi: el mismo motor del plugin, desde la terminal.
# ---------------------------------------------------------------------------
if(FOUREVERHUM_BUILD_TOOLS)
    juce_add_console_app(hum2midi PRODUCT_NAME "hum2midi")
    target_sources(hum2midi PRIVATE tools/hum2midi.cpp Source/midi/MidiExporter.cpp)
    target_compile_definitions(hum2midi PRIVATE
        JUCE_USE_CURL=0
        JUCE_WEB_BROWSER=0
        JUCE_USE_MP3AUDIOFORMAT=1)
    target_link_libraries(hum2midi PRIVATE fourever_dsp juce::juce_audio_formats
                                           juce::juce_recommended_config_flags
                                           juce::juce_recommended_warning_flags)
endif()
