# Aspect Ratio Fixes

`AspectRatioFixes.asi` is a GTA San Andreas plugin that corrects the HUD,
crosshairs, text, SA-MP textdraws, FOV and world sprites for the display's
aspect ratio.

The game lays its 2D elements out for 4:3. On a wider screen the radar turns
into an oval, the crosshair and sniper scope stretch, text runs wide and
SA-MP textdraws drift apart. Each fix is a separate switch and is applied to
the display's actual aspect ratio.

## Features

- Round radar and radar blips.
- Round weapon reticle, lock-on marker, sniper scope and camera ring.
- HUD block (weapon, ammo, bars, money, clock, wanted level) laid out for a
  chosen aspect.
- All game text at the proportions of a chosen aspect.
- Horizontal-plus FOV.
- Width correction for pickups, coronas, sun, moon, lights, birds, clouds,
  checkpoints and effects.
- SA-MP textdraws laid out for 16:9 and centred on wider screens.
- Black frame covering the multisampling edge line.
- Optional camera crosshair removal and camera/sniper HUD suppression.
- INI reload on Alt+H.

## Requirements

- GTA San Andreas 1.0 US (Compact or Hoodlum executable), or a SA-MP
  installation based on it.
- An ASI loader, such as Silent's ASI Loader or Ultimate ASI Loader.
- The SA-MP 0.3.7-R1 or 0.3.7-R3-1 client for the textdraw module.

Other executables and other `samp.dll` builds are left untouched. Do not run
`HudFix.asi` beside `AspectRatioFixes.asi`; the latter supersedes it.

## Installation

1. Extract `AspectRatioFixes.asi` and `AspectRatioFixes.ini` into the GTA San
   Andreas directory or its `scripts` directory.
2. Start the game.

`extras\cameraCrosshair.png` is an optional replacement camera viewfinder
texture; `extras\README.md` explains how to install it.

## Configuration

```ini
# Aspect Ratio Fixes v1.4.1
# Created by sonochiwa
# Source code: https://github.com/sonochiwa/sa-aspect-ratio-fixes
# Default reload hotkey: Alt + H

[general]
log=0
showNotifications=1
hotkeyEnabled=1
hotkeyModifier=18
hotkeyKey=72

[radar]
roundRadar=1
roundBlips=1
# Only while roundRadar=1. Units of screen height, not pixels.
diameter=86
marginLeft=40
marginBottom=28

[crosshair]
roundCrosshair=1
roundScope=1
noCameraCrosshair=0
hideCameraHud=0
hideSniperHud=0

[hud]
fixPlayerInfo=1
# The aspect the block is laid out for. 0 keeps the screen's own.
playerInfoAspect=16:9
# Percent of that size. 0 keeps it.
playerInfoScale=0
# Right margin in units of screen height. 0 keeps the game's 32.
playerInfoMarginRight=0

[text]
fixText=1
# The aspect the text is proportioned for. 0 keeps the screen's own.
textAspect=16:9

[widescreen]
useScreenAspect=0
# Does nothing while useScreenAspect=0.
fixFov=1

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

[aaEdgeFrame]
# Pixels per side. 0 leaves that edge alone.
left=1
top=1
right=1
bottom=1

[samp]
fitTextdraws=1
# The aspect the server laid its textdraws out for. Wider screens only.
textdrawAspect=16:9
```

| Setting | Default | Meaning |
| --- | ---: | --- |
| `[general]` | | |
| `log` | `0` | Writes `AspectRatioFixes.log` next to the plugin: the geometry it computed and the result of every patch group. |
| `showNotifications` | `1` | Shows a game message when the INI is reloaded. |
| `hotkeyEnabled` | `1` | Enables the reload hotkey. |
| `hotkeyModifier` | `18` | Modifier as a decimal Win32 virtual-key code; `18` is Alt, `0` means none. |
| `hotkeyKey` | `72` | Main key as a decimal Win32 virtual-key code; `72` is H. Removing it or setting `0` disables the hotkey. |
| `[radar]` | | |
| `roundRadar` | `1` | Gives the radar frame one scale on both axes, so the circle is round. |
| `roundBlips` | `1` | Draws blip icons as wide as they are tall. |
| `diameter` | `86` | Radar diameter in HUD units of screen height. Only while `roundRadar=1`. |
| `marginLeft` | `40` | Radar left margin in the same units. Only while `roundRadar=1`. |
| `marginBottom` | `28` | Radar bottom margin in the same units. Only while `roundRadar=1`. |
| `[crosshair]` | | |
| `roundCrosshair` | `1` | Corrects the weapon reticle and the rocket lock-on marker. |
| `roundScope` | `1` | Corrects the sniper scope and the camera viewfinder ring, and fills the sniper surround to the screen edges. |
| `noCameraCrosshair` | `0` | Hides the crosshair while the camera is in use. |
| `hideCameraHud` | `0` | Hides the HUD while the camera is in use. |
| `hideSniperHud` | `0` | Hides the HUD while the sniper scope is in use. |
| `[hud]` | | |
| `fixPlayerInfo` | `1` | Lays the weapon icon, ammo, bars, money, clock and wanted level out as one block. |
| `playerInfoAspect` | `16:9` | The display aspect the block is laid out for, as `16:9` or `1.7778`. `0` keeps the screen's own. |
| `playerInfoScale` | `0` | Size of the block in percent, about its top right corner. `0` keeps the game's own. |
| `playerInfoMarginRight` | `0` | The money's right margin in HUD units of screen height. `0` keeps the game's 32. |
| `[text]` | | |
| `fixText` | `1` | Proportions every other piece of text the font draws for `textAspect`. |
| `textAspect` | `16:9` | The display aspect the text is proportioned for. `0` keeps the screen's own. |
| `[widescreen]` | | |
| `useScreenAspect` | `0` | Sets the game's aspect ratio to the screen's own instead of 4:3. |
| `fixFov` | `1` | Converts the FOV for that aspect. Does nothing while `useScreenAspect=0`. |
| `[worldSprites]` | | |
| `pickups` … `cameraEffects` | `1` | Corrects the width of that kind of world sprite. One switch per kind. |
| `[aaEdgeFrame]` | | |
| `left`, `top`, `right`, `bottom` | `1` | Thickness in pixels of the black frame covering the multisampling edge bug on that side. `0` leaves the edge alone; all four at `0` skip the pass. |
| `[samp]` | | |
| `fitTextdraws` | `1` | Lays SA-MP textdraws out in a centred area of `textdrawAspect` on wider screens. |
| `textdrawAspect` | `16:9` | The aspect the server laid its textdraws out for. Screens not wider than it are left alone. |

Every key is a switch and takes effect on the next reload; Alt+H reloads the
INI in game. The radar layout applies only while `roundRadar=1`, and `fixFov`
only while `useScreenAspect=1`. The aspect keys name the display an element
was designed for: `16:9` keeps the familiar look on an ultrawide screen,
`4:3` gives the original proportions, `0` uses the screen's own.

## Release Integrity

Releases are built by GitHub Actions from the tagged commit and carry a
SHA-256 file and a build-provenance attestation:

```text
gh attestation verify AspectRatioFixes-vX.Y.Z.zip -R sonochiwa/sa-aspect-ratio-fixes
```

## License

MIT. See [LICENSE](LICENSE).
