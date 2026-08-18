#pragma once

#include <windows.h>

namespace config {

// A key with an optional modifier, both as Win32 virtual-key codes. A key of
// 0 means the hotkey is disabled.
struct Hotkey {
    int modifier;
    int key;
};

struct Settings {
    bool log = false;
    bool showReloadMessage = true;
    Hotkey reloadHotkey = {VK_MENU, 'H'};

    bool roundRadar = true;
    bool roundBlips = true;
    float radarDiameter = 76.0f;
    float radarMarginLeft = 40.0f;
    float radarMarginBottom = 28.0f;

    bool roundCrosshair = true;
    bool roundScope = true;
    bool noCameraCrosshair = false;
    bool hideCameraHud = false;
    bool hideSniperHud = false;

    bool useScreenAspect = true;
    bool fixFov = true;

    bool spritePickups = true;
    bool spriteCoronas = true;
    bool spriteCoronaReflections = true;
    bool spriteSunMoon = true;
    bool spritePointLights = true;
    bool spriteBirds = true;
    bool spriteClouds = true;
    bool spriteCheckpoints = true;
    bool spriteWeaponEffects = true;
    bool spriteCameraEffects = true;
    bool spriteTargetingMeasurements = false;

    // Diagnostic. Corrects one candidate SCREEN_STRETCH_X site at a time so it
    // can be identified in game; see references\stretch-x-sites.md.
    bool probeEnabled = false;
    int probeGroup = 0;
    Hotkey probeHotkey = {VK_MENU, 'P'};
};

// Builds "<module directory>\<module name>.ini". Returns false when the path
// does not fit into MAX_PATH.
bool GetPath(HMODULE module, char (&path)[MAX_PATH]);

// Writes the canonical configuration embedded at build time when the file is
// missing. The resource is compiled from Config\AspectRatioFixes.ini, so the
// generated file is byte for byte identical to it.
void CreateDefault(HMODULE module, const char* path);

Settings Load(const char* path);

}  // namespace config
