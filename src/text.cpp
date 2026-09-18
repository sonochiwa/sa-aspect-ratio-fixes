#include "text.h"

#include "addresses.h"
#include "game.h"
#include "hooking.h"
#include "log.h"
#include "patch.h"

#include <cmath>

namespace text {
namespace {

using SetScaleFn = void (__cdecl*)(float, float);
using GetTextRectFn = void (__cdecl*)(Rect*, float, float, const char*);

// Read by the font's outline and drop shadow pass in place of the pooled
// SCREEN_STRETCH_X factor.
float g_fontStretchX = kStockStretchX;
bool g_textdrawPrinting = false;
float g_textdrawScale = 1.0f;

float g_textScale = 1.0f;
float g_textStretchX = kStockStretchX;
bool g_textdrawDrawing = false;
bool g_hudPassDrawing = false;
bool g_textPatched = false;

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

// Runs in place of the GetTextRect call inside CFont::PrintString. The rect
// it returns is the textdraw's box: the text's extent padded on each side,
// by 4 pixels in the stock game and by 4 units of the real screen size under
// SilentPatch. The vertical padding is what a 16:9 display of this height
// has as well. A horizontal one that equals the stretched stock value came
// from the real width, so it is re-stretched by the layout width; anything
// else, the stock 4 pixels included, is left as it is.
void __cdecl FontGetTextRectHook(Rect* rect, float x, float y, const char* string) {
    reinterpret_cast<GetTextRectFn>(game::kFontGetTextRect)(rect, x, y, string);
    if (!g_textdrawPrinting || g_textdrawScale == 1.0f)
        return;

    const float stretched = game::kFontBoxPadding * game_api::ScreenWidth() / game::kDesignWidth;
    const float fitted = stretched * g_textdrawScale;
    constexpr float kTolerance = 0.5f;

    float leftPadding = 0.0f;
    float rightPadding = 0.0f;
    bool hasLeft = true;
    if (*reinterpret_cast<const bool*>(game::kFontCentre)) {
        const float half = *reinterpret_cast<const float*>(game::kFontCentreSize) * 0.5f;
        leftPadding = x - half - rect->left;
        rightPadding = rect->right - x - half;
    } else if (*reinterpret_cast<const bool*>(game::kFontRightJustify)) {
        // The left edge comes from a wrap SA-MP never sets, so only the
        // right one is the textdraw's.
        hasLeft = false;
        rightPadding = rect->right - x;
    } else {
        leftPadding = x - rect->left;
        rightPadding = rect->right - *reinterpret_cast<const float*>(game::kFontWrapX);
    }

    if (hasLeft && std::fabs(leftPadding - stretched) < kTolerance)
        rect->left += leftPadding - fitted;
    if (std::fabs(rightPadding - stretched) < kTolerance)
        rect->right -= rightPadding - fitted;
}

} // namespace

void ApplyFontOutline() {
    hooking::ApplyGroup("font outline", game::kFontShadowStretchXSites, game::kStretchX, &g_fontStretchX);
}

void ApplyFontBoxHook() {
    hooking::ApplyCallGroup("font box padding", game::kFontGetTextRectCallSites, game::kFontGetTextRect,
                            reinterpret_cast<const void*>(FontGetTextRectHook));
}

// Both prologues are verified before either is written, and a second install
// that fails puts the first prologue back, so the module is in or out as a
// whole: text scaled at one door and not the other would be worse than
// neither. The hooks are pass-throughs while fixText is off, so a reload can
// turn the module on without rewriting code mid-frame.
void ApplyTextScale(const config::Settings& settings, const Resolution& resolution) {
    constexpr size_t kStolen = sizeof(game::kFontSetScalePrologue);
    constexpr size_t kStolenLang = sizeof(game::kFontSetScaleLangPrologue);

    if (!hooking::BytesMatch(game::kFontSetScale, game::kFontSetScalePrologue, kStolen) ||
        !hooking::BytesMatch(game::kFontSetScaleLang, game::kFontSetScaleLangPrologue, kStolenLang)) {
        logging::Write("text scale hooks       SKIPPED, unexpected prologue");
        return;
    }

    void* resume = nullptr;
    if (!hooking::InstallTrampolineHook(game::kFontSetScale, game::kFontSetScalePrologue, kStolen,
                                        game::kFontSetScaleBody, reinterpret_cast<const void*>(FontSetScaleHook),
                                        &resume)) {
        logging::Write("text scale hooks       FAILED (CFont::SetScale entry)");
        return;
    }
    g_setScaleOriginal = reinterpret_cast<SetScaleFn>(resume);

    void* resumeLang = nullptr;
    if (!hooking::InstallTrampolineHook(game::kFontSetScaleLang, game::kFontSetScaleLangPrologue, kStolenLang,
                                        game::kFontSetScaleLangBody,
                                        reinterpret_cast<const void*>(FontSetScaleLangHook), &resumeLang)) {
        patch::WriteMemory(game::kFontSetScale, game::kFontSetScalePrologue, kStolen);
        logging::Write("text scale hooks       FAILED (CFont::SetScaleLang entry, SetScale restored)");
        return;
    }
    g_setScaleLangOriginal = reinterpret_cast<SetScaleFn>(resumeLang);

    g_textPatched = true;
    UpdateLayout(settings, static_cast<float>(resolution.width), static_cast<float>(resolution.height));
    logging::Write("text scale hooks       patched (CFont::SetScale and SetScaleLang entries)");
}

// The width its caller stretched by the real factor is multiplied by the
// ratio of the wanted pixels per unit to it. The outline pass reads the same
// ratio, so both stay stock until the scale hooks are in; a scaled outline
// on unscaled glyphs is worse than neither.
//
// The ambient outline factor is written here, from the worker. A reload that
// lands while the render thread is inside a HUD pass or a textdraw print
// overrides that pass's factor for its remainder; the pass restores the
// ambient value when it ends, so the effect is confined to that one frame.
void UpdateLayout(const config::Settings& settings, float screenWidth, float screenHeight) {
    const float pixelsPerUnitX = screenWidth / game::kDesignWidth;
    float textPixelsX = pixelsPerUnitX;
    if (g_textPatched && settings.fixText && settings.textAspect > 0.0f)
        textPixelsX = screenHeight * settings.textAspect / game::kDesignWidth;
    g_textScale = textPixelsX / pixelsPerUnitX;
    g_textStretchX = g_textScale * kStockStretchX;
    g_fontStretchX = g_textStretchX;

    // Logged from here rather than with the rest of the geometry, so the
    // figure printed at startup is the one computed after the hooks went in.
    if (!g_textPatched)
        return;
    logging::Write("  text: %.4f px per unit wide (game default %.4f)", static_cast<double>(textPixelsX),
                   static_cast<double>(pixelsPerUnitX));
}

void BeginTextdrawDraw(float scale) {
    g_textdrawScale = scale;
    g_textdrawDrawing = true;
}

void EndTextdrawDraw() {
    g_textdrawDrawing = false;
}

// The print is where the font reads the real width, so the layout's factor
// is published to the outline pass and to the box hook for exactly its
// duration.
void BeginTextdrawPrint() {
    g_fontStretchX = kStockStretchX * g_textdrawScale;
    g_textdrawPrinting = true;
}

void EndTextdrawPrint() {
    g_textdrawPrinting = false;
    g_fontStretchX = g_textStretchX;
}

void BeginHudPass(float stretchX) {
    g_hudPassDrawing = true;
    g_fontStretchX = stretchX;
}

void EndHudPass() {
    g_fontStretchX = g_textStretchX;
    g_hudPassDrawing = false;
}

} // namespace text
