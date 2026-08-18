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
- Log notes naming option combinations that cancel themselves out.

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
# Default reload hotkey: Alt + H

[general]
log=0
hotkeyEnabled=1
hotkeyModifier=18
hotkeyKey=72
showNotifications=1

[radar]
enabled=1
roundRadar=1
roundBlips=1
diameter=76
marginLeft=40
marginBottom=28

[crosshair]
enabled=1
aspectMode=1
roundScope=1
noCameraCrosshair=0
hideCameraHud=0
hideSniperHud=0

[widescreen]
fixFov=1
useScreenAspect=1

[worldSprites]
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
targetingMeasurements=0
```

`showNotifications=1` displays a white GTA message after a successful hot
reload. `roundRadar` corrects the radar frame, its circular mask and the
elements pinned to it; `roundBlips` corrects the blip icons drawn inside it.
Blip positions come from the frame's transform, so either can be turned off
without misplacing anything. `diameter`, `marginLeft` and `marginBottom`
describe the radar in HUD units of screen height and therefore only apply while
`roundRadar=1`; with it off the radar keeps its stock rectangle. Setting
`enabled=0` turns off both groups and the layout. `aspectMode=0` keeps vanilla stretch, `1` uses square pixel proportions and
`2` reproduces the 4:3 appearance. World-sprite keys can be toggled
independently and take effect after an INI reload. `fixFov` is applied only
when the stock `CDraw::SetFOV` is present; an existing widescreen/FOV hook wins.

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
