# Changelog

## 1.5.0

- Changed the INI reload to a word typed in game, `ASPECTFIXES` by default;
  the hotkey keys are gone and the reload message is always shown.
- Added `README.txt` to the release archive.

## 1.4.1

- Fixed the sniper fill covering the MSAA edge line on its own; the line
  now shows or hides uniformly per `[aaEdgeFrame]`.

## 1.4.0

- Changed the shipped INI back to explicit values: every key is present at
  its default and the file the plugin creates is the file to edit. An
  existing file with commented-out keys keeps working.
- Changed the reload hotkey to the numeric form used by the other plugins:
  `hotkeyEnabled`, `hotkeyModifier` and `hotkeyKey` under `[general]`, Alt+H
  by default. `reloadHotkey=Alt+H` is no longer read; the probe hotkey uses
  the same three keys under `[probe]`.
- Changed `showReloadMessage` to `showNotifications`.
- Added version information to the plugin file.
- Removed `README.txt` and the optional texture from the release archive; the
  repository README is the documentation and the texture stays in `extras`.

## 1.3.0

- Changed the shipped INI to list every key commented out at its default. The
  file as generated changes nothing; a key applies once it is uncommented and
  changed, and goes back to its default when commented out or deleted again.
  Existing files with explicit values keep working as before.
- Added `[text]`. Every piece of text the game draws through its font,
  subtitles, help boxes, area and vehicle names, big messages, script text,
  the radio name and the frontend, keeps the proportions of a `textAspect`
  display, `16:9` by default, whatever the screen's own, outlines included.
  Textdraws and the player info block keep their own settings. `0` keeps the
  screen's own proportions.
- Added `[aaEdgeFrame]`. Each side of the frame covering the multisampling
  edge bug, `left`, `top`, `right` and `bottom`, has its own thickness in
  pixels: `1`, the default, covers the affected edge sample as before, more
  draws a bar, `0` leaves that edge alone. With every side at zero the frame
  is not drawn and its render pass is skipped.

## 1.2.0

- Added `[hud]`. The weapon icon, its ammo, the health, armour and breath
  bars, the money counter, the clock and the wanted level are laid out as one
  block for `playerInfoAspect`, `16:9` by default, so an ultrawide screen
  shows them the way a 16:9 display of the same height does instead of
  stretched with the screen; `4:3` gives the original proportions, `0` keeps
  the screen's own. `playerInfoScale` resizes the block about its top right
  corner and `playerInfoMarginRight` sets its margin from the right edge in
  units of screen height; zero keeps the game's own for each. Text outlines
  and bar outlines follow the block, everything else in the game keeps the
  stock values.
- Added `samp.fitTextdraws`. SA-MP textdraws are laid out for a 16:9 display
  and centred on any screen wider than that, so an ultrawide monitor shows
  them at the proportions the server designed them for instead of stretched
  by `aspect / (16/9)`. Text, boxes, sprites and model previews all follow,
  and so does the rectangle a selectable textdraw is hit-tested against. The
  aspect is `samp.textdrawAspect`, written as `16:9` or as a quotient, and a
  screen that is not wider than it is left alone. The box padding SilentPatch
  stretches by the screen width and the font's outline offsets follow the
  layout as well, so a tight grid of boxed textdraws keeps the gaps it has on
  a 16:9 display. The 0.3.7-R1 and 0.3.7-R3-1 clients are mapped; any other
  `samp.dll` build, or one that another plugin has already modified at those
  sites, is named in the log and left untouched.

## 1.1.1

- Fixed the one-pixel AA edge frame inheriting RenderWare 2D state from the
  weather/sky pass, which could tint the frame instead of drawing it black.
- Fixed that frame leaking its temporary RenderWare 2D state into the next
  frame, which could make birds, skidmarks and other transparent world effects
  render through geometry.

## 1.1.0

- Fixed the fill that covers the bands beside a narrowed sniper scope not
  being drawn under a HUD replacement. It hung on the call to
  `CHud::DrawCrossHairs` inside `CHud::Draw`, and a plugin that redirects
  `CHud::Draw` at its prologue never runs the original body, so the hook was
  written and never reached while the replacement called `DrawCrossHairs`
  itself. `CHud::DrawCrossHairs` is hooked at its own entry now, which every
  caller goes through.
- Fixed `hideCameraHud` and `hideSniperHud` doing nothing when another plugin
  owns `CHud::Draw`. SAMPFUNCS takes its prologue for an overlay, and declining
  was correct, but an overlay that hooks a prologue reaches the body through a
  trampoline of its own, so the body still runs every frame. The entry is used
  when it is free and the body when it is not. Which one applied is named in
  the log.
- Fixed the sniper fill reading the reticle's scale factor rather than the
  scope's. The two are equal while both corrections are on, so this was only
  reachable with `roundCrosshair=0` and `roundScope=1`, where the fill was
  sized for a reticle that is not on screen.
- Removed `worldSprites.targetingMeasurements` from the shipped INI. It is the
  one key that changes how the game plays rather than how it looks, its effect
  has never been measured, and it carried the last warning comment in the file.
  It is still read, so it can be added by hand.
## 1.0.0

- Kept full-screen map blips on their world coordinates while the map is
  panned. Map-only position conversions retain the stock horizontal screen
  scale, while icon dimensions remain aspect-corrected.

- Added a white in-game confirmation message after a successful INI reload;
  it can be disabled with `general.showNotifications=0`.

- Renamed the public plugin and runtime files to SA Aspect Ratio Fixes,
  `AspectRatioFixes.asi` and `AspectRatioFixes.ini`.
- Added optional horizontal-plus gameplay FOV and real framebuffer aspect-ratio
  correction with conflict detection for an existing `CDraw` hook.
- Added independently reloadable world-sprite width corrections for pickups,
  coronas, reflections, sun/moon, point lights, birds, clouds, checkpoints,
  weapon effects and camera effects.
- Added a disabled-by-default experimental correction for sprite measurements
  used by weapon target selection.

- Added configurable INI hot reload. The default combination is `Alt+H`;
  `general.reloadHotkey` is written the way it is spoken, `Alt+H`, taking an
  optional Alt, Ctrl or Shift and a letter, digit or function key. `none`
  disables it, and an unreadable value keeps the default rather than silently
  unbinding the key.

- Fixed `hideCameraHud` and `hideSniperHud` being bypassed when another plugin
  replaced the original `Render2dStuff` HUD call after startup. HUD visibility
  is now intercepted directly at the verified `CHud::Draw` entry.

- Added `crosshair.noCameraCrosshair` to hide the camera viewfinder while
  leaving the rest of the game HUD unchanged.
- Added `crosshair.hideCameraHud` to hide the game HUD in camera aiming mode.
  The camera viewfinder remains unless `noCameraCrosshair=1` too.
- Added `crosshair.hideSniperHud` to hide the game HUD while the sniper scope
  is active. The scope and its corrected black surround remain visible.

- Draw a black one-pixel frame on all four screen edges to cover the samples
  exposed by the game's multisampling bug. The earlier Widescreen Fix approach
  only covered the top and left edges unless its symmetric mode was selected.
- Fill the two vertical gaps opened beside the sniper scope by `roundScope=1`.
  The fill follows the corrected scope width at every resolution, overlaps its
  outer black edge by one pixel, and is drawn before the rest of the HUD so it
  cannot cover the radar, weapon icon or other interface elements.

- Added `radar.roundRadar` and `radar.roundBlips`, which correct the radar
  frame and the blip icons drawn inside it independently. The two groups of
  SCREEN_STRETCH_X sites are separate: the frame group covers the radar-space
  transform and everything `CHud::DrawRadar` draws around the circle, the blip
  group covers only the size calculations in `CRadar::DrawEntityBlip`,
  `CRadar::DrawRadarSprite` and `CRadar::ShowRadarTraceWithHeight`. Blip
  positions come from the frame's transform, so either group can be left stock
  without misplacing anything.
- Added a geometrically exact circular radar. The game describes the radar as
  94 units wide and 76 units tall and maps the two axes with different factors,
  so it is an ellipse even at 4:3 and grows wider with the display aspect
  ratio. The plugin makes the horizontal scale used by the radar equal to the
  vertical one, so a circle is a circle on any resolution.
- Added round blip icons. They are sized with the horizontal scale on one axis
  and the vertical scale on the other, so without this they are ovals. Blips on
  the map screen are drawn by the same code and are fixed with them.
- Added a resolution independent radar layout: the diameter and the two margins
  are configured in HUD units of screen height and keep the same proportions on
  every aspect ratio.
- Added aspect correction for the weapon crosshair reticle and the rocket
  launcher lock-on, giving both square proportions.
- Added `crosshair.roundScope`, which makes the sniper scope circle and the
  camera viewfinder ring round. The ring is drawn from a rectangle 256 units
  wide against 192 tall although its texture holds a round ring, so squaring
  the rectangle is enough; the viewfinder frame comes from a different
  rectangle and is left alone. On the sniper rifle the same switch narrows the
  black surround without moving the filler that reaches the screen edges, so
  two vertical bands of the scene show through beside the scope; set it to `0`
  to keep the vanilla scope.
- Added a correction for the rocket launcher lock-on target, under
  `roundCrosshair`. `CWeaponEffects::Render` clamps the marker to a minimum
  of 28 against 20 before drawing it, and at every distance it was measured at
  those clamps, not the projection, were what decided its shape. The two numbers
  are in different units: the width counts in `screenWidth / 640` and the height
  in `screenHeight / 448`. The width minimum is therefore set to
  `20 * (640 * screenHeight) / (448 * screenWidth)`, which is 16.07 at 16:9 and
  20 at 4:3, so both clamps land on the same number of pixels.
- Added a watcher that re-reads every operand the plugin wrote and, with
  `log=1`, reports the first time one stops pointing at the plugin. A second
  modification patching the same instruction would otherwise undo a group
  without leaving a trace.
- Added recalculation of the geometry whenever the resolution changes while the
  game is running. The game reports 640x480 until RenderWare has started, so
  this is required rather than a convenience.
- Added movement of the elements the game pins to the radar: the plane ring,
  the altimeter, the corner masks, the zone name, the vehicle name and the trip
  skip prompt.
- Added restoration of the circular radar mask when another modification has
  replaced the `CRadar::DrawRadarMask` prologue to force a square radar.
- Added verification of every patch site before anything is written. The
  radar's 59 operands are verified as one group, so a partially applied
  geometry is impossible, and an executable that is not 1.0 US is left
  untouched.
- Added a `[probe]` diagnostic. It repoints a candidate group of
  SCREEN_STRETCH_X sites at one variable per site and steps the correction
  through them with a hotkey, naming the current site on screen and in the log,
  so an unmapped site can be identified in a running game before it is turned
  into a module. Two candidate groups are shipped, both in the CHud range.
  The section is absent from the shipped INI and has to be added by hand;
  without it the probe is off and corrects nothing.
- Every setting is a single on/off switch for one fix, named after what it
  does: `roundRadar`, `roundBlips`, `roundCrosshair`, `roundScope`. There are
  no master switches above them and no mode numbers, so there is exactly one
  key to change per fix and only one way to write "off".
- Named the keys so the file needs almost no commentary: `showReloadMessage`
  says what it shows, `reloadHotkey` carries `Alt+H` rather than two
  virtual-key codes, and `roundScope` no longer depends on `roundCrosshair`,
  since the scope replaces the reticle rather than sharing the screen with it.
  Three comments remain, for the units the radar layout is written in, the one
  dependency left, and the experimental sprite option.
- Added log notes for option combinations that cancel themselves out, such as
  `fixFov=1` with `useScreenAspect=0`, where the conversion scales by
  aspect / (4/3) and the aspect is held at 4:3, so it returns the angle
  unchanged. A combination like that is otherwise indistinguishable from a
  patch that failed to apply, because the game simply looks the same.
- Added an optional `AspectRatioFixes.log` describing the computed geometry
  and the result of every patch group.
- Added generation of `AspectRatioFixes.ini` when it is missing, byte for byte
  identical to the canonical `Config\AspectRatioFixes.ini` compiled into the
  plugin.
