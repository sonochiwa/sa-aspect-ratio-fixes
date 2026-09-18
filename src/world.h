#pragma once

#include "config.h"

// World sprites and the widescreen FOV. The sprite callers of
// CSprite::CalcScreenCoors are redirected through wrappers that correct the
// projected width; CDraw::SetFOV and CalculateAspectRatio are redirected so
// the FOV follows the screen aspect.
namespace world {

void ApplySprites();
void ApplyFovFix();
void PublishSettings(const config::Settings& settings);
// screenHeight / screenWidth, applied to every corrected sprite width.
void SetSpriteWidthCorrection(float correction);

} // namespace world
