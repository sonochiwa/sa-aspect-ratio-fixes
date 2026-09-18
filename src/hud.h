#pragma once

#include "config.h"

// CHud::Draw, intercepted for the camera and sniper HUD switches and as the
// place on the game thread where the worker's on-screen notifications are
// posted.
namespace hud {

// Hooks CHud::Draw at its entry when that is free, or at its body when an
// overlay already owns the entry.
void ApplyVisibilityHook();
void PublishSettings(const config::Settings& settings);

// Shown on the next frame by the game thread. The worker owns no GTA message
// queue, so it only publishes the request.
void RequestReloadNotification();

} // namespace hud
