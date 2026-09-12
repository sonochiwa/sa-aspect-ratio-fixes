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
- Weapon icon, ammo, bars, money, clock and wanted level laid out as one
  block for a chosen aspect, with its scale and right margin adjustable.
- Optional camera crosshair removal and camera/sniper HUD suppression.
- Four-sided one-pixel frame covering the game's multisampling edge bug.
- Horizontal-plus FOV and real framebuffer aspect ratio on an unmodified
  `CDraw` path.
- Selective width correction for pickups, coronas, reflections, sun/moon,
  point lights, birds, clouds, checkpoints and weapon/camera effects.
- Experimental targeting-measurement correction, absent from the shipped INI
  because it changes target selection rather than anything visible.
- SA-MP textdraws laid out for 16:9 and centred on any wider screen, with
  the hit rectangle of a selectable textdraw following the drawing.
- Runtime INI reload on a hotkey written as `Alt+H`; default is Alt+H.
- One on/off key per fix, with the two real dependencies named in the file and
  in the log.

## Requirements

- GTA San Andreas 1.0 US or a SA-MP installation based on it.
- An ASI loader.
- The SA-MP 0.3.7-R1 or 0.3.7-R3-1 client for the textdraw module. Every
  other module is independent of SA-MP.

This build uses fixed 1.0 US addresses, and the textdraw module uses fixed
0.3.7-R1 and 0.3.7-R3-1 offsets into `samp.dll`. Unsupported or already-modified patch sites
are left untouched, and any other `samp.dll` build is reported in the log and
not touched at all. Do not run `HudFix.asi` beside `AspectRatioFixes.asi`;
the latter supersedes it.

## Installation

Copy `AspectRatioFixes.asi` and `AspectRatioFixes.ini` to the game directory or
its `scripts` directory. Press Alt+H after editing the INI.

## Configuration

```ini
# SA Aspect Ratio Fixes v1.2.0
# Created by sonochiwa
# Source code: https://github.com/sonochiwa/sa-aspect-ratio-fixes

[general]
log=0
showReloadMessage=1
reloadHotkey=Alt+H

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

[samp]
fitTextdraws=1
# The aspect the server laid its textdraws out for. Wider screens only.
textdrawAspect=16:9
```

Every key is an independent on/off switch and takes effect on the next reload.
Two of them depend on another being on and say so in the file: the radar layout
is used only while `roundRadar=1`, and `fixFov` does nothing while
`useScreenAspect=0`, because the conversion scales by the screen aspect that
setting provides. Both are reported in the log when `log=1`.

`worldSprites.targetingMeasurements` is read but not shipped, like `[probe]`.
It corrects the two calls where `CSprite::CalcScreenCoors` is used to measure
rather than to draw, and their result decides which target a weapon locks on
to, so it changes aim behaviour without changing a single pixel. Add the key by
hand to experiment with it.

`reloadHotkey` is written the way it is spoken: an optional `Alt`, `Ctrl` or
`Shift`, then a letter, a digit or `F1` to `F12`. `none` disables it. A value
the plugin cannot read is treated as a typo and leaves the default binding in
place rather than silently unbinding the key.

`textdrawAspect` is the aspect the server designed its textdraws for, written
as `16:9` or as the quotient `1.7778`. A screen that is not wider than it is
left alone, so a 16:9 or 16:10 display sees no change; a wider one shows the
textdraws exactly as a 16:9 display of the same height would, centred, with
the rest of the width left to the game. Servers lay textdraws out for 16:9,
which is why that is the default.

`playerInfoAspect` does the same for the HUD block in the top right corner,
the weapon icon and ammo, the three bars, the money, the clock and the wanted
level: the block is drawn the way a display of that aspect and this height
would draw it, so `16:9` keeps the familiar look on an ultrawide screen and
`4:3` gives the original proportions. `playerInfoScale` is a percentage that
resizes the block about its top right corner, and `playerInfoMarginRight` is
the money's margin from the right edge in the same units of screen height
the radar's margins use; the game's own is 32. Zero keeps the game's own
value for any of the three. There is no top margin: the block's vertical
positions are separate literals per element rather than distances from an
edge, so only the scale moves them.

## Probing unmapped sites

`references\stretch-x-sites.md` lists every instruction in the executable that
reads the `SCREEN_STRETCH_X` literal, grouped by the function containing it.
233 sites in 64 functions; the modules above patch 27 of them in 7.

A site cannot be classified from its encoding. Each of those functions mixes
position conversions with size calculations, and correcting a position is what
drifted the map blips, so a candidate group has to be watched in game before it
becomes a module. Adding a `[probe]` section to the INI does that. It is a
development tool and is deliberately absent from the shipped file:

```ini
[probe]
enabled=1
group=0
hotkey=Alt+P
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

The player info block, the weapon icon and ammo, the three bars, the money,
the clock and the wanted level, is drawn by `CHud::DrawPlayerInfo` and
`CHud::DrawWantedLevel` against the right edge: every width and horizontal
margin as `SCREEN_STRETCH_X(units)` of a screen width read on the spot, every
height and vertical position as `SCREEN_STRETCH_Y(units)`. The plugin repoints
the 27 horizontal factor operands, the 27 width reads, the 26 vertical factor
operands and the pre-multiplied literal that holds the icon's margin. The
horizontal factor sets the aspect, both factors set the scale, and the width
the positions are measured back from sets the margin, with the factor
divided by that width so sizes stay put. The two passes are wrapped so that
the font's outline offsets and the bar outline drawn by
`CSprite2d::DrawBarChart` use the same factors for exactly their duration;
every other bar and text in the game keeps the stock ones.

SA-MP maps textdraws to the framebuffer the way the game maps its HUD, so on a
screen wider than 16:9 they are stretched by `aspect / (16/9)`.
`CTextDraw::Draw` reads the screen width once on each of its two paths, and
both loads are absolute operands. The plugin repoints them at a width of its
own, the width of a 16:9 area of the screen's height, which makes the whole
function lay out as it would on a 16:9 display. The four places an X position
leaves the function, the text print, the wrap edge, the sprite rectangle and
the stored hit rectangle, are shifted right by half the difference. Scales and
widths need no shift, so the scale and centre-size calls are not touched.

Two things inside the game's own font follow the real width rather than the
layout: the padding `CFont::GetTextRect` puts around a box, which SilentPatch
stretches by the screen size, and the offsets the outline and drop shadow pass
applies to each copy of the string. The call to `GetTextRect` inside
`CFont::PrintString` is retargeted, and while a textdraw is being printed a
horizontal padding that equals the stretched stock value is re-stretched by
the layout width; the nine `SCREEN_STRETCH_X` operands of the outline pass are
repointed at a variable that holds the layout's factor for exactly the
duration of that print. The game's own text is printed outside it and keeps
its stock values.

## Release Integrity

Tagged release archives are built by GitHub Actions from the corresponding
source revision. Each release includes a SHA-256 checksum and a signed build
provenance attestation. Verify the attestation with GitHub CLI:

```text
gh attestation verify AspectRatioFixes-v1.2.0.zip -R sonochiwa/sa-aspect-ratio-fixes
```

This verifies the archive's origin and integrity; it is not a guarantee that
the software is bug-free or safe.

## License

MIT. See `LICENSE`.
