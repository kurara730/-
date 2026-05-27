# Effekseer Runtime Notes

- CMake builds enable Effekseer DX11 runtime with `SWEETS_USE_EFFEKSEER=ON`.
- The SDK root is `third_party/effekseer/EffekseerForCpp1.80.3/EffekseerForCpp1.80.3`.
- Replaceable effects are stored in `assets/effects/`.
- The placeholder texture files required by these effects are stored under `assets/effects/Texture/` and `assets/effects/Textures/`.
- Current effect ids:
  - `sword_slash`: chocolate melee and charged slash.
  - `ult_shortcake`: shortcake ultimate.
  - `ult_chocolate`: chocolate ultimate and combo ultimate placeholder.
  - `ult_cheese`: cheese fortress ultimate.
  - `ult_roll`: roll screen slam ultimate.
- Missing `.efkefc` files, disabled runtime, or load failures keep the game running with existing ring/particle fallback visuals.
- `SweetsActionDX11_Game.sln` remains dependency-safe with `SWEETS_USE_EFFEKSEER=0`. It will show gameplay fallback particles if Effekseer is disabled. Use `cmake --build build --config Debug` or the generated CMake solution for active Effekseer runtime playback.

# Coop Slot Notes

- 2P-4P default to `Off`.
- In character select, click each slot's `Off / AI / Pad` buttons to set participation.
- `AI` always uses AI control.
- `Pad` only uses the assigned XInput gamepad and does not automatically become AI when disconnected.
- Retry preserves the selected slot modes and characters.
