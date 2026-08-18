# SA Aspect Ratio Fixes

`AspectRatioFixes.asi` is a modular aspect-ratio correction suite for GTA San
Andreas 1.0 US.

It corrects HUD geometry, aiming overlays, gameplay FOV and selected world
sprites without applying a single global scale to unrelated effects. Every
address-specific patch verifies the original instruction or call target first.
If another ASI already owns a hook, the conflicting group is skipped.

## Features

- Exact circular radar and round radar blips at any resolution, each
  correctable on its own.
- Correct weapon reticle, rocket lock-on, sniper scope and camera ring.
- Black sniper surround without transparent side gaps.
- Optional camera crosshair removal and camera/sniper HUD suppression.
- Four-sided one-pixel frame covering the game's multisampling edge bug.
- Horizontal-plus FOV and real framebuffer aspect ratio on an unmodified
  `CDraw` path.
- Selective width correction for pickups, coronas, reflections, sun/moon,
  point lights, birds, clouds, checkpoints and weapon/camera effects.
- Experimental targeting-measurement correction, disabled by default because
  it can affect target selection rather than visuals alone.
- Runtime INI reload with configurable hotkey; default is Alt+H.
- One on/off key per fix, with the two real dependencies named in the file and
  in the log.

## Requirements

- GTA San Andreas 1.0 US or a SA-MP installation based on it.
- An ASI loader.

This build uses fixed 1.0 US addresses. Unsupported or already-modified patch
sites are left untouched. Do not run `HudFix.asi` beside `AspectRatioFixes.asi`;
the latter supersedes it.

## Installation

Copy `AspectRatioFixes.asi` and `AspectRatioFixes.ini` to the game directory or
its `scripts` directory. Press Alt+H after editing the INI.

## Configuration

```ini
# SA Aspect Ratio Fixes v1.0.0
# Created by sonochiwa
# Source code: https://github.com/sonochiwa/sa-aspect-ratio-fixes
#
# Every fix is its own switch: 1 turns it on, 0 leaves the game untouched.
# Edit this file and press Alt+H in game to apply it without restarting.

[general]
# Write AspectRatioFixes.log next to the plugin.
log=0
# Show a message in game after a successful reload.
showNotifications=1
# Reload hotkey, as decimal Win32 virtual-key codes. 18 is Alt and 72 is H.
# Set hotkeyKey=0 to disable the hotkey entirely.
hotkeyModifier=18
hotkeyKey=72

[radar]
# Draw the radar as a circle. The game draws an ellipse on every resolution.
roundRadar=1
# Draw the blip icons round. Independent of roundRadar.
roundBlips=1
# Radar size and position, in units of screen height, so they hold on any
# resolution. Only used while roundRadar=1.
diameter=76
marginLeft=40
marginBottom=28

[crosshair]
# Give the weapon reticle and the rocket lock-on square proportions.
roundCrosshair=1
# Make the sniper scope circle and the camera viewfinder ring round, and fill
# the bands the narrower scope opens beside itself. Needs roundCrosshair=1.
roundScope=1
# Hide the camera viewfinder, leaving the rest of the HUD alone.
noCameraCrosshair=0
# Hide the game HUD while aiming with the camera or the sniper rifle.
hideCameraHud=0
hideSniperHud=0

[widescreen]
# Render at the real aspect ratio of the screen instead of the game's fixed
# 4:3 assumption.
useScreenAspect=1
# Widen the field of view to match, so a wider screen shows more rather than
# cropping. Does nothing while useScreenAspect=0.
fixFov=1

[worldSprites]
# Correct the width of each kind of world sprite. Each is independent, so a
# category that looks wrong in a particular mod can be turned off on its own.
pickups=1
coronas=1
coronaReflections=1
sunMoon=1
pointLights=1
birds=1
clouds=1
checkpoints=1
weaponEffects=1
cameraEffects=1
# Experimental. Affects weapon target selection rather than visuals alone.
targetingMeasurements=0

[probe]
# Diagnostic for development, not a fix. Corrects one candidate site at a time
# so it can be identified in game; see the README.
enabled=0
group=0
hotkeyModifier=18
hotkeyKey=80
```

Every key is an independent on/off switch and takes effect on the next reload.
Two of them depend on another being on, and the file says so where it matters:
the radar layout is only used while `roundRadar=1`, and `fixFov` does nothing
while `useScreenAspect=0` because the conversion scales by the screen aspect
that setting provides. Both are also reported in the log when `log=1`.

## Probing unmapped sites

`references\stretch-x-sites.md` lists every instruction in the executable that
reads the `SCREEN_STRETCH_X` literal, grouped by the function containing it.
233 sites in 64 functions; the modules above patch 27 of them in 7.

A site cannot be classified from its encoding. Each of those functions mixes
position conversions with size calculations, and correcting a position is what
drifted the map blips, so a candidate group has to be watched in game before it
becomes a module. The `[probe]` section does that:

```ini
[probe]
enabled=1
group=0
hotkeyModifier=18
hotkeyKey=80
```

Every site in the selected group is repointed at its own variable, and Alt+P
steps the correction through them one at a time: none, site 1, site 2, and so
on, then the whole group, then none again. The current selection is printed as
a GTA message and written to the log with its address, so the element that
moves can be named without leaving the game. `group` selects one of the two
largest unmapped candidates in the HUD range: `0` is 0x0058EAF0 and `1` is
0x00589650. The weapon icon is the reason they are interesting.

This is a diagnostic, not a feature. It corrects nothing on its own and is off
by default.

## Building

Build `AspectRatioFixes.sln` as `Release|Win32` with Visual Studio 2022 and the
v143 toolset. Outputs are written to `build\`.

## Repository Layout

- `Config\AspectRatioFixes.ini` - canonical default configuration.
- `src\` - plugin source, verified game layout and resources.
- `extras\` - optional camera viewfinder texture and its installation notes.
- `build\` - generated files.

## How It Works

The HUD is authored in a 640x448 space. The radar and crosshair modules repoint
verified horizontal operands to resolution-aware values. World sprites are
different: the plugin redirects only categorized calls to
`CSprite::CalcScreenCoors`, calls the original projection, then multiplies the
returned width by `screenHeight / screenWidth`. Authored width/height ratios
inside each effect remain intact.

The FOV module converts the game's 4:3 horizontal FOV to the current aspect
using the standard tangent conversion. It refuses to overwrite another
plugin's `CDraw` hook.

## License

MIT. See `LICENSE`.
