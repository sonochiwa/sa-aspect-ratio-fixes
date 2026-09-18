#pragma once

#include "config.h"
#include "types.h"

// The player info block: the weapon icon and its ammo, the health, armour
// and breath bars, the money counter, the clock and the wanted level. It is
// described by pixels per unit on each axis and the width its positions are
// measured back from.
namespace player_info {

void Apply(const config::Settings& settings, const Resolution& resolution);

// The aspect chooses the horizontal pixels per unit the way a display of
// that aspect and this height would, the scale multiplies both axes, and the
// margin moves the anchor so the money's right edge, 32 units in from the
// anchor, lands where asked. Each of the three keeps the game's own value at
// zero.
void UpdateLayout(const config::Settings& settings, float screenWidth, float screenHeight);

// For the geometry log.
float PixelsPerUnitX();
float PixelsPerUnitY(float screenHeight);
float RightEdgeInset(float screenWidth);

} // namespace player_info
