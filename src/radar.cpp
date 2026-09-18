#include "radar.h"

#include "addresses.h"
#include "addresses_radar.h"
#include "hooking.h"
#include "log.h"
#include "patch.h"
#include "types.h"

namespace radar {
namespace {

// Variables the patched instructions read instead of the pooled literals.
// They are plain floats: x86 aligned 32 bit stores are atomic, so the render
// thread can never observe a half written value.
float g_left = kStockRadarLeft;
float g_top = kStockRadarTop;
float g_high = kStockRadarHigh;
float g_wide = kStockRadarWide;
float g_stretchX = kStockStretchX;
float g_blipStretchX = kStockStretchX;
bool g_patched = false;

// The radar geometry only adds up when the horizontal scale, the rectangle
// and the elements pinned to it are all rewritten together.
bool SitesVerify() {
    return patch::VerifyOperands(game::kRadarStretchXSites, game::kStretchX) &&
           patch::VerifyOperands(game::kBlipStretchXSites, game::kStretchX) &&
           patch::VerifyOperands(game::kRadarLeftSites, game::kRadarLeft) &&
           patch::VerifyOperands(game::kRadarTopSites, game::kRadarTop) &&
           patch::VerifyOperands(game::kRadarHighSites, game::kRadarHigh) &&
           patch::VerifyOperands(game::kRadarWideSites, game::kRadarWide) &&
           patch::VerifyOperands(game::kDependentTopSites, game::kRadarTop) &&
           patch::VerifyOperands(game::kDependentHighSites, game::kRadarHigh);
}

} // namespace

void Apply(const config::Settings& settings) {
    if (g_patched)
        return;

    if (!SitesVerify()) {
        logging::Write("radar: unexpected bytes at one or more sites, the radar was left untouched");
        return;
    }

    bool applied = true;
    applied &= hooking::ApplyGroup("radar scale", game::kRadarStretchXSites, game::kStretchX, &g_stretchX);
    applied &= hooking::ApplyGroup("blip scale", game::kBlipStretchXSites, game::kStretchX, &g_blipStretchX);
    applied &= hooking::ApplyGroup("radar left", game::kRadarLeftSites, game::kRadarLeft, &g_left);
    applied &= hooking::ApplyGroup("radar top", game::kRadarTopSites, game::kRadarTop, &g_top);
    applied &= hooking::ApplyGroup("radar height", game::kRadarHighSites, game::kRadarHigh, &g_high);
    applied &= hooking::ApplyGroup("radar width", game::kRadarWideSites, game::kRadarWide, &g_wide);
    applied &= hooking::ApplyGroup("dependent top", game::kDependentTopSites, game::kRadarTop, &g_top);
    applied &= hooking::ApplyGroup("dependent height", game::kDependentHighSites, game::kRadarHigh, &g_high);

    g_patched = applied;
    if (applied && settings.roundRadar)
        RestoreMask();
}

void RestoreMask() {
    if (!patch::IsReadable(game::kDrawRadarMask, sizeof(game::kDrawRadarMaskPrologue))) {
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
        patch::WriteMemory(game::kDrawRadarMask, game::kDrawRadarMaskPrologue, sizeof(game::kDrawRadarMaskPrologue));
    logging::Write("radar mask: %s", restored ? "restored the circular mask" : "failed to restore the circular mask");
}

// The frame and the blip icons are corrected independently. Blip positions
// come from the frame's transform, so a corrected frame carries them even
// while the icons themselves keep the game's own proportions, and rounded
// icons sit correctly inside a vanilla ellipse.
//
// Patched operands keep pointing at our variables after a hot reload.
// Restoring their stock values disables a group without rewriting
// executable code from the polling thread.
void UpdateGeometry(const config::Settings& settings, float screenWidth, float screenHeight, float squareStretch) {
    if (settings.roundRadar) {
        // SCREEN_STRETCH_X(a) evaluates to a * screenWidth * factor. Making
        // that a * screenHeight / 448 gives the radar one scale on both axes.
        g_stretchX = squareStretch;
        // The diameter and the margins are HUD units of screen height, which
        // only describe the radar once its two axes share one scale. They
        // stay stock while the frame does.
        g_wide = settings.radarDiameter;
        g_high = settings.radarDiameter;
        g_left = settings.radarMarginLeft;
        g_top = settings.radarMarginBottom + settings.radarDiameter;
    } else {
        g_stretchX = kStockStretchX;
        g_wide = kStockRadarWide;
        g_high = kStockRadarHigh;
        g_left = kStockRadarLeft;
        g_top = kStockRadarTop;
    }

    g_blipStretchX = settings.roundBlips ? squareStretch : kStockStretchX;

    const float pixelsPerUnitX = screenWidth / game::kDesignWidth;
    const float pixelsPerUnitY = screenHeight / game::kDesignHeight;
    const float unitPixels = screenHeight / game::kDesignHeight;
    logging::Write("  inside the radar: %.4f px on both axes", static_cast<double>(unitPixels));

    // Reported from the live variables rather than from the settings, so a
    // group left at its stock factor is logged as what the game will
    // actually draw instead of as what the INI asked for.
    const float radarPixelsPerUnitX = screenWidth * g_stretchX;
    const float blipPixelsPerUnitX = screenWidth * g_blipStretchX;
    logging::Write("  radar: %.1f x %.1f px, %.1f px from the left, %.1f px from the bottom",
                   static_cast<double>(g_wide * radarPixelsPerUnitX), static_cast<double>(g_high * pixelsPerUnitY),
                   static_cast<double>(g_left * radarPixelsPerUnitX),
                   static_cast<double>((g_top - g_high) * pixelsPerUnitY));
    logging::Write("  blip icon: %.1f x %.1f px (game default %.1f x %.1f)",
                   static_cast<double>(16.0f * blipPixelsPerUnitX), static_cast<double>(16.0f * pixelsPerUnitY),
                   static_cast<double>(16.0f * pixelsPerUnitX), static_cast<double>(16.0f * pixelsPerUnitY));
}

} // namespace radar
