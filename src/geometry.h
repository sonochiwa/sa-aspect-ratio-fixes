#pragma once

#include "config.h"
#include "types.h"

namespace geometry {

// RsGlobal's framebuffer size, once RenderWare reports one in range.
bool GetResolution(Resolution& resolution);

// The pooled literals live in the image, so this answers before the game has
// finished starting up.
bool IsSupportedExecutable();

// Recomputes every plugin owned value for the current resolution. This runs
// on every resolution change, so the geometry is correct in windowed mode,
// after a video settings change and on any display the game can open.
void Update(const config::Settings& settings, const Resolution& resolution);

} // namespace geometry
