#include "aa_edge.h"

#include "addresses.h"
#include "game.h"
#include "hooking.h"
#include "log.h"
#include "patch.h"
#include "types.h"

#include <windows.h>

#include <cstddef>
#include <cstdlib>

namespace aa_edge {
namespace {

using RenderStateSetFn = int (__cdecl*)(int, void*);
using RenderStateGetFn = int (__cdecl*)(int, void*);

volatile LONG g_left = 1;
volatile LONG g_top = 1;
volatile LONG g_right = 1;
volatile LONG g_bottom = 1;

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

static_assert(offsetof(RwGlobalsPrefix, device.renderStateSet) == 0x20, "unexpected RenderWare globals layout");
static_assert(offsetof(RwGlobalsPrefix, device.renderStateGet) == 0x24, "unexpected RenderWare globals layout");

struct SavedRenderState {
    int state = 0;
    void* value = nullptr;
    bool valid = false;
};

// DefinedState2d changes these states, while CSprite2d::DrawRect
// additionally clears the texture raster. Texture U/V addressing is saved
// separately: the combined TEXTUREADDRESS state can lose an asymmetric pair.
// TEXTURERASTER, TEXTUREADDRESSU, TEXTUREADDRESSV, TEXTUREPERSPECTIVE,
// ZTESTENABLE, SHADEMODE, ZWRITEENABLE, TEXTUREFILTER, SRCBLEND, DESTBLEND,
// VERTEXALPHAENABLE, BORDERCOLOR, FOGENABLE, CULLMODE, ALPHATESTFUNCTION,
// ALPHATESTFUNCTIONREF.
constexpr int kRenderStateIds[] = {1, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 20, 29, 30};

class RenderStateGuard {
public:
    RenderStateGuard() {
        const auto* globals = *reinterpret_cast<RwGlobalsPrefix* const*>(game::kRwEngineInstance);
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
            saved.state = kRenderStateIds[i];
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
    SavedRenderState saved_[_countof(kRenderStateIds)] = {};
};

// Rendered at the same tail point used by Widescreen Fix's HideAABug=2.
// Coordinates deliberately extend beyond the surface: at one pixel they
// cover, after rasterisation, exactly the four one-pixel edge samples
// affected by MSAA. Each side has its own thickness, a thicker side grows
// inward from that same edge, and a frame with no side left is skipped
// entirely so the 2D state is not touched for nothing.
void HideAABugHook() {
    const float left = static_cast<float>(InterlockedCompareExchange(&g_left, 0, 0));
    const float top = static_cast<float>(InterlockedCompareExchange(&g_top, 0, 0));
    const float right = static_cast<float>(InterlockedCompareExchange(&g_right, 0, 0));
    const float bottom = static_cast<float>(InterlockedCompareExchange(&g_bottom, 0, 0));
    if (left <= 0.0f && top <= 0.0f && right <= 0.0f && bottom <= 0.0f)
        return;

    const float width = game_api::ScreenWidth();
    const float height = game_api::ScreenHeight();

    // DefinedState2d disables depth testing/writes and changes a dozen other
    // persistent RenderWare states. This hook runs at the very end of the 2D
    // pass, so leaking those values makes some transparent world effects in
    // the next frame (notably birds and skidmarks) render through geometry.
    // Restore the exact incoming state after drawing the edge frame.
    const RenderStateGuard stateGuard;
    game_api::DefinedState2d();
    if (top > 0.0f)
        game_api::DrawBlackRect({0.0f, -5.0f, width, top - 0.5f});
    if (left > 0.0f)
        game_api::DrawBlackRect({-5.0f, -1.0f, left - 0.5f, height});
    if (bottom > 0.0f)
        game_api::DrawBlackRect({0.0f, height - bottom - 0.5f, width, height + 5.0f});
    if (right > 0.0f)
        game_api::DrawBlackRect({width - right, 0.0f, width + 5.0f, height + 5.0f});
}

} // namespace

void Apply() {
    if (!patch::IsReadable(game::kRender2dStuffReturn, 5)) {
        logging::Write("AA edge frame          SKIPPED, unexpected bytes");
        return;
    }

    // Retail builds use either a RET or a tail JMP here. Both leave the
    // stack ready for HideAABugHook to return directly to FrontendIdle's
    // caller.
    const uint8_t original = *reinterpret_cast<const uint8_t*>(game::kRender2dStuffReturn);
    if (original != 0xC3 && original != 0xE9) {
        logging::Write("AA edge frame          SKIPPED, unexpected opcode %02X", static_cast<unsigned>(original));
        return;
    }

    const bool applied = hooking::WriteRelativeBranch(game::kRender2dStuffReturn, 0xE9,
                                                      reinterpret_cast<const void*>(HideAABugHook));
    logging::Write("AA edge frame          %s (pixels per side from the INI)", applied ? "patched" : "FAILED");
}

void PublishSettings(const config::Settings& settings) {
    InterlockedExchange(&g_left, settings.aaEdgeLeft);
    InterlockedExchange(&g_top, settings.aaEdgeTop);
    InterlockedExchange(&g_right, settings.aaEdgeRight);
    InterlockedExchange(&g_bottom, settings.aaEdgeBottom);
}

} // namespace aa_edge
