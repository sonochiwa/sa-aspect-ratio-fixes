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
constexpr uintptr_t kScreenWidth  = 0x00C17044; // RsGlobal.maximumWidth  (int32)
constexpr uintptr_t kScreenHeight = 0x00C17048; // RsGlobal.maximumHeight (int32)

// FrontendIdle reaches its final 2D tail here. Retail layouts use a RET or a
// tail JMP. The widescreen fix uses the same hook to draw over half-covered
// edge samples left by multisampling. A normal function can return straight to
// FrontendIdle's caller because the stack has already been restored here.
constexpr uintptr_t kRender2dStuffReturn = 0x0053E90E;

// CSprite2d::DrawRect(const CRect&, const CRGBA&). Using the game's own 2D
// renderer keeps these rectangles in the same render target and render-state
// path as the HUD (a Direct3D EndScene/Present clear does not reach it).
constexpr uintptr_t kDrawRect = 0x00727B60;

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
constexpr uintptr_t kStretchX   = 0x00859520; // 1.0f / 640.0f
constexpr uintptr_t kRadarLeft  = 0x00858A10; // 40.0f,  radar left edge
constexpr uintptr_t kRadarTop   = 0x00866B70; // 104.0f, radar top edge above the bottom
constexpr uintptr_t kRadarHigh  = 0x00866B74; // 76.0f,  radar height
constexpr uintptr_t kRadarWide  = 0x00866B78; // 94.0f,  radar width

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
constexpr float kDesignWidth  = 640.0f;
constexpr float kDesignHeight = 448.0f;

// Radar geometry. Sites are the addresses of the x87 instructions; the operand
// that gets repointed starts two bytes later.
//
// CRadar::TransformRadarPointToScreenSpace @ 0x583480 maps radar space to the
// screen, CHud::DrawRadar @ 0x58A330 draws the plane ring, the altimeter and
// the four corner masks, CRadar::DrawEntityBlip @ 0x587000 scales blips.

constexpr uintptr_t kRadarLeftSites[] = {
    0x005834D2, // CRadar::TransformRadarPointToScreenSpace
    0x0058A467, // CHud::DrawRadar, plane ring sprite
    0x0058A5E0, // CHud::DrawRadar, altimeter background
    0x0058A6E4, // CHud::DrawRadar, altimeter height line
    0x0058A799, // CHud::DrawRadar, corner mask 1
    0x0058A834, // CHud::DrawRadar, corner mask 2
    0x0058A8E7, // CHud::DrawRadar, corner mask 3
    0x0058A988, // CHud::DrawRadar, corner mask 4
};

constexpr uintptr_t kRadarTopSites[] = {
    0x005834FE, // CRadar::TransformRadarPointToScreenSpace
    0x0058A497, // CHud::DrawRadar, plane ring sprite
    0x0058A60C, // CHud::DrawRadar, altimeter background
    0x0058A71C, // CHud::DrawRadar, altimeter height line
    0x0058A7C5, // CHud::DrawRadar, corner mask 1
    0x0058A866, // CHud::DrawRadar, corner mask 2
    0x0058A911, // CHud::DrawRadar, corner mask 3
    0x0058A9C5, // CHud::DrawRadar, corner mask 4
};

constexpr uintptr_t kRadarHighSites[] = {
    0x005834F4, // CRadar::TransformRadarPointToScreenSpace
    0x0058A47B, // CHud::DrawRadar, plane ring sprite
    0x0058A630, // CHud::DrawRadar, altimeter background
    0x0058A6A9, // CHud::DrawRadar, altimeter height line
    0x0058A70C, // CHud::DrawRadar, altimeter height line
    0x0058A7FF, // CHud::DrawRadar, corner mask 1
    0x0058A8A9, // CHud::DrawRadar, corner mask 2
    0x0058A91F, // CHud::DrawRadar, corner mask 3
    0x0058A9D3, // CHud::DrawRadar, corner mask 4
};

constexpr uintptr_t kRadarWideSites[] = {
    0x005834C0, // CRadar::TransformRadarPointToScreenSpace
    0x00587819, // CRadar::DrawEntityBlip
    0x0058A447, // CHud::DrawRadar, plane ring sprite
    0x0058A7E7, // CHud::DrawRadar, corner mask 1
    0x0058A83E, // CHud::DrawRadar, corner mask 2
    0x0058A941, // CHud::DrawRadar, corner mask 3
    0x0058A99B, // CHud::DrawRadar, corner mask 4
};

// Elements the game positions against the top edge of the radar. They have to
// follow the radar, otherwise resizing it makes them overlap or drift away.

constexpr uintptr_t kDependentTopSites[] = {
    0x0058A1A5, // CHud::DrawTripSkip
    0x0058A2D4, // CHud::DrawTripSkip
    0x0058AE2C, // CHud::DrawAreaName
    0x0058B133, // CHud::DrawVehicleName
};

constexpr uintptr_t kDependentHighSites[] = {
    0x0058AE38, // CHud::DrawAreaName
};

// SCREEN_STRETCH_X sites. Each one was confirmed to be preceded by a
// `fild dword ptr [RsGlobal.maximumWidth]`, so repointing the factor changes
// only the horizontal scale of that function.
//
// Repointing these makes the horizontal scale inside the radar equal to the
// vertical one, which is what turns the radar rectangle into a square pixel
// space: the map circle, the black ring and every blip sprite then share one
// isotropic unit.
//
// The sites are split into two groups because the radar frame and the blip
// icons drawn inside it are independent elements. The frame group covers the
// radar-space to screen-space transform and everything CHud::DrawRadar draws
// around the circle. The blip group covers only the size calculations of the
// sprites drawn on top of it, which are `x +/- SCREEN_STRETCH_X(8)` against
// `y +/- SCREEN_STRETCH_Y(8)` and are therefore ovals while they read the
// stock factor. Blip positions come from the transform in the frame group, so
// either group can be corrected without the other leaving anything misplaced.
//
// Position conversions that only run while the full-screen map is open are
// deliberately absent from both groups. They convert map-space X coordinates
// with the stock SCREEN_STRETCH_X factor so that blips stay attached to the
// map while it is panned. Only the later size calculations in those functions
// are corrected.

constexpr uintptr_t kRadarStretchXSites[] = {
    0x005834BA,                         // CRadar::TransformRadarPointToScreenSpace
    0x0058A441, 0x0058A5D8, 0x0058A6DE, // CHud::DrawRadar
    0x0058A791, 0x0058A82E, 0x0058A8DF,
    0x0058A982,
};

constexpr uintptr_t kBlipStretchXSites[] = {
    0x0058410B, 0x00584190,             // CRadar::ShowRadarTraceWithHeight
    0x00584249, 0x005842E6, 0x0058439C,
    0x00584434,
    0x0058603F,                         // CRadar::DrawRadarSprite
    0x005876D4, 0x0058774B, 0x0058780A, // CRadar::DrawEntityBlip
    0x0058788F, 0x0058792E, 0x00587A1A,
    0x00587AAA,
};

// Probe groups. These are candidates rather than mapped elements: they are the
// largest functions in the CHud range that read SCREEN_STRETCH_X and that no
// module patches yet, taken from the full scan in
// references\stretch-x-sites.md. The weapon icon is the reason they are
// interesting.
//
// Which of their sites are sizes and which are position conversions cannot be
// told apart from the encoding, and correcting a position is what drifted the
// map blips. The probe therefore repoints each site at its own variable and
// corrects one at a time, so a site can be watched in game before any of this
// is turned into a module.

constexpr size_t kMaxProbeSites = 16;

// 0x0058EAF0, in the CHud range below DrawCrossHairs.
constexpr uintptr_t kProbeGroupA[] = {
    0x0058EB3F, 0x0058EC0C, 0x0058EE7E, 0x0058EEF4, 0x0058EF50,
    0x0058EFC5, 0x0058F116, 0x0058F194, 0x0058F55C, 0x0058F5F4,
    0x0058F91C, 0x0058F993, 0x0058F9D0, 0x0058FA5D,
};

// 0x00589650, in the CHud range above DrawRadar.
constexpr uintptr_t kProbeGroupB[] = {
    0x005896D8, 0x00589703, 0x005897C3, 0x0058986D, 0x0058990C,
    0x00589A16, 0x00589B2D, 0x00589C73, 0x00589D61, 0x00589E49,
    0x00589F31, 0x0058A013, 0x0058A090, 0x0058A134,
};

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
constexpr uintptr_t kViewfinderWidth = 0x00858FB4; // 256.0f
constexpr float kViewfinderHeight = 192.0f;        // its vertical counterpart

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
constexpr uintptr_t kLockOnMinWidth = 0x00859AD8; // 28.0f

constexpr uintptr_t kLockOnMinWidthSites[] = {
    0x00742E50, 0x00742E61, 0x00742EDE, 0x00742EEF,
    0x00742FFD, 0x0074300E, 0x007430A2, 0x007430B3,
};

}  // namespace game
