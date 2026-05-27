cmake_minimum_required(VERSION 3.20)

project(SweetsActionDX11 LANGUAGES CXX)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

add_executable(SweetsActionDX11 WIN32
    src/AudioSystem.cpp
    src/AssetLoaders.cpp
    src/AssetCatalog.cpp
    src/ChargeAttack.cpp
    src/CoopController.cpp
    src/EnemyController.cpp
    src/GameLogic.cpp
    src/ModelLibrary.cpp
    src/PlayerController.cpp
    src/ReflectionSystem.cpp
    src/Renderer.cpp
    src/SpriteLibrary.cpp
    src/StageFactory.cpp
    src/SweetsApp.cpp
    src/TextureLibrary.cpp
    src/AudioSystem.h
    src/AssetLoaders.h
    src/AssetCatalog.h
    src/GameDefs.h
    src/GameTypes.h
    src/ModelLibrary.h
    src/ReflectionSystem.h
    src/SpriteLibrary.h
    src/StageFactory.h
    src/SweetsApp.h
    src/TextureLibrary.h
    src/main.cpp
    assets/shaders/basic_lit.hlsl
    assets/shaders/effect_additive.hlsl
    assets/shaders/sprite_unlit.hlsl
    assets/audio/233_BPM163.mp3
    assets/audio/ruins.mp3
)

target_compile_features(SweetsActionDX11 PRIVATE cxx_std_17)
target_compile_definitions(SweetsActionDX11 PRIVATE
    UNICODE
    _UNICODE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
)

set_property(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" PROPERTY VS_STARTUP_PROJECT SweetsActionDX11)
set_target_properties(SweetsActionDX11 PROPERTIES
    VS_DEBUGGER_COMMAND "$<TARGET_FILE:SweetsActionDX11>"
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:SweetsActionDX11>"
)

if(MSVC)
    target_compile_options(SweetsActionDX11 PRIVATE /W4 /permissive- /EHsc /utf-8)
endif()

set_source_files_properties(assets/shaders/basic_lit.hlsl PROPERTIES VS_TOOL_OVERRIDE "None")
set_source_files_properties(assets/shaders/effect_additive.hlsl PROPERTIES VS_TOOL_OVERRIDE "None")
set_source_files_properties(assets/shaders/sprite_unlit.hlsl PROPERTIES VS_TOOL_OVERRIDE "None")

add_custom_command(TARGET SweetsActionDX11 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/assets"
        "$<TARGET_FILE_DIR:SweetsActionDX11>/assets"
)

target_link_libraries(SweetsActionDX11 PRIVATE
    d3d11
    dxgi
    d3dcompiler
    d2d1
    dwrite
    winmm
    windowscodecs
    xinput9_1_0
)
