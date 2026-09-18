#include "textdraws.h"

#include "addresses.h"
#include "game.h"
#include "hooking.h"
#include "log.h"
#include "patch.h"
#include "samp_addresses.h"
#include "text.h"

#include <windows.h>

#include <cmath>
#include <cstring>

namespace textdraws {
namespace {

// CTextDraw::Draw is thiscall with no stack arguments, which a fastcall
// function with an unused second parameter receives correctly: `this` in
// ecx, garbage in edx, nothing to clean up.
using TextDrawDrawFn = void (__fastcall*)(void*, void*);
using TextDrawSpriteFn = void (__cdecl*)(void*, const Rect*, const void*);
using TextDrawSetWrapxFn = void (__cdecl*)(float);
using TextDrawPrintStringFn = void (__cdecl*)(float, float, const char*);

// `width` is what CTextDraw::Draw reads where it used to read
// RsGlobal.maximumWidth, `offset` how far right the layout area sits. The
// two belong together: a width from one resolution beside an offset from
// another puts every textdraw off centre for a frame. The worker publishes
// them as one 64 bit value, and the draw hook takes both from it in one read
// before the original runs.
struct Layout {
    int32_t width;
    float offset;
};
static_assert(sizeof(Layout) == sizeof(LONGLONG), "Layout must fit one interlocked exchange");

volatile LONGLONG g_layoutPacked = 0;

// Read by the two repointed loads in CTextDraw::Draw and by the forwarders
// below, written only by the draw hook on the render thread.
int32_t g_width = 0;
float g_offset = 0.0f;

// The client functions the hooks forward to, resolved against the loaded
// module once its build is known.
uintptr_t g_sampDraw = 0;
uintptr_t g_sampSpriteForward = 0;
uintptr_t g_sampWrapxForward = 0;
uintptr_t g_sampPrintForward = 0;
bool g_patched = false;

// Sites inside samp.dll, resolved against its base once it is found. They
// have static storage because the guards keep pointing at them.
uintptr_t g_drawCallSite[1];
uintptr_t g_spriteWidthSite[1];
uintptr_t g_textWidthSite[1];
uintptr_t g_spriteCallSite[1];
uintptr_t g_wrapxCallSite[1];
uintptr_t g_printCallSite[1];

void PublishLayout(int32_t width, float offset) {
    const Layout layout = {width, offset};
    LONGLONG packed = 0;
    std::memcpy(&packed, &layout, sizeof(packed));
    InterlockedExchange64(&g_layoutPacked, packed);
}

// Takes the layout for this draw, runs the original against it and shifts
// the click rectangle it stored, so a selectable textdraw is hit where it is
// drawn. The rectangle is ints; the shift is rounded the same way the draw
// rounds the positions it is derived from.
void __fastcall TextDrawDrawHook(uint8_t* textdraw, void*) {
    const LONGLONG packed = InterlockedCompareExchange64(&g_layoutPacked, 0, 0);
    Layout layout;
    std::memcpy(&layout, &packed, sizeof(layout));
    if (layout.width <= 0) {
        layout.width = *reinterpret_cast<const int32_t*>(game::kScreenWidth);
        layout.offset = 0.0f;
    }
    g_width = layout.width;
    g_offset = layout.offset;
    const float screenWidth = game_api::ScreenWidth();
    const float scale = screenWidth > 0.0f ? static_cast<float>(layout.width) / screenWidth : 1.0f;

    text::BeginTextdrawDraw(scale);
    reinterpret_cast<TextDrawDrawFn>(g_sampDraw)(textdraw, nullptr);
    text::EndTextdrawDraw();

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
void __cdecl TextDrawSpriteHook(void* sprite, const Rect* rect, const void* color) {
    Rect shifted = *rect;
    shifted.left += g_offset;
    shifted.right += g_offset;
    reinterpret_cast<TextDrawSpriteFn>(g_sampSpriteForward)(sprite, &shifted, color);
}

void __cdecl TextDrawSetWrapxHook(float wrap) {
    reinterpret_cast<TextDrawSetWrapxFn>(g_sampWrapxForward)(wrap + g_offset);
}

void __cdecl TextDrawPrintStringHook(float x, float y, const char* string) {
    text::BeginTextdrawPrint();
    reinterpret_cast<TextDrawPrintStringFn>(g_sampPrintForward)(x + g_offset, y, string);
    text::EndTextdrawPrint();
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

} // namespace

// Every site is checked before the first one is written: the two width loads
// against their full encoding, the four calls against the targets samp.dll
// links them to. A call that another modification has already retargeted
// fails that check and leaves the whole group untouched.
void ApplySampTextdraws(const config::Settings& settings, const Resolution& resolution) {
    const HMODULE module = GetModuleHandleA("samp.dll");
    if (!module) {
        logging::Write("samp textdraws         SKIPPED, samp.dll is not loaded");
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
        logging::Write("samp textdraws         SKIPPED, samp.dll build %08X is not mapped "
                       "(0.3.7-R1 and 0.3.7-R3-1 are)",
                       static_cast<unsigned>(stamp));
        return;
    }

    g_drawCallSite[0] = base + build->drawTextDrawCall;
    g_spriteWidthSite[0] = base + build->spriteWidthRead;
    g_textWidthSite[0] = base + build->textWidthRead;
    g_spriteCallSite[0] = base + build->spriteDrawCall;
    g_wrapxCallSite[0] = base + build->setWrapxCall;
    g_printCallSite[0] = base + build->printStringCall;

    const bool intact =
        hooking::RelativeCallTargets(g_drawCallSite[0], base + build->textDrawDraw) &&
        hooking::BytesMatch(g_spriteWidthSite[0], samp::kSpriteWidthReadBytes, sizeof(samp::kSpriteWidthReadBytes)) &&
        hooking::BytesMatch(g_textWidthSite[0], samp::kTextWidthReadBytes, sizeof(samp::kTextWidthReadBytes)) &&
        hooking::RelativeCallTargets(g_spriteCallSite[0], base + build->spriteDrawForward) &&
        hooking::RelativeCallTargets(g_wrapxCallSite[0], base + build->setWrapxForward) &&
        hooking::RelativeCallTargets(g_printCallSite[0], base + build->printStringForward);
    if (!intact) {
        logging::Write("samp textdraws         SKIPPED, unexpected bytes (%s)", build->name);
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
    g_width = resolution.width;
    g_offset = 0.0f;

    bool applied = true;
    applied &= patch::RepointOperands(g_spriteWidthSite, game::kScreenWidth, &g_width);
    applied &= patch::RepointMovEaxOperand(g_textWidthSite[0], game::kScreenWidth, &g_width);
    applied &= hooking::WriteRelativeBranch(g_spriteCallSite[0], 0xE8,
                                            reinterpret_cast<const void*>(TextDrawSpriteHook));
    applied &= hooking::WriteRelativeBranch(g_wrapxCallSite[0], 0xE8,
                                            reinterpret_cast<const void*>(TextDrawSetWrapxHook));
    applied &= hooking::WriteRelativeBranch(g_printCallSite[0], 0xE8,
                                            reinterpret_cast<const void*>(TextDrawPrintStringHook));
    // The draw hook goes in last: everything it runs is already redirected.
    applied &= hooking::WriteRelativeBranch(g_drawCallSite[0], 0xE8, reinterpret_cast<const void*>(TextDrawDrawHook));

    logging::Write("samp textdraws         %s (%s, 2 operands, 4 calls)", applied ? "patched" : "FAILED",
                   build->name);
    if (!applied)
        return;

    g_patched = true;
    UpdateLayout(settings, static_cast<float>(resolution.width), static_cast<float>(resolution.height));
    hooking::RememberGuard("samp width (sprite)", g_spriteWidthSite, 1, hooking::kOperandOffset, false, &g_width);
    hooking::RememberGuard("samp width (text)", g_textWidthSite, 1, hooking::kMovEaxOperandOffset, false, &g_width);
    hooking::RememberGuard("samp sprite call", g_spriteCallSite, 1, hooking::kCallOperandOffset, true,
                           reinterpret_cast<const void*>(TextDrawSpriteHook));
    hooking::RememberGuard("samp wrap call", g_wrapxCallSite, 1, hooking::kCallOperandOffset, true,
                           reinterpret_cast<const void*>(TextDrawSetWrapxHook));
    hooking::RememberGuard("samp print call", g_printCallSite, 1, hooking::kCallOperandOffset, true,
                           reinterpret_cast<const void*>(TextDrawPrintStringHook));
    hooking::RememberGuard("samp draw call", g_drawCallSite, 1, hooking::kCallOperandOffset, true,
                           reinterpret_cast<const void*>(TextDrawDrawHook));
}

bool Patched() {
    return g_patched;
}

// A 16:9 screen is not wider than 16:9, so the plugin changes nothing there;
// the small tolerance keeps 1920x1080, which is 1.7777 against a 1.7778
// setting, on that side of the line.
void UpdateLayout(const config::Settings& settings, float screenWidth, float screenHeight) {
    constexpr float kAspectTolerance = 0.001f;
    const float aspect = screenWidth / screenHeight;
    const float layoutAspect = settings.textdrawAspect;

    int32_t width = static_cast<int32_t>(screenWidth);
    float offset = 0.0f;
    const bool fitted = settings.fitTextdraws && aspect > layoutAspect + kAspectTolerance;
    if (fitted) {
        width = static_cast<int32_t>(std::lround(screenHeight * layoutAspect));
        offset = (screenWidth - static_cast<float>(width)) * 0.5f;
    }
    PublishLayout(width, offset);

    if (!g_patched)
        return;
    if (fitted) {
        logging::Write("  textdraws: laid out %d px wide, %.1f px from the left (screen is wider than %.4f)", width,
                       static_cast<double>(offset), static_cast<double>(layoutAspect));
    } else if (settings.fitTextdraws) {
        logging::Write("  textdraws: screen is not wider than %.4f, unchanged", static_cast<double>(layoutAspect));
    } else {
        logging::Write("  textdraws: unchanged (fitTextdraws=0)");
    }
}

} // namespace textdraws
