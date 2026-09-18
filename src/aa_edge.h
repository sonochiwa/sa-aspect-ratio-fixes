#pragma once

#include "config.h"

// The frame over the multisampling edge bug: after rasterisation the four
// one-pixel edge samples of the screen are left half covered by MSAA, and a
// black frame drawn at the tail of the 2D pass hides them.
namespace aa_edge {

void Apply();
void PublishSettings(const config::Settings& settings);

} // namespace aa_edge
