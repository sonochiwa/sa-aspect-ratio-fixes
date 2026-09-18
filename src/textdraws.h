#pragma once

#include "config.h"
#include "types.h"

// SA-MP textdraws. CTextDraw::Draw reads the screen width where the layout
// width is substituted, so with the width of a 16:9 area of the screen's
// height it lays out as it would on a 16:9 display, and every X position
// that leaves the function is shifted by how far right that area sits.
namespace textdraws {

void ApplySampTextdraws(const config::Settings& settings, const Resolution& resolution);
bool Patched();

// Lays the textdraws out in a centred area of the configured aspect when the
// screen is wider than that, and leaves them at the screen's own width
// otherwise.
void UpdateLayout(const config::Settings& settings, float screenWidth, float screenHeight);

} // namespace textdraws
