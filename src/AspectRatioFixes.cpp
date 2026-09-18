// GTA San Andreas authors its HUD in a 640x448 design space and maps it to
// the framebuffer with two independent factors:
//
//     pixels_x = units_x * RsGlobal.maximumWidth  / 640
//     pixels_y = units_y * RsGlobal.maximumHeight / 448
//
// One HUD unit is therefore only as wide as it is tall when the display
// aspect ratio happens to be 640:448, that is 1.4286:1. On every other
// resolution everything the HUD draws is stretched horizontally by
// aspect / 1.4286: the radar is an ellipse, blips are ovals, the crosshair
// and the scope are wider than tall, text and the player info block are
// stretched, and SA-MP textdraws laid out for 16:9 spread across a wider
// screen.
//
// The game reads the scale factors and the radar rectangle through 32 bit
// absolute operands, so the plugin repoints those operands at variables of
// its own and keeps the variables up to date when the resolution changes.
// Nothing rewrites code except the few call sites and prologues that have
// to be redirected at a hook, and every site is verified before it is
// written. Each module below owns its variables and its sites; this file
// only decides the order they go in, follows the framebuffer size, and
// reloads the INI on the hotkey.

#include "aa_edge.h"
#include "config.h"
#include "crosshair.h"
#include "geometry.h"
#include "hooking.h"
#include "hud.h"
#include "log.h"
#include "player_info.h"
#include "probe.h"
#include "radar.h"
#include "text.h"
#include "textdraws.h"
#include "version.h"
#include "world.h"

#include <windows.h>

namespace {

constexpr DWORD kStartupPollMs = 50;
constexpr DWORD kStartupTimeoutMs = 120000;
constexpr DWORD kWatcherPollMs = 50;

config::Settings g_settings;

void PublishRenderSettings() {
    crosshair::PublishSettings(g_settings);
    hud::PublishSettings(g_settings);
    world::PublishSettings(g_settings);
    aa_edge::PublishSettings(g_settings);
}

void ApplyReloadedSettings(HMODULE module, const char* path, const Resolution& resolution) {
    const config::Settings previous = g_settings;
    const config::Settings reloaded = config::Load(path);

    if (reloaded.log && !previous.log)
        logging::Enable(module);

    g_settings = reloaded;
    geometry::Update(g_settings, resolution);

    // All operands are attached to plugin-owned variables during startup,
    // including disabled modules. Reloading therefore changes only aligned
    // data values and never rewrites executable instructions mid-frame.
    if (g_settings.roundRadar && !previous.roundRadar)
        radar::RestoreMask();

    PublishRenderSettings();
    probe::UpdateSelection(g_settings);
    logging::Write("configuration reloaded");

    if (g_settings.showNotifications)
        hud::RequestReloadNotification();

    if (!reloaded.log && previous.log)
        logging::Disable();
}

void ServiceReloadHotkey(HMODULE module, const char* path, const Resolution& resolution, bool& wasDown) {
    const bool down = g_settings.hotkey.IsDown();
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

    logging::Write(PLUGIN_NAME " v" PLUGIN_VERSION);

    if (!geometry::IsSupportedExecutable()) {
        logging::Write("unsupported executable, nothing was patched");
        return 0;
    }

    // RsGlobal only reports the framebuffer size once RenderWare is up, so
    // the geometry cannot be computed at load time.
    Resolution resolution;
    for (DWORD waited = 0; waited < kStartupTimeoutMs; waited += kStartupPollMs) {
        if (geometry::GetResolution(resolution))
            break;
        Sleep(kStartupPollMs);
    }

    if (resolution.width == 0) {
        logging::Write("gave up waiting for RsGlobal to report a resolution");
        return 0;
    }

    geometry::Update(g_settings, resolution);

    // Patch every configurable module once. Disabled modules publish the
    // exact stock constants, so the reload hotkey can enable them without
    // modifying executable instructions while the render thread is active.
    radar::Apply(g_settings);
    crosshair::Apply();
    probe::Apply(g_settings);
    probe::UpdateSelection(g_settings);

    // Pass-through hooks go in even when their options are off, so a hot
    // reload can enable those options without rewriting code mid-frame.
    crosshair::ApplyDrawHook();
    hud::ApplyVisibilityHook();
    PublishRenderSettings();

    world::ApplyFovFix();
    world::ApplySprites();
    text::ApplyFontOutline();
    text::ApplyTextScale(g_settings, resolution);
    player_info::Apply(g_settings, resolution);
    textdraws::ApplySampTextdraws(g_settings, resolution);
    if (textdraws::Patched())
        text::ApplyFontBoxHook();

    aa_edge::Apply();

    // Keep following the framebuffer size. Only plugin owned variables are
    // written from here, never game code.
    bool hotkeyWasDown = false;
    bool probeKeyWasDown = false;
    for (;;) {
        Sleep(kWatcherPollMs);

        Resolution current;
        if (geometry::GetResolution(current) && !(current == resolution)) {
            resolution = current;
            geometry::Update(g_settings, resolution);
            probe::UpdateSelection(g_settings);
        }

        ServiceReloadHotkey(module, path, resolution, hotkeyWasDown);
        probe::ServiceHotkey(g_settings, probeKeyWasDown);
        hooking::CheckGuards();
    }
}

} // namespace

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(instance);
        const HANDLE thread = CreateThread(nullptr, 0, PluginThread, instance, 0, nullptr);
        if (thread)
            CloseHandle(thread);
    }
    return TRUE;
}
