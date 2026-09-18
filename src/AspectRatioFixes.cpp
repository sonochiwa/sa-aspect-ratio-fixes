// SA Aspect Ratio Fixes
//
// GTA San Andreas authors its HUD in a 640x448 design space and maps it to the
// framebuffer with two independent factors:
//
//     pixels_x = units_x * RsGlobal.maximumWidth  / 640
//     pixels_y = units_y * RsGlobal.maximumHeight / 448
//
// One HUD unit is therefore only as wide as it is tall when the display aspect
// ratio happens to be 640:448, that is 1.4286:1. On every other resolution
// everything the HUD draws is stretched horizontally by aspect / 1.4286.
//
// For the radar that error lands three times. Its rectangle is 94 units wide
// and 76 units tall, so it is an ellipse even at 4:3 (94/640 * 4/3, divided by
// 76/448, is 1.1544). The circular mask is drawn through the same transform,
// so it follows the ellipse instead of correcting it. And every blip is drawn
// as `x +/- SCREEN_STRETCH_X(8)` against `y +/- SCREEN_STRETCH_Y(8)`, so the
// icons are ovals too.
//
// Rather than solving each of those separately, the plugin repoints the
// horizontal factor used by the radar code so that it equals the vertical one.
// Inside the radar a HUD unit then covers the same number of pixels on both
// axes, and the geometry follows from that: a circle is a square rectangle, the
// corner masks that draw the black ring are padded symmetrically, and a blip is
// as wide as it is tall. Position conversions that only run while the
// full-screen map is open keep the game's own scaling, because they position
// against the map origin instead.
//
// The frame and the icons read that factor through two separate groups of
// instructions, so each has its own variable and its own switch. Blip
// positions are produced by the frame's transform rather than by the icon
// code, which is what lets either group stay stock without the other leaving
// anything out of place.
//
// Nothing here rewrites code. The game reads the scale factors and the radar
// rectangle through 32 bit absolute operands, so the plugin only repoints those
// operands at its own variables and keeps the variables up to date when the
// resolution changes.

#include <windows.h>

#include <cstdio>
#include <cstring>
#include <cstdint>
#include <cmath>

#include "config.h"
#include "game_layout.h"
#include "log.h"
#include "patch.h"
#include "samp_layout.h"

namespace {

constexpr char kVersion[] = "1.3.0";

// Stock values of the pooled literals the plugin repoints. They double as the
// executable check: an executable that does not hold exactly these values at
// these addresses is not the 1.0 US build this plugin was mapped against.
constexpr float kStockStretchX  = 1.0f / 640.0f;
constexpr float kStockStretchY  = 1.0f / 448.0f;
constexpr float kStockRadarLeft = 40.0f;
constexpr float kStockRadarTop  = 104.0f;
constexpr float kStockRadarHigh = 76.0f;
constexpr float kStockRadarWide = 94.0f;

constexpr int32_t kMinScreenSize = 320;
constexpr int32_t kMaxScreenSize = 32768;

constexpr DWORD kStartupPollMs = 50;
constexpr DWORD kStartupTimeoutMs = 120000;
constexpr DWORD kWatcherPollMs = 50;

// Variables the patched instructions read instead of the pooled literals. They
// are plain floats: x86 aligned 32 bit stores are atomic, so the render thread
// can never observe a half written value.
float g_radarLeft = kStockRadarLeft;
float g_radarTop  = kStockRadarTop;
float g_radarHigh = kStockRadarHigh;
float g_radarWide = kStockRadarWide;
float g_radarStretchX = kStockStretchX;
float g_blipStretchX = kStockStretchX;
float g_crosshairStretchX = kStockStretchX;
float g_scopeStretchX = kStockStretchX;

// The lock-on target's minimum width. CWeaponEffects::Render clamps the marker
// to 28 pixels wide against 20 tall, and measurement shows the width it compares
// against carries the HUD's horizontal stretch while the height does not, so the
// minimum has to carry the inverse of that stretch rather than a flat 20.
constexpr float kLockOnMinHeight = 20.0f;
float g_lockOnMinWidth = kLockOnMinHeight;

// The camera viewfinder ring's width, made equal to its height so that the
// round ring in the texture lands round on screen.
float g_viewfinderWidth = game::kViewfinderHeight;

// Probe state. Every site in the selected group is repointed at its own slot,
// so selecting one is a matter of writing floats rather than rewriting code,
// exactly like every other module. Index -1 corrects nothing and index
// g_probeCount corrects the whole group at once.
float g_probeStretch[game::kMaxProbeSites];
const uintptr_t* g_probeSites = nullptr;
size_t g_probeCount = 0;
int g_probeIndex = -1;
char g_probeMessage[96] = {};
float g_squareStretch = kStockStretchX;
volatile LONG g_probeNotificationPending = 0;

config::Settings g_settings;

// The render thread reads these while the worker thread reloads the INI.
// Publishing precomputed booleans through InterlockedExchange avoids racing on
// the multi-field Settings object.
volatile LONG g_noCameraCrosshair = 0;
volatile LONG g_hideCameraHud = 0;
volatile LONG g_hideSniperHud = 0;
volatile LONG g_drawSniperFill = 0;
volatile LONG g_fixFov = 0;
volatile LONG g_useScreenAspect = 0;
volatile LONG g_spritePickups = 0;
volatile LONG g_spriteCoronas = 0;
volatile LONG g_spriteCoronaReflections = 0;
volatile LONG g_spriteSunMoon = 0;
volatile LONG g_spritePointLights = 0;
volatile LONG g_spriteBirds = 0;
volatile LONG g_spriteClouds = 0;
volatile LONG g_spriteCheckpoints = 0;
volatile LONG g_spriteWeaponEffects = 0;
volatile LONG g_spriteCameraEffects = 0;
volatile LONG g_spriteTargeting = 0;
volatile LONG g_aaEdgeLeft = 1;
volatile LONG g_aaEdgeTop = 1;
volatile LONG g_aaEdgeRight = 1;
volatile LONG g_aaEdgeBottom = 1;
volatile LONG g_reloadNotificationPending = 0;

float g_worldSpriteWidthCorrection = 1.0f;

// SA-MP textdraw layout. CTextDraw::Draw reads `width` where it used to read
// RsGlobal.maximumWidth, so with the screen's own width it lays out exactly as
// stock, and with the width of a 16:9 area of the screen's height it lays out
// as it would on a 16:9 display. `offset` is how far right that area sits.
//
// The two belong together: a width from one resolution beside an offset from
// another puts every textdraw off centre for a frame. The worker publishes
// them as one 64 bit value, and the draw hook takes both from it in one read
// before the original runs.
struct TextdrawLayout {
    int32_t width;
    float offset;
};
static_assert(sizeof(TextdrawLayout) == sizeof(LONGLONG),
              "TextdrawLayout must fit one interlocked exchange");

volatile LONGLONG g_textdrawLayoutPacked = 0;

// Read by the two repointed loads in CTextDraw::Draw and by the forwarders
// below, written only by the draw hook on the render thread. `scale` is the
// layout width over the screen width, which is what every horizontal
// quantity the font derives from the real width has to be multiplied by.
int32_t g_textdrawWidth = 0;
float g_textdrawOffset = 0.0f;
float g_textdrawScale = 1.0f;

// Read by the font's outline and drop shadow pass in place of the pooled
// SCREEN_STRETCH_X factor. It holds whatever the text being printed is
// scaled by: the layout's factor while a textdraw is printed, the block's
// during a HUD pass, and `g_textStretchX`, the factor of every other piece
// of text, the rest of the time.
float g_fontStretchX = kStockStretchX;
bool g_textdrawPrinting = false;

// Text in general. Every caller of CFont::SetScale passes a width it has
// multiplied by the real SCREEN_STRETCH_X; the hook multiplies it again by
// `g_textScale`, the ratio of the wanted pixels per unit to the real ones,
// except inside a textdraw draw or a HUD pass, which scale their own text.
float g_textScale = 1.0f;
float g_textStretchX = kStockStretchX;
bool g_textdrawDrawing = false;
bool g_hudPassDrawing = false;
bool g_textPatched = false;

// The player info block. The two factors are what its sites multiply the
// width and height they read by; the width they read is `g_playerInfoWidth`,
// so the horizontal factor is pixels per unit divided by that width and the
// block's positions, measured back from it, land where the margin asks.
// The icon's margin literal is the same fraction of that width.
float g_playerInfoStretchX = kStockStretchX;
float g_playerInfoStretchY = kStockStretchY;
int32_t g_playerInfoWidth = 0;
float g_weaponIconMargin = 0.0f;
float g_weaponIconMarginStock = 0.0f;

// The factors the helpers the block shares with the rest of the game use
// while the block is drawn. They read the real screen size, so the
// horizontal one is pixels per unit over the real width.
float g_hudPassStretchX = kStockStretchX;
float g_hudPassStretchY = kStockStretchY;
float g_playerInfoPassStretchX = kStockStretchX;

// The client functions the hooks forward to, resolved against the loaded
// module once its build is known.
uintptr_t g_sampDraw = 0;
uintptr_t g_sampSpriteForward = 0;
uintptr_t g_sampWrapxForward = 0;
uintptr_t g_sampPrintForward = 0;
bool g_textdrawsPatched = false;

bool g_radarPatched = false;
bool g_crosshairScalePatched = false;
bool g_lockOnPatched = false;
bool g_scopePatched = false;
bool g_viewfinderPatched = false;

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

using DrawRectFn = int (__cdecl*)(const Rect&, const Color&);
using DefinedState2dFn = void (__cdecl*)();
using DrawCrossHairsFn = void (__cdecl*)();

using RenderStateSetFn = int (__cdecl*)(int, void*);
using RenderStateGetFn = int (__cdecl*)(int, void*);

// Prefixes of RenderWare 3.6's non-debug RwGlobals and RwDevice. Keeping the
// declarations local avoids taking a dependency on the full RenderWare SDK.
struct RwDevicePrefix {
    float gammaCorrection;
    void* system;
    float zBufferNear;
    float zBufferFar;
    RenderStateSetFn renderStateSet;
    RenderStateGetFn renderStateGet;
};

struct RwGlobalsPrefix {
    void* currentCamera;
    void* currentWorld;
    uint16_t renderFrame;
    uint16_t lightFrame;
    uint16_t padding[2];
    RwDevicePrefix device;
};

static_assert(offsetof(RwGlobalsPrefix, device.renderStateSet) == 0x20,
              "unexpected RenderWare globals layout");
static_assert(offsetof(RwGlobalsPrefix, device.renderStateGet) == 0x24,
              "unexpected RenderWare globals layout");

struct SavedRenderState {
    int state = 0;
    void* value = nullptr;
    bool valid = false;
};

// DefinedState2d changes these states, while CSprite2d::DrawRect additionally
// clears the texture raster. Texture U/V addressing is saved separately: the
// combined TEXTUREADDRESS state can lose an asymmetric pair.
constexpr int kAARenderStateIds[] = {
    1,  // TEXTURERASTER
    3,  // TEXTUREADDRESSU
    4,  // TEXTUREADDRESSV
    5,  // TEXTUREPERSPECTIVE
    6,  // ZTESTENABLE
    7,  // SHADEMODE
    8,  // ZWRITEENABLE
    9,  // TEXTUREFILTER
    10, // SRCBLEND
    11, // DESTBLEND
    12, // VERTEXALPHAENABLE
    13, // BORDERCOLOR
    14, // FOGENABLE
    20, // CULLMODE
    29, // ALPHATESTFUNCTION
    30, // ALPHATESTFUNCTIONREF
};

class RenderStateGuard {
public:
    RenderStateGuard() {
        const auto* globals = *reinterpret_cast<RwGlobalsPrefix* const*>(
            game::kRwEngineInstance);
        if (!globals)
            return;

        set_ = globals->device.renderStateSet;
        const auto get = globals->device.renderStateGet;
        if (!set_ || !get) {
            set_ = nullptr;
            return;
        }

        for (size_t i = 0; i < _countof(saved_); ++i) {
            auto& saved = saved_[i];
            saved.state = kAARenderStateIds[i];
            saved.valid = get(saved.state, &saved.value) != 0;
        }
    }

    ~RenderStateGuard() {
        if (!set_)
            return;

        for (size_t i = _countof(saved_); i != 0; --i) {
            const auto& saved = saved_[i - 1];
            if (saved.valid)
                set_(saved.state, saved.value);
        }
    }

    RenderStateGuard(const RenderStateGuard&) = delete;
    RenderStateGuard& operator=(const RenderStateGuard&) = delete;

private:
    RenderStateSetFn set_ = nullptr;
    SavedRenderState saved_[_countof(kAARenderStateIds)] = {};
};

using DrawHudFn = void (__cdecl*)();

// The copied prologue of CHud::DrawCrossHairs followed by a jump back into it.
// Calling this runs the original function without going through its entry, so
// it stays correct even though the entry now holds our branch.
DrawCrossHairsFn g_drawCrossHairsOriginal = nullptr;

// Set when the HUD is intercepted at its body because another plugin already
// owns the entry. The body has passed the disabled check by then, so resuming
// through it must not repeat that check.
DrawHudFn g_drawHudResume = nullptr;
using AddMessageJumpFn = void (__cdecl*)(const char*, uint32_t, uint16_t, bool);

struct Vec3 {
    float x;
    float y;
    float z;
};

using CalcScreenCoorsFn = bool (__cdecl*)(const Vec3&, Vec3*, float*, float*,
                                          bool, bool);

constexpr Color kBlack = {0, 0, 0, 255};

bool CalcScreenCoorsFor(const Vec3& input, Vec3* output, float* width,
                        float* height, bool checkMax, bool checkMin,
                        volatile LONG* enabled) {
    const bool visible = reinterpret_cast<CalcScreenCoorsFn>(
        game::kCalcScreenCoors)(input, output, width, height, checkMax,
                                checkMin);
    if (visible && width &&
        InterlockedCompareExchange(enabled, 0, 0) != 0)
        *width *= g_worldSpriteWidthCorrection;
    return visible;
}

#define DEFINE_SPRITE_WRAPPER(name, flag)                                      \
    bool __cdecl name(const Vec3& input, Vec3* output, float* width,           \
                      float* height, bool checkMax, bool checkMin) {            \
        return CalcScreenCoorsFor(input, output, width, height, checkMax,      \
                                  checkMin, &flag);                            \
    }

DEFINE_SPRITE_WRAPPER(CalcPickupSprite, g_spritePickups)
DEFINE_SPRITE_WRAPPER(CalcCoronaSprite, g_spriteCoronas)
DEFINE_SPRITE_WRAPPER(CalcCoronaReflectionSprite, g_spriteCoronaReflections)
DEFINE_SPRITE_WRAPPER(CalcSunMoonSprite, g_spriteSunMoon)
DEFINE_SPRITE_WRAPPER(CalcPointLightSprite, g_spritePointLights)
DEFINE_SPRITE_WRAPPER(CalcBirdSprite, g_spriteBirds)
DEFINE_SPRITE_WRAPPER(CalcCloudSprite, g_spriteClouds)
DEFINE_SPRITE_WRAPPER(CalcCheckpointSprite, g_spriteCheckpoints)
DEFINE_SPRITE_WRAPPER(CalcWeaponEffectSprite, g_spriteWeaponEffects)
DEFINE_SPRITE_WRAPPER(CalcCameraEffectSprite, g_spriteCameraEffects)
DEFINE_SPRITE_WRAPPER(CalcTargetingSprite, g_spriteTargeting)

#undef DEFINE_SPRITE_WRAPPER

// CTextDraw::Draw is thiscall with no stack arguments, which a fastcall
// function with an unused second parameter receives correctly: `this` in
// ecx, garbage in edx, nothing to clean up.
using TextDrawDrawFn = void (__fastcall*)(void*, void*);
using TextDrawSpriteFn = void (__cdecl*)(void*, const Rect*, const void*);
using TextDrawSetWrapxFn = void (__cdecl*)(float);
using TextDrawPrintStringFn = void (__cdecl*)(float, float, const char*);

void PublishTextdrawLayout(int32_t width, float offset) {
    const TextdrawLayout layout = {width, offset};
    LONGLONG packed = 0;
    std::memcpy(&packed, &layout, sizeof(packed));
    InterlockedExchange64(&g_textdrawLayoutPacked, packed);
}

// Takes the layout for this draw, runs the original against it and shifts
// the click rectangle it stored, so a selectable textdraw is hit where it is
// drawn. The rectangle is ints; the shift is rounded the same way the draw
// rounds the positions it is derived from.
void __fastcall TextDrawDrawHook(uint8_t* textdraw, void*) {
    const LONGLONG packed =
        InterlockedCompareExchange64(&g_textdrawLayoutPacked, 0, 0);
    TextdrawLayout layout;
    std::memcpy(&layout, &packed, sizeof(layout));
    if (layout.width <= 0) {
        layout.width = *reinterpret_cast<const int32_t*>(game::kScreenWidth);
        layout.offset = 0.0f;
    }
    g_textdrawWidth = layout.width;
    g_textdrawOffset = layout.offset;
    const float screenWidth = static_cast<float>(
        *reinterpret_cast<const int32_t*>(game::kScreenWidth));
    g_textdrawScale = screenWidth > 0.0f
        ? static_cast<float>(layout.width) / screenWidth
        : 1.0f;

    g_textdrawDrawing = true;
    reinterpret_cast<TextDrawDrawFn>(g_sampDraw)(
        textdraw, nullptr);
    g_textdrawDrawing = false;

    if (layout.offset == 0.0f)
        return;

    const auto shift = static_cast<int32_t>(std::lround(layout.offset));
    int32_t edge = 0;
    std::memcpy(&edge, textdraw + samp::kClickLeftOffset, sizeof(edge));
    edge += shift;
    std::memcpy(textdraw + samp::kClickLeftOffset, &edge, sizeof(edge));
    std::memcpy(&edge, textdraw + samp::kClickRightOffset, sizeof(edge));
    edge += shift;
    std::memcpy(textdraw + samp::kClickRightOffset, &edge, sizeof(edge));
}

// The three forwarders below stand in for the calls that carry an X
// position out of CTextDraw::Draw. Each shifts that position and hands the
// call on to the function samp.dll was calling.
void __cdecl TextDrawSpriteHook(void* sprite, const Rect* rect,
                                const void* color) {
    Rect shifted = *rect;
    shifted.left += g_textdrawOffset;
    shifted.right += g_textdrawOffset;
    reinterpret_cast<TextDrawSpriteFn>(g_sampSpriteForward)(sprite, &shifted,
                                                          color);
}

void __cdecl TextDrawSetWrapxHook(float wrap) {
    reinterpret_cast<TextDrawSetWrapxFn>(g_sampWrapxForward)(
        wrap + g_textdrawOffset);
}

// The print is where the font reads the real width, so the layout's factor is
// published to the outline pass and to the box hook for exactly its duration.
void __cdecl TextDrawPrintStringHook(float x, float y, const char* text) {
    g_fontStretchX = kStockStretchX * g_textdrawScale;
    g_textdrawPrinting = true;
    reinterpret_cast<TextDrawPrintStringFn>(g_sampPrintForward)(
        x + g_textdrawOffset, y, text);
    g_textdrawPrinting = false;
    g_fontStretchX = g_textStretchX;
}

using HudPassFn = void (__cdecl*)();

// The block's text is printed and its bars are outlined inside these two
// passes, so the font's outline offsets and the bar outline follow the
// block's factor for exactly their duration and nothing else's.
void RunHudPass(uintptr_t pass) {
    g_hudPassDrawing = true;
    g_fontStretchX = g_playerInfoPassStretchX;
    g_hudPassStretchX = g_playerInfoPassStretchX;
    g_hudPassStretchY = g_playerInfoStretchY;
    reinterpret_cast<HudPassFn>(pass)();
    g_hudPassStretchY = kStockStretchY;
    g_hudPassStretchX = kStockStretchX;
    g_fontStretchX = g_textStretchX;
    g_hudPassDrawing = false;
}

using SetScaleFn = void (__cdecl*)(float, float);

// The copied prologues of the two scale setters, each followed by a jump
// back into its function.
SetScaleFn g_setScaleOriginal = nullptr;
SetScaleFn g_setScaleLangOriginal = nullptr;

// The ratio is 1.0 while the module is off or not in, so there is no switch
// to publish separately from it: glyphs and outline read values that one
// function writes together.
float ScaledTextWidth(float width) {
    if (!g_textdrawDrawing && !g_hudPassDrawing)
        width *= g_textScale;
    return width;
}

void __cdecl FontSetScaleHook(float width, float height) {
    g_setScaleOriginal(ScaledTextWidth(width), height);
}

void __cdecl FontSetScaleLangHook(float width, float height) {
    g_setScaleLangOriginal(ScaledTextWidth(width), height);
}

void __cdecl DrawPlayerInfoHook() {
    RunHudPass(game::kDrawPlayerInfo);
}

void __cdecl DrawWantedLevelHook() {
    RunHudPass(game::kDrawWantedLevel);
}

using GetTextRectFn = void (__cdecl*)(Rect*, float, float, const char*);

// Runs in place of the GetTextRect call inside CFont::PrintString. The rect
// it returns is the textdraw's box: the text's extent padded on each side,
// by 4 pixels in the stock game and by 4 units of the real screen size under
// SilentPatch. The vertical padding is what a 16:9 display of this height
// has as well. A horizontal one that equals the stretched stock value came
// from the real width, so it is re-stretched by the layout width; anything
// else, the stock 4 pixels included, is left as it is.
void __cdecl FontGetTextRectHook(Rect* rect, float x, float y,
                                 const char* text) {
    reinterpret_cast<GetTextRectFn>(game::kFontGetTextRect)(rect, x, y, text);
    if (!g_textdrawPrinting || g_textdrawScale == 1.0f)
        return;

    const float screenWidth = static_cast<float>(
        *reinterpret_cast<const int32_t*>(game::kScreenWidth));
    const float stretched =
        game::kFontBoxPadding * screenWidth / game::kDesignWidth;
    const float fitted = stretched * g_textdrawScale;
    constexpr float kTolerance = 0.5f;

    float leftPadding = 0.0f;
    float rightPadding = 0.0f;
    bool hasLeft = true;
    if (*reinterpret_cast<const bool*>(game::kFontCentre)) {
        const float half =
            *reinterpret_cast<const float*>(game::kFontCentreSize) * 0.5f;
        leftPadding = x - half - rect->left;
        rightPadding = rect->right - x - half;
    } else if (*reinterpret_cast<const bool*>(game::kFontRightJustify)) {
        // The left edge comes from a wrap SA-MP never sets, so only the
        // right one is the textdraw's.
        hasLeft = false;
        rightPadding = rect->right - x;
    } else {
        leftPadding = x - rect->left;
        rightPadding =
            rect->right - *reinterpret_cast<const float*>(game::kFontWrapX);
    }

    if (hasLeft && std::fabs(leftPadding - stretched) < kTolerance)
        rect->left += leftPadding - fitted;
    if (std::fabs(rightPadding - stretched) < kTolerance)
        rect->right -= rightPadding - fitted;
}

void __cdecl SetFovHook(float fov) {
    if (InterlockedCompareExchange(&g_fixFov, 0, 0) != 0) {
        const float aspect = *reinterpret_cast<const float*>(
            game::kAspectRatio);
        constexpr float kPi = 3.14159265358979323846f;
        const float radians = fov * kPi / 180.0f;
        fov = 2.0f * std::atan(std::tan(radians * 0.5f) *
                              (aspect / (4.0f / 3.0f))) * 180.0f / kPi;
    }
    *reinterpret_cast<float*>(game::kFov) = fov;
}

void __cdecl CalculateAspectRatioHook() {
    const float width = static_cast<float>(
        *reinterpret_cast<const int32_t*>(game::kScreenWidth));
    const float height = static_cast<float>(
        *reinterpret_cast<const int32_t*>(game::kScreenHeight));
    if (width > 0.0f && height > 0.0f) {
        *reinterpret_cast<float*>(game::kAspectRatio) =
            InterlockedCompareExchange(&g_useScreenAspect, 0, 0) != 0
                ? width / height
                : 4.0f / 3.0f;
    }
}

void DrawRect(const Rect& rect) {
    reinterpret_cast<DrawRectFn>(game::kDrawRect)(rect, kBlack);
}

void DefinedState2d() {
    reinterpret_cast<DefinedState2dFn>(game::kDefinedState2d)();
}

int16_t GetCameraMode() {
    const auto* camera = reinterpret_cast<const uint8_t*>(game::kCamera);
    const uint8_t active = camera[game::kCameraActiveIndexOffset];
    if (active >= 3)
        return -1;

    int16_t mode = 0;
    std::memcpy(&mode,
                camera + game::kCameraArrayOffset +
                    active * game::kCameraSize + game::kCameraModeOffset,
                sizeof(mode));
    return mode;
}

bool IsSniperCamera() {
    return GetCameraMode() == game::kCameraModeSniper;
}

// The corrected sniper texture occupies center +/- SCREEN_STRETCH_X(210).
// Vanilla's outer filler still ends at the old, wider bounds. Extending black
// from the screen edges to the corrected bounds closes those two gaps. One
// pixel of overlap prevents fractional coordinates/MSAA from opening a seam.
void DrawSniperSideFill() {
    if (!IsSniperCamera())
        return;

    const float width = static_cast<float>(
        *reinterpret_cast<const int32_t*>(game::kScreenWidth));
    const float height = static_cast<float>(
        *reinterpret_cast<const int32_t*>(game::kScreenHeight));
    // The fill has to follow the scope's own factor, not the reticle's. They
    // are equal while both corrections are on, but roundScope stands alone, so
    // reading the reticle's here would size the fill for a scope that is not
    // on screen.
    const float halfScope = 210.0f * width * g_scopeStretchX;
    const float center = width * 0.5f;
    constexpr float kOutside = 5.0f;
    constexpr float kOverlap = 1.0f;

    DrawRect({-kOutside, -kOutside, center - halfScope + kOverlap,
              height + kOutside});
    DrawRect({center + halfScope - kOverlap, -kOutside, width + kOutside,
              height + kOutside});
}

void DrawCrossHairsHook() {
    if (GetCameraMode() == game::kCameraModeCamera &&
        InterlockedCompareExchange(&g_noCameraCrosshair, 0, 0) != 0)
        return;

    g_drawCrossHairsOriginal();

    if (InterlockedCompareExchange(&g_drawSniperFill, 0, 0) != 0)
        DrawSniperSideFill();
}

void DrawOriginalHud() {
    // When the hook sits at the body, the disabled check has already run and
    // the trampoline resumes one instruction in, so neither may be repeated.
    if (g_drawHudResume) {
        g_drawHudResume();
        return;
    }

    // This reproduces the seven-byte instruction and branch replaced at the
    // CHud::Draw entry. The body starts after both, so it does not recurse into
    // DrawHudHook and its eventual RET returns normally to this helper.
    if (*reinterpret_cast<const uint8_t*>(game::kHudDisabled) == 1)
        return;

    reinterpret_cast<DrawHudFn>(game::kDrawHudBody)();
}

void ShowPendingReloadNotification() {
    if (InterlockedExchange(&g_reloadNotificationPending, 0) == 0)
        return;

    reinterpret_cast<AddMessageJumpFn>(game::kAddMessageJump)(
        "~w~Aspect Ratio Fixes: configuration reloaded", 2500, 0, false);
}

// The message buffer is written by the worker on a key press and read by the
// game thread on the next frame. AddMessageJump copies the string, and a torn
// read could only garble one diagnostic message, so the interlocked flag is
// the only synchronisation this needs.
void ShowPendingProbeNotification() {
    if (InterlockedExchange(&g_probeNotificationPending, 0) == 0)
        return;

    reinterpret_cast<AddMessageJumpFn>(game::kAddMessageJump)(
        g_probeMessage, 3000, 0, false);
}

void DrawHudHook() {
    // The reload hotkey is polled by the worker. GTA message queues are owned
    // by the game thread, so the worker only publishes this one-shot request.
    ShowPendingReloadNotification();
    ShowPendingProbeNotification();

    // Preserve the game's clean camera capture frame.
    if (*reinterpret_cast<const bool*>(game::kTakePhoto)) {
        DrawOriginalHud();
        return;
    }

    const int16_t mode = GetCameraMode();
    const bool hideHud =
        (mode == game::kCameraModeCamera &&
         InterlockedCompareExchange(&g_hideCameraHud, 0, 0) != 0) ||
        (mode == game::kCameraModeSniper &&
         InterlockedCompareExchange(&g_hideSniperHud, 0, 0) != 0);

    if (hideHud) {
        DrawCrossHairsHook();
        return;
    }

    DrawOriginalHud();
}

// Rendered at the same tail point used by Widescreen Fix's HideAABug=2.
// Coordinates deliberately extend beyond the surface: at one pixel they
// cover, after rasterisation, exactly the four one-pixel edge samples
// affected by MSAA. Each side has its own thickness, a thicker side grows
// inward from that same edge, and a frame with no side left is skipped
// entirely so the 2D state is not touched for nothing.
void HideAABugHook() {
    const float left = static_cast<float>(
        InterlockedCompareExchange(&g_aaEdgeLeft, 0, 0));
    const float top = static_cast<float>(
        InterlockedCompareExchange(&g_aaEdgeTop, 0, 0));
    const float right = static_cast<float>(
        InterlockedCompareExchange(&g_aaEdgeRight, 0, 0));
    const float bottom = static_cast<float>(
        InterlockedCompareExchange(&g_aaEdgeBottom, 0, 0));
    if (left <= 0.0f && top <= 0.0f && right <= 0.0f && bottom <= 0.0f)
        return;

    const float width = static_cast<float>(
        *reinterpret_cast<const int32_t*>(game::kScreenWidth));
    const float height = static_cast<float>(
        *reinterpret_cast<const int32_t*>(game::kScreenHeight));

    // DefinedState2d disables depth testing/writes and changes a dozen other
    // persistent RenderWare states. This hook runs at the very end of the 2D
    // pass, so leaking those values makes some transparent world effects in
    // the next frame (notably birds and skidmarks) render through geometry.
    // Restore the exact incoming state after drawing the edge frame.
    const RenderStateGuard stateGuard;
    DefinedState2d();
    if (top > 0.0f)
        DrawRect({0.0f, -5.0f, width, top - 0.5f});
    if (left > 0.0f)
        DrawRect({-5.0f, -1.0f, left - 0.5f, height});
    if (bottom > 0.0f)
        DrawRect({0.0f, height - bottom - 0.5f, width, height + 5.0f});
    if (right > 0.0f)
        DrawRect({width - right, 0.0f, width + 5.0f, height + 5.0f});
}

struct Resolution {
    int32_t width = 0;
    int32_t height = 0;

    bool operator==(const Resolution& other) const {
        return width == other.width && height == other.height;
    }
};

bool GetResolution(Resolution& resolution) {
    Resolution current;
    if (!patch::ReadInt32(game::kScreenWidth, current.width))
        return false;
    if (!patch::ReadInt32(game::kScreenHeight, current.height))
        return false;

    if (current.width < kMinScreenSize || current.width > kMaxScreenSize)
        return false;
    if (current.height < kMinScreenSize || current.height > kMaxScreenSize)
        return false;

    resolution = current;
    return true;
}

bool HoldsStockValue(uintptr_t address, float expected) {
    float value = 0.0f;
    return patch::ReadFloat(address, value) && value == expected;
}

bool IsSupportedExecutable() {
    return HoldsStockValue(game::kStretchX, kStockStretchX) &&
           HoldsStockValue(game::kStretchY, kStockStretchY) &&
           HoldsStockValue(game::kRadarLeft, kStockRadarLeft) &&
           HoldsStockValue(game::kRadarTop, kStockRadarTop) &&
           HoldsStockValue(game::kRadarHigh, kStockRadarHigh) &&
           HoldsStockValue(game::kRadarWide, kStockRadarWide);
}

// Several options are only meaningful in combination with another one, and a
// combination that cancels itself out is indistinguishable from a patch that
// failed to apply: the game simply looks unchanged. Each of these is therefore
// named in the log rather than left for the user to work out from the source.
void LogSettingConflicts() {
    if (g_settings.fixFov && !g_settings.useScreenAspect) {
        logging::Write("  note: fixFov has no effect while useScreenAspect=0. "
                       "The conversion scales by aspect / (4/3), and "
                       "useScreenAspect=0 holds that aspect at 4:3, so it "
                       "returns the angle unchanged");
    }

    if (!g_settings.roundRadar &&
        (g_settings.radarDiameter != kStockRadarHigh ||
         g_settings.radarMarginLeft != kStockRadarLeft ||
         g_settings.radarMarginBottom != kStockRadarTop - kStockRadarHigh)) {
        logging::Write("  note: diameter and margins are ignored while "
                       "roundRadar=0. They are HUD units of screen height, "
                       "which only describe the radar once its axes share one "
                       "scale");
    }

}

// Lays SA-MP textdraws out in a centred area of the configured aspect when the
// screen is wider than that, and leaves them at the screen's own width
// otherwise. A 16:9 screen is not wider than 16:9, so the plugin changes
// nothing there; the small tolerance keeps 1920x1080, which is 1.7777 against
// a 1.7778 setting, on that side of the line.
void UpdateTextdrawLayout(float screenWidth, float screenHeight) {
    constexpr float kAspectTolerance = 0.001f;
    const float aspect = screenWidth / screenHeight;
    const float layoutAspect = g_settings.textdrawAspect;

    int32_t width = static_cast<int32_t>(screenWidth);
    float offset = 0.0f;
    const bool fitted = g_settings.fitTextdraws &&
                        aspect > layoutAspect + kAspectTolerance;
    if (fitted) {
        width = static_cast<int32_t>(std::lround(screenHeight * layoutAspect));
        offset = (screenWidth - static_cast<float>(width)) * 0.5f;
    }
    PublishTextdrawLayout(width, offset);

    if (!g_textdrawsPatched)
        return;
    if (fitted) {
        logging::Write("  textdraws: laid out %d px wide, %.1f px from the "
                       "left (screen is wider than %.4f)",
                       width, static_cast<double>(offset),
                       static_cast<double>(layoutAspect));
    } else if (g_settings.fitTextdraws) {
        logging::Write("  textdraws: screen is not wider than %.4f, unchanged",
                       static_cast<double>(layoutAspect));
    } else {
        logging::Write("  textdraws: unchanged (fitTextdraws=0)");
    }
}

// Every other piece of text: the width its caller stretched by the real
// factor is multiplied by the ratio of the wanted pixels per unit to it. The
// outline pass reads the same ratio, so both stay stock until the scale
// hooks are in; a scaled outline on unscaled glyphs is worse than neither.
//
// The ambient outline factor is written here, from the worker. A reload that
// lands while the render thread is inside a HUD pass or a textdraw print
// overrides that pass's factor for its remainder; the pass restores the
// ambient value when it ends, so the effect is confined to that one frame.
void UpdateTextLayout(float screenWidth, float screenHeight) {
    const float pixelsPerUnitX = screenWidth / game::kDesignWidth;
    float textPixelsX = pixelsPerUnitX;
    if (g_textPatched && g_settings.fixText && g_settings.textAspect > 0.0f)
        textPixelsX = screenHeight * g_settings.textAspect / game::kDesignWidth;
    g_textScale = textPixelsX / pixelsPerUnitX;
    g_textStretchX = g_textScale * kStockStretchX;
    g_fontStretchX = g_textStretchX;

    // Logged from here rather than with the rest of the geometry, so the
    // figure printed at startup is the one computed after the hooks went in.
    if (!g_textPatched)
        return;
    logging::Write("  text: %.4f px per unit wide (game default %.4f)",
                   static_cast<double>(textPixelsX),
                   static_cast<double>(pixelsPerUnitX));
}

// The player info block is described by pixels per unit on each axis and the
// width its positions are measured back from. The aspect chooses the
// horizontal pixels per unit the way a display of that aspect and this height
// would, the scale multiplies both axes, and the margin moves the anchor so
// the money's right edge, 32 units in from the anchor, lands where asked. Each
// of the three keeps the game's own value at zero.
void UpdatePlayerInfoLayout(float screenWidth, float screenHeight) {
    float pixelsX = screenWidth / game::kDesignWidth;
    float pixelsY = screenHeight / game::kDesignHeight;
    float anchor = screenWidth;

    if (g_settings.fixPlayerInfo) {
        if (g_settings.playerInfoAspect > 0.0f)
            pixelsX = screenHeight * g_settings.playerInfoAspect / game::kDesignWidth;
        if (g_settings.playerInfoScale > 0) {
            const float scale = static_cast<float>(g_settings.playerInfoScale) / 100.0f;
            pixelsX *= scale;
            pixelsY *= scale;
        }
        if (g_settings.playerInfoMarginRight > 0.0f) {
            constexpr float kMoneyMargin = 32.0f;
            const float wanted = g_settings.playerInfoMarginRight *
                                 screenHeight / game::kDesignHeight;
            anchor = screenWidth - (wanted - kMoneyMargin * pixelsX);
        }
    }

    // The anchor is read through an integer load, and the block is measured
    // back from it in whole pixels either way.
    const auto width = static_cast<int32_t>(std::lround(anchor));
    if (width < kMinScreenSize)
        return;

    g_playerInfoStretchX = pixelsX / static_cast<float>(width);
    g_playerInfoStretchY = pixelsY / screenHeight;
    g_playerInfoPassStretchX = pixelsX / screenWidth;
    g_weaponIconMargin = g_weaponIconMarginStock * game::kDesignWidth * pixelsX /
                         static_cast<float>(width);
    g_playerInfoWidth = width;
}

// Recomputes every plugin owned value for the current resolution. This runs on
// every resolution change, so the geometry is correct in windowed mode, after a
// video settings change and on any display the game can open.
void UpdateGeometry(const Resolution& resolution) {
    const auto screenWidth = static_cast<float>(resolution.width);
    const auto screenHeight = static_cast<float>(resolution.height);

    // Pixels covered by one HUD unit on each axis, as the game computes them.
    const float pixelsPerUnitX = screenWidth / game::kDesignWidth;
    const float pixelsPerUnitY = screenHeight / game::kDesignHeight;

    const float squareStretch =
        screenHeight / (game::kDesignHeight * screenWidth);
    g_squareStretch = squareStretch;
    g_worldSpriteWidthCorrection = screenHeight / screenWidth;

    if (g_settings.useScreenAspect)
        *reinterpret_cast<float*>(game::kAspectRatio) = screenWidth / screenHeight;

    // The frame and the blip icons are corrected independently. Blip positions
    // come from the frame's transform, so a corrected frame carries them even
    // while the icons themselves keep the game's own proportions, and rounded
    // icons sit correctly inside a vanilla ellipse.
    //
    // Patched operands keep pointing at our variables after a hot reload.
    // Restoring their stock values disables a group without rewriting
    // executable code from the polling thread.
    if (g_settings.roundRadar) {
        // SCREEN_STRETCH_X(a) evaluates to a * screenWidth * factor. Making
        // that a * screenHeight / 448 gives the radar one scale on both axes.
        g_radarStretchX = squareStretch;
        // The diameter and the margins are HUD units of screen height, which
        // only describe the radar once its two axes share one scale. They stay
        // stock while the frame does.
        g_radarWide = g_settings.radarDiameter;
        g_radarHigh = g_settings.radarDiameter;
        g_radarLeft = g_settings.radarMarginLeft;
        g_radarTop =
            g_settings.radarMarginBottom + g_settings.radarDiameter;
    } else {
        g_radarStretchX = kStockStretchX;
        g_radarWide = kStockRadarWide;
        g_radarHigh = kStockRadarHigh;
        g_radarLeft = kStockRadarLeft;
        g_radarTop = kStockRadarTop;
    }

    g_blipStretchX = g_settings.roundBlips ? squareStretch : kStockStretchX;

    g_crosshairStretchX =
        g_settings.roundCrosshair ? squareStretch : kStockStretchX;

    // The scope replaces the reticle rather than sharing the screen with it,
    // so the two are corrected independently.
    const bool correctScope = g_settings.roundScope;
    g_scopeStretchX = correctScope ? squareStretch : kStockStretchX;
    g_viewfinderWidth = correctScope ? game::kViewfinderHeight : 256.0f;

    UpdatePlayerInfoLayout(screenWidth, screenHeight);

    UpdateTextLayout(screenWidth, screenHeight);

    // One unit of the lock-on marker's width covers screenWidth / 640 pixels
    // while one unit of its height covers screenHeight / 448, so the minimum
    // width has to be divided by the ratio of the two to clamp both axes to the
    // same number of pixels.
    g_lockOnMinWidth = g_settings.roundCrosshair
        ? kLockOnMinHeight * (game::kDesignWidth * screenHeight) /
              (game::kDesignHeight * screenWidth)
        : 28.0f;

    const float unitPixels = screenHeight / game::kDesignHeight;

    logging::Write("resolution %dx%d, aspect %.4f",
                   resolution.width, resolution.height,
                   static_cast<double>(screenWidth / screenHeight));
    logging::Write("  one HUD unit: %.4f px wide, %.4f px tall (game default)",
                   static_cast<double>(pixelsPerUnitX),
                   static_cast<double>(pixelsPerUnitY));
    logging::Write("  inside the radar: %.4f px on both axes",
                   static_cast<double>(unitPixels));
    // Reported from the live variables rather than from the settings, so a
    // group left at its stock factor is logged as what the game will actually
    // draw instead of as what the INI asked for.
    const float radarPixelsPerUnitX = screenWidth * g_radarStretchX;
    const float blipPixelsPerUnitX = screenWidth * g_blipStretchX;

    logging::Write("  radar: %.1f x %.1f px, %.1f px from the left, "
                   "%.1f px from the bottom",
                   static_cast<double>(g_radarWide * radarPixelsPerUnitX),
                   static_cast<double>(g_radarHigh * pixelsPerUnitY),
                   static_cast<double>(g_radarLeft * radarPixelsPerUnitX),
                   static_cast<double>((g_radarTop - g_radarHigh) *
                                       pixelsPerUnitY));
    logging::Write("  blip icon: %.1f x %.1f px (game default %.1f x %.1f)",
                   static_cast<double>(16.0f * blipPixelsPerUnitX),
                   static_cast<double>(16.0f * pixelsPerUnitY),
                   static_cast<double>(16.0f * pixelsPerUnitX),
                   static_cast<double>(16.0f * pixelsPerUnitY));
    logging::Write("  crosshair scale: %.4f px per unit (was %.4f)",
                   static_cast<double>(screenWidth * g_crosshairStretchX),
                   static_cast<double>(pixelsPerUnitX));
    logging::Write("  lock-on minimum width: %.4f units against %.1f tall",
                   static_cast<double>(g_lockOnMinWidth),
                   static_cast<double>(kLockOnMinHeight));
    logging::Write("  player info: %.4f x %.4f px per unit (game default %.4f x "
                   "%.4f), right edge %.1f px in",
                   static_cast<double>(static_cast<float>(g_playerInfoWidth) *
                                       g_playerInfoStretchX),
                   static_cast<double>(screenHeight * g_playerInfoStretchY),
                   static_cast<double>(pixelsPerUnitX),
                   static_cast<double>(pixelsPerUnitY),
                   static_cast<double>(screenWidth -
                                       static_cast<float>(g_playerInfoWidth) +
                                       32.0f * static_cast<float>(g_playerInfoWidth) *
                                           g_playerInfoStretchX));

    UpdateTextdrawLayout(screenWidth, screenHeight);

    LogSettingConflicts();
}

// The probe is diagnostic and is off unless the INI asks for it. It repoints a
// whole candidate group, then corrects one site at a time so the element that
// moves can be identified before any of it becomes a module. Nothing is
// rewritten while stepping: the selection only changes which slot holds the
// corrected factor.
void ApplyProbe() {
    if (!g_settings.probeEnabled)
        return;

    if (g_settings.probeGroup == 1) {
        g_probeSites = game::kProbeGroupB;
        g_probeCount = sizeof(game::kProbeGroupB) / sizeof(uintptr_t);
    } else {
        g_probeSites = game::kProbeGroupA;
        g_probeCount = sizeof(game::kProbeGroupA) / sizeof(uintptr_t);
    }

    if (g_probeCount > game::kMaxProbeSites) {
        logging::Write("probe: group %d has more sites than slots",
                       g_settings.probeGroup);
        g_probeCount = 0;
        return;
    }

    for (size_t i = 0; i < g_probeCount; ++i)
        g_probeStretch[i] = kStockStretchX;

    // Verified as a whole first, so a group that does not match the expected
    // encoding leaves the executable untouched rather than half repointed.
    if (!patch::VerifyOperands(g_probeSites, g_probeCount, game::kStretchX)) {
        logging::Write("probe: group %d has unexpected bytes, not applied",
                       g_settings.probeGroup);
        g_probeCount = 0;
        return;
    }

    for (size_t i = 0; i < g_probeCount; ++i) {
        if (!patch::RepointOperands(g_probeSites + i, 1, game::kStretchX,
                                    &g_probeStretch[i])) {
            logging::Write("probe: site %u failed to repoint",
                           static_cast<unsigned>(i));
            g_probeCount = 0;
            return;
        }
    }

    logging::Write("probe                  patched group %d (%u sites), "
                   "step with the probe hotkey",
                   g_settings.probeGroup,
                   static_cast<unsigned>(g_probeCount));
    for (size_t i = 0; i < g_probeCount; ++i)
        logging::Write("  probe %2u  0x%08X", static_cast<unsigned>(i + 1),
                       static_cast<unsigned>(g_probeSites[i]));
}

// Publishes the current selection into the slots and describes it both in the
// log and on screen, so stepping does not require leaving the game.
void UpdateProbeSelection() {
    if (g_probeCount == 0)
        return;

    for (size_t i = 0; i < g_probeCount; ++i) {
        const bool corrected =
            g_probeIndex == static_cast<int>(g_probeCount) ||
            g_probeIndex == static_cast<int>(i);
        g_probeStretch[i] = corrected ? g_squareStretch : kStockStretchX;
    }

    if (g_probeIndex < 0) {
        std::snprintf(g_probeMessage, sizeof(g_probeMessage),
                      "~w~Probe: none, group %d", g_settings.probeGroup);
        logging::Write("probe: none");
    } else if (g_probeIndex == static_cast<int>(g_probeCount)) {
        std::snprintf(g_probeMessage, sizeof(g_probeMessage),
                      "~w~Probe: all %u sites, group %d",
                      static_cast<unsigned>(g_probeCount),
                      g_settings.probeGroup);
        logging::Write("probe: all %u sites",
                       static_cast<unsigned>(g_probeCount));
    } else {
        std::snprintf(g_probeMessage, sizeof(g_probeMessage),
                      "~w~Probe %u/%u  0x%08X",
                      static_cast<unsigned>(g_probeIndex + 1),
                      static_cast<unsigned>(g_probeCount),
                      static_cast<unsigned>(g_probeSites[g_probeIndex]));
        logging::Write("probe: %u/%u at 0x%08X",
                       static_cast<unsigned>(g_probeIndex + 1),
                       static_cast<unsigned>(g_probeCount),
                       static_cast<unsigned>(g_probeSites[g_probeIndex]));
    }

    InterlockedExchange(&g_probeNotificationPending, 1);
}

void RestoreRadarMask() {
    if (!patch::IsReadable(game::kDrawRadarMask,
                           sizeof(game::kDrawRadarMaskPrologue))) {
        logging::Write("radar mask: CRadar::DrawRadarMask is not readable");
        return;
    }

    const auto* code = reinterpret_cast<const uint8_t*>(game::kDrawRadarMask);
    bool intact = true;
    for (size_t i = 0; i < sizeof(game::kDrawRadarMaskPrologue); ++i) {
        if (code[i] != game::kDrawRadarMaskPrologue[i]) {
            intact = false;
            break;
        }
    }

    if (intact) {
        logging::Write("radar mask: already intact");
        return;
    }

    const bool restored =
        patch::WriteMemory(game::kDrawRadarMask, game::kDrawRadarMaskPrologue,
                           sizeof(game::kDrawRadarMaskPrologue));
    logging::Write("radar mask: %s",
                   restored ? "restored the circular mask"
                            : "failed to restore the circular mask");
}

// Another modification can repoint the same instructions after this plugin has
// already done so, which would undo a group without leaving any trace in the
// log. Every group that applies is therefore remembered and re-read by the
// watcher, so a group that stops pointing at us is reported once.
//
// A site's 32 bit operand is either an absolute address or, for a call, a
// displacement from the next instruction; both resolve to the address the
// site points at, which is what is compared.
struct PatchedGroup {
    const char* name;
    const uintptr_t* sites;
    size_t count;
    size_t operandOffset;
    bool relative;
    uintptr_t target;
    bool reported;
};

constexpr size_t kMaxGuards = 32;
PatchedGroup g_guards[kMaxGuards] = {};
size_t g_guardCount = 0;

// The operand of an x87 instruction with an absolute address starts two bytes
// into it, which is what RepointOperands writes. `mov eax, [disp32]` has no
// ModR/M byte, so its operand starts one byte in, and so does the
// displacement of a relative call.
constexpr size_t kOperandOffset = 2;
constexpr size_t kMovEaxOperandOffset = 1;
constexpr size_t kCallOperandOffset = 1;
constexpr size_t kCallLength = 5;

void RememberGuard(const char* name, const uintptr_t* sites, size_t count,
                   size_t operandOffset, bool relative, const void* target) {
    if (g_guardCount >= kMaxGuards)
        return;

    PatchedGroup& guard = g_guards[g_guardCount++];
    guard.name = name;
    guard.sites = sites;
    guard.count = count;
    guard.operandOffset = operandOffset;
    guard.relative = relative;
    guard.target = reinterpret_cast<uintptr_t>(target);
    guard.reported = false;
}

template <size_t N>
bool ApplyGroup(const char* name, const uintptr_t (&sites)[N],
                uintptr_t expected, const void* target) {
    const bool applied = patch::RepointOperands(sites, expected, target);
    logging::Write("%-22s %s (%u sites)", name,
                   applied ? "patched" : "SKIPPED, unexpected bytes",
                   static_cast<unsigned>(N));

    if (applied)
        RememberGuard(name, sites, N, kOperandOffset, false, target);
    return applied;
}

void CheckGuards() {
    for (size_t g = 0; g < g_guardCount; ++g) {
        PatchedGroup& guard = g_guards[g];
        if (guard.reported)
            continue;

        for (size_t i = 0; i < guard.count; ++i) {
            int32_t operand = 0;
            if (!patch::ReadInt32(guard.sites[i] + guard.operandOffset,
                                  operand))
                continue;

            const uintptr_t current =
                guard.relative
                    ? guard.sites[i] + kCallLength +
                          static_cast<uintptr_t>(operand)
                    : static_cast<uintptr_t>(static_cast<uint32_t>(operand));
            if (current == guard.target)
                continue;

            logging::Write("%-22s site %08X now points at %08X instead of %08X, "
                           "another modification has overwritten it",
                           guard.name, static_cast<unsigned>(guard.sites[i]),
                           static_cast<unsigned>(current),
                           static_cast<unsigned>(guard.target));
            guard.reported = true;
            break;
        }
    }
}

bool WriteRelativeBranch(uintptr_t site, uint8_t opcode, const void* target) {
    const intptr_t displacement =
        reinterpret_cast<intptr_t>(target) - static_cast<intptr_t>(site + 5);
    if (displacement < INT32_MIN || displacement > INT32_MAX)
        return false;

    uint8_t branch[5] = {opcode, 0, 0, 0, 0};
    const auto relative = static_cast<int32_t>(displacement);
    std::memcpy(branch + 1, &relative, sizeof(relative));
    return patch::WriteMemory(site, branch, sizeof(branch));
}

bool RelativeCallTargets(uintptr_t site, uintptr_t expected) {
    if (!patch::IsReadable(site, 5))
        return false;

    const auto* code = reinterpret_cast<const uint8_t*>(site);
    if (code[0] != 0xE8)
        return false;

    int32_t relative = 0;
    std::memcpy(&relative, code + 1, sizeof(relative));
    return site + 5 + relative == expected;
}

template <size_t N>
void ApplySpriteCallGroup(const char* name, const uintptr_t (&sites)[N],
                          uintptr_t expected, const void* wrapper) {
    for (size_t i = 0; i < N; ++i) {
        if (!RelativeCallTargets(sites[i], expected)) {
            logging::Write("%-22s SKIPPED, call %08X was changed",
                           name, static_cast<unsigned>(sites[i]));
            return;
        }
    }

    bool applied = true;
    for (size_t i = 0; i < N; ++i)
        applied &= WriteRelativeBranch(sites[i], 0xE8, wrapper);
    logging::Write("%-22s %s (%u calls)", name,
                   applied ? "patched" : "FAILED",
                   static_cast<unsigned>(N));
}

void ApplyWorldSprites() {
    if (!patch::IsReadable(game::kCalcScreenCoors, 1) ||
        *reinterpret_cast<const uint8_t*>(game::kCalcScreenCoors) == 0xE9) {
        logging::Write("world sprites          SKIPPED, projection is hooked");
        return;
    }

    ApplySpriteCallGroup("sprite pickups", game::kSpritePickupSites,
                         game::kCalcScreenCoors, CalcPickupSprite);
    ApplySpriteCallGroup("sprite coronas", game::kSpriteCoronaSites,
                         game::kCalcScreenCoors, CalcCoronaSprite);
    ApplySpriteCallGroup("sprite reflections",
                         game::kSpriteCoronaReflectionSites,
                         game::kCalcScreenCoors, CalcCoronaReflectionSprite);
    ApplySpriteCallGroup("sprite sun/moon", game::kSpriteSunMoonSites,
                         game::kCalcScreenCoors, CalcSunMoonSprite);
    ApplySpriteCallGroup("sprite point lights", game::kSpritePointLightSites,
                         game::kCalcScreenCoors, CalcPointLightSprite);
    ApplySpriteCallGroup("sprite birds", game::kSpriteBirdSites,
                         game::kCalcScreenCoors, CalcBirdSprite);
    ApplySpriteCallGroup("sprite clouds", game::kSpriteCloudSites,
                         game::kCalcScreenCoors, CalcCloudSprite);
    ApplySpriteCallGroup("sprite checkpoints", game::kSpriteCheckpointSites,
                         game::kCalcScreenCoors, CalcCheckpointSprite);
    ApplySpriteCallGroup("sprite weapon FX", game::kSpriteWeaponEffectSites,
                         game::kCalcScreenCoors, CalcWeaponEffectSprite);
    ApplySpriteCallGroup("sprite camera FX", game::kSpriteCameraEffectSites,
                         game::kCalcScreenCoors, CalcCameraEffectSprite);
    ApplySpriteCallGroup("sprite targeting", game::kSpriteTargetingSites,
                         game::kCalcScreenCoors, CalcTargetingSprite);
}

void ApplyFovFix() {
    if (patch::IsReadable(game::kSetFov, 1) &&
        *reinterpret_cast<const uint8_t*>(game::kSetFov) != 0xE9) {
        ApplySpriteCallGroup("widescreen FOV", game::kSetFovCallSites,
                             game::kSetFov,
                             reinterpret_cast<const void*>(SetFovHook));
    } else {
        logging::Write("widescreen FOV         SKIPPED, function is hooked");
    }

    if (patch::IsReadable(game::kCalculateAspectRatio, 1) &&
        *reinterpret_cast<const uint8_t*>(game::kCalculateAspectRatio) != 0xE9) {
        ApplySpriteCallGroup(
            "screen aspect", game::kCalculateAspectCallSites,
            game::kCalculateAspectRatio,
            reinterpret_cast<const void*>(CalculateAspectRatioHook));
    } else {
        logging::Write("screen aspect          SKIPPED, function is hooked");
    }
}


bool BytesMatch(uintptr_t address, const uint8_t* expected, size_t size) {
    return patch::IsReadable(address, size) &&
           std::memcmp(reinterpret_cast<const void*>(address), expected,
                       size) == 0;
}

// The client is identified by its PE link timestamp rather than by a version
// resource, because every 0.3.7 build reports 0.3.7 there.
uint32_t ModuleTimeDateStamp(uintptr_t base) {
    if (!patch::IsReadable(base, sizeof(IMAGE_DOS_HEADER)))
        return 0;
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0)
        return 0;

    const uintptr_t headers = base + static_cast<uintptr_t>(dos->e_lfanew);
    if (!patch::IsReadable(headers, sizeof(IMAGE_NT_HEADERS32)))
        return 0;
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>(headers);
    if (nt->Signature != IMAGE_NT_SIGNATURE)
        return 0;
    return nt->FileHeader.TimeDateStamp;
}

// Sites inside samp.dll, resolved against its base once it is found. They
// have static storage because the guards keep pointing at them.
uintptr_t g_sampDrawCallSite[1];
uintptr_t g_sampSpriteWidthSite[1];
uintptr_t g_sampTextWidthSite[1];
uintptr_t g_sampSpriteCallSite[1];
uintptr_t g_sampWrapxCallSite[1];
uintptr_t g_sampPrintCallSite[1];

// Every site is checked before the first one is written: the two width loads
// against their full encoding, the four calls against the targets samp.dll
// links them to. A call that another modification has already retargeted
// fails that check and leaves the whole group untouched.
void ApplySampTextdraws(const Resolution& resolution) {
    const HMODULE module = GetModuleHandleA("samp.dll");
    if (!module) {
        logging::Write("samp textdraws         SKIPPED, samp.dll is not "
                       "loaded");
        return;
    }

    const auto base = reinterpret_cast<uintptr_t>(module);
    const uint32_t stamp = ModuleTimeDateStamp(base);
    const samp::Build* build = nullptr;
    for (const samp::Build& candidate : samp::kBuilds) {
        if (candidate.timeDateStamp == stamp)
            build = &candidate;
    }
    if (!build) {
        logging::Write("samp textdraws         SKIPPED, samp.dll build %08X "
                       "is not mapped (0.3.7-R1 and 0.3.7-R3-1 are)",
                       static_cast<unsigned>(stamp));
        return;
    }

    g_sampDrawCallSite[0] = base + build->drawTextDrawCall;
    g_sampSpriteWidthSite[0] = base + build->spriteWidthRead;
    g_sampTextWidthSite[0] = base + build->textWidthRead;
    g_sampSpriteCallSite[0] = base + build->spriteDrawCall;
    g_sampWrapxCallSite[0] = base + build->setWrapxCall;
    g_sampPrintCallSite[0] = base + build->printStringCall;

    const bool intact =
        RelativeCallTargets(g_sampDrawCallSite[0],
                            base + build->textDrawDraw) &&
        BytesMatch(g_sampSpriteWidthSite[0], samp::kSpriteWidthReadBytes,
                   sizeof(samp::kSpriteWidthReadBytes)) &&
        BytesMatch(g_sampTextWidthSite[0], samp::kTextWidthReadBytes,
                   sizeof(samp::kTextWidthReadBytes)) &&
        RelativeCallTargets(g_sampSpriteCallSite[0],
                            base + build->spriteDrawForward) &&
        RelativeCallTargets(g_sampWrapxCallSite[0],
                            base + build->setWrapxForward) &&
        RelativeCallTargets(g_sampPrintCallSite[0],
                            base + build->printStringForward);
    if (!intact) {
        logging::Write("samp textdraws         SKIPPED, unexpected bytes (%s)",
                       build->name);
        return;
    }

    // The forwarders read their targets, the repointed loads read the width
    // and the draw hook reads the layout, so all of them are in place before
    // any branch can reach them. The width starts at the screen's own, which
    // is what the loads were reading, in case a draw reaches the function
    // without passing through the hook.
    g_sampDraw = base + build->textDrawDraw;
    g_sampSpriteForward = base + build->spriteDrawForward;
    g_sampWrapxForward = base + build->setWrapxForward;
    g_sampPrintForward = base + build->printStringForward;
    g_textdrawWidth = resolution.width;
    g_textdrawOffset = 0.0f;

    bool applied = true;
    applied &= patch::RepointOperands(g_sampSpriteWidthSite, game::kScreenWidth,
                                      &g_textdrawWidth);
    applied &= patch::RepointMovEaxOperand(g_sampTextWidthSite[0],
                                           game::kScreenWidth,
                                           &g_textdrawWidth);
    applied &= WriteRelativeBranch(g_sampSpriteCallSite[0], 0xE8,
                                   reinterpret_cast<const void*>(
                                       TextDrawSpriteHook));
    applied &= WriteRelativeBranch(g_sampWrapxCallSite[0], 0xE8,
                                   reinterpret_cast<const void*>(
                                       TextDrawSetWrapxHook));
    applied &= WriteRelativeBranch(g_sampPrintCallSite[0], 0xE8,
                                   reinterpret_cast<const void*>(
                                       TextDrawPrintStringHook));
    // The draw hook goes in last: everything it runs is already redirected.
    applied &= WriteRelativeBranch(g_sampDrawCallSite[0], 0xE8,
                                   reinterpret_cast<const void*>(
                                       TextDrawDrawHook));

    logging::Write("samp textdraws         %s (%s, 2 operands, 4 calls)",
                   applied ? "patched" : "FAILED", build->name);
    if (!applied)
        return;

    g_textdrawsPatched = true;
    UpdateTextdrawLayout(static_cast<float>(resolution.width),
                         static_cast<float>(resolution.height));
    RememberGuard("samp width (sprite)", g_sampSpriteWidthSite, 1,
                  kOperandOffset, false, &g_textdrawWidth);
    RememberGuard("samp width (text)", g_sampTextWidthSite, 1,
                  kMovEaxOperandOffset, false, &g_textdrawWidth);
    RememberGuard("samp sprite call", g_sampSpriteCallSite, 1,
                  kCallOperandOffset, true,
                  reinterpret_cast<const void*>(TextDrawSpriteHook));
    RememberGuard("samp wrap call", g_sampWrapxCallSite, 1,
                  kCallOperandOffset, true,
                  reinterpret_cast<const void*>(TextDrawSetWrapxHook));
    RememberGuard("samp print call", g_sampPrintCallSite, 1,
                  kCallOperandOffset, true,
                  reinterpret_cast<const void*>(TextDrawPrintStringHook));
    RememberGuard("samp draw call", g_sampDrawCallSite, 1,
                  kCallOperandOffset, true,
                  reinterpret_cast<const void*>(TextDrawDrawHook));
}

// The font's outline pass reads the pooled factor; the variable it is
// repointed at holds the text module's factor, stock until that module is
// in, except for the duration of a print that asks for another.
void ApplyFontOutline() {
    ApplyGroup("font outline", game::kFontShadowStretchXSites, game::kStretchX,
               &g_fontStretchX);
}

// The box behind a textdraw is sized inside the font from the real width.
// The hook acts only while the client is printing one, so it is taken only
// once the client's draw has been hooked.
void ApplyFontBoxHook() {
    ApplySpriteCallGroup("font box padding", game::kFontGetTextRectCallSites,
                         game::kFontGetTextRect,
                         reinterpret_cast<const void*>(FontGetTextRectHook));
}

// The block's four groups describe one layout between them: a factor over a
// width that is not the one the sites read would put every size off, so all
// of them are verified before any is written and either all apply or none
// does. The two pass wrappers are verified with them: they are what keeps
// the text module away from text the block has already scaled, so a block
// without them would have its text scaled twice. The shared helpers' sites
// are pass-throughs on their own.
void ApplyPlayerInfo(const Resolution& resolution) {
    if (!patch::ReadFloat(game::kWeaponIconMargin, g_weaponIconMarginStock) ||
        !RelativeCallTargets(game::kDrawPlayerInfoCallSites[0],
                             game::kDrawPlayerInfo) ||
        !RelativeCallTargets(game::kDrawWantedLevelCallSites[0],
                             game::kDrawWantedLevel) ||
        !patch::VerifyOperands(game::kPlayerInfoStretchXSites, game::kStretchX) ||
        !patch::VerifyOperands(game::kPlayerInfoWidthReadSites,
                               game::kScreenWidth) ||
        !patch::VerifyOperands(game::kPlayerInfoStretchYSites, game::kStretchY) ||
        !patch::VerifyOperands(game::kWeaponIconMarginSites,
                               game::kWeaponIconMargin)) {
        logging::Write("player info            SKIPPED, unexpected bytes");
        return;
    }

    // The margin literal was not known when the geometry was first computed.
    UpdatePlayerInfoLayout(static_cast<float>(resolution.width),
                           static_cast<float>(resolution.height));

    ApplyGroup("player info width", game::kPlayerInfoStretchXSites,
               game::kStretchX, &g_playerInfoStretchX);
    ApplyGroup("player info anchor", game::kPlayerInfoWidthReadSites,
               game::kScreenWidth, &g_playerInfoWidth);
    ApplyGroup("player info height", game::kPlayerInfoStretchYSites,
               game::kStretchY, &g_playerInfoStretchY);
    ApplyGroup("weapon icon margin", game::kWeaponIconMarginSites,
               game::kWeaponIconMargin, &g_weaponIconMargin);
    ApplyGroup("bar outline width", game::kHudPassStretchXSites,
               game::kStretchX, &g_hudPassStretchX);
    ApplyGroup("bar outline height", game::kHudPassStretchYSites,
               game::kStretchY, &g_hudPassStretchY);
    ApplySpriteCallGroup("player info pass", game::kDrawPlayerInfoCallSites,
                         game::kDrawPlayerInfo,
                         reinterpret_cast<const void*>(DrawPlayerInfoHook));
    ApplySpriteCallGroup("wanted level pass", game::kDrawWantedLevelCallSites,
                         game::kDrawWantedLevel,
                         reinterpret_cast<const void*>(DrawWantedLevelHook));
}

void ApplyAABugFix() {
    if (!patch::IsReadable(game::kRender2dStuffReturn, 5)) {
        logging::Write("AA edge frame          SKIPPED, unexpected bytes");
        return;
    }

    // Retail builds use either a RET or a tail JMP here. Both leave the stack
    // ready for HideAABugHook to return directly to FrontendIdle's caller.
    const uint8_t original =
        *reinterpret_cast<const uint8_t*>(game::kRender2dStuffReturn);
    if (original != 0xC3 && original != 0xE9) {
        logging::Write("AA edge frame          SKIPPED, unexpected opcode %02X",
                       static_cast<unsigned>(original));
        return;
    }

    const bool applied = WriteRelativeBranch(game::kRender2dStuffReturn, 0xE9,
                                              reinterpret_cast<const void*>(
                                                  HideAABugHook));
    logging::Write("AA edge frame          %s (pixels per side from the INI)",
                   applied ? "patched" : "FAILED");
}

// Installs a trampoline for `stolen` bytes at `address` and points `resume` at
// it. The caller has already established that those bytes are whole
// instructions and that none of them is position dependent.
bool InstallTrampolineHook(uintptr_t address, const uint8_t* expected,
                           size_t stolen, uintptr_t resumeAt,
                           const void* hook, void** resume) {
    auto* trampoline = static_cast<uint8_t*>(
        VirtualAlloc(nullptr, stolen + 5, MEM_COMMIT | MEM_RESERVE,
                     PAGE_EXECUTE_READWRITE));
    if (!trampoline)
        return false;

    std::memcpy(trampoline, expected, stolen);
    trampoline[stolen] = 0xE9;
    const auto backwards = static_cast<int32_t>(
        static_cast<intptr_t>(resumeAt) -
        reinterpret_cast<intptr_t>(trampoline + stolen + 5));
    std::memcpy(trampoline + stolen + 1, &backwards, sizeof(backwards));

    const intptr_t displacement = reinterpret_cast<intptr_t>(hook) -
                                  static_cast<intptr_t>(address + 5);
    if (displacement < INT32_MIN || displacement > INT32_MAX) {
        VirtualFree(trampoline, 0, MEM_RELEASE);
        return false;
    }

    // Publish the trampoline before the branch, or a frame drawn in between
    // calls through a null pointer.
    *resume = trampoline;

    uint8_t branch[16];
    std::memset(branch, 0x90, stolen);
    branch[0] = 0xE9;
    const auto relative = static_cast<int32_t>(displacement);
    std::memcpy(branch + 1, &relative, sizeof(relative));

    if (patch::WriteMemory(address, branch, stolen))
        return true;

    *resume = nullptr;
    VirtualFree(trampoline, 0, MEM_RELEASE);
    return false;
}

// Text in general is rescaled at the two entries every caller goes through.
// Both prologues are verified before either is written, and a second install
// that fails puts the first prologue back, so the module is in or out as a
// whole: text scaled at one door and not the other would be worse than
// neither. The hooks are pass-throughs while fixText is off, so a reload can
// turn the module on without rewriting code mid-frame.
void ApplyTextScale(const Resolution& resolution) {
    constexpr size_t kStolen = sizeof(game::kFontSetScalePrologue);
    constexpr size_t kStolenLang = sizeof(game::kFontSetScaleLangPrologue);

    if (!BytesMatch(game::kFontSetScale, game::kFontSetScalePrologue,
                        kStolen) ||
        !BytesMatch(game::kFontSetScaleLang,
                        game::kFontSetScaleLangPrologue, kStolenLang)) {
        logging::Write("text scale hooks       SKIPPED, unexpected prologue");
        return;
    }

    void* resume = nullptr;
    if (!InstallTrampolineHook(game::kFontSetScale, game::kFontSetScalePrologue,
                               kStolen, game::kFontSetScaleBody,
                               reinterpret_cast<const void*>(FontSetScaleHook),
                               &resume)) {
        logging::Write("text scale hooks       FAILED (CFont::SetScale entry)");
        return;
    }
    g_setScaleOriginal = reinterpret_cast<SetScaleFn>(resume);

    void* resumeLang = nullptr;
    if (!InstallTrampolineHook(
            game::kFontSetScaleLang, game::kFontSetScaleLangPrologue,
            kStolenLang, game::kFontSetScaleLangBody,
            reinterpret_cast<const void*>(FontSetScaleLangHook),
            &resumeLang)) {
        patch::WriteMemory(game::kFontSetScale, game::kFontSetScalePrologue,
                           kStolen);
        logging::Write("text scale hooks       FAILED (CFont::SetScaleLang "
                       "entry, SetScale restored)");
        return;
    }
    g_setScaleLangOriginal = reinterpret_cast<SetScaleFn>(resumeLang);

    g_textPatched = true;
    UpdateTextLayout(static_cast<float>(resolution.width),
                     static_cast<float>(resolution.height));
    logging::Write("text scale hooks       patched (CFont::SetScale and "
                   "SetScaleLang entries)");
}

// Hooks CHud::DrawCrossHairs at its own entry. The call to it inside
// CHud::Draw is deliberately not used: a HUD replacement that redirects
// CHud::Draw at its prologue never runs the original body, so a hook there is
// written and then never reached, while the replacement still calls
// DrawCrossHairs itself. That is what left the sniper fill missing.
void ApplyCrosshairDrawHook() {
    constexpr size_t kStolen = sizeof(game::kDrawCrossHairsPrologue);

    if (!patch::IsReadable(game::kDrawCrossHairs, kStolen) ||
        std::memcmp(reinterpret_cast<const void*>(game::kDrawCrossHairs),
                    game::kDrawCrossHairsPrologue, kStolen) != 0) {
        logging::Write("crosshair draw hook    SKIPPED, unexpected prologue");
        return;
    }

    void* resume = nullptr;
    const bool applied = InstallTrampolineHook(
        game::kDrawCrossHairs, game::kDrawCrossHairsPrologue, kStolen,
        game::kDrawCrossHairsBody,
        reinterpret_cast<const void*>(DrawCrossHairsHook), &resume);
    g_drawCrossHairsOriginal = reinterpret_cast<DrawCrossHairsFn>(resume);
    logging::Write("crosshair draw hook    %s (CHud::DrawCrossHairs entry)",
                   applied ? "patched" : "FAILED");
}
// The HUD is intercepted at the entry of CHud::Draw when that is free. When it
// is not, the overlay that took it reaches the body through a trampoline of its
// own, so the body still runs and hooking that works alongside it instead of
// fighting for the entry.
void ApplyHudVisibilityHook() {
    constexpr size_t kEntryStolen = sizeof(game::kDrawHudPrologue);

    if (patch::IsReadable(game::kDrawHud, kEntryStolen) &&
        std::memcmp(reinterpret_cast<const void*>(game::kDrawHud),
                    game::kDrawHudPrologue, kEntryStolen) == 0) {
        const intptr_t displacement =
            reinterpret_cast<intptr_t>(DrawHudHook) -
            static_cast<intptr_t>(game::kDrawHud + 5);
        if (displacement < INT32_MIN || displacement > INT32_MAX) {
            logging::Write("aim-mode HUD hook      FAILED, target out of range");
            return;
        }

        uint8_t branch[kEntryStolen];
        std::memset(branch, 0x90, sizeof(branch));
        branch[0] = 0xE9;
        const auto relative = static_cast<int32_t>(displacement);
        std::memcpy(branch + 1, &relative, sizeof(relative));
        const bool applied =
            patch::WriteMemory(game::kDrawHud, branch, sizeof(branch));
        logging::Write("aim-mode HUD hook      %s (CHud::Draw entry)",
                       applied ? "patched" : "FAILED");
        return;
    }

    constexpr size_t kBodyStolen = sizeof(game::kDrawHudBodyPrologue);
    if (!patch::IsReadable(game::kDrawHudBody, kBodyStolen) ||
        std::memcmp(reinterpret_cast<const void*>(game::kDrawHudBody),
                    game::kDrawHudBodyPrologue, kBodyStolen) != 0) {
        logging::Write("aim-mode HUD hook      SKIPPED, entry and body are "
                       "both taken");
        return;
    }

    void* resume = nullptr;
    const bool applied = InstallTrampolineHook(
        game::kDrawHudBody, game::kDrawHudBodyPrologue, kBodyStolen,
        game::kDrawHudBodyResume, reinterpret_cast<const void*>(DrawHudHook),
        &resume);
    g_drawHudResume = reinterpret_cast<DrawHudFn>(resume);
    logging::Write("aim-mode HUD hook      %s (CHud::Draw body, entry is "
                   "owned by another plugin)",
                   applied ? "patched" : "FAILED");
}
// The radar geometry only adds up when the horizontal scale, the rectangle and
// the elements pinned to it are all rewritten together, so every site is
// verified before the first one is written.
bool RadarSitesVerify() {
    return patch::VerifyOperands(game::kRadarStretchXSites, game::kStretchX) &&
           patch::VerifyOperands(game::kBlipStretchXSites, game::kStretchX) &&
           patch::VerifyOperands(game::kRadarLeftSites, game::kRadarLeft) &&
           patch::VerifyOperands(game::kRadarTopSites, game::kRadarTop) &&
           patch::VerifyOperands(game::kRadarHighSites, game::kRadarHigh) &&
           patch::VerifyOperands(game::kRadarWideSites, game::kRadarWide) &&
           patch::VerifyOperands(game::kDependentTopSites, game::kRadarTop) &&
           patch::VerifyOperands(game::kDependentHighSites, game::kRadarHigh);
}

void ApplyRadar() {
    if (g_radarPatched)
        return;

    if (!RadarSitesVerify()) {
        logging::Write("radar: unexpected bytes at one or more sites, "
                       "the radar was left untouched");
        return;
    }

    bool applied = true;
    applied &= ApplyGroup("radar scale", game::kRadarStretchXSites,
                          game::kStretchX, &g_radarStretchX);
    applied &= ApplyGroup("blip scale", game::kBlipStretchXSites,
                          game::kStretchX, &g_blipStretchX);
    applied &= ApplyGroup("radar left", game::kRadarLeftSites,
                          game::kRadarLeft, &g_radarLeft);
    applied &= ApplyGroup("radar top", game::kRadarTopSites,
                          game::kRadarTop, &g_radarTop);
    applied &= ApplyGroup("radar height", game::kRadarHighSites,
                          game::kRadarHigh, &g_radarHigh);
    applied &= ApplyGroup("radar width", game::kRadarWideSites,
                          game::kRadarWide, &g_radarWide);
    applied &= ApplyGroup("dependent top", game::kDependentTopSites,
                          game::kRadarTop, &g_radarTop);
    applied &= ApplyGroup("dependent height", game::kDependentHighSites,
                          game::kRadarHigh, &g_radarHigh);

    g_radarPatched = applied;
    if (applied && g_settings.roundRadar)
        RestoreRadarMask();
}

void ApplyCrosshair() {
    if (!g_crosshairScalePatched) {
        g_crosshairScalePatched =
            ApplyGroup("crosshair", game::kCrosshairStretchXSites,
                       game::kStretchX, &g_crosshairStretchX);
    }

    if (!g_lockOnPatched) {
        g_lockOnPatched =
            ApplyGroup("lock-on minimum", game::kLockOnMinWidthSites,
                       game::kLockOnMinWidth, &g_lockOnMinWidth);
    }

    if (!g_scopePatched) {
        g_scopePatched =
            ApplyGroup("scope", game::kScopeStretchXSites,
                       game::kStretchX, &g_scopeStretchX);
    }
    if (!g_viewfinderPatched) {
        g_viewfinderPatched =
            ApplyGroup("viewfinder ring", game::kViewfinderWidthSites,
                       game::kViewfinderWidth, &g_viewfinderWidth);
    }
}

void PublishRenderSettings() {
    const bool correctScope =
        g_settings.roundScope;

    InterlockedExchange(&g_noCameraCrosshair,
                        g_settings.noCameraCrosshair ? 1 : 0);
    InterlockedExchange(&g_hideCameraHud,
                        g_settings.hideCameraHud ? 1 : 0);
    InterlockedExchange(&g_hideSniperHud,
                        g_settings.hideSniperHud ? 1 : 0);
    InterlockedExchange(&g_drawSniperFill, correctScope ? 1 : 0);
    InterlockedExchange(&g_fixFov, g_settings.fixFov ? 1 : 0);
    InterlockedExchange(&g_useScreenAspect,
                        g_settings.useScreenAspect ? 1 : 0);
    InterlockedExchange(&g_spritePickups, g_settings.spritePickups ? 1 : 0);
    InterlockedExchange(&g_spriteCoronas, g_settings.spriteCoronas ? 1 : 0);
    InterlockedExchange(&g_spriteCoronaReflections,
                        g_settings.spriteCoronaReflections ? 1 : 0);
    InterlockedExchange(&g_spriteSunMoon, g_settings.spriteSunMoon ? 1 : 0);
    InterlockedExchange(&g_spritePointLights,
                        g_settings.spritePointLights ? 1 : 0);
    InterlockedExchange(&g_spriteBirds, g_settings.spriteBirds ? 1 : 0);
    InterlockedExchange(&g_spriteClouds, g_settings.spriteClouds ? 1 : 0);
    InterlockedExchange(&g_spriteCheckpoints,
                        g_settings.spriteCheckpoints ? 1 : 0);
    InterlockedExchange(&g_spriteWeaponEffects,
                        g_settings.spriteWeaponEffects ? 1 : 0);
    InterlockedExchange(&g_spriteCameraEffects,
                        g_settings.spriteCameraEffects ? 1 : 0);
    InterlockedExchange(&g_spriteTargeting,
                        g_settings.spriteTargetingMeasurements ? 1 : 0);
    InterlockedExchange(&g_aaEdgeLeft, g_settings.aaEdgeLeft);
    InterlockedExchange(&g_aaEdgeTop, g_settings.aaEdgeTop);
    InterlockedExchange(&g_aaEdgeRight, g_settings.aaEdgeRight);
    InterlockedExchange(&g_aaEdgeBottom, g_settings.aaEdgeBottom);
}

void ApplyReloadedSettings(HMODULE module, const char* path,
                           const Resolution& resolution) {
    const config::Settings previous = g_settings;
    const config::Settings reloaded = config::Load(path);

    if (reloaded.log && !previous.log)
        logging::Enable(module);

    g_settings = reloaded;
    UpdateGeometry(resolution);

    // All operands are attached to plugin-owned variables during startup,
    // including disabled modules. Reloading therefore changes only aligned
    // data values and never rewrites executable instructions mid-frame.
    if (g_settings.roundRadar && !previous.roundRadar)
        RestoreRadarMask();

    PublishRenderSettings();
    UpdateProbeSelection();
    logging::Write("configuration reloaded");

    if (g_settings.showReloadMessage)
        InterlockedExchange(&g_reloadNotificationPending, 1);

    if (!reloaded.log && previous.log)
        logging::Disable();
}

bool IsVirtualKeyDown(int key) {
    return key != 0 && (GetAsyncKeyState(key) & 0x8000) != 0;
}

// Steps the probe: none, then each site in turn, then the whole group, then
// none again. Only plugin owned floats are written from here.
void ServiceProbeHotkey(bool& wasDown) {
    if (!g_settings.probeEnabled || g_probeCount == 0 ||
        g_settings.probeHotkey.key == 0) {
        wasDown = false;
        return;
    }

    const bool down =
        IsVirtualKeyDown(g_settings.probeHotkey.key) &&
        (g_settings.probeHotkey.modifier == 0 ||
         IsVirtualKeyDown(g_settings.probeHotkey.modifier));
    if (down && !wasDown) {
        ++g_probeIndex;
        if (g_probeIndex > static_cast<int>(g_probeCount))
            g_probeIndex = -1;
        UpdateProbeSelection();
    }
    wasDown = down;
}

void ServiceReloadHotkey(HMODULE module, const char* path,
                         const Resolution& resolution, bool& wasDown) {
    if (g_settings.reloadHotkey.key == 0) {
        wasDown = false;
        return;
    }

    const bool down =
        IsVirtualKeyDown(g_settings.reloadHotkey.key) &&
        (g_settings.reloadHotkey.modifier == 0 ||
         IsVirtualKeyDown(g_settings.reloadHotkey.modifier));
    if (down && !wasDown)
        ApplyReloadedSettings(module, path, resolution);
    wasDown = down;
}

DWORD WINAPI PluginThread(LPVOID parameter) {
    const auto module = static_cast<HMODULE>(parameter);

    char path[MAX_PATH] = {};
    if (!config::GetPath(module, path))
        return 0;

    config::CreateDefault(module, path);
    g_settings = config::Load(path);

    if (g_settings.log)
        logging::Enable(module);

    logging::Write("SA Aspect Ratio Fixes v%s", kVersion);

    // The literals live in the image, so this answers before the game has
    // finished starting up.
    if (!IsSupportedExecutable()) {
        logging::Write("unsupported executable, nothing was patched");
        return 0;
    }

    // RsGlobal only reports the framebuffer size once RenderWare is up, so the
    // geometry cannot be computed at load time.
    Resolution resolution;
    for (DWORD waited = 0; waited < kStartupTimeoutMs;
         waited += kStartupPollMs) {
        if (GetResolution(resolution))
            break;
        Sleep(kStartupPollMs);
    }

    if (resolution.width == 0) {
        logging::Write("gave up waiting for RsGlobal to report a resolution");
        return 0;
    }

    UpdateGeometry(resolution);

    // Patch every configurable module once. Disabled modules publish the
    // exact stock constants, allowing Alt+H to enable them without modifying
    // executable instructions while the render thread is active.
    ApplyRadar();
    ApplyCrosshair();
    ApplyProbe();
    UpdateProbeSelection();

    // Install pass-through hooks even when their current options are off so a
    // hot reload can enable those options without rewriting code mid-frame.
    ApplyCrosshairDrawHook();
    ApplyHudVisibilityHook();
    PublishRenderSettings();

    ApplyFovFix();
    ApplyWorldSprites();
    ApplyFontOutline();
    ApplyTextScale(resolution);
    ApplyPlayerInfo(resolution);
    ApplySampTextdraws(resolution);
    if (g_textdrawsPatched)
        ApplyFontBoxHook();

    ApplyAABugFix();

    // Keep following the framebuffer size. Only plugin owned variables are
    // written from here, never game code.
    bool hotkeyWasDown = false;
    bool probeKeyWasDown = false;
    for (;;) {
        Sleep(kWatcherPollMs);

        Resolution current;
        if (GetResolution(current) && !(current == resolution)) {
            resolution = current;
            UpdateGeometry(resolution);
            UpdateProbeSelection();
        }

        ServiceReloadHotkey(module, path, resolution, hotkeyWasDown);
        ServiceProbeHotkey(probeKeyWasDown);
        CheckGuards();
    }
}

}  // namespace

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(instance);
        const HANDLE thread =
            CreateThread(nullptr, 0, PluginThread, instance, 0, nullptr);
        if (thread)
            CloseHandle(thread);
    }
    return TRUE;
}
