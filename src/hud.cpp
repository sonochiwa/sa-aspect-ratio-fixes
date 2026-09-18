#include "hud.h"

#include "addresses.h"
#include "crosshair.h"
#include "game.h"
#include "hooking.h"
#include "log.h"
#include "patch.h"
#include "probe.h"

#include <windows.h>

#include <cstring>

namespace hud {
namespace {

using DrawHudFn = void (__cdecl*)();

volatile LONG g_hideCameraHud = 0;
volatile LONG g_hideSniperHud = 0;
volatile LONG g_reloadNotificationPending = 0;

// Set when the HUD is intercepted at its body because another plugin already
// owns the entry. The body has passed the disabled check by then, so
// resuming through it must not repeat that check.
DrawHudFn g_drawHudResume = nullptr;

void DrawOriginalHud() {
    if (g_drawHudResume) {
        g_drawHudResume();
        return;
    }

    // This reproduces the seven-byte instruction and branch replaced at the
    // CHud::Draw entry. The body starts after both, so it does not recurse
    // into DrawHudHook and its eventual RET returns normally to this helper.
    if (*reinterpret_cast<const uint8_t*>(game::kHudDisabled) == 1)
        return;

    reinterpret_cast<DrawHudFn>(game::kDrawHudBody)();
}

void ShowPendingReloadNotification() {
    if (InterlockedExchange(&g_reloadNotificationPending, 0) == 0)
        return;
    game_api::AddMessage("~w~Aspect Ratio Fixes: configuration reloaded", 2500);
}

void DrawHudHook() {
    ShowPendingReloadNotification();
    probe::ShowPendingNotification();

    // Preserve the game's clean camera capture frame.
    if (*reinterpret_cast<const bool*>(game::kTakePhoto)) {
        DrawOriginalHud();
        return;
    }

    const int16_t mode = game_api::GetCameraMode();
    const bool hideHud =
        (mode == game::kCameraModeCamera && InterlockedCompareExchange(&g_hideCameraHud, 0, 0) != 0) ||
        (mode == game::kCameraModeSniper && InterlockedCompareExchange(&g_hideSniperHud, 0, 0) != 0);

    if (hideHud) {
        crosshair::DrawCrossHairsHook();
        return;
    }

    DrawOriginalHud();
}

} // namespace

// When the entry is not free, the overlay that took it reaches the body
// through a trampoline of its own, so the body still runs and hooking that
// works alongside it instead of fighting for the entry.
void ApplyVisibilityHook() {
    constexpr size_t kEntryStolen = sizeof(game::kDrawHudPrologue);

    if (hooking::BytesMatch(game::kDrawHud, game::kDrawHudPrologue, kEntryStolen)) {
        const intptr_t displacement =
            reinterpret_cast<intptr_t>(DrawHudHook) - static_cast<intptr_t>(game::kDrawHud + 5);
        if (displacement < INT32_MIN || displacement > INT32_MAX) {
            logging::Write("aim-mode HUD hook      FAILED, target out of range");
            return;
        }

        uint8_t branch[kEntryStolen];
        std::memset(branch, 0x90, sizeof(branch));
        branch[0] = 0xE9;
        const auto relative = static_cast<int32_t>(displacement);
        std::memcpy(branch + 1, &relative, sizeof(relative));
        const bool applied = patch::WriteMemory(game::kDrawHud, branch, sizeof(branch));
        logging::Write("aim-mode HUD hook      %s (CHud::Draw entry)", applied ? "patched" : "FAILED");
        return;
    }

    constexpr size_t kBodyStolen = sizeof(game::kDrawHudBodyPrologue);
    if (!hooking::BytesMatch(game::kDrawHudBody, game::kDrawHudBodyPrologue, kBodyStolen)) {
        logging::Write("aim-mode HUD hook      SKIPPED, entry and body are both taken");
        return;
    }

    void* resume = nullptr;
    const bool applied =
        hooking::InstallTrampolineHook(game::kDrawHudBody, game::kDrawHudBodyPrologue, kBodyStolen,
                                       game::kDrawHudBodyResume, reinterpret_cast<const void*>(DrawHudHook), &resume);
    g_drawHudResume = reinterpret_cast<DrawHudFn>(resume);
    logging::Write("aim-mode HUD hook      %s (CHud::Draw body, entry is owned by another plugin)",
                   applied ? "patched" : "FAILED");
}

void PublishSettings(const config::Settings& settings) {
    InterlockedExchange(&g_hideCameraHud, settings.hideCameraHud ? 1 : 0);
    InterlockedExchange(&g_hideSniperHud, settings.hideSniperHud ? 1 : 0);
}

void RequestReloadNotification() {
    InterlockedExchange(&g_reloadNotificationPending, 1);
}

} // namespace hud
