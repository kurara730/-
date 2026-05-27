# Asset Replacement Notes

The current renderer uses procedural fallback meshes so the project builds without external assets.

Replaceable visual entries are registered in `src/AssetCatalog.cpp` and default texture/sprite IDs are registered in `src/AssetLoaders.cpp`.

Each `VisualAsset` can point to:

- `texturePath`: future material texture for a 3D mesh
- `spritePath`: future 2D sprite or billboard image
- `modelPath`: future FBX/glTF model path

The DX11 renderer currently reads the catalog for fallback colors and mesh roles. `TextureLibrary` now loads WIC-supported image files into shader resource views, and the placeholder PNGs in `assets/textures` can be replaced later with final art using the same filenames or by changing the registered paths.

Default placeholder textures:

- `assets/textures/shortcake.png`
- `assets/textures/chocolate.png`
- `assets/textures/cheese.png`
- `assets/textures/roll.png`
- `assets/textures/enemy.png`
- `assets/textures/pickup.png`
