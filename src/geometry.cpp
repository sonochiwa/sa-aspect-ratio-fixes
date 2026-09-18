#include "geometry.h"

#include "addresses.h"
#include "crosshair.h"
#include "log.h"
#include "patch.h"
#include "player_info.h"
#include "probe.h"
#include "radar.h"
#include "text.h"
#include "textdraws.h"
#include "world.h"

namespace geometry {
namespace {

bool HoldsStockValue(uintptr_t address, float expected) {
    float value = 0.0f;
    return patch::ReadFloat(address, value) && value == expected;
}

// Several options are only meaningful in combination with another one, and
// a combination that cancels itself out is indistinguishable from a patch
// that failed to apply: the game simply looks unchanged. Each of these is
// therefore named in the log rather than left for the user to work out from
// the source.
void LogSettingConflicts(const config::Settings& settings) {
    if (settings.fixFov && !settings.useScreenAspect) {
        logging::Write("  note: fixFov has no effect while useScreenAspect=0. The conversion scales by "
                       "aspect / (4/3), and useScreenAspect=0 holds that aspect at 4:3, so it returns the "
                       "angle unchanged");
    }

    if (!settings.roundRadar &&
        (settings.radarDiameter != kStockRadarHigh || settings.radarMarginLeft != kStockRadarLeft ||
         settings.radarMarginBottom != kStockRadarTop - kStockRadarHigh)) {
        logging::Write("  note: diameter and margins are ignored while roundRadar=0. They are HUD units of "
                       "screen height, which only describe the radar once its axes share one scale");
    }
}

} // namespace

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

bool IsSupportedExecutable() {
    return HoldsStockValue(game::kStretchX, kStockStretchX) && HoldsStockValue(game::kStretchY, kStockStretchY) &&
           HoldsStockValue(game::kRadarLeft, kStockRadarLeft) && HoldsStockValue(game::kRadarTop, kStockRadarTop) &&
           HoldsStockValue(game::kRadarHigh, kStockRadarHigh) && HoldsStockValue(game::kRadarWide, kStockRadarWide);
}

void Update(const config::Settings& settings, const Resolution& resolution) {
    const auto screenWidth = static_cast<float>(resolution.width);
    const auto screenHeight = static_cast<float>(resolution.height);

    // Pixels covered by one HUD unit on each axis, as the game computes them.
    const float pixelsPerUnitX = screenWidth / game::kDesignWidth;
    const float pixelsPerUnitY = screenHeight / game::kDesignHeight;

    // The horizontal factor that makes a HUD unit cover as many pixels as
    // the vertical one does.
    const float squareStretch = screenHeight / (game::kDesignHeight * screenWidth);
    probe::SetSquareStretch(squareStretch);
    world::SetSpriteWidthCorrection(screenHeight / screenWidth);

    if (settings.useScreenAspect)
        *reinterpret_cast<float*>(game::kAspectRatio) = screenWidth / screenHeight;

    logging::Write("resolution %dx%d, aspect %.4f", resolution.width, resolution.height,
                   static_cast<double>(screenWidth / screenHeight));
    logging::Write("  one HUD unit: %.4f px wide, %.4f px tall (game default)", static_cast<double>(pixelsPerUnitX),
                   static_cast<double>(pixelsPerUnitY));

    radar::UpdateGeometry(settings, screenWidth, screenHeight, squareStretch);
    crosshair::UpdateGeometry(settings, screenWidth, screenHeight, squareStretch);
    player_info::UpdateLayout(settings, screenWidth, screenHeight);
    logging::Write("  player info: %.4f x %.4f px per unit (game default %.4f x %.4f), right edge %.1f px in",
                   static_cast<double>(player_info::PixelsPerUnitX()),
                   static_cast<double>(player_info::PixelsPerUnitY(screenHeight)), static_cast<double>(pixelsPerUnitX),
                   static_cast<double>(pixelsPerUnitY), static_cast<double>(player_info::RightEdgeInset(screenWidth)));
    text::UpdateLayout(settings, screenWidth, screenHeight);
    textdraws::UpdateLayout(settings, screenWidth, screenHeight);

    LogSettingConflicts(settings);
}

} // namespace geometry
