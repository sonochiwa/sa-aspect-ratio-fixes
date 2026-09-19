# Aspect Ratio Fixes

`AspectRatioFixes.asi` is a standalone GTA San Andreas plugin that corrects
the HUD, the aiming overlays, text, SA-MP textdraws, the gameplay FOV and
selected world sprites for the display's aspect ratio.

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
- Every other piece of text, subtitles, help boxes, area and vehicle names,
  big messages, script text and the frontend, at the proportions of a chosen
  aspect, outline included.
- Optional camera crosshair removal and camera/sniper HUD suppression.
- Four-sided frame covering the game's multisampling edge bug, each side at
  its own thickness in pixels or off.
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

- GTA San Andreas 1.0 US (Compact or Hoodlum executable), or a SA-MP
  installation based on it.
- An ASI loader, such as Silent's ASI Loader or Ultimate ASI Loader.
- The SA-MP 0.3.7-R1 or 0.3.7-R3-1 client for the textdraw module. Every
  other module is independent of SA-MP.

The plugin uses fixed 1.0 US addresses, and the textdraw module fixed
0.3.7-R1 and 0.3.7-R3-1 offsets into `samp.dll`. Unsupported or already
modified patch sites are left untouched, any other `samp.dll` build is
reported in the log and not touched at all, and on any other executable
nothing is patched. Do not run `HudFix.asi` beside `AspectRatioFixes.asi`;
the latter supersedes it.

## Installation

1. Extract `AspectRatioFixes.asi` and `AspectRatioFixes.ini` into the GTA San
   Andreas directory or its `scripts` directory.
2. Start the game.

`extras\cameraCrosshair.png` in the repository is an optional replacement
camera viewfinder texture; `extras\README.md` explains how to install it.

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

Every key is an independent switch and takes effect on the next reload. The
hotkey reloads the INI without restarting the game; the main key is
intercepted only while the modifier is held. Two keys depend on another being
on: the radar layout is used only while `roundRadar=1`, and `fixFov` does
nothing while `useScreenAspect=0`, because the conversion scales by the
screen aspect that setting provides. Both are reported in the log when
`log=1`.

`worldSprites.targetingMeasurements` is read but not shipped, like `[probe]`.
It corrects the two calls where `CSprite::CalcScreenCoors` is used to measure
rather than to draw, and their result decides which target a weapon locks on
to, so it changes aim behaviour without changing a single pixel. Add the key
by hand to experiment with it.

`[text]` covers every piece of text the game draws through its font that no
other module already handles: subtitles, help boxes, area and vehicle names,
big messages, script text, the radio name, the frontend. Any screen shows it
the way a display of `textAspect` and the same height would; `4:3` gives the
original proportions. Positions, wrap widths and boxes drawn around text are
not moved, only the glyphs and their outlines change width.

`[aaEdgeFrame]` gives each side of the frame its thickness in pixels. `1`
covers exactly the edge sample the multisampling bug affects, a larger value
draws a black bar of that many pixels inward from the edge, and `0` leaves
that edge alone, for a display or a driver profile that only shows the bug on
some edges.

`textdrawAspect` is the aspect the server designed its textdraws for. A
screen that is not wider than it is left alone, so a 16:9 or 16:10 display
sees no change; a wider one shows the textdraws exactly as a 16:9 display of
the same height would, centred, with the rest of the width left to the game.

`playerInfoAspect` does the same for the HUD block in the top right corner:
the block is drawn the way a display of that aspect and this height would
draw it, so `16:9` keeps the familiar look on an ultrawide screen and `4:3`
gives the original proportions. `playerInfoScale` resizes the block about its
top right corner and `playerInfoMarginRight` is the money's margin from the
right edge in the same units of screen height the radar's margins use. There
is no top margin: the block's vertical positions are separate literals per
element rather than distances from an edge, so only the scale moves them.

## Building

Visual Studio 2022 (v143), `Release|Win32`. Open `AspectRatioFixes.sln` or
run:

```powershell
msbuild AspectRatioFixes.sln /t:Rebuild /p:Configuration=Release /p:Platform=Win32
```

The plugin is written to `build\AspectRatioFixes.asi` next to a copy of the
INI. `Config\AspectRatioFixes.ini` is compiled into the plugin as an `RCDATA`
resource, so the INI written when the file is missing is byte for byte the
canonical one.

## Repository Layout

```text
AspectRatioFixes.sln
README.md
CHANGELOG.md
LICENSE
.github\workflows\release.yml   Tagged release build, checksum and attestation
Config\
  AspectRatioFixes.ini          Canonical configuration, embedded as RCDATA
extras\
  cameraCrosshair.png           Optional rectangular camera viewfinder frame
  README.md                     How to install it
src\
  AspectRatioFixes.cpp          DllMain, module order, the resolution watcher and the reload hotkey
  AspectRatioFixes.rc           Version resource and the embedded INI
  AspectRatioFixes.vcxproj
  aa_edge.cpp / aa_edge.h       Black frame over the multisampling edge bug
  addresses.h                   Game addresses, prologues and patch sites
  addresses_radar.h             Radar patch sites and probe groups
  config.cpp / config.h         INI creation and loading
  crosshair.cpp / crosshair.h   Reticle, lock-on, scope, viewfinder, sniper fill
  game.cpp / game.h             Thin calls into the game shared by modules
  geometry.cpp / geometry.h     Resolution tracking and per-resolution recompute
  hooking.cpp / hooking.h       Branch writing, trampolines, site guards
  hud.cpp / hud.h               CHud::Draw hook, HUD hiding, notifications
  log.cpp / log.h               Optional log file
  patch.cpp / patch.h           Operand verification and repointing
  player_info.cpp / .h          The weapon, bars, money, clock and wanted level block
  probe.cpp / probe.h           Diagnostic site probe
  radar.cpp / radar.h           Radar frame and blips
  samp_addresses.h              Offsets into the SA-MP client, per build
  text.cpp / text.h             Font scale and outline hooks
  textdraws.cpp / textdraws.h   SA-MP textdraw layout
  types.h                       Stock literals and the game structs the modules share
  world.cpp / world.h           World sprites and the widescreen FOV
  resource.h
  version.h
```

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
every other bar in the game keeps the stock ones and every other piece of
text the text module's.

Every piece of text the game draws sets its scale through `CFont::SetScale`
or `CFont::SetScaleLang`, a second entry that applies a language-dependent
width and stores the result itself, and every caller hands both a width it
has already multiplied by `SCREEN_STRETCH_X`. The plugin hooks the two
entries and multiplies the width again by the ratio of the wanted pixels per
unit to the real ones, except while a textdraw or the player info block is
being drawn, which scale their own text. The font's outline pass reads the
same ratio, so an outline keeps the proportions of its glyphs. Both hooks go
in together or not at all, and the outline factor stays stock until they do.

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
duration of that print. The game's own text is printed outside it, at the
text module's factor.

### Probing unmapped sites

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
hotkeyEnabled=1
hotkeyModifier=18
hotkeyKey=80
```

Every site in the selected group is repointed at its own variable, and the
probe hotkey, Alt+P by default, steps the correction through them one at a time: none, site 1, site 2, and so
on, then the whole group, then none again. The current selection is printed as
a GTA message and written to the log with its address, so the element that
moves can be named without leaving the game. `group` selects one of the two
largest unmapped candidates in the HUD range: `0` is 0x0058EAF0 and `1` is
0x00589650. The weapon icon is the reason they are interesting.

This is a diagnostic, not a feature. It corrects nothing on its own and is off
by default.

## Release Integrity

Tagged releases are built by GitHub Actions from the tagged commit. Each
release carries `AspectRatioFixes-vX.Y.Z.zip`, its SHA-256 in
`AspectRatioFixes-vX.Y.Z.zip.sha256` and a signed build-provenance
attestation, which proves that the archive was produced by this repository's
workflow from that revision. It does not prove the code is bug-free.

```text
gh attestation verify AspectRatioFixes-vX.Y.Z.zip -R sonochiwa/sa-aspect-ratio-fixes
```

## License

MIT. See [LICENSE](LICENSE).
