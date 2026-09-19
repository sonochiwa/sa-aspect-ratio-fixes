#include "crosshair.h"

#include "addresses.h"
#include "game.h"
#include "hooking.h"
#include "log.h"
#include "patch.h"
#include "types.h"

#include <windows.h>

#include <cstring>

namespace crosshair {
namespace {

using DrawCrossHairsFn = void (__cdecl*)();

float g_stretchX = kStockStretchX;
float g_scopeStretchX = kStockStretchX;

// The lock-on target's minimum width. CWeaponEffects::Render clamps the
// marker to 28 pixels wide against 20 tall, and measurement shows the width
// it compares against carries the HUD's horizontal stretch while the height
// does not, so the minimum has to carry the inverse of that stretch rather
// than a flat 20.
constexpr float kLockOnMinHeight = 20.0f;
float g_lockOnMinWidth = kLockOnMinHeight;

// The camera viewfinder ring's width, made equal to its height so that the
// round ring in the texture lands round on screen.
float g_viewfinderWidth = game::kViewfinderHeight;

bool g_scalePatched = false;
bool g_lockOnPatched = false;
bool g_scopePatched = false;
bool g_viewfinderPatched = false;

// The render thread reads these while the worker thread reloads the INI.
volatile LONG g_noCameraCrosshair = 0;
volatile LONG g_drawSniperFill = 0;

// The copied prologue of CHud::DrawCrossHairs followed by a jump back into
// it. Calling this runs the original function without going through its
// entry, so it stays correct even though the entry now holds our branch.
DrawCrossHairsFn g_drawCrossHairsOriginal = nullptr;

// The corrected sniper texture occupies center +/- SCREEN_STRETCH_X(210).
// Vanilla's outer filler still ends at the old, wider bounds. Extending
// black from the screen edges to the corrected bounds closes those two gaps.
// One pixel of overlap prevents fractional coordinates/MSAA from opening a
// seam. The outer edges stop exactly at the screen like vanilla's own
// filler, so the MSAA edge line stays the same across the whole width and
// [aaEdgeFrame] alone decides whether it is covered.
void DrawSniperSideFill() {
    if (!game_api::IsSniperCamera())
        return;

    const float width = game_api::ScreenWidth();
    const float height = game_api::ScreenHeight();
    // The fill has to follow the scope's own factor, not the reticle's. They
    // are equal while both corrections are on, but roundScope stands alone,
    // so reading the reticle's here would size the fill for a scope that is
    // not on screen.
    const float halfScope = 210.0f * width * g_scopeStretchX;
    const float center = width * 0.5f;
    constexpr float kOverlap = 1.0f;

    game_api::DrawBlackRect({0.0f, 0.0f, center - halfScope + kOverlap, height});
    game_api::DrawBlackRect({center + halfScope - kOverlap, 0.0f, width, height});
}

} // namespace

void DrawCrossHairsHook() {
    if (game_api::GetCameraMode() == game::kCameraModeCamera &&
        InterlockedCompareExchange(&g_noCameraCrosshair, 0, 0) != 0)
        return;

    g_drawCrossHairsOriginal();

    if (InterlockedCompareExchange(&g_drawSniperFill, 0, 0) != 0)
        DrawSniperSideFill();
}

void Apply() {
    if (!g_scalePatched) {
        g_scalePatched =
            hooking::ApplyGroup("crosshair", game::kCrosshairStretchXSites, game::kStretchX, &g_stretchX);
    }
    if (!g_lockOnPatched) {
        g_lockOnPatched = hooking::ApplyGroup("lock-on minimum", game::kLockOnMinWidthSites, game::kLockOnMinWidth,
                                              &g_lockOnMinWidth);
    }
    if (!g_scopePatched) {
        g_scopePatched = hooking::ApplyGroup("scope", game::kScopeStretchXSites, game::kStretchX, &g_scopeStretchX);
    }
    if (!g_viewfinderPatched) {
        g_viewfinderPatched = hooking::ApplyGroup("viewfinder ring", game::kViewfinderWidthSites,
                                                  game::kViewfinderWidth, &g_viewfinderWidth);
    }
}

// The call to DrawCrossHairs inside CHud::Draw is deliberately not used: a
// HUD replacement that redirects CHud::Draw at its prologue never runs the
// original body, so a hook there is written and then never reached, while
// the replacement still calls DrawCrossHairs itself. That is what left the
// sniper fill missing.
void ApplyDrawHook() {
    constexpr size_t kStolen = sizeof(game::kDrawCrossHairsPrologue);

    if (!hooking::BytesMatch(game::kDrawCrossHairs, game::kDrawCrossHairsPrologue, kStolen)) {
        logging::Write("crosshair draw hook    SKIPPED, unexpected prologue");
        return;
    }

    void* resume = nullptr;
    const bool applied = hooking::InstallTrampolineHook(game::kDrawCrossHairs, game::kDrawCrossHairsPrologue, kStolen,
                                                        game::kDrawCrossHairsBody,
                                                        reinterpret_cast<const void*>(DrawCrossHairsHook), &resume);
    g_drawCrossHairsOriginal = reinterpret_cast<DrawCrossHairsFn>(resume);
    logging::Write("crosshair draw hook    %s (CHud::DrawCrossHairs entry)", applied ? "patched" : "FAILED");
}

void UpdateGeometry(const config::Settings& settings, float screenWidth, float screenHeight, float squareStretch) {
    g_stretchX = settings.roundCrosshair ? squareStretch : kStockStretchX;

    // The scope replaces the reticle rather than sharing the screen with it,
    // so the two are corrected independently.
    g_scopeStretchX = settings.roundScope ? squareStretch : kStockStretchX;
    g_viewfinderWidth = settings.roundScope ? game::kViewfinderHeight : 256.0f;

    // One unit of the lock-on marker's width covers screenWidth / 640 pixels
    // while one unit of its height covers screenHeight / 448, so the minimum
    // width has to be divided by the ratio of the two to clamp both axes to
    // the same number of pixels.
    g_lockOnMinWidth = settings.roundCrosshair
                           ? kLockOnMinHeight * (game::kDesignWidth * screenHeight) /
                                 (game::kDesignHeight * screenWidth)
                           : 28.0f;

    const float pixelsPerUnitX = screenWidth / game::kDesignWidth;
    logging::Write("  crosshair scale: %.4f px per unit (was %.4f)", static_cast<double>(screenWidth * g_stretchX),
                   static_cast<double>(pixelsPerUnitX));
    logging::Write("  lock-on minimum width: %.4f units against %.1f tall", static_cast<double>(g_lockOnMinWidth),
                   static_cast<double>(kLockOnMinHeight));
}

void PublishSettings(const config::Settings& settings) {
    InterlockedExchange(&g_noCameraCrosshair, settings.noCameraCrosshair ? 1 : 0);
    InterlockedExchange(&g_drawSniperFill, settings.roundScope ? 1 : 0);
}

} // namespace crosshair
