#pragma once

#include "config.h"

// World sprites and the widescreen FOV. The sprite callers of
// CSprite::CalcScreenCoors are redirected through wrappers that correct the
// projected width; CDraw::SetFOV and CalculateAspectRatio are redirected so
// the FOV follows the screen aspect. The camera's draw-distance multiplier
// keeps reading the unconverted FOV, and the aim ray is widened with the view
// so bullets go where the crosshair is.
namespace world {

void ApplySprites();
void ApplyFovFix();
void PublishSettings(const config::Settings& settings);
// screenHeight / screenWidth, applied to every corrected sprite width.
void SetSpriteWidthCorrection(float correction);

} // namespace world
