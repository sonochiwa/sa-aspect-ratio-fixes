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

// Hotkeys are written the way a person says them, "Alt+H", so the file does
// not have to explain that 18 is Alt and 72 is H. A value naming no key
// disables the hotkey.
bool EqualsNoCase(const char* text, size_t length, const char* word) {
    for (size_t i = 0; i < length; ++i) {
        char left = text[i];
        if (left >= 'a' && left <= 'z')
            left = static_cast<char>(left - 'a' + 'A');
        char right = word[i];
        if (right == '\0')
            return false;
        if (right >= 'a' && right <= 'z')
            right = static_cast<char>(right - 'a' + 'A');
        if (left != right)
            return false;
    }
    return word[length] == '\0';
}

// One token of a hotkey: a modifier name, a letter or digit, or Fn.
bool ParseHotkeyToken(const char* token, size_t length, Hotkey& hotkey) {
    if (length == 0)
        return true;

    if (EqualsNoCase(token, length, "alt")) {
        hotkey.modifier = VK_MENU;
        return true;
    }
    if (EqualsNoCase(token, length, "ctrl") ||
        EqualsNoCase(token, length, "control")) {
        hotkey.modifier = VK_CONTROL;
        return true;
    }
    if (EqualsNoCase(token, length, "shift")) {
        hotkey.modifier = VK_SHIFT;
        return true;
    }
    if (EqualsNoCase(token, length, "none"))
        return true;

    if (length == 1) {
        const char c = token[0];
        if (c >= 'a' && c <= 'z') {
            hotkey.key = c - 'a' + 'A';
            return true;
        }
        if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) {
            hotkey.key = c;
            return true;
        }
        return false;
    }

    if ((token[0] == 'f' || token[0] == 'F') && length <= 3) {
        int number = 0;
        for (size_t i = 1; i < length; ++i) {
            if (token[i] < '0' || token[i] > '9')
                return false;
            number = number * 10 + (token[i] - '0');
        }
        if (number >= 1 && number <= 12) {
            hotkey.key = VK_F1 + number - 1;
            return true;
        }
    }
    return false;
}

Hotkey ParseHotkey(const char* text, const Hotkey& fallback) {
    Hotkey parsed = {0, 0};
    const char* cursor = text;
    const char* tokenStart = cursor;
    bool valid = true;

    for (;; ++cursor) {
        if (*cursor == '+' || *cursor == '\0') {
            const char* tokenEnd = cursor;
            while (tokenStart < tokenEnd &&
                   (*tokenStart == ' ' || *tokenStart == '\t'))
                ++tokenStart;
            while (tokenEnd > tokenStart &&
                   (tokenEnd[-1] == ' ' || tokenEnd[-1] == '\t'))
                --tokenEnd;

            if (!ParseHotkeyToken(tokenStart,
                                  static_cast<size_t>(tokenEnd - tokenStart),
                                  parsed)) {
                valid = false;
            }
            if (*cursor == '\0')
                break;
            tokenStart = cursor + 1;
        }
    }

    // A value the plugin cannot read is a typo, not a request to change the
    // binding, so the default is kept rather than silently disabling it.
    if (!valid)
        return fallback;
    return parsed;
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

// A count of pixels: never negative, and no larger than any screen the game
// can open.
int ReadPixels(const char* path, const char* section, const char* key,
               int fallback) {
    const int value = ReadInt(path, section, key, fallback);
    if (value < 0)
        return 0;
    if (value > 4096)
        return 4096;
    return value;
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

// An aspect ratio is written the way it is spoken, "16:9", or as the plain
// quotient, "1.7778". Both sides of the colon go through the same parser as
// every other number in the file.
bool ParseAspect(const char* text, float& value) {
    const char* separator = text;
    while (*separator != '\0' && *separator != ':' && *separator != '/')
        ++separator;

    if (*separator == '\0')
        return ParseFloat(text, value);

    float width = 0.0f;
    float height = 0.0f;
    if (!ParseFloat(text, width) || !ParseFloat(separator + 1, height))
        return false;
    if (width <= 0.0f || height <= 0.0f)
        return false;

    value = width / height;
    return true;
}

float ReadAspect(const char* path, const char* section, const char* key,
                 float fallback, float minimum, float maximum) {
    char value[64] = {};
    if (!ReadRaw(path, section, key, value))
        return fallback;

    float parsed = 0.0f;
    if (!ParseAspect(value, parsed))
        return fallback;

    // Zero is the one value below the range that means something: keep the
    // screen's own aspect.
    if (parsed == 0.0f)
        return 0.0f;
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
    settings.showReloadMessage =
        ReadBool(path, "general", "showReloadMessage", true);

    char hotkey[64] = {};
    const Hotkey reloadFallback = {VK_MENU, 'H'};
    settings.reloadHotkey =
        ReadRaw(path, "general", "reloadHotkey", hotkey)
            ? ParseHotkey(hotkey, reloadFallback)
            : reloadFallback;
    settings.roundRadar = ReadBool(path, "radar", "roundRadar", true);
    settings.roundBlips = ReadBool(path, "radar", "roundBlips", true);
    settings.radarDiameter =
        ReadFloat(path, "radar", "diameter", 86.0f, 8.0f, 448.0f);
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

    settings.fixPlayerInfo = ReadBool(path, "hud", "fixPlayerInfo", true);
    settings.playerInfoAspect = ReadAspect(path, "hud", "playerInfoAspect",
                                           16.0f / 9.0f, 1.0f, 4.0f);
    settings.playerInfoScale = ReadInt(path, "hud", "playerInfoScale", 0);
    if (settings.playerInfoScale != 0) {
        if (settings.playerInfoScale < 25)
            settings.playerInfoScale = 25;
        if (settings.playerInfoScale > 400)
            settings.playerInfoScale = 400;
    }
    settings.playerInfoMarginRight = ReadFloat(
        path, "hud", "playerInfoMarginRight", 0.0f, 0.0f, 320.0f);

    settings.fixText = ReadBool(path, "text", "fixText", true);
    settings.textAspect =
        ReadAspect(path, "text", "textAspect", 16.0f / 9.0f, 1.0f, 4.0f);

    settings.useScreenAspect =
        ReadBool(path, "widescreen", "useScreenAspect", false);
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

    settings.aaEdgeLeft = ReadPixels(path, "aaEdgeFrame", "left", 1);
    settings.aaEdgeTop = ReadPixels(path, "aaEdgeFrame", "top", 1);
    settings.aaEdgeRight = ReadPixels(path, "aaEdgeFrame", "right", 1);
    settings.aaEdgeBottom = ReadPixels(path, "aaEdgeFrame", "bottom", 1);

    settings.fitTextdraws = ReadBool(path, "samp", "fitTextdraws", true);
    settings.textdrawAspect = ReadAspect(path, "samp", "textdrawAspect",
                                         16.0f / 9.0f, 1.0f, 4.0f);

    settings.probeEnabled = ReadBool(path, "probe", "enabled", false);
    settings.probeGroup = ReadInt(path, "probe", "group", 0);
    if (settings.probeGroup < 0 || settings.probeGroup > 1)
        settings.probeGroup = 0;
    char probeKey[64] = {};
    const Hotkey probeFallback = {VK_MENU, 'P'};
    settings.probeHotkey = ReadRaw(path, "probe", "hotkey", probeKey)
                               ? ParseHotkey(probeKey, probeFallback)
                               : probeFallback;
    return settings;
}

}  // namespace config
