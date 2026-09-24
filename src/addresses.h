// Addresses and patch sites for gta_sa.exe 1.0 US (14,383,616 bytes).
//
// GTA San Andreas draws its HUD in a fixed 640x448 design space and maps it to
// pixels with two pooled float literals:
//
//     SCREEN_STRETCH_X(a) = a * RsGlobal.maximumWidth  * (1.0f / 640.0f)
//     SCREEN_STRETCH_Y(a) = a * RsGlobal.maximumHeight * (1.0f / 448.0f)
//
// Both factors live in writable data, and every instruction that uses them
// encodes their address as a 32 bit absolute operand. The same is true for the
// four literals that describe the radar rectangle. The plugin therefore never
// rewrites code: it only repoints those operands at its own variables.
//
// Every site below was extracted from the retail executable by scanning for
// x87 instructions with a mod=00 rm=101 (disp32) memory operand, and each one
// is re-verified against the running process before it is patched.

#pragma once

#include <cstddef>
#include <cstdint>

namespace game {

// RsGlobal @ 0x00C17040.
// RsGlobal.maximumWidth  (int32)
constexpr uintptr_t kScreenWidth = 0x00C17044;
// RsGlobal.maximumHeight (int32)
constexpr uintptr_t kScreenHeight = 0x00C17048;

// FrontendIdle reaches its final 2D tail here. Retail layouts use a RET or a
// tail JMP. The widescreen fix uses the same hook to draw over half-covered
// edge samples left by multisampling. A normal function can return straight to
// FrontendIdle's caller because the stack has already been restored here.
constexpr uintptr_t kRender2dStuffReturn = 0x0053E90E;

// CSprite2d::DrawRect(const CRect&, const CRGBA&). Using the game's own 2D
// renderer keeps these rectangles in the same render target and render-state
// path as the HUD (a Direct3D EndScene/Present clear does not reach it).
constexpr uintptr_t kDrawRect = 0x00727B60;

// DefinedState2d resets RenderWare state for untextured 2D primitives.
constexpr uintptr_t kDefinedState2d = 0x00734750;

// Pointer to RenderWare's non-debug RwGlobals instance. The AA edge-frame
// hook snapshots the device render states through the callbacks stored there
// before asking DefinedState2d to configure immediate-mode drawing.
constexpr uintptr_t kRwEngineInstance = 0x00C97B24;

// CMessages::AddMessageJump(const char*, uint32, uint16, bool).
constexpr uintptr_t kAddMessageJump = 0x0069F1E0;

// CDraw and CSprite entry points/globals used by the widescreen and selective
// world-sprite modules.
constexpr uintptr_t kSetFov = 0x006FF410;
constexpr uintptr_t kCalculateAspectRatio = 0x006FF420;
constexpr uintptr_t kFov = 0x008D5038;
constexpr uintptr_t kAspectRatio = 0x00C3EFA4;
constexpr uintptr_t kCalcScreenCoors = 0x0070CE30;
constexpr uintptr_t kSetFovCallSites[] = {
    0x0052C976, 0x0053BD7A, 0x005BA224,
};
constexpr uintptr_t kCalculateAspectCallSites[] = {
    0x0053D694, 0x0053D7B1, 0x0053D966, 0x0053E770, 0x0053EB19,
};

constexpr uintptr_t kSpritePickupSites[] = {0x00455A6A};
constexpr uintptr_t kSpriteCoronaSites[] = {0x006FB009, 0x006FB24E};
constexpr uintptr_t kSpriteCoronaReflectionSites[] = {0x006FB868};
constexpr uintptr_t kSpriteSunMoonSites[] = {0x006FC75D};
constexpr uintptr_t kSpritePointLightSites[] = {0x0070075A, 0x00700A9C};
constexpr uintptr_t kSpriteBirdSites[] = {0x0071290F};
constexpr uintptr_t kSpriteCloudSites[] = {
    0x00713ABB, 0x00713E81, 0x00713FB9, 0x007141BB, 0x007142CF,
    0x00715E8B,
};
constexpr uintptr_t kSpriteCheckpointSites[] = {0x00725D02};
constexpr uintptr_t kSpriteWeaponEffectSites[] = {
    0x00742D90, 0x0074318D, 0x00743AF0,
};
constexpr uintptr_t kSpriteCameraEffectSites[] = {
    0x0073AA2A, 0x0073C325, 0x0073C5C2,
};
constexpr uintptr_t kSpriteTargetingSites[] = {0x0073E320, 0x0073E47B};

// CHud::Draw calls DrawCrossHairs once near its start. The call itself is
// located at runtime so the patch can verify both its opcode and destination.
constexpr uintptr_t kDrawHud = 0x0058FAE0;
constexpr uintptr_t kDrawCrossHairs = 0x0058E020;

// CHud::DrawCrossHairs is hooked at its own entry rather than at the call to
// it inside CHud::Draw. A HUD replacement that redirects CHud::Draw at its
// prologue never runs the original body, so a hook on that call site is
// installed but never reached, while the replacement still calls DrawCrossHairs
// itself. Hooking the function catches it either way.
//
// The first two instructions are `sub esp, 30h` and a movzx from an absolute
// address. Ten bytes, neither of them position dependent, so they can be
// copied into a trampoline and re-executed from anywhere.
constexpr uintptr_t kDrawCrossHairsBody = 0x0058E02A;
constexpr uint8_t kDrawCrossHairsPrologue[] = {
    0x83, 0xEC, 0x30,                                // sub  esp, 30h
    0x0F, 0xB6, 0x05, 0x74, 0xCD, 0xB7, 0x00,        // movzx eax, byte ptr ds:[B7CD74]
};

// The first instruction in CHud::Draw is a seven-byte test of this frontend
// flag. The HUD visibility hook replaces that complete instruction and enters
// the original function immediately after its conditional branch. Hooking the
// function itself catches every caller and cannot be bypassed when another ASI
// redirects Render2dStuff's original call later during startup.
constexpr uintptr_t kDrawHudBody = 0x0058FAED;
constexpr uintptr_t kHudDisabled = 0x00A43088;
constexpr uint8_t kDrawHudPrologue[] = {
    0x80, 0x3D, 0x88, 0x30, 0xA4, 0x00, 0x01, // cmp byte ptr ds:[A43088], 1
};

// SAMPFUNCS and other overlays take the prologue above for themselves, and
// they reach the body through a trampoline of their own, so the body still
// runs. Hooking it instead of the entry therefore coexists with them.
//
// The body opens with a five byte load from an absolute address, which is
// exactly the width of a relative jump and needs no relocating, so the branch
// replaces one whole instruction and nothing has to be padded.
constexpr uintptr_t kDrawHudBodyResume = 0x0058FAF2;
constexpr uint8_t kDrawHudBodyPrologue[] = {
    0xA0, 0xC1, 0xA7, 0xC8, 0x00, // mov al, byte ptr ds:[C8A7C1]
};

// CFont. SA-MP prints a textdraw through CFont::PrintString and draws its
// box through the font's background, so two things inside the font follow
// the real screen width rather than the width the textdraw was laid out for:
//
// The box is sized by CFont::GetTextRect, which pads the text's extent by a
// 4.0f literal on every side. SilentPatch repoints those operands at 4 units
// stretched by the real screen size, which is what a textdraw's box has on a
// 16:9 display too, except that the horizontal one comes out of the real
// width. The call to GetTextRect inside PrintString is retargeted, and while
// a textdraw is being printed a horizontal padding that equals the stretched
// stock value is re-stretched by the layout width instead. The vertical one
// is left alone, and so is the stock 4 pixels of an unpatched game.
//
// The outline and drop shadow pass offsets every copy of the string by
// SCREEN_STRETCH_X(size). Those nine sites read the pooled factor like the
// HUD does, so they are repointed at a variable that holds the factor of
// whatever text is being printed: the layout's for a textdraw, the block's
// during a HUD pass, the text module's for everything else.
constexpr uintptr_t kFontGetTextRect = 0x0071A620;
constexpr uintptr_t kFontGetTextRectCallSites[] = {
    0x0071A77B, // CFont::PrintString, the box behind the text
};
// RenderState, bool
constexpr uintptr_t kFontCentre = 0x00C71A79;
// RenderState, bool
constexpr uintptr_t kFontRightJustify = 0x00C71A7A;
// RenderState, float
constexpr uintptr_t kFontWrapX = 0x00C71A88;
// RenderState, float
constexpr uintptr_t kFontCentreSize = 0x00C71A8C;
constexpr float kFontBoxPadding = 4.0f;

constexpr uintptr_t kFontShadowStretchXSites[] = {
    0x00719C0D, 0x00719C6E, 0x00719D2D, 0x00719D94, 0x00719DD1,
    0x00719E0E, 0x00719E4B, 0x00719E6F, 0x00719E97,
};

// Every piece of text the game draws sets its scale through one of two
// doors, and every caller hands both a width it has already multiplied by
// SCREEN_STRETCH_X. CFont::SetScale(float w, float h) stores the pair; the
// HUD, script text, big messages and one subtitle branch use it.
// CFont::SetScaleLang(float w, float h) is the same store behind a check of
// the language, which widens the text for some of them, and never passes
// through SetScale; help boxes, area and vehicle names, odd job messages, the
// other subtitle branches and the frontend use that one. Hooking both
// entries rescales the width for all of them at once. Each prologue is one
// or two whole instructions with nothing position dependent in them, and
// both functions are cdecl with nothing else on the stack.
constexpr uintptr_t kFontSetScale = 0x00719380;
constexpr uintptr_t kFontSetScaleBody = 0x00719388;
constexpr uint8_t kFontSetScalePrologue[] = {
    0x8B, 0x44, 0x24, 0x04, // mov eax, [esp+4]
    0x8B, 0x4C, 0x24, 0x08, // mov ecx, [esp+8]
};
constexpr uintptr_t kFontSetScaleLang = 0x007193A0;
constexpr uintptr_t kFontSetScaleLangBody = 0x007193A7;
constexpr uint8_t kFontSetScaleLangPrologue[] = {
    0x0F, 0xBE, 0x05, 0xCC, 0x67, 0xBA, 0x00, // movsx eax, byte ptr [BA67CC]
};

// The player info block: the weapon icon and its ammo, the health, armour and
// breath bars, the money counter, the clock and the wanted level. CHud draws
// all of it against the right edge: every width and horizontal margin as
// SCREEN_STRETCH_X(units) taken from a screen width it reads on the spot,
// every height and vertical position as SCREEN_STRETCH_Y(units). Three
// things about the block are therefore decided by three sets of operands:
//
//   the horizontal factor, which sets the proportion the block is laid out
//   for; the vertical factor, which with the horizontal one sets its size;
//   and the width it is anchored to, which sets its margin from the edge.
//
// CHud::DrawPlayerInfo @ 0x58EAF0 draws the clock, the bars and the money and
// positions the icon and the ammo, CHud::DrawWeaponIcon @ 0x58D7D0 draws the
// icon, 0x5893B0 the ammo, 0x5890A0, 0x589190 and 0x589270 the three bars,
// and CHud::DrawWantedLevel @ 0x58D9A0 the stars. The sites that position
// the second player's icon and ammo are included so a two player game draws
// both the same way.
constexpr uintptr_t kPlayerInfoStretchXSites[] = {
    0x0058EB3F, 0x0058EC0C,             // clock scale and position
    0x0058EE7E, 0x0058EEF4,             // health bar position
    0x0058EF50, 0x0058EFC5,             // armour bar position
    0x0058F116, 0x0058F194,             // breath bar position
    0x0058F55C, 0x0058F5F4,             // money scale and position
    0x0058F91C, 0x0058F993,             // weapon icon position
    0x0058F9D0, 0x0058FA5D,             // ammo position
    0x0058D8C3, 0x0058D92D,             // CHud::DrawWeaponIcon, 47 wide
    0x005894C5, 0x005894E9,             // ammo scale and centre size
    0x00589155, 0x0058922D, 0x005892CA, // bar widths, 62, 62 and 109
    0x0058937E,
    0x0058DCB8, 0x0058DD00, 0x0058DD7E, // CHud::DrawWantedLevel
    0x0058DF71, 0x0058DFE5,
};

// Every read of RsGlobal.maximumWidth in those functions. Each one either
// feeds a site above or is the width a position is measured back from, so
// repointing them all at one value anchors the block to that width while the
// factor, divided by the same value, keeps every size what it was.
constexpr uintptr_t kPlayerInfoWidthReadSites[] = {
    0x0058EB39, 0x0058EBE2, 0x0058EE45, 0x0058EEAB, 0x0058EF15, // DrawPlayerInfo
    0x0058EF7C, 0x0058F0DB, 0x0058F14B, 0x0058F556, 0x0058F5AE,
    0x0058F8FA, 0x0058F956, 0x0058F9C6, 0x0058FA52,
    0x0058D8BC, 0x0058D927,                                     // DrawWeaponIcon
    0x005894BF, 0x005894E0,                                     // ammo
    0x0058914F, 0x00589227, 0x005892BF, 0x00589378,             // bars
    0x0058DCB2, 0x0058DCFA, 0x0058DD77, 0x0058DF6B, 0x0058DFDB, // DrawWantedLevel
};

// The vertical factor. Positions are measured down from the top edge as
// plain products, so scaling this scales the block about the top edge.
constexpr uintptr_t kPlayerInfoStretchYSites[] = {
    0x0058EB29, 0x0058EBF9, 0x0058EE60, 0x0058EEC8, 0x0058EF32, // DrawPlayerInfo
    0x0058EF99, 0x0058F0F8, 0x0058F168, 0x0058F546, 0x0058F5CE,
    0x0058F90B, 0x0058F972, 0x0058F9C0, 0x0058FA4A,
    0x0058D882, 0x0058D945,                                     // DrawWeaponIcon
    0x005894AF,                                                 // ammo
    0x0058913E, 0x00589216, 0x00589346,                         // bars
    0x0058DCA2, 0x0058DD68, 0x0058DDF4, 0x0058DEE4, 0x0058DF55, // DrawWantedLevel
    0x0058DF9B,
};

// The icon's 111 unit margin inside the block is a pre-multiplied literal,
// 111 / 640, applied to the width read directly rather than through the
// pooled factor. It is repointed at a variable that carries the same units
// at the block's factor and width.
constexpr uintptr_t kWeaponIconMargin = 0x00866C84;
constexpr uintptr_t kWeaponIconMarginSites[] = {
    0x0058F92D, 0x0058F9F5, // icon and ammo, first player
    0x0058FA8E,             // ammo, second player
};

// Two helpers the block shares with the rest of the game: CSprite2d::
// DrawBarChart outlines every bar SCREEN_STRETCH_X(2) by SCREEN_STRETCH_Y(2)
// thick, and 0x588B60 moves a bar up by SCREEN_STRETCH_Y(units) while the
// wanted level is hidden. Their sites are repointed at variables that hold
// the block's factors only while the block is being drawn.
constexpr uintptr_t kHudPassStretchXSites[] = {
    0x007288F5, 0x00728941, // CSprite2d::DrawBarChart
};
constexpr uintptr_t kHudPassStretchYSites[] = {
    0x00728864, 0x007288A9, // CSprite2d::DrawBarChart
    0x00588B9C,             // bar offset helper
};

// CHud::Draw calls the two passes back to back. Both are wrapped so the
// font's outline pass and the shared helpers use the block's factors for
// exactly their duration.
constexpr uintptr_t kDrawPlayerInfo = 0x0058EAF0;
constexpr uintptr_t kDrawWantedLevel = 0x0058D9A0;
constexpr uintptr_t kDrawPlayerInfoCallSites[] = {0x0058FBD6};
constexpr uintptr_t kDrawWantedLevelCallSites[] = {0x0058FBDB};

// CCamera layout in 1.0 US. The active camera index is a byte and each CCam
// stores its eCamMode as a 16 bit value at +0x0C.
constexpr uintptr_t kCamera = 0x00B6F028;
constexpr size_t kCameraActiveIndexOffset = 0x59;
constexpr size_t kCameraArrayOffset = 0x174;
constexpr size_t kCameraSize = 0x238;
constexpr size_t kCameraModeOffset = 0x0C;
constexpr int16_t kCameraModeSniper = 7;
constexpr int16_t kCameraModeCamera = 46;

// CWeapon::ms_bTakePhoto. CHud::Draw intentionally returns on the capture
// frame so the saved photograph has no viewfinder; the HUD wrapper preserves
// that behaviour.
constexpr uintptr_t kTakePhoto = 0x00C8A7C1;

// Pooled float literals.
// 1.0f / 640.0f
constexpr uintptr_t kStretchX = 0x00859520;
// 1.0f / 448.0f
constexpr uintptr_t kStretchY = 0x00859524;
// 40.0f,  radar left edge
constexpr uintptr_t kRadarLeft = 0x00858A10;
// 104.0f, radar top edge above the bottom
constexpr uintptr_t kRadarTop = 0x00866B70;
// 76.0f,  radar height
constexpr uintptr_t kRadarHigh = 0x00866B74;
// 94.0f,  radar width
constexpr uintptr_t kRadarWide = 0x00866B78;
// 18.0f,  among other uses how much smaller than the radar the plane ring is
constexpr uintptr_t kRadarRingInset = 0x00859008;
// 4.0f,   among other uses how far the corner masks reach past the radar
constexpr uintptr_t kRadarMaskPad = 0x00858B90;

// CRadar::DrawRadarMask. Some square radar modifications replace the prologue
// with a RET or a JMP; restoring it brings the circular mask back.
constexpr uintptr_t kDrawRadarMask = 0x00585700;

constexpr uint8_t kDrawRadarMaskPrologue[] = {
    0x55, 0x8B, 0xEC, 0x83, 0xE4, 0xF8, 0x83, 0xEC,
    0x6C, 0xA1, 0x24, 0x7B, 0xC9, 0x00, 0x53, 0x56,
    0x57, 0x6A, 0x00, 0x6A, 0x01, 0xC7, 0x44, 0x24,
    0x20, 0x00, 0x00, 0x80, 0xBF, 0xC7, 0x44, 0x24,
};

// The design space the HUD is authored in.
constexpr float kDesignWidth = 640.0f;
constexpr float kDesignHeight = 448.0f;

// CHud::DrawCrossHairs computes five sizes, established by patching them one
// group at a time and looking at what moved on screen:
//
//   0x58E2FA, 0x58E4ED  the weapon reticle, a 64x64 unit square
//   0x58E75B            the rocket launcher lock-on frame and target
//   0x58E7CE, 0x58E7F8  the scope overlay, 210x210 and 256x192
//
// The last two are one element, not alternatives: the branch above 0x58E7CE
// skips it and lands on 0x58E7F8, and falls through into it otherwise.
//
// The scope sites are kept separate because roundScope can be disabled. When
// they are corrected, the DrawCrossHairs call hook below extends the black
// surround to the new, narrower texture bounds.
constexpr uintptr_t kCrosshairStretchXSites[] = {
    0x0058E2FA, 0x0058E4ED, // reticle sprite
    0x0058E75B,             // rocket launcher lock-on
};

// The scope overlay, shared by the sniper rifle and the camera. The branch
// above 0x58E7CE skips it and lands on 0x58E7F8, and falls through into it
// otherwise, so the two rectangles are one element rather than alternatives.
//
// Correcting these makes the scope circle and the camera viewfinder ellipse
// round. It also narrows the textured black surround around the sniper circle;
// DrawSniperSideFill covers the remainder out to the screen edges. The camera
// has no such surround and is unaffected by that fill.
constexpr uintptr_t kScopeStretchXSites[] = {
    0x0058E7CE, 0x0058E7F8, // CHud::DrawCrossHairs, 210x210 and 256x192
};

// The camera viewfinder's ring is drawn from that 256x192 rectangle while its
// frame comes from a different one, measured at 1.835 against the ring's 1.333
// on the same frame. The texture holds a round ring, so the oval is entirely
// the rectangle's doing and squaring it is enough. 256.0f is a pooled literal
// used all over the game, so only this one instruction is repointed.
// 256.0f
constexpr uintptr_t kViewfinderWidth = 0x00858FB4;
// its vertical counterpart
constexpr float kViewfinderHeight = 192.0f;

constexpr uintptr_t kViewfinderWidthSites[] = {
    0x0058E7FE, // CHud::DrawCrossHairs, SCREEN_STRETCH_X(256) for the ring
};

// CWeaponEffects::Render draws the rocket launcher lock-on target and clamps it
// to a minimum of 28 against 20 before drawing it. At every distance the marker
// was measured at, those clamps rather than the projection were what decided its
// shape, so correcting them is what makes it round.
//
// The two numbers are not in the same unit. The width is counted in units of
// SCREEN_WIDTH / 640 and the height in units of SCREEN_HEIGHT / 448, so 28
// against 20 is not the 1.4 it looks like: on a 16:9 display it comes out at
// 1.4 * 640 * height / (448 * width) = 1.78 wide against tall. The plugin
// therefore repoints the width minimum at a value carrying the inverse of that
// stretch instead of at a flat 20.
//
// Each of the four draw calls loads the literal twice, once to compare and once
// on the branch that takes it.
// 28.0f
constexpr uintptr_t kLockOnMinWidth = 0x00859AD8;

constexpr uintptr_t kLockOnMinWidthSites[] = {
    0x00742E50, 0x00742E61, 0x00742EDE, 0x00742EEF,
    0x00742FFD, 0x0074300E, 0x007430A2, 0x007430B3,
};

} // namespace game
