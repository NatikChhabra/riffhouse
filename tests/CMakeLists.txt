juce_add_console_app(riffhouse_tests PRODUCT_NAME "riffhouse_tests")
target_sources(riffhouse_tests PRIVATE ${RIFFHOUSE_CORE_SOURCES} TestMain.cpp TestSignals.cpp)
target_include_directories(riffhouse_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_definitions(riffhouse_tests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 RIFFHOUSE_PRESET_DIR="${CMAKE_SOURCE_DIR}/presets")
target_link_libraries(riffhouse_tests PRIVATE
    nam_core juce::juce_dsp juce::juce_audio_formats juce::juce_audio_utils juce::juce_cryptography
    juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
if (TARGET riffhouse_presets)
    target_link_libraries(riffhouse_tests PRIVATE riffhouse_presets)
    target_compile_definitions(riffhouse_tests PRIVATE RIFFHOUSE_HAS_PRESETS=1)
endif()
target_compile_definitions(riffhouse_tests PRIVATE RIFFHOUSE_NAM_EXAMPLES="${namcore_SOURCE_DIR}/example_models")
