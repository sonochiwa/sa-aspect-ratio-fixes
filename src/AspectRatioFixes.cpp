// SA Aspect Ratio Fixes
//
// GTA San Andreas authors its HUD in a 640x448 design space and maps it to the
// framebuffer with two independent factors:
//
//     pixels_x = units_x * RsGlobal.maximumWidth  / 640
//     pixels_y = units_y * RsGlobal.maximumHeight / 448
//
// One HUD unit is therefore only as wide as it is tall when the display aspect
// ratio happens to be 640:448, that is 1.4286:1. On every other resolution
// everything the HUD draws is stretched horizontally by aspect / 1.4286.
//
// For the radar that error lands three times. Its rectangle is 94 units wide
// and 76 units tall, so it is an ellipse even at 4:3 (94/640 * 4/3, divided by
// 76/448, is 1.1544). The circular mask is drawn through the same transform,
// so it follows the ellipse instead of correcting it. And every blip is drawn
// as `x +/- SCREEN_STRETCH_X(8)` against `y +/- SCREEN_STRETCH_Y(8)`, so the
// icons are ovals too.
//
// Rather than solving each of those separately, the plugin repoints the
// horizontal factor used by the radar code so that it equals the vertical one.
// Inside the radar a HUD unit then covers the same number of pixels on both
// axes, and the geometry follows from that: a circle is a square rectangle, the
// corner masks that draw the black ring are padded symmetrically, and a blip is
// as wide as it is tall. Position conversions that only run while the
// full-screen map is open keep the game's own scaling, because they position
// against the map origin instead.
//
// The frame and the icons read that factor through two separate groups of
// instructions, so each has its own variable and its own switch. Blip
// positions are produced by the frame's transform rather than by the icon
// code, which is what lets either group stay stock without the other leaving
// anything out of place.
//
// Nothing here rewrites code. The game reads the scale factors and the radar
// rectangle through 32 bit absolute operands, so the plugin only repoints those
// operands at its own variables and keeps the variables up to date when the
// resolution changes.

#include <windows.h>

#include <cstdio>
#include <cstring>
#include <cstdint>
#include <cmath>

#include "config.h"
#include "game_layout.h"
#include "log.h"
#include "patch.h"

namespace {

constexpr char kVersion[] = "1.0.0";

// Stock values of the pooled literals the plugin repoints. They double as the
// executable check: an executable that does not hold exactly these values at
// these addresses is not the 1.0 US build this plugin was mapped against.
constexpr float kStockStretchX  = 1.0f / 640.0f;
constexpr float kStockRadarLeft = 40.0f;
constexpr float kStockRadarTop  = 104.0f;
constexpr float kStockRadarHigh = 76.0f;
constexpr float kStockRadarWide = 94.0f;

constexpr int32_t kMinScreenSize = 320;
constexpr int32_t kMaxScreenSize = 32768;

constexpr DWORD kStartupPollMs = 50;
constexpr DWORD kStartupTimeoutMs = 120000;
constexpr DWORD kWatcherPollMs = 50;

// Variables the patched instructions read instead of the pooled literals. They
// are plain floats: x86 aligned 32 bit stores are atomic, so the render thread
// can never observe a half written value.
float g_radarLeft = kStockRadarLeft;
float g_radarTop  = kStockRadarTop;
float g_radarHigh = kStockRadarHigh;
float g_radarWide = kStockRadarWide;
float g_radarStretchX = kStockStretchX;
float g_blipStretchX = kStockStretchX;
float g_crosshairStretchX = kStockStretchX;
float g_scopeStretchX = kStockStretchX;

// The lock-on target's minimum width. CWeaponEffects::Render clamps the marker
// to 28 pixels wide against 20 tall, and measurement shows the width it compares
// against carries the HUD's horizontal stretch while the height does not, so the
// minimum has to carry the inverse of that stretch rather than a flat 20.
constexpr float kLockOnMinHeight = 20.0f;
float g_lockOnMinWidth = kLockOnMinHeight;

// The camera viewfinder ring's width, made equal to its height so that the
// round ring in the texture lands round on screen.
float g_viewfinderWidth = game::kViewfinderHeight;

// Probe state. Every site in the selected group is repointed at its own slot,
// so selecting one is a matter of writing floats rather than rewriting code,
// exactly like every other module. Index -1 corrects nothing and index
// g_probeCount corrects the whole group at once.
float g_probeStretch[game::kMaxProbeSites];
const uintptr_t* g_probeSites = nullptr;
size_t g_probeCount = 0;
int g_probeIndex = -1;
char g_probeMessage[96] = {};
float g_squareStretch = kStockStretchX;

// The copied prologue of CHud::DrawCrossHairs followed by a jump back into it.
// Calling this runs the original function whoever hooked its entry.
volatile LONG g_probeNotificationPending = 0;

config::Settings g_settings;

// The render thread reads these while the worker thread reloads the INI.
// Publishing precomputed booleans through InterlockedExchange avoids racing on
// the multi-field Settings object.
volatile LONG g_noCameraCrosshair = 0;
volatile LONG g_hideCameraHud = 0;
volatile LONG g_hideSniperHud = 0;
volatile LONG g_drawSniperFill = 0;
volatile LONG g_fixFov = 0;
volatile LONG g_useScreenAspect = 0;
volatile LONG g_spritePickups = 0;
volatile LONG g_spriteCoronas = 0;
volatile LONG g_spriteCoronaReflections = 0;
volatile LONG g_spriteSunMoon = 0;
volatile LONG g_spritePointLights = 0;
volatile LONG g_spriteBirds = 0;
volatile LONG g_spriteClouds = 0;
volatile LONG g_spriteCheckpoints = 0;
volatile LONG g_spriteWeaponEffects = 0;
volatile LONG g_spriteCameraEffects = 0;
volatile LONG g_spriteTargeting = 0;
volatile LONG g_reloadNotificationPending = 0;

float g_worldSpriteWidthCorrection = 1.0f;

bool g_radarPatched = false;
bool g_crosshairScalePatched = false;
bool g_lockOnPatched = false;
bool g_scopePatched = false;
bool g_viewfinderPatched = false;

// Minimal game types used by CSprite2d::DrawRect. Their field order and
// packing match the 1.0 US classes (CRect is left, bottom, right, top).
struct Rect {
    float left;
    float bottom;
    float right;
    float top;
};

#pragma pack(push, 1)
struct Color {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
    uint8_t alpha;
};
#pragma pack(pop)

using DrawRectFn = int (__cdecl*)(const Rect&, const Color&);
using DrawCrossHairsFn = void (__cdecl*)();

// The copied prologue of CHud::DrawCrossHairs followed by a jump back into it.
// Calling this runs the original function without going through its entry, so
// it stays correct even though the entry now holds our branch.
DrawCrossHairsFn g_drawCrossHairsOriginal = nullptr;
using DrawHudFn = void (__cdecl*)();
using AddMessageJumpFn = void (__cdecl*)(const char*, uint32_t, uint16_t, bool);

struct Vec3 {
    float x;
    float y;
    float z;
};

using CalcScreenCoorsFn = bool (__cdecl*)(const Vec3&, Vec3*, float*, float*,
                                          bool, bool);

constexpr Color kBlack = {0, 0, 0, 255};

bool CalcScreenCoorsFor(const Vec3& input, Vec3* output, float* width,
                        float* height, bool checkMax, bool checkMin,
                        volatile LONG* enabled) {
    const bool visible = reinterpret_cast<CalcScreenCoorsFn>(
        game::kCalcScreenCoors)(input, output, width, height, checkMax,
                                checkMin);
    if (visible && width &&
        InterlockedCompareExchange(enabled, 0, 0) != 0)
        *width *= g_worldSpriteWidthCorrection;
    return visible;
}

#define DEFINE_SPRITE_WRAPPER(name, flag)                                      \
    bool __cdecl name(const Vec3& input, Vec3* output, float* width,           \
                      float* height, bool checkMax, bool checkMin) {            \
        return CalcScreenCoorsFor(input, output, width, height, checkMax,      \
                                  checkMin, &flag);                            \
    }

DEFINE_SPRITE_WRAPPER(CalcPickupSprite, g_spritePickups)
DEFINE_SPRITE_WRAPPER(CalcCoronaSprite, g_spriteCoronas)
DEFINE_SPRITE_WRAPPER(CalcCoronaReflectionSprite, g_spriteCoronaReflections)
DEFINE_SPRITE_WRAPPER(CalcSunMoonSprite, g_spriteSunMoon)
DEFINE_SPRITE_WRAPPER(CalcPointLightSprite, g_spritePointLights)
DEFINE_SPRITE_WRAPPER(CalcBirdSprite, g_spriteBirds)
DEFINE_SPRITE_WRAPPER(CalcCloudSprite, g_spriteClouds)
DEFINE_SPRITE_WRAPPER(CalcCheckpointSprite, g_spriteCheckpoints)
DEFINE_SPRITE_WRAPPER(CalcWeaponEffectSprite, g_spriteWeaponEffects)
DEFINE_SPRITE_WRAPPER(CalcCameraEffectSprite, g_spriteCameraEffects)
DEFINE_SPRITE_WRAPPER(CalcTargetingSprite, g_spriteTargeting)

#undef DEFINE_SPRITE_WRAPPER

void __cdecl SetFovHook(float fov) {
    if (InterlockedCompareExchange(&g_fixFov, 0, 0) != 0) {
        const float aspect = *reinterpret_cast<const float*>(
            game::kAspectRatio);
        constexpr float kPi = 3.14159265358979323846f;
        const float radians = fov * kPi / 180.0f;
        fov = 2.0f * std::atan(std::tan(radians * 0.5f) *
                              (aspect / (4.0f / 3.0f))) * 180.0f / kPi;
    }
    *reinterpret_cast<float*>(game::kFov) = fov;
}

void __cdecl CalculateAspectRatioHook() {
    const float width = static_cast<float>(
        *reinterpret_cast<const int32_t*>(game::kScreenWidth));
    const float height = static_cast<float>(
        *reinterpret_cast<const int32_t*>(game::kScreenHeight));
    if (width > 0.0f && height > 0.0f) {
        *reinterpret_cast<float*>(game::kAspectRatio) =
            InterlockedCompareExchange(&g_useScreenAspect, 0, 0) != 0
                ? width / height
                : 4.0f / 3.0f;
    }
}

void DrawRect(const Rect& rect) {
    reinterpret_cast<DrawRectFn>(game::kDrawRect)(rect, kBlack);
}

int16_t GetCameraMode() {
    const auto* camera = reinterpret_cast<const uint8_t*>(game::kCamera);
    const uint8_t active = camera[game::kCameraActiveIndexOffset];
    if (active >= 3)
        return -1;

    int16_t mode = 0;
    std::memcpy(&mode,
                camera + game::kCameraArrayOffset +
                    active * game::kCameraSize + game::kCameraModeOffset,
                sizeof(mode));
    return mode;
}

bool IsSniperCamera() {
    return GetCameraMode() == game::kCameraModeSniper;
}

// The corrected sniper texture occupies center +/- SCREEN_STRETCH_X(210).
// Vanilla's outer filler still ends at the old, wider bounds. Extending black
// from the screen edges to the corrected bounds closes those two gaps. One
// pixel of overlap prevents fractional coordinates/MSAA from opening a seam.
void DrawSniperSideFill() {
    if (!IsSniperCamera())
        return;

    const float width = static_cast<float>(
        *reinterpret_cast<const int32_t*>(game::kScreenWidth));
    const float height = static_cast<float>(
        *reinterpret_cast<const int32_t*>(game::kScreenHeight));
    // The fill has to follow the scope's own factor, not the reticle's. They
    // are equal while both corrections are on, but roundScope stands alone, so
    // reading the reticle's here would size the fill for a scope that is not
    // on screen.
    const float halfScope = 210.0f * width * g_scopeStretchX;
    const float center = width * 0.5f;
    constexpr float kOutside = 5.0f;
    constexpr float kOverlap = 1.0f;

    DrawRect({-kOutside, -kOutside, center - halfScope + kOverlap,
              height + kOutside});
    DrawRect({center + halfScope - kOverlap, -kOutside, width + kOutside,
              height + kOutside});
}

void DrawCrossHairsHook() {
    if (GetCameraMode() == game::kCameraModeCamera &&
        InterlockedCompareExchange(&g_noCameraCrosshair, 0, 0) != 0)
        return;

    g_drawCrossHairsOriginal();

    if (InterlockedCompareExchange(&g_drawSniperFill, 0, 0) != 0)
        DrawSniperSideFill();
}

void DrawOriginalHud() {
    // This reproduces the seven-byte instruction and branch replaced at the
    // CHud::Draw entry. The body starts after both, so it does not recurse into
    // DrawHudHook and its eventual RET returns normally to this helper.
    if (*reinterpret_cast<const uint8_t*>(game::kHudDisabled) == 1)
        return;

    reinterpret_cast<DrawHudFn>(game::kDrawHudBody)();
}

void ShowPendingReloadNotification() {
    if (InterlockedExchange(&g_reloadNotificationPending, 0) == 0)
        return;

    reinterpret_cast<AddMessageJumpFn>(game::kAddMessageJump)(
        "~w~Aspect Ratio Fixes: configuration reloaded", 2500, 0, false);
}

// The message buffer is written by the worker on a key press and read by the
// game thread on the next frame. AddMessageJump copies the string, and a torn
// read could only garble one diagnostic message, so the interlocked flag is
// the only synchronisation this needs.
void ShowPendingProbeNotification() {
    if (InterlockedExchange(&g_probeNotificationPending, 0) == 0)
        return;

    reinterpret_cast<AddMessageJumpFn>(game::kAddMessageJump)(
        g_probeMessage, 3000, 0, false);
}

void DrawHudHook() {
    // The reload hotkey is polled by the worker. GTA message queues are owned
    // by the game thread, so the worker only publishes this one-shot request.
    ShowPendingReloadNotification();
    ShowPendingProbeNotification();

    // Preserve the game's clean camera capture frame.
    if (*reinterpret_cast<const bool*>(game::kTakePhoto)) {
        DrawOriginalHud();
        return;
    }

    const int16_t mode = GetCameraMode();
    const bool hideHud =
        (mode == game::kCameraModeCamera &&
         InterlockedCompareExchange(&g_hideCameraHud, 0, 0) != 0) ||
        (mode == game::kCameraModeSniper &&
         InterlockedCompareExchange(&g_hideSniperHud, 0, 0) != 0);

    if (hideHud) {
        DrawCrossHairsHook();
        return;
    }

    DrawOriginalHud();
}

// Rendered at the same tail point used by Widescreen Fix's HideAABug=2.
// Coordinates deliberately extend beyond the surface: after rasterisation
// they cover exactly the four one-pixel edge samples affected by MSAA.
void HideAABugHook() {
    const float width = static_cast<float>(
        *reinterpret_cast<const int32_t*>(game::kScreenWidth));
    const float height = static_cast<float>(
        *reinterpret_cast<const int32_t*>(game::kScreenHeight));

    DrawRect({0.0f, -5.0f, width, 0.5f});
    DrawRect({-5.0f, -1.0f, 0.5f, height});
    DrawRect({0.0f, height - 1.5f, width, height + 5.0f});
    DrawRect({width - 1.0f, 0.0f, width + 5.0f, height + 5.0f});
}

struct Resolution {
    int32_t width = 0;
    int32_t height = 0;

    bool operator==(const Resolution& other) const {
        return width == other.width && height == other.height;
    }
};

bool GetResolution(Resolution& resolution) {
    Resolution current;
    if (!patch::ReadInt32(game::kScreenWidth, current.width))
        return false;
    if (!patch::ReadInt32(game::kScreenHeight, current.height))
        return false;

    if (current.width < kMinScreenSize || current.width > kMaxScreenSize)
        return false;
    if (current.height < kMinScreenSize || current.height > kMaxScreenSize)
        return false;

    resolution = current;
    return true;
}

bool HoldsStockValue(uintptr_t address, float expected) {
    float value = 0.0f;
    return patch::ReadFloat(address, value) && value == expected;
}

bool IsSupportedExecutable() {
    return HoldsStockValue(game::kStretchX, kStockStretchX) &&
           HoldsStockValue(game::kRadarLeft, kStockRadarLeft) &&
           HoldsStockValue(game::kRadarTop, kStockRadarTop) &&
           HoldsStockValue(game::kRadarHigh, kStockRadarHigh) &&
           HoldsStockValue(game::kRadarWide, kStockRadarWide);
}

// Several options are only meaningful in combination with another one, and a
// combination that cancels itself out is indistinguishable from a patch that
// failed to apply: the game simply looks unchanged. Each of these is therefore
// named in the log rather than left for the user to work out from the source.
void LogSettingConflicts() {
    if (g_settings.fixFov && !g_settings.useScreenAspect) {
        logging::Write("  note: fixFov has no effect while useScreenAspect=0. "
                       "The conversion scales by aspect / (4/3), and "
                       "useScreenAspect=0 holds that aspect at 4:3, so it "
                       "returns the angle unchanged");
    }

    if (!g_settings.roundRadar &&
        (g_settings.radarDiameter != kStockRadarHigh ||
         g_settings.radarMarginLeft != kStockRadarLeft ||
         g_settings.radarMarginBottom != kStockRadarTop - kStockRadarHigh)) {
        logging::Write("  note: diameter and margins are ignored while "
                       "roundRadar=0. They are HUD units of screen height, "
                       "which only describe the radar once its axes share one "
                       "scale");
    }

}

// Recomputes every plugin owned value for the current resolution. This runs on
// every resolution change, so the geometry is correct in windowed mode, after a
// video settings change and on any display the game can open.
void UpdateGeometry(const Resolution& resolution) {
    const auto screenWidth = static_cast<float>(resolution.width);
    const auto screenHeight = static_cast<float>(resolution.height);

    // Pixels covered by one HUD unit on each axis, as the game computes them.
    const float pixelsPerUnitX = screenWidth / game::kDesignWidth;
    const float pixelsPerUnitY = screenHeight / game::kDesignHeight;

    const float squareStretch =
        screenHeight / (game::kDesignHeight * screenWidth);
    g_squareStretch = squareStretch;
    g_worldSpriteWidthCorrection = screenHeight / screenWidth;

    if (g_settings.useScreenAspect)
        *reinterpret_cast<float*>(game::kAspectRatio) = screenWidth / screenHeight;

    // The frame and the blip icons are corrected independently. Blip positions
    // come from the frame's transform, so a corrected frame carries them even
    // while the icons themselves keep the game's own proportions, and rounded
    // icons sit correctly inside a vanilla ellipse.
    //
    // Patched operands keep pointing at our variables after a hot reload.
    // Restoring their stock values disables a group without rewriting
    // executable code from the polling thread.
    if (g_settings.roundRadar) {
        // SCREEN_STRETCH_X(a) evaluates to a * screenWidth * factor. Making
        // that a * screenHeight / 448 gives the radar one scale on both axes.
        g_radarStretchX = squareStretch;
        // The diameter and the margins are HUD units of screen height, which
        // only describe the radar once its two axes share one scale. They stay
        // stock while the frame does.
        g_radarWide = g_settings.radarDiameter;
        g_radarHigh = g_settings.radarDiameter;
        g_radarLeft = g_settings.radarMarginLeft;
        g_radarTop =
            g_settings.radarMarginBottom + g_settings.radarDiameter;
    } else {
        g_radarStretchX = kStockStretchX;
        g_radarWide = kStockRadarWide;
        g_radarHigh = kStockRadarHigh;
        g_radarLeft = kStockRadarLeft;
        g_radarTop = kStockRadarTop;
    }

    g_blipStretchX = g_settings.roundBlips ? squareStretch : kStockStretchX;

    g_crosshairStretchX =
        g_settings.roundCrosshair ? squareStretch : kStockStretchX;

    // The scope replaces the reticle rather than sharing the screen with it,
    // so the two are corrected independently.
    const bool correctScope = g_settings.roundScope;
    g_scopeStretchX = correctScope ? squareStretch : kStockStretchX;
    g_viewfinderWidth = correctScope ? game::kViewfinderHeight : 256.0f;

    // One unit of the lock-on marker's width covers screenWidth / 640 pixels
    // while one unit of its height covers screenHeight / 448, so the minimum
    // width has to be divided by the ratio of the two to clamp both axes to the
    // same number of pixels.
    g_lockOnMinWidth = g_settings.roundCrosshair
        ? kLockOnMinHeight * (game::kDesignWidth * screenHeight) /
              (game::kDesignHeight * screenWidth)
        : 28.0f;

    const float unitPixels = screenHeight / game::kDesignHeight;

    logging::Write("resolution %dx%d, aspect %.4f",
                   resolution.width, resolution.height,
                   static_cast<double>(screenWidth / screenHeight));
    logging::Write("  one HUD unit: %.4f px wide, %.4f px tall (game default)",
                   static_cast<double>(pixelsPerUnitX),
                   static_cast<double>(pixelsPerUnitY));
    logging::Write("  inside the radar: %.4f px on both axes",
                   static_cast<double>(unitPixels));
    // Reported from the live variables rather than from the settings, so a
    // group left at its stock factor is logged as what the game will actually
    // draw instead of as what the INI asked for.
    const float radarPixelsPerUnitX = screenWidth * g_radarStretchX;
    const float blipPixelsPerUnitX = screenWidth * g_blipStretchX;

    logging::Write("  radar: %.1f x %.1f px, %.1f px from the left, "
                   "%.1f px from the bottom",
                   static_cast<double>(g_radarWide * radarPixelsPerUnitX),
                   static_cast<double>(g_radarHigh * pixelsPerUnitY),
                   static_cast<double>(g_radarLeft * radarPixelsPerUnitX),
                   static_cast<double>((g_radarTop - g_radarHigh) *
                                       pixelsPerUnitY));
    logging::Write("  blip icon: %.1f x %.1f px (game default %.1f x %.1f)",
                   static_cast<double>(16.0f * blipPixelsPerUnitX),
                   static_cast<double>(16.0f * pixelsPerUnitY),
                   static_cast<double>(16.0f * pixelsPerUnitX),
                   static_cast<double>(16.0f * pixelsPerUnitY));
    logging::Write("  crosshair scale: %.4f px per unit (was %.4f)",
                   static_cast<double>(screenWidth * g_crosshairStretchX),
                   static_cast<double>(pixelsPerUnitX));
    logging::Write("  lock-on minimum width: %.4f units against %.1f tall",
                   static_cast<double>(g_lockOnMinWidth),
                   static_cast<double>(kLockOnMinHeight));

    LogSettingConflicts();
}

// The probe is diagnostic and is off unless the INI asks for it. It repoints a
// whole candidate group, then corrects one site at a time so the element that
// moves can be identified before any of it becomes a module. Nothing is
// rewritten while stepping: the selection only changes which slot holds the
// corrected factor.
void ApplyProbe() {
    if (!g_settings.probeEnabled)
        return;

    if (g_settings.probeGroup == 1) {
        g_probeSites = game::kProbeGroupB;
        g_probeCount = sizeof(game::kProbeGroupB) / sizeof(uintptr_t);
    } else {
        g_probeSites = game::kProbeGroupA;
        g_probeCount = sizeof(game::kProbeGroupA) / sizeof(uintptr_t);
    }

    if (g_probeCount > game::kMaxProbeSites) {
        logging::Write("probe: group %d has more sites than slots",
                       g_settings.probeGroup);
        g_probeCount = 0;
        return;
    }

    for (size_t i = 0; i < g_probeCount; ++i)
        g_probeStretch[i] = kStockStretchX;

    // Verified as a whole first, so a group that does not match the expected
    // encoding leaves the executable untouched rather than half repointed.
    if (!patch::VerifyOperands(g_probeSites, g_probeCount, game::kStretchX)) {
        logging::Write("probe: group %d has unexpected bytes, not applied",
                       g_settings.probeGroup);
        g_probeCount = 0;
        return;
    }

    for (size_t i = 0; i < g_probeCount; ++i) {
        if (!patch::RepointOperands(g_probeSites + i, 1, game::kStretchX,
                                    &g_probeStretch[i])) {
            logging::Write("probe: site %u failed to repoint",
                           static_cast<unsigned>(i));
            g_probeCount = 0;
            return;
        }
    }

    logging::Write("probe                  patched group %d (%u sites), "
                   "step with the probe hotkey",
                   g_settings.probeGroup,
                   static_cast<unsigned>(g_probeCount));
    for (size_t i = 0; i < g_probeCount; ++i)
        logging::Write("  probe %2u  0x%08X", static_cast<unsigned>(i + 1),
                       static_cast<unsigned>(g_probeSites[i]));
}

// Publishes the current selection into the slots and describes it both in the
// log and on screen, so stepping does not require leaving the game.
void UpdateProbeSelection() {
    if (g_probeCount == 0)
        return;

    for (size_t i = 0; i < g_probeCount; ++i) {
        const bool corrected =
            g_probeIndex == static_cast<int>(g_probeCount) ||
            g_probeIndex == static_cast<int>(i);
        g_probeStretch[i] = corrected ? g_squareStretch : kStockStretchX;
    }

    if (g_probeIndex < 0) {
        std::snprintf(g_probeMessage, sizeof(g_probeMessage),
                      "~w~Probe: none, group %d", g_settings.probeGroup);
        logging::Write("probe: none");
    } else if (g_probeIndex == static_cast<int>(g_probeCount)) {
        std::snprintf(g_probeMessage, sizeof(g_probeMessage),
                      "~w~Probe: all %u sites, group %d",
                      static_cast<unsigned>(g_probeCount),
                      g_settings.probeGroup);
        logging::Write("probe: all %u sites",
                       static_cast<unsigned>(g_probeCount));
    } else {
        std::snprintf(g_probeMessage, sizeof(g_probeMessage),
                      "~w~Probe %u/%u  0x%08X",
                      static_cast<unsigned>(g_probeIndex + 1),
                      static_cast<unsigned>(g_probeCount),
                      static_cast<unsigned>(g_probeSites[g_probeIndex]));
        logging::Write("probe: %u/%u at 0x%08X",
                       static_cast<unsigned>(g_probeIndex + 1),
                       static_cast<unsigned>(g_probeCount),
                       static_cast<unsigned>(g_probeSites[g_probeIndex]));
    }

    InterlockedExchange(&g_probeNotificationPending, 1);
}

void RestoreRadarMask() {
    if (!patch::IsReadable(game::kDrawRadarMask,
                           sizeof(game::kDrawRadarMaskPrologue))) {
        logging::Write("radar mask: CRadar::DrawRadarMask is not readable");
        return;
    }

    const auto* code = reinterpret_cast<const uint8_t*>(game::kDrawRadarMask);
    bool intact = true;
    for (size_t i = 0; i < sizeof(game::kDrawRadarMaskPrologue); ++i) {
        if (code[i] != game::kDrawRadarMaskPrologue[i]) {
            intact = false;
            break;
        }
    }

    if (intact) {
        logging::Write("radar mask: already intact");
        return;
    }

    const bool restored =
        patch::WriteMemory(game::kDrawRadarMask, game::kDrawRadarMaskPrologue,
                           sizeof(game::kDrawRadarMaskPrologue));
    logging::Write("radar mask: %s",
                   restored ? "restored the circular mask"
                            : "failed to restore the circular mask");
}

// Another modification can repoint the same instructions after this plugin has
// already done so, which would undo a group without leaving any trace in the
// log. Every group that applies is therefore remembered and re-read by the
// watcher, so a group that stops pointing at us is reported once.
struct PatchedGroup {
    const char* name;
    const uintptr_t* sites;
    size_t count;
    uintptr_t target;
    bool reported;
};

constexpr size_t kMaxGuards = 16;
PatchedGroup g_guards[kMaxGuards] = {};
size_t g_guardCount = 0;

template <size_t N>
bool ApplyGroup(const char* name, const uintptr_t (&sites)[N],
                uintptr_t expected, const void* target) {
    const bool applied = patch::RepointOperands(sites, expected, target);
    logging::Write("%-22s %s (%u sites)", name,
                   applied ? "patched" : "SKIPPED, unexpected bytes",
                   static_cast<unsigned>(N));

    if (applied && g_guardCount < kMaxGuards) {
        PatchedGroup& guard = g_guards[g_guardCount++];
        guard.name = name;
        guard.sites = sites;
        guard.count = N;
        guard.target = reinterpret_cast<uintptr_t>(target);
        guard.reported = false;
    }
    return applied;
}

// The operand of an x87 instruction with an absolute address starts two bytes
// into it, which is what RepointOperands writes.
constexpr size_t kOperandOffset = 2;

void CheckGuards() {
    for (size_t g = 0; g < g_guardCount; ++g) {
        PatchedGroup& guard = g_guards[g];
        if (guard.reported)
            continue;

        for (size_t i = 0; i < guard.count; ++i) {
            int32_t operand = 0;
            if (!patch::ReadInt32(guard.sites[i] + kOperandOffset, operand))
                continue;

            const auto current = static_cast<uintptr_t>(
                static_cast<uint32_t>(operand));
            if (current == guard.target)
                continue;

            logging::Write("%-22s site %08X now points at %08X instead of %08X, "
                           "another modification has overwritten it",
                           guard.name, static_cast<unsigned>(guard.sites[i]),
                           static_cast<unsigned>(current),
                           static_cast<unsigned>(guard.target));
            guard.reported = true;
            break;
        }
    }
}

bool WriteRelativeBranch(uintptr_t site, uint8_t opcode, const void* target) {
    const intptr_t displacement =
        reinterpret_cast<intptr_t>(target) - static_cast<intptr_t>(site + 5);
    if (displacement < INT32_MIN || displacement > INT32_MAX)
        return false;

    uint8_t branch[5] = {opcode, 0, 0, 0, 0};
    const auto relative = static_cast<int32_t>(displacement);
    std::memcpy(branch + 1, &relative, sizeof(relative));
    return patch::WriteMemory(site, branch, sizeof(branch));
}

bool RelativeCallTargets(uintptr_t site, uintptr_t expected) {
    if (!patch::IsReadable(site, 5))
        return false;

    const auto* code = reinterpret_cast<const uint8_t*>(site);
    if (code[0] != 0xE8)
        return false;

    int32_t relative = 0;
    std::memcpy(&relative, code + 1, sizeof(relative));
    return site + 5 + relative == expected;
}

template <size_t N>
void ApplySpriteCallGroup(const char* name, const uintptr_t (&sites)[N],
                          uintptr_t expected, const void* wrapper) {
    for (size_t i = 0; i < N; ++i) {
        if (!RelativeCallTargets(sites[i], expected)) {
            logging::Write("%-22s SKIPPED, call %08X was changed",
                           name, static_cast<unsigned>(sites[i]));
            return;
        }
    }

    bool applied = true;
    for (size_t i = 0; i < N; ++i)
        applied &= WriteRelativeBranch(sites[i], 0xE8, wrapper);
    logging::Write("%-22s %s (%u calls)", name,
                   applied ? "patched" : "FAILED",
                   static_cast<unsigned>(N));
}

void ApplyWorldSprites() {
    if (!patch::IsReadable(game::kCalcScreenCoors, 1) ||
        *reinterpret_cast<const uint8_t*>(game::kCalcScreenCoors) == 0xE9) {
        logging::Write("world sprites          SKIPPED, projection is hooked");
        return;
    }

    ApplySpriteCallGroup("sprite pickups", game::kSpritePickupSites,
                         game::kCalcScreenCoors, CalcPickupSprite);
    ApplySpriteCallGroup("sprite coronas", game::kSpriteCoronaSites,
                         game::kCalcScreenCoors, CalcCoronaSprite);
    ApplySpriteCallGroup("sprite reflections",
                         game::kSpriteCoronaReflectionSites,
                         game::kCalcScreenCoors, CalcCoronaReflectionSprite);
    ApplySpriteCallGroup("sprite sun/moon", game::kSpriteSunMoonSites,
                         game::kCalcScreenCoors, CalcSunMoonSprite);
    ApplySpriteCallGroup("sprite point lights", game::kSpritePointLightSites,
                         game::kCalcScreenCoors, CalcPointLightSprite);
    ApplySpriteCallGroup("sprite birds", game::kSpriteBirdSites,
                         game::kCalcScreenCoors, CalcBirdSprite);
    ApplySpriteCallGroup("sprite clouds", game::kSpriteCloudSites,
                         game::kCalcScreenCoors, CalcCloudSprite);
    ApplySpriteCallGroup("sprite checkpoints", game::kSpriteCheckpointSites,
                         game::kCalcScreenCoors, CalcCheckpointSprite);
    ApplySpriteCallGroup("sprite weapon FX", game::kSpriteWeaponEffectSites,
                         game::kCalcScreenCoors, CalcWeaponEffectSprite);
    ApplySpriteCallGroup("sprite camera FX", game::kSpriteCameraEffectSites,
                         game::kCalcScreenCoors, CalcCameraEffectSprite);
    ApplySpriteCallGroup("sprite targeting", game::kSpriteTargetingSites,
                         game::kCalcScreenCoors, CalcTargetingSprite);
}

void ApplyFovFix() {
    if (patch::IsReadable(game::kSetFov, 1) &&
        *reinterpret_cast<const uint8_t*>(game::kSetFov) != 0xE9) {
        ApplySpriteCallGroup("widescreen FOV", game::kSetFovCallSites,
                             game::kSetFov,
                             reinterpret_cast<const void*>(SetFovHook));
    } else {
        logging::Write("widescreen FOV         SKIPPED, function is hooked");
    }

    if (patch::IsReadable(game::kCalculateAspectRatio, 1) &&
        *reinterpret_cast<const uint8_t*>(game::kCalculateAspectRatio) != 0xE9) {
        ApplySpriteCallGroup(
            "screen aspect", game::kCalculateAspectCallSites,
            game::kCalculateAspectRatio,
            reinterpret_cast<const void*>(CalculateAspectRatioHook));
    } else {
        logging::Write("screen aspect          SKIPPED, function is hooked");
    }
}


void ApplyAABugFix() {
    if (!patch::IsReadable(game::kRender2dStuffReturn, 5)) {
        logging::Write("AA edge frame          SKIPPED, unexpected bytes");
        return;
    }

    // Retail builds use either a RET or a tail JMP here. Both leave the stack
    // ready for HideAABugHook to return directly to FrontendIdle's caller.
    const uint8_t original =
        *reinterpret_cast<const uint8_t*>(game::kRender2dStuffReturn);
    if (original != 0xC3 && original != 0xE9) {
        logging::Write("AA edge frame          SKIPPED, unexpected opcode %02X",
                       static_cast<unsigned>(original));
        return;
    }

    const bool applied = WriteRelativeBranch(game::kRender2dStuffReturn, 0xE9,
                                              reinterpret_cast<const void*>(
                                                  HideAABugHook));
    logging::Write("AA edge frame          %s (four sides)",
                   applied ? "patched" : "FAILED");
}

// Hooks CHud::DrawCrossHairs at its own entry. The call to it inside
// CHud::Draw is deliberately not used: a HUD replacement that redirects
// CHud::Draw at its prologue never runs the original body, so a hook there is
// written but never reached, while the replacement still calls DrawCrossHairs
// itself. That is exactly the configuration that left the sniper fill missing.
void ApplyCrosshairDrawHook() {
    constexpr size_t kStolen = sizeof(game::kDrawCrossHairsPrologue);

    if (!patch::IsReadable(game::kDrawCrossHairs, kStolen) ||
        std::memcmp(reinterpret_cast<const void*>(game::kDrawCrossHairs),
                    game::kDrawCrossHairsPrologue, kStolen) != 0) {
        logging::Write("crosshair draw hook    SKIPPED, unexpected prologue");
        return;
    }

    // The trampoline holds the stolen instructions and a jump past them. Both
    // are position independent, so copying them is enough; nothing needs to be
    // rewritten for the new address.
    auto* trampoline = static_cast<uint8_t*>(
        VirtualAlloc(nullptr, kStolen + 5, MEM_COMMIT | MEM_RESERVE,
                     PAGE_EXECUTE_READWRITE));
    if (!trampoline) {
        logging::Write("crosshair draw hook    FAILED, no executable memory");
        return;
    }

    std::memcpy(trampoline, game::kDrawCrossHairsPrologue, kStolen);
    trampoline[kStolen] = 0xE9;
    const auto backwards = static_cast<int32_t>(
        static_cast<intptr_t>(game::kDrawCrossHairsBody) -
        reinterpret_cast<intptr_t>(trampoline + kStolen + 5));
    std::memcpy(trampoline + kStolen + 1, &backwards, sizeof(backwards));

    const intptr_t displacement =
        reinterpret_cast<intptr_t>(DrawCrossHairsHook) -
        static_cast<intptr_t>(game::kDrawCrossHairs + 5);
    if (displacement < INT32_MIN || displacement > INT32_MAX) {
        logging::Write("crosshair draw hook    FAILED, target out of range");
        VirtualFree(trampoline, 0, MEM_RELEASE);
        return;
    }

    // The trampoline has to be usable before the entry starts routing into the
    // hook, otherwise a frame drawn in between calls through a null pointer.
    g_drawCrossHairsOriginal = reinterpret_cast<DrawCrossHairsFn>(trampoline);

    uint8_t branch[kStolen];
    std::memset(branch, 0x90, sizeof(branch));
    branch[0] = 0xE9;
    const auto relative = static_cast<int32_t>(displacement);
    std::memcpy(branch + 1, &relative, sizeof(relative));

    const bool applied =
        patch::WriteMemory(game::kDrawCrossHairs, branch, sizeof(branch));
    if (!applied)
        g_drawCrossHairsOriginal = nullptr;
    logging::Write("crosshair draw hook    %s (CHud::DrawCrossHairs entry)",
                   applied ? "patched" : "FAILED");
}
void ApplyHudVisibilityHook() {
    if (!patch::IsReadable(game::kDrawHud,
                           sizeof(game::kDrawHudPrologue)) ||
        std::memcmp(reinterpret_cast<const void*>(game::kDrawHud),
                    game::kDrawHudPrologue,
                    sizeof(game::kDrawHudPrologue)) != 0) {
        logging::Write("aim-mode HUD hook      SKIPPED, unexpected prologue");
        return;
    }

    const intptr_t displacement =
        reinterpret_cast<intptr_t>(DrawHudHook) -
        static_cast<intptr_t>(game::kDrawHud + 5);
    if (displacement < INT32_MIN || displacement > INT32_MAX) {
        logging::Write("aim-mode HUD hook      FAILED, target out of range");
        return;
    }

    uint8_t branch[sizeof(game::kDrawHudPrologue)] = {
        0xE9, 0, 0, 0, 0, 0x90, 0x90,
    };
    const auto relative = static_cast<int32_t>(displacement);
    std::memcpy(branch + 1, &relative, sizeof(relative));
    const bool applied = patch::WriteMemory(game::kDrawHud, branch,
                                             sizeof(branch));
    logging::Write("aim-mode HUD hook      %s (CHud::Draw entry)",
                   applied ? "patched" : "FAILED");
}

// The radar geometry only adds up when the horizontal scale, the rectangle and
// the elements pinned to it are all rewritten together, so every site is
// verified before the first one is written.
bool RadarSitesVerify() {
    return patch::VerifyOperands(game::kRadarStretchXSites, game::kStretchX) &&
           patch::VerifyOperands(game::kBlipStretchXSites, game::kStretchX) &&
           patch::VerifyOperands(game::kRadarLeftSites, game::kRadarLeft) &&
           patch::VerifyOperands(game::kRadarTopSites, game::kRadarTop) &&
           patch::VerifyOperands(game::kRadarHighSites, game::kRadarHigh) &&
           patch::VerifyOperands(game::kRadarWideSites, game::kRadarWide) &&
           patch::VerifyOperands(game::kDependentTopSites, game::kRadarTop) &&
           patch::VerifyOperands(game::kDependentHighSites, game::kRadarHigh);
}

void ApplyRadar() {
    if (g_radarPatched)
        return;

    if (!RadarSitesVerify()) {
        logging::Write("radar: unexpected bytes at one or more sites, "
                       "the radar was left untouched");
        return;
    }

    bool applied = true;
    applied &= ApplyGroup("radar scale", game::kRadarStretchXSites,
                          game::kStretchX, &g_radarStretchX);
    applied &= ApplyGroup("blip scale", game::kBlipStretchXSites,
                          game::kStretchX, &g_blipStretchX);
    applied &= ApplyGroup("radar left", game::kRadarLeftSites,
                          game::kRadarLeft, &g_radarLeft);
    applied &= ApplyGroup("radar top", game::kRadarTopSites,
                          game::kRadarTop, &g_radarTop);
    applied &= ApplyGroup("radar height", game::kRadarHighSites,
                          game::kRadarHigh, &g_radarHigh);
    applied &= ApplyGroup("radar width", game::kRadarWideSites,
                          game::kRadarWide, &g_radarWide);
    applied &= ApplyGroup("dependent top", game::kDependentTopSites,
                          game::kRadarTop, &g_radarTop);
    applied &= ApplyGroup("dependent height", game::kDependentHighSites,
                          game::kRadarHigh, &g_radarHigh);

    g_radarPatched = applied;
    if (applied && g_settings.roundRadar)
        RestoreRadarMask();
}

void ApplyCrosshair() {
    if (!g_crosshairScalePatched) {
        g_crosshairScalePatched =
            ApplyGroup("crosshair", game::kCrosshairStretchXSites,
                       game::kStretchX, &g_crosshairStretchX);
    }

    if (!g_lockOnPatched) {
        g_lockOnPatched =
            ApplyGroup("lock-on minimum", game::kLockOnMinWidthSites,
                       game::kLockOnMinWidth, &g_lockOnMinWidth);
    }

    if (!g_scopePatched) {
        g_scopePatched =
            ApplyGroup("scope", game::kScopeStretchXSites,
                       game::kStretchX, &g_scopeStretchX);
    }
    if (!g_viewfinderPatched) {
        g_viewfinderPatched =
            ApplyGroup("viewfinder ring", game::kViewfinderWidthSites,
                       game::kViewfinderWidth, &g_viewfinderWidth);
    }
}

void PublishRenderSettings() {
    const bool correctScope =
        g_settings.roundScope;

    InterlockedExchange(&g_noCameraCrosshair,
                        g_settings.noCameraCrosshair ? 1 : 0);
    InterlockedExchange(&g_hideCameraHud,
                        g_settings.hideCameraHud ? 1 : 0);
    InterlockedExchange(&g_hideSniperHud,
                        g_settings.hideSniperHud ? 1 : 0);
    InterlockedExchange(&g_drawSniperFill, correctScope ? 1 : 0);
    InterlockedExchange(&g_fixFov, g_settings.fixFov ? 1 : 0);
    InterlockedExchange(&g_useScreenAspect,
                        g_settings.useScreenAspect ? 1 : 0);
    InterlockedExchange(&g_spritePickups, g_settings.spritePickups ? 1 : 0);
    InterlockedExchange(&g_spriteCoronas, g_settings.spriteCoronas ? 1 : 0);
    InterlockedExchange(&g_spriteCoronaReflections,
                        g_settings.spriteCoronaReflections ? 1 : 0);
    InterlockedExchange(&g_spriteSunMoon, g_settings.spriteSunMoon ? 1 : 0);
    InterlockedExchange(&g_spritePointLights,
                        g_settings.spritePointLights ? 1 : 0);
    InterlockedExchange(&g_spriteBirds, g_settings.spriteBirds ? 1 : 0);
    InterlockedExchange(&g_spriteClouds, g_settings.spriteClouds ? 1 : 0);
    InterlockedExchange(&g_spriteCheckpoints,
                        g_settings.spriteCheckpoints ? 1 : 0);
    InterlockedExchange(&g_spriteWeaponEffects,
                        g_settings.spriteWeaponEffects ? 1 : 0);
    InterlockedExchange(&g_spriteCameraEffects,
                        g_settings.spriteCameraEffects ? 1 : 0);
    InterlockedExchange(&g_spriteTargeting,
                        g_settings.spriteTargetingMeasurements ? 1 : 0);
}

void ApplyReloadedSettings(HMODULE module, const char* path,
                           const Resolution& resolution) {
    const config::Settings previous = g_settings;
    const config::Settings reloaded = config::Load(path);

    if (reloaded.log && !previous.log)
        logging::Enable(module);

    g_settings = reloaded;
    UpdateGeometry(resolution);

    // All operands are attached to plugin-owned variables during startup,
    // including disabled modules. Reloading therefore changes only aligned
    // data values and never rewrites executable instructions mid-frame.
    if (g_settings.roundRadar && !previous.roundRadar)
        RestoreRadarMask();

    PublishRenderSettings();
    UpdateProbeSelection();
    logging::Write("configuration reloaded");

    if (g_settings.showReloadMessage)
        InterlockedExchange(&g_reloadNotificationPending, 1);

    if (!reloaded.log && previous.log)
        logging::Disable();
}

bool IsVirtualKeyDown(int key) {
    return key != 0 && (GetAsyncKeyState(key) & 0x8000) != 0;
}

// Steps the probe: none, then each site in turn, then the whole group, then
// none again. Only plugin owned floats are written from here.
void ServiceProbeHotkey(bool& wasDown) {
    if (!g_settings.probeEnabled || g_probeCount == 0 ||
        g_settings.probeHotkey.key == 0) {
        wasDown = false;
        return;
    }

    const bool down =
        IsVirtualKeyDown(g_settings.probeHotkey.key) &&
        (g_settings.probeHotkey.modifier == 0 ||
         IsVirtualKeyDown(g_settings.probeHotkey.modifier));
    if (down && !wasDown) {
        ++g_probeIndex;
        if (g_probeIndex > static_cast<int>(g_probeCount))
            g_probeIndex = -1;
        UpdateProbeSelection();
    }
    wasDown = down;
}

void ServiceReloadHotkey(HMODULE module, const char* path,
                         const Resolution& resolution, bool& wasDown) {
    if (g_settings.reloadHotkey.key == 0) {
        wasDown = false;
        return;
    }

    const bool down =
        IsVirtualKeyDown(g_settings.reloadHotkey.key) &&
        (g_settings.reloadHotkey.modifier == 0 ||
         IsVirtualKeyDown(g_settings.reloadHotkey.modifier));
    if (down && !wasDown)
        ApplyReloadedSettings(module, path, resolution);
    wasDown = down;
}

DWORD WINAPI PluginThread(LPVOID parameter) {
    const auto module = static_cast<HMODULE>(parameter);

    char path[MAX_PATH] = {};
    if (!config::GetPath(module, path))
        return 0;

    config::CreateDefault(module, path);
    g_settings = config::Load(path);

    if (g_settings.log)
        logging::Enable(module);

    logging::Write("SA Aspect Ratio Fixes v%s", kVersion);

    // The literals live in the image, so this answers before the game has
    // finished starting up.
    if (!IsSupportedExecutable()) {
        logging::Write("unsupported executable, nothing was patched");
        return 0;
    }

    // RsGlobal only reports the framebuffer size once RenderWare is up, so the
    // geometry cannot be computed at load time.
    Resolution resolution;
    for (DWORD waited = 0; waited < kStartupTimeoutMs;
         waited += kStartupPollMs) {
        if (GetResolution(resolution))
            break;
        Sleep(kStartupPollMs);
    }

    if (resolution.width == 0) {
        logging::Write("gave up waiting for RsGlobal to report a resolution");
        return 0;
    }

    UpdateGeometry(resolution);

    // Patch every configurable module once. Disabled modules publish the
    // exact stock constants, allowing Alt+H to enable them without modifying
    // executable instructions while the render thread is active.
    ApplyRadar();
    ApplyCrosshair();
    ApplyProbe();
    UpdateProbeSelection();

    // Install pass-through hooks even when their current options are off so a
    // hot reload can enable those options without rewriting code mid-frame.
    ApplyCrosshairDrawHook();
    ApplyHudVisibilityHook();
    PublishRenderSettings();

    ApplyFovFix();
    ApplyWorldSprites();

    ApplyAABugFix();

    // Keep following the framebuffer size. Only plugin owned variables are
    // written from here, never game code.
    bool hotkeyWasDown = false;
    bool probeKeyWasDown = false;
    for (;;) {
        Sleep(kWatcherPollMs);

        Resolution current;
        if (GetResolution(current) && !(current == resolution)) {
            resolution = current;
            UpdateGeometry(resolution);
            UpdateProbeSelection();
        }

        ServiceReloadHotkey(module, path, resolution, hotkeyWasDown);
        ServiceProbeHotkey(probeKeyWasDown);
        CheckGuards();
    }
}

}  // namespace

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(instance);
        const HANDLE thread =
            CreateThread(nullptr, 0, PluginThread, instance, 0, nullptr);
        if (thread)
            CloseHandle(thread);
    }
    return TRUE;
}
