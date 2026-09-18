#pragma once

#include "config.h"

// The radar. The plugin repoints the horizontal factor used by the radar
// code so that it equals the vertical one: inside the radar a HUD unit then
// covers the same number of pixels on both axes, so a circle is a square
// rectangle, the corner masks are padded symmetrically and a blip is as wide
// as it is tall. The frame and the blip icons read that factor through two
// separate groups of instructions, so each has its own variable and switch.
namespace radar {

// Every site is verified before the first one is written.
void Apply(const config::Settings& settings);

// Some square radar modifications replace the prologue of
// CRadar::DrawRadarMask with a RET or a JMP; restoring it brings the circular
// mask back.
void RestoreMask();

// Publishes the plugin-owned values for the current resolution and logs them.
// `squareStretch` is the horizontal factor that makes a HUD unit square.
void UpdateGeometry(const config::Settings& settings, float screenWidth, float screenHeight, float squareStretch);

} // namespace radar
