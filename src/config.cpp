#include "config.h"

#include <cstring>

#include "resource.h"

namespace config {
namespace {

// GetPrivateProfileString is the only INI reader available here, and strtod
// would follow the process locale, where a decimal point may not be '.'. The
// values in this file are short and simple, so they are parsed directly and
// both '.' and ',' are accepted as the separator.
bool ParseFloat(const char* text, float& value) {
    const char* cursor = text;
    while (*cursor == ' ' || *cursor == '\t')
        ++cursor;

    float sign = 1.0f;
    if (*cursor == '+' || *cursor == '-') {
        if (*cursor == '-')
            sign = -1.0f;
        ++cursor;
    }

    bool anyDigit = false;
    double result = 0.0;
    while (*cursor >= '0' && *cursor <= '9') {
        result = result * 10.0 + (*cursor - '0');
        anyDigit = true;
        ++cursor;
    }

    if (*cursor == '.' || *cursor == ',') {
        ++cursor;
        double scale = 0.1;
        while (*cursor >= '0' && *cursor <= '9') {
            result += (*cursor - '0') * scale;
            scale *= 0.1;
            anyDigit = true;
            ++cursor;
        }
    }

    if (!anyDigit)
        return false;

    value = sign * static_cast<float>(result);
    return true;
}

bool ReadRaw(const char* path, const char* section, const char* key,
             char (&value)[64]) {
    GetPrivateProfileStringA(section, key, "", value,
                             static_cast<DWORD>(sizeof(value)), path);
    return value[0] != '\0';
}

int ReadInt(const char* path, const char* section, const char* key,
            int fallback) {
    char value[64] = {};
    if (!ReadRaw(path, section, key, value))
        return fallback;

    float parsed = 0.0f;
    if (!ParseFloat(value, parsed))
        return fallback;
    return static_cast<int>(parsed);
}

bool ReadBool(const char* path, const char* section, const char* key,
              bool fallback) {
    return ReadInt(path, section, key, fallback ? 1 : 0) != 0;
}

float ReadFloat(const char* path, const char* section, const char* key,
                float fallback, float minimum, float maximum) {
    char value[64] = {};
    if (!ReadRaw(path, section, key, value))
        return fallback;

    float parsed = 0.0f;
    if (!ParseFloat(value, parsed))
        return fallback;

    if (parsed < minimum)
        return minimum;
    if (parsed > maximum)
        return maximum;
    return parsed;
}

}  // namespace

bool GetPath(HMODULE module, char (&path)[MAX_PATH]) {
    const DWORD length = GetModuleFileNameA(module, path, MAX_PATH);
    if (length == 0 || length >= MAX_PATH)
        return false;

    char* dot = std::strrchr(path, '.');
    if (!dot)
        return false;

    const size_t used = static_cast<size_t>(dot - path);
    if (used + 5 > MAX_PATH)
        return false;

    std::memcpy(dot, ".ini", 5);
    return true;
}

void CreateDefault(HMODULE module, const char* path) {
    if (GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES)
        return;

    const HRSRC resource =
        FindResourceW(module, MAKEINTRESOURCEW(IDR_DEFAULT_INI), RT_RCDATA);
    if (!resource)
        return;

    const HGLOBAL loaded = LoadResource(module, resource);
    const void* data = loaded ? LockResource(loaded) : nullptr;
    const DWORD size = SizeofResource(module, resource);
    if (!data || size == 0)
        return;

    const HANDLE file = CreateFileA(path, GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                                    FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
        return;

    DWORD written = 0;
    const BOOL written_ok = WriteFile(file, data, size, &written, nullptr);
    CloseHandle(file);
    if (!written_ok || written != size)
        DeleteFileA(path);
}

Settings Load(const char* path) {
    Settings settings;

    settings.log = ReadBool(path, "general", "log", false);
    settings.showNotifications =
        ReadBool(path, "general", "showNotifications", true);
    settings.hotkeyModifier =
        ReadInt(path, "general", "hotkeyModifier", VK_MENU);
    if (settings.hotkeyModifier < 0 || settings.hotkeyModifier > 255)
        settings.hotkeyModifier = VK_MENU;
    // A missing key deliberately disables hotkey handling. Do not replace
    // this fallback with the compiled default: users may remove the key to
    // opt out without changing any other setting.
    settings.hotkeyKey = ReadInt(path, "general", "hotkeyKey", 0);
    if (settings.hotkeyKey < 0 || settings.hotkeyKey > 255)
        settings.hotkeyKey = 0;

    settings.roundRadar = ReadBool(path, "radar", "roundRadar", true);
    settings.roundBlips = ReadBool(path, "radar", "roundBlips", true);
    settings.radarDiameter =
        ReadFloat(path, "radar", "diameter", 76.0f, 8.0f, 448.0f);
    settings.radarMarginLeft =
        ReadFloat(path, "radar", "marginLeft", 40.0f, 0.0f, 400.0f);
    settings.radarMarginBottom =
        ReadFloat(path, "radar", "marginBottom", 28.0f, 0.0f, 400.0f);

    settings.roundCrosshair =
        ReadBool(path, "crosshair", "roundCrosshair", true);
    settings.roundScope = ReadBool(path, "crosshair", "roundScope", true);
    settings.noCameraCrosshair =
        ReadBool(path, "crosshair", "noCameraCrosshair", false);
    settings.hideCameraHud =
        ReadBool(path, "crosshair", "hideCameraHud", false);
    settings.hideSniperHud =
        ReadBool(path, "crosshair", "hideSniperHud", false);

    settings.useScreenAspect =
        ReadBool(path, "widescreen", "useScreenAspect", true);
    settings.fixFov = ReadBool(path, "widescreen", "fixFov", true);
    settings.spritePickups =
        ReadBool(path, "worldSprites", "pickups", true);
    settings.spriteCoronas =
        ReadBool(path, "worldSprites", "coronas", true);
    settings.spriteCoronaReflections =
        ReadBool(path, "worldSprites", "coronaReflections", true);
    settings.spriteSunMoon =
        ReadBool(path, "worldSprites", "sunMoon", true);
    settings.spritePointLights =
        ReadBool(path, "worldSprites", "pointLights", true);
    settings.spriteBirds =
        ReadBool(path, "worldSprites", "birds", true);
    settings.spriteClouds =
        ReadBool(path, "worldSprites", "clouds", true);
    settings.spriteCheckpoints =
        ReadBool(path, "worldSprites", "checkpoints", true);
    settings.spriteWeaponEffects =
        ReadBool(path, "worldSprites", "weaponEffects", true);
    settings.spriteCameraEffects =
        ReadBool(path, "worldSprites", "cameraEffects", true);
    settings.spriteTargetingMeasurements =
        ReadBool(path, "worldSprites", "targetingMeasurements", false);

    settings.probeEnabled = ReadBool(path, "probe", "enabled", false);
    settings.probeGroup = ReadInt(path, "probe", "group", 0);
    if (settings.probeGroup < 0 || settings.probeGroup > 1)
        settings.probeGroup = 0;
    settings.probeHotkeyModifier =
        ReadInt(path, "probe", "hotkeyModifier", VK_MENU);
    if (settings.probeHotkeyModifier < 0 || settings.probeHotkeyModifier > 255)
        settings.probeHotkeyModifier = VK_MENU;
    settings.probeHotkeyKey = ReadInt(path, "probe", "hotkeyKey", 'P');
    if (settings.probeHotkeyKey < 0 || settings.probeHotkeyKey > 255)
        settings.probeHotkeyKey = 0;
    return settings;
}

}  // namespace config
