cmake_minimum_required(VERSION 3.20)

project(SweetsActionDX11 LANGUAGES CXX)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

option(SWEETS_USE_EFFEKSEER "Enable Effekseer DX11 runtime effects" ON)
set(EFFEKSEER_ROOT
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/effekseer/EffekseerForCpp1.80.3/EffekseerForCpp1.80.3"
    CACHE PATH "Effekseer for C++ 1.80.3 root")

add_executable(SweetsActionDX11 WIN32
    src/AudioSystem.cpp
    src/AssetLoaders.cpp
    src/AssetCatalog.cpp
    src/ChargeAttack.cpp
    src/AppUtil.cpp
    src/AppWindow.cpp
    src/CombatEffects.cpp
    src/CombatLoop.cpp
    src/CoopController.cpp
    src/DataTables.cpp
    src/DimensionRules.cpp
    src/EffekseerSystem.cpp
    src/EnemyController.cpp
    src/FrameView.cpp
    src/GameFlow.cpp
    src/GameplayView.cpp
    src/GraphicsDevice.cpp
    src/HiddenBossLogic.cpp
    src/InputFlow.cpp
    src/MediaView.cpp
    src/MenuView.cpp
    src/ModelLibrary.cpp
    src/PlayerController.cpp
    src/ReflectionSystem.cpp
    src/SaveData.cpp
    src/SpriteLibrary.cpp
    src/SpriteCanvas.cpp
    src/StageFactory.cpp
    src/TextureLibrary.cpp
    src/VideoSystem.cpp
    src/AudioSystem.h
    src/AssetLoaders.h
    src/AssetCatalog.h
    src/DataTables.h
    src/EffekseerSystem.h
    src/GameDefs.h
    src/GameMath.h
    src/GameStateTypes.h
    src/GameTypes.h
    src/GameUiTypes.h
    src/ModelLibrary.h
    src/ReflectionSystem.h
    src/SpriteLibrary.h
    src/SpriteCanvas.h
    src/StageFactory.h
    src/SweetsApp.h
    src/TextureLibrary.h
    src/VideoSystem.h
    src/main.cpp
    assets/shaders/basic_lit.hlsl
    assets/shaders/effect_additive.hlsl
    assets/shaders/postprocess.hlsl
    assets/shaders/sprite_unlit.hlsl
    assets/textures/title.jpg
    assets/textures/2d/player_shortcake.png
    assets/textures/2d/player_chocolate.png
    assets/textures/2d/player_cheese.png
    assets/textures/2d/player_roll.png
    assets/textures/2d/enemy_normal.png
    assets/textures/2d/enemy_shield.png
    assets/textures/2d/enemy_split.png
    assets/textures/2d/enemy_healer.png
    assets/textures/2d/enemy_barrier.png
    assets/textures/2d/enemy_mirror.png
    assets/textures/2d/enemy_mine.png
    assets/textures/2d/enemy_teleport.png
    assets/textures/2d/boss_normal.png
    assets/textures/2d/boss_hidden.png
    assets/textures/2d/shot_player.png
    assets/textures/2d/shot_enemy.png
    assets/textures/2d/slash.png
    assets/textures/2d/obstacle_wall.png
    assets/textures/2d/pickup_attack.png
    assets/textures/2d/pickup_slow.png
    assets/textures/2d/pickup_invincible.png
    assets/textures/2d/pickup_magnet.png
    assets/textures/2d/pickup_bomb.png
    assets/textures/2d/pickup_heal.png
    assets/textures/2d/pickup_ult.png
    assets/textures/2d/pickup_spread.png
    assets/textures/2d/pickup_speed.png
    assets/textures/2d/pickup_score.png
    assets/audio/233_BPM163.mp3
    assets/audio/333_BPM177.mp3
    assets/audio/ruins.mp3
    "assets/audio/Lonery boy.wav"
    assets/audio/hidden_gauge1.mp3
    assets/audio/hidden_gauge2.mp3
    assets/audio/hidden_gauge3.mp3
    assets/audio/se/choco_slash.mp3
    assets/audio/se/ultimate_slash.mp3
    assets/effects/sword_slash.efkefc
    assets/effects/sword/sword_slash.efkefc
    assets/effects/sword/Sword9.efkproj
    assets/effects/sword/Texture/Thunder1.png
    assets/effects/sword/Texture/Line01.png
    assets/effects/sword/Texture/Particle01.png
    assets/effects/sword/Texture/AuroraRing.png
    assets/effects/ult_shortcake.efkefc
    assets/effects/ult_chocolate.efkefc
    assets/effects/ult_cheese.efkefc
    assets/effects/ult_roll.efkefc
    assets/effects/Texture/SwordLine01.png
    assets/effects/Texture/Particle01.png
    assets/effects/Texture/Particle02.png
    assets/effects/Texture/Cloud01.png
    assets/effects/Texture/Flame01.png
    assets/effects/Textures/Aura01_T.png
    assets/effects/Textures/Aura02_T.png
    assets/effects/Textures/Aura04_T.png
    assets/effects/Textures/Aura06_T.png
    assets/effects/Textures/Normal01.png
    assets/video/README_video.md
    assets/data/characters.csv
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

if(SWEETS_USE_EFFEKSEER AND EXISTS "${EFFEKSEER_ROOT}/CMakeLists.txt")
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(BUILD_GL OFF CACHE BOOL "" FORCE)
    set(BUILD_VULKAN OFF CACHE BOOL "" FORCE)
    set(BUILD_DX9 OFF CACHE BOOL "" FORCE)
    set(BUILD_DX11 ON CACHE BOOL "" FORCE)
    set(BUILD_DX12 OFF CACHE BOOL "" FORCE)
    set(BUILD_METAL OFF CACHE BOOL "" FORCE)
    set(BUILD_TEST OFF CACHE BOOL "" FORCE)
    set(BUILD_VIEWER OFF CACHE BOOL "" FORCE)
    set(BUILD_EDITOR OFF CACHE BOOL "" FORCE)
    set(USE_XAUDIO2 OFF CACHE BOOL "" FORCE)
    set(USE_DSOUND OFF CACHE BOOL "" FORCE)
    set(USE_OPENAL OFF CACHE BOOL "" FORCE)
    set(USE_OSM OFF CACHE BOOL "" FORCE)
    if(MSVC)
        set(USE_MSVC_RUNTIME_LIBRARY_DLL ON CACHE BOOL "" FORCE)
    endif()
    add_subdirectory("${EFFEKSEER_ROOT}" "${CMAKE_BINARY_DIR}/effekseer" EXCLUDE_FROM_ALL)
    target_compile_definitions(SweetsActionDX11 PRIVATE SWEETS_USE_EFFEKSEER=1)
    target_link_libraries(SweetsActionDX11 PRIVATE Effekseer EffekseerRendererDX11)
    target_include_directories(SweetsActionDX11 PRIVATE "${EFFEKSEER_ROOT}/src/include")
else()
    target_compile_definitions(SweetsActionDX11 PRIVATE SWEETS_USE_EFFEKSEER=0)
endif()

set_source_files_properties(assets/shaders/basic_lit.hlsl PROPERTIES VS_TOOL_OVERRIDE "None")
set_source_files_properties(assets/shaders/effect_additive.hlsl PROPERTIES VS_TOOL_OVERRIDE "None")
set_source_files_properties(assets/shaders/postprocess.hlsl PROPERTIES VS_TOOL_OVERRIDE "None")
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
    xaudio2
    mf
    mfplat
    mfreadwrite
    mfuuid
    ole32
    windowscodecs
    xinput9_1_0
)
