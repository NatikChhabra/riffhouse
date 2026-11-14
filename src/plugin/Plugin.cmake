# The Riffhouse app (standalone) and plugin formats.

file(GLOB RIFFHOUSE_FONTS CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/resources/fonts/*.ttf")
# the logo: the badge and the script name for the top bar (tools/brand/logo builds them)
set(RIFFHOUSE_BRAND "${CMAKE_SOURCE_DIR}/resources/brand/mark-160.png" "${CMAKE_SOURCE_DIR}/resources/brand/wordmark-cream.png")
juce_add_binary_data(riffhouse_assets NAMESPACE RiffhouseAssets HEADER_NAME AssetData.h SOURCES ${RIFFHOUSE_FONTS} ${RIFFHOUSE_BRAND})
# Pictures of the gear from the 3D renderer (tools/gear3d). Without them the gear is drawn by hand.
file(GLOB RIFFHOUSE_GEAR_ART CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/resources/gear/*.jpg" "${CMAKE_SOURCE_DIR}/resources/gear/*.png")
if (RIFFHOUSE_GEAR_ART)
    juce_add_binary_data(riffhouse_gear NAMESPACE RiffhouseGear HEADER_NAME GearData.h SOURCES ${RIFFHOUSE_GEAR_ART})
endif()

set(RIFFHOUSE_FORMATS Standalone VST3)
if (APPLE)
    list(APPEND RIFFHOUSE_FORMATS AU)
endif()

set(RIFFHOUSE_ICON "")
if (EXISTS "${CMAKE_SOURCE_DIR}/resources/icon.png")
    set(RIFFHOUSE_ICON "${CMAKE_SOURCE_DIR}/resources/icon.png")
endif()
# small sizes get a plainer drawing of the same mark
set(RIFFHOUSE_ICON_SMALL "${RIFFHOUSE_ICON}")
if (EXISTS "${CMAKE_SOURCE_DIR}/resources/icon-small.png")
    set(RIFFHOUSE_ICON_SMALL "${CMAKE_SOURCE_DIR}/resources/icon-small.png")
endif()

juce_add_plugin(Riffhouse
    PRODUCT_NAME "Riffhouse"
    COMPANY_NAME "Riffhouse"
    COMPANY_WEBSITE "https://github.com/NatikChhabra/riffhouse"
    BUNDLE_ID "io.github.natikchhabra.riffhouse"
    VERSION ${PROJECT_VERSION}
    PLUGIN_MANUFACTURER_CODE Rfhs
    PLUGIN_CODE Rfh1
    FORMATS ${RIFFHOUSE_FORMATS}
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "Riffhouse needs your audio input to hear your guitar."
    VST3_CATEGORIES Fx Distortion
    AU_MAIN_TYPE kAudioUnitType_Effect
    COPY_PLUGIN_AFTER_BUILD FALSE
    ICON_BIG "${RIFFHOUSE_ICON}"
    ICON_SMALL "${RIFFHOUSE_ICON_SMALL}")

target_sources(Riffhouse PRIVATE
    ${RIFFHOUSE_CORE_SOURCES}
    plugin/PluginProcessor.cpp
    plugin/Recorder.cpp
    plugin/Takes.cpp
    plugin/Export.cpp
    plugin/Tuner.cpp
    plugin/AutoSetup.cpp
    plugin/DeviceGuard.cpp
    plugin/DeviceLog.cpp
    plugin/AudioSwitch.cpp
    plugin/AudioOpen.cpp
    plugin/AudioProbe.cpp
    plugin/Reports.cpp
    plugin/Updater.cpp
    plugin/LevelMatcher.cpp
    plugin/Version.cpp
    ui/Motion.cpp
    ui/Theme.cpp
    ui/Widgets.cpp
    ui/AudioPicker.cpp
    ui/Gear.cpp
    ui/GearArt.cpp
    ui/Modules.cpp
    ui/Panels.cpp
    ui/PresetBrowser.cpp
    ui/SetupWizard.cpp
    ui/TakesView.cpp
    ui/SongSheet.cpp
    ui/Editor.cpp
    standalone/RiffhouseApp.cpp)

target_include_directories(Riffhouse PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_compile_definitions(Riffhouse PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=1
    JUCE_USE_MP3AUDIOFORMAT=1
    JUCE_USE_FLAC=1
    JUCE_USE_OGGVORBIS=1)

# Where a report goes after the player says yes: a relay that files it in a private repository.
# Release builds get it from the REPORT_URL secret; a build from source has none and sends nothing.
# It is kept in the cache: a build can re-run CMake by itself (a new file in a JUCE module folder
# does it) without the environment, and 0.1.5.2 shipped with no relay that way.
if (NOT "$ENV{RIFFHOUSE_REPORT_URL}" STREQUAL "")
    string(STRIP "$ENV{RIFFHOUSE_REPORT_URL}" _riffhouse_relay)
    set(RIFFHOUSE_REPORT_URL "${_riffhouse_relay}" CACHE INTERNAL "Where reports go")
endif()
if (RIFFHOUSE_REPORT_URL MATCHES "^https://[^\"]+$")
    set_source_files_properties("${CMAKE_CURRENT_LIST_DIR}/Reports.cpp" PROPERTIES COMPILE_DEFINITIONS "RIFFHOUSE_REPORT_URL=\"${RIFFHOUSE_REPORT_URL}\"")
    message(STATUS "Reports go to the relay")
endif()

# Each system gets only its own audio back ends: asking for JACK or ALSA on
# Windows makes JUCE look for headers that do not exist there.
if (WIN32)
    target_compile_definitions(Riffhouse PUBLIC JUCE_WASAPI=1 JUCE_DIRECTSOUND=1)
elseif (UNIX AND NOT APPLE)
    target_compile_definitions(Riffhouse PUBLIC JUCE_JACK=1 JUCE_ALSA=1)
endif()

if (WIN32 AND RIFFHOUSE_ASIO_SDK_DIR)
    target_compile_definitions(Riffhouse PUBLIC JUCE_ASIO=1)
    target_include_directories(Riffhouse PUBLIC
        "${RIFFHOUSE_ASIO_SDK_DIR}"
        "${RIFFHOUSE_ASIO_SDK_DIR}/common"
        "${RIFFHOUSE_ASIO_SDK_DIR}/host"
        "${RIFFHOUSE_ASIO_SDK_DIR}/host/pc")
    message(STATUS "Riffhouse: ASIO enabled from ${RIFFHOUSE_ASIO_SDK_DIR}")
endif()

if (RIFFHOUSE_GEAR_ART)
    target_link_libraries(Riffhouse PRIVATE riffhouse_gear)
    target_compile_definitions(Riffhouse PRIVATE RIFFHOUSE_HAS_GEAR_ART=1)
endif()

target_link_libraries(Riffhouse
    PRIVATE
        nam_core
        riffhouse_stretch
        riffhouse_lame
        riffhouse_assets
        juce::juce_audio_utils
        juce::juce_cryptography
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

if (TARGET riffhouse_presets)
    target_link_libraries(Riffhouse PRIVATE riffhouse_presets)
endif()

if (MSVC)
    target_compile_options(Riffhouse PRIVATE /bigobj)
endif()

# The version the player sees: "0.1.0" for a release, "0.1.0-rc2" for a test build (the release
# workflow passes the tag), and the commit for anything built from source in between.
set(RIFFHOUSE_VERSION_LABEL "" CACHE STRING "Version shown in the app. Empty: the project version, marked as built from source")
set(_riffhouse_label "${RIFFHOUSE_VERSION_LABEL}")
if (NOT _riffhouse_label)
    set(_riffhouse_label "${PROJECT_VERSION}-dev")
    execute_process(COMMAND git rev-parse --short=7 HEAD WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
                    OUTPUT_VARIABLE _riffhouse_sha OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    if (_riffhouse_sha)
        string(APPEND _riffhouse_label ".${_riffhouse_sha}")
    endif()
endif()
set_source_files_properties("${CMAKE_CURRENT_LIST_DIR}/Version.cpp" PROPERTIES COMPILE_DEFINITIONS "RIFFHOUSE_VERSION_LABEL=\"${_riffhouse_label}\"")
message(STATUS "Riffhouse: version label ${_riffhouse_label}")
