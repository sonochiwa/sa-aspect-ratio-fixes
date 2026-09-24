# Changelog

## 1.5.1

- Fixed the aircraft horizon and the black radar ring not fitting the map
  when `diameter` is changed.

## 1.5.0

- Changed the INI reload to a word typed in game, `ASPECTFIXES` by default;
  the hotkey keys are gone and the reload message is always shown.
- Added `README.txt` to the release archive.

## 1.4.1

- Fixed the sniper fill hiding the MSAA edge line on the sides while the
  centre still showed it; `[aaEdgeFrame]` alone decides now.

## 1.4.0

- Changed the shipped INI back to explicit values.
- Changed the reload hotkey to `hotkeyEnabled`/`hotkeyModifier`/`hotkeyKey`.
- Changed `showReloadMessage` to `showNotifications`.
- Added version information to the plugin file.
- Removed `README.txt` and the optional texture from the release archive.

## 1.3.0

- Changed the shipped INI to list every key commented out at its default.
- Added `[text]`: all game text keeps the proportions of a `textAspect`
  display, `16:9` by default.
- Added `[aaEdgeFrame]`: a thickness in pixels per screen edge, `0` leaves
  that edge alone.

## 1.2.0

- Added `[hud]`: the weapon, ammo, bars, money, clock and wanted level are
  laid out as one block for `playerInfoAspect`, with scale and right margin.
- Added `samp.fitTextdraws`: SA-MP textdraws are laid out for 16:9 and
  centred on wider screens, hit rectangles included.

## 1.1.1

- Fixed the edge frame being tinted instead of black.
- Fixed birds, skidmarks and other transparent effects rendering through
  geometry after the edge frame was drawn.

## 1.1.0

- Fixed the sniper fill not being drawn under a HUD replacement.
- Fixed `hideCameraHud` and `hideSniperHud` doing nothing when another
  plugin owns the HUD.
- Fixed the sniper fill being sized wrong with `roundCrosshair=0` and
  `roundScope=1`.
- Removed `worldSprites.targetingMeasurements` from the shipped INI; it is
  still read.

## 1.0.0

- Round radar, blips, crosshair, lock-on marker, sniper scope and camera
  ring, each with its own switch.
- Resolution-independent radar size and margins.
- Black filler beside the corrected sniper scope and a one-pixel frame over
  the multisampling edge bug.
- Optional horizontal-plus FOV and world sprite width corrections.
- Optional camera crosshair removal and camera/sniper HUD suppression.
- INI reload on Alt+H with an on-screen message, optional log, INI created
  when missing.
