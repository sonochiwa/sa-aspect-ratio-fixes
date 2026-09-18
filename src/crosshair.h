#pragma once

#include "config.h"

// The weapon reticle, the lock-on marker, the sniper scope and the camera
// viewfinder, all drawn by CHud::DrawCrossHairs with the HUD's horizontal
// stretch.
namespace crosshair {

// Repoints the reticle, lock-on, scope and viewfinder sites.
void Apply();
// Hooks CHud::DrawCrossHairs at its own entry, for the camera crosshair
// switch and the sniper side fill.
void ApplyDrawHook();

void UpdateGeometry(const config::Settings& settings, float screenWidth, float screenHeight, float squareStretch);
void PublishSettings(const config::Settings& settings);

// The hook body, also run by the HUD hook when the HUD is hidden.
void DrawCrossHairsHook();

} // namespace crosshair
