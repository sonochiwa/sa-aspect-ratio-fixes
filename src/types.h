#pragma once

#include <cstdint>

// Stock values of the pooled literals the plugin repoints. They double as the
// executable check: an executable that does not hold exactly these values at
// these addresses is not the 1.0 US build this plugin was mapped against.
constexpr float kStockStretchX = 1.0f / 640.0f;
constexpr float kStockStretchY = 1.0f / 448.0f;
constexpr float kStockRadarLeft = 40.0f;
constexpr float kStockRadarTop = 104.0f;
constexpr float kStockRadarHigh = 76.0f;
constexpr float kStockRadarWide = 94.0f;

constexpr int32_t kMinScreenSize = 320;
constexpr int32_t kMaxScreenSize = 32768;

// Minimal game types used by CSprite2d::DrawRect. Their field order and
// packing match the 1.0 US classes (CRect is left, bottom, right, top).
struct Rect {
    float left;
    float bottom;
    float right;
    float top;
};

#pragma pack(push, 1)
struct Color {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
    uint8_t alpha;
};
#pragma pack(pop)

struct Vec3 {
    float x;
    float y;
    float z;
};

struct Resolution {
    int32_t width = 0;
    int32_t height = 0;

    bool operator==(const Resolution& other) const {
        return width == other.width && height == other.height;
    }
};
