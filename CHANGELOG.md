# Changelog

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
  `general.hotkeyEnabled`, `general.hotkeyModifier` and `general.hotkeyKey`
  use decimal Win32 virtual-key codes.

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
  launcher lock-on, with a choice between square proportions and the
  proportions of a 4:3 display.
- Added `crosshair.roundScope`, which makes the sniper scope circle and the
  camera viewfinder ring round. The ring is drawn from a rectangle 256 units
  wide against 192 tall although its texture holds a round ring, so squaring
  the rectangle is enough; the viewfinder frame comes from a different
  rectangle and is left alone. On the sniper rifle the same switch narrows the
  black surround without moving the filler that reaches the screen edges, so
  two vertical bands of the scene show through beside the scope; set it to `0`
  to keep the vanilla scope.
- Added a correction for the rocket launcher lock-on target, under
  `crosshair.enabled`. `CWeaponEffects::Render` clamps the marker to a minimum
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
- Added verification of all 77 patch sites before anything is written. The
  radar is verified as a whole, so a partially applied geometry is impossible,
  and an executable that is not 1.0 US is left untouched.
- Added a `[probe]` diagnostic. It repoints a candidate group of
  SCREEN_STRETCH_X sites at one variable per site and steps the correction
  through them with a hotkey, naming the current site on screen and in the log,
  so an unmapped site can be identified in a running game before it is turned
  into a module. Two candidate groups are shipped, both in the CHud range.
  Off by default and corrects nothing on its own.
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
