# Roadmap

## Implemented and validated

- Existing radar, crosshair, scope fill, AA frame and aim-mode HUD modules were
  previously observed in GTA SA at 2560x1440.
- Release Win32 builds cleanly and every categorized world-sprite call site was
  statically verified to target `CSprite::CalcScreenCoors` in the installed
  GTA SA 1.0 US executable.
- FOV and aspect hooks verify their stock prologues and decline to overwrite an
  existing Widescreen Fix or SilentPatch-style hook.

## Implemented, validation pending

- Gameplay FOV on a stock `CDraw` path at 4:3, 16:9 and ultrawide.
- Every world-sprite category in a running game, especially elongated cloud
  textures and corona reflections.
- Hot reload of every new widescreen and world-sprite option.

## Planned after visual validation

- Proportional weapon icon and configurable HUD scale.
- Subtitle scale and safe-area width.
- Main-menu text safe-area corrections.
- Cutscene border and framing options.
- Resolution-list and windowed-mode quality-of-life fixes.

These remain planned because their draw sites overlap broad frontend code and
need screenshot-based verification before they can be shipped as defaults.
