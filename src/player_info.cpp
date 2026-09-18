#include "player_info.h"

#include "addresses.h"
#include "hooking.h"
#include "log.h"
#include "patch.h"
#include "text.h"

#include <cmath>

namespace player_info {
namespace {

using HudPassFn = void (__cdecl*)();

// The two factors are what the block's sites multiply the width and height
// they read by; the width they read is `g_width`, so the horizontal factor
// is pixels per unit divided by that width and the block's positions,
// measured back from it, land where the margin asks. The icon's margin
// literal is the same fraction of that width.
float g_stretchX = kStockStretchX;
float g_stretchY = kStockStretchY;
int32_t g_width = 0;
float g_weaponIconMargin = 0.0f;
float g_weaponIconMarginStock = 0.0f;

// The factors the helpers the block shares with the rest of the game use
// while the block is drawn. They read the real screen size, so the
// horizontal one is pixels per unit over the real width.
float g_hudPassStretchX = kStockStretchX;
float g_hudPassStretchY = kStockStretchY;
float g_passStretchX = kStockStretchX;

// The block's text is printed and its bars are outlined inside these two
// passes, so the font's outline offsets and the bar outline follow the
// block's factor for exactly their duration and nothing else's.
void RunHudPass(uintptr_t pass) {
    text::BeginHudPass(g_passStretchX);
    g_hudPassStretchX = g_passStretchX;
    g_hudPassStretchY = g_stretchY;
    reinterpret_cast<HudPassFn>(pass)();
    g_hudPassStretchY = kStockStretchY;
    g_hudPassStretchX = kStockStretchX;
    text::EndHudPass();
}

void __cdecl DrawPlayerInfoHook() {
    RunHudPass(game::kDrawPlayerInfo);
}

void __cdecl DrawWantedLevelHook() {
    RunHudPass(game::kDrawWantedLevel);
}

} // namespace

// The block's four groups describe one layout between them: a factor over a
// width that is not the one the sites read would put every size off, so all
// of them are verified before any is written and either all apply or none
// does. The two pass wrappers are verified with them: they are what keeps
// the text module away from text the block has already scaled, so a block
// without them would have its text scaled twice. The shared helpers' sites
// are pass-throughs on their own.
void Apply(const config::Settings& settings, const Resolution& resolution) {
    if (!patch::ReadFloat(game::kWeaponIconMargin, g_weaponIconMarginStock) ||
        !hooking::RelativeCallTargets(game::kDrawPlayerInfoCallSites[0], game::kDrawPlayerInfo) ||
        !hooking::RelativeCallTargets(game::kDrawWantedLevelCallSites[0], game::kDrawWantedLevel) ||
        !patch::VerifyOperands(game::kPlayerInfoStretchXSites, game::kStretchX) ||
        !patch::VerifyOperands(game::kPlayerInfoWidthReadSites, game::kScreenWidth) ||
        !patch::VerifyOperands(game::kPlayerInfoStretchYSites, game::kStretchY) ||
        !patch::VerifyOperands(game::kWeaponIconMarginSites, game::kWeaponIconMargin)) {
        logging::Write("player info            SKIPPED, unexpected bytes");
        return;
    }

    // The margin literal was not known when the geometry was first computed.
    UpdateLayout(settings, static_cast<float>(resolution.width), static_cast<float>(resolution.height));

    hooking::ApplyGroup("player info width", game::kPlayerInfoStretchXSites, game::kStretchX, &g_stretchX);
    hooking::ApplyGroup("player info anchor", game::kPlayerInfoWidthReadSites, game::kScreenWidth, &g_width);
    hooking::ApplyGroup("player info height", game::kPlayerInfoStretchYSites, game::kStretchY, &g_stretchY);
    hooking::ApplyGroup("weapon icon margin", game::kWeaponIconMarginSites, game::kWeaponIconMargin,
                        &g_weaponIconMargin);
    hooking::ApplyGroup("bar outline width", game::kHudPassStretchXSites, game::kStretchX, &g_hudPassStretchX);
    hooking::ApplyGroup("bar outline height", game::kHudPassStretchYSites, game::kStretchY, &g_hudPassStretchY);
    hooking::ApplyCallGroup("player info pass", game::kDrawPlayerInfoCallSites, game::kDrawPlayerInfo,
                            reinterpret_cast<const void*>(DrawPlayerInfoHook));
    hooking::ApplyCallGroup("wanted level pass", game::kDrawWantedLevelCallSites, game::kDrawWantedLevel,
                            reinterpret_cast<const void*>(DrawWantedLevelHook));
}

void UpdateLayout(const config::Settings& settings, float screenWidth, float screenHeight) {
    float pixelsX = screenWidth / game::kDesignWidth;
    float pixelsY = screenHeight / game::kDesignHeight;
    float anchor = screenWidth;

    if (settings.fixPlayerInfo) {
        if (settings.playerInfoAspect > 0.0f)
            pixelsX = screenHeight * settings.playerInfoAspect / game::kDesignWidth;
        if (settings.playerInfoScale > 0) {
            const float scale = static_cast<float>(settings.playerInfoScale) / 100.0f;
            pixelsX *= scale;
            pixelsY *= scale;
        }
        if (settings.playerInfoMarginRight > 0.0f) {
            constexpr float kMoneyMargin = 32.0f;
            const float wanted = settings.playerInfoMarginRight * screenHeight / game::kDesignHeight;
            anchor = screenWidth - (wanted - kMoneyMargin * pixelsX);
        }
    }

    // The anchor is read through an integer load, and the block is measured
    // back from it in whole pixels either way.
    const auto width = static_cast<int32_t>(std::lround(anchor));
    if (width < kMinScreenSize)
        return;

    g_stretchX = pixelsX / static_cast<float>(width);
    g_stretchY = pixelsY / screenHeight;
    g_passStretchX = pixelsX / screenWidth;
    g_weaponIconMargin = g_weaponIconMarginStock * game::kDesignWidth * pixelsX / static_cast<float>(width);
    g_width = width;
}

float PixelsPerUnitX() {
    return static_cast<float>(g_width) * g_stretchX;
}

float PixelsPerUnitY(float screenHeight) {
    return screenHeight * g_stretchY;
}

float RightEdgeInset(float screenWidth) {
    return screenWidth - static_cast<float>(g_width) + 32.0f * static_cast<float>(g_width) * g_stretchX;
}

} // namespace player_info
