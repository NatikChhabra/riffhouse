set(RIFFHOUSE_CORE_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/dsp/Amp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/dsp/Cabinet.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/dsp/NeuralCapture.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/dsp/Pedals.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/engine/Params.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/engine/Presets.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/engine/Rig.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/engine/UpdateInfo.cpp)
set(RIFFHOUSE_CORE_SOURCES ${RIFFHOUSE_CORE_SOURCES} PARENT_SCOPE)

file(GLOB RIFFHOUSE_FACTORY_PRESETS CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/presets/*.json")
if (RIFFHOUSE_FACTORY_PRESETS)
    juce_add_binary_data(riffhouse_presets NAMESPACE RiffhousePresets HEADER_NAME PresetData.h SOURCES ${RIFFHOUSE_FACTORY_PRESETS})
endif()

if (RIFFHOUSE_BUILD_PLUGIN)
    include(${CMAKE_CURRENT_SOURCE_DIR}/plugin/Plugin.cmake)
endif()
