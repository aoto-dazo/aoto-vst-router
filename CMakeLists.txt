cmake_minimum_required(VERSION 3.22)
project(AotoVSTRouter VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE をローカルに置いた場合はそれを使い、無ければ GitHub から取得
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
    add_subdirectory(JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 9.0.4
        GIT_SHALLOW ON)
    FetchContent_MakeAvailable(JUCE)
endif()

juce_add_gui_app(AotoVSTRouter
    PRODUCT_NAME "Aoto VST Router"
    COMPANY_NAME "Aoto"
    VERSION 1.0.0)

juce_generate_juce_header(AotoVSTRouter)

target_sources(AotoVSTRouter PRIVATE
    Source/Main.cpp
    Source/AudioEngine.cpp
    Source/MainComponent.cpp
    Source/Pages.cpp)

target_compile_definitions(AotoVSTRouter PRIVATE
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:AotoVSTRouter,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:AotoVSTRouter,JUCE_VERSION>"
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_ASIO=1                      # RME など ASIO ドライバで低レイテンシー
    JUCE_WASAPI=1
    JUCE_DIRECTSOUND=0
    JUCE_PLUGINHOST_VST3=1
    JUCE_PLUGINHOST_VST=0
    JUCE_PLUGINHOST_LADSPA=0
    JUCE_PLUGINHOST_LV2=0
    JUCE_PLUGINHOST_AU=0
    JUCE_JACK=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(AotoVSTRouter PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_audio_devices
    juce::juce_gui_extra
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)

if(MSVC)
    target_compile_options(AotoVSTRouter PRIVATE /utf-8)
    # 配布しやすいように VC ランタイムを静的リンク
    set_property(TARGET AotoVSTRouter PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()
