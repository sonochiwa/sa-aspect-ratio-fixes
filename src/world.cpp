#include "world.h"

#include "addresses.h"
#include "game.h"
#include "hooking.h"
#include "log.h"
#include "patch.h"
#include "types.h"

#include <windows.h>

#include <cmath>

namespace world {
namespace {

using CalcScreenCoorsFn = bool (__cdecl*)(const Vec3&, Vec3*, float*, float*, bool, bool);

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

float g_spriteWidthCorrection = 1.0f;

bool CalcScreenCoorsFor(const Vec3& input, Vec3* output, float* width, float* height, bool checkMax, bool checkMin,
                        volatile LONG* enabled) {
    const bool visible =
        reinterpret_cast<CalcScreenCoorsFn>(game::kCalcScreenCoors)(input, output, width, height, checkMax, checkMin);
    if (visible && width && InterlockedCompareExchange(enabled, 0, 0) != 0)
        *width *= g_spriteWidthCorrection;
    return visible;
}

#define DEFINE_SPRITE_WRAPPER(name, flag)                                                                    \
    bool __cdecl name(const Vec3& input, Vec3* output, float* width, float* height, bool checkMax, bool checkMin) { \
        return CalcScreenCoorsFor(input, output, width, height, checkMax, checkMin, &flag);                   \
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

void __cdecl SetFovHook(float fov) {
    if (InterlockedCompareExchange(&g_fixFov, 0, 0) != 0) {
        const float aspect = *reinterpret_cast<const float*>(game::kAspectRatio);
        constexpr float kPi = 3.14159265358979323846f;
        const float radians = fov * kPi / 180.0f;
        fov = 2.0f * std::atan(std::tan(radians * 0.5f) * (aspect / (4.0f / 3.0f))) * 180.0f / kPi;
    }
    *reinterpret_cast<float*>(game::kFov) = fov;
}

void __cdecl CalculateAspectRatioHook() {
    const float width = game_api::ScreenWidth();
    const float height = game_api::ScreenHeight();
    if (width > 0.0f && height > 0.0f) {
        *reinterpret_cast<float*>(game::kAspectRatio) =
            InterlockedCompareExchange(&g_useScreenAspect, 0, 0) != 0 ? width / height : 4.0f / 3.0f;
    }
}

} // namespace

void ApplySprites() {
    if (!patch::IsReadable(game::kCalcScreenCoors, 1) ||
        *reinterpret_cast<const uint8_t*>(game::kCalcScreenCoors) == 0xE9) {
        logging::Write("world sprites          SKIPPED, projection is hooked");
        return;
    }

    hooking::ApplyCallGroup("sprite pickups", game::kSpritePickupSites, game::kCalcScreenCoors, CalcPickupSprite);
    hooking::ApplyCallGroup("sprite coronas", game::kSpriteCoronaSites, game::kCalcScreenCoors, CalcCoronaSprite);
    hooking::ApplyCallGroup("sprite reflections", game::kSpriteCoronaReflectionSites, game::kCalcScreenCoors,
                            CalcCoronaReflectionSprite);
    hooking::ApplyCallGroup("sprite sun/moon", game::kSpriteSunMoonSites, game::kCalcScreenCoors, CalcSunMoonSprite);
    hooking::ApplyCallGroup("sprite point lights", game::kSpritePointLightSites, game::kCalcScreenCoors,
                            CalcPointLightSprite);
    hooking::ApplyCallGroup("sprite birds", game::kSpriteBirdSites, game::kCalcScreenCoors, CalcBirdSprite);
    hooking::ApplyCallGroup("sprite clouds", game::kSpriteCloudSites, game::kCalcScreenCoors, CalcCloudSprite);
    hooking::ApplyCallGroup("sprite checkpoints", game::kSpriteCheckpointSites, game::kCalcScreenCoors,
                            CalcCheckpointSprite);
    hooking::ApplyCallGroup("sprite weapon FX", game::kSpriteWeaponEffectSites, game::kCalcScreenCoors,
                            CalcWeaponEffectSprite);
    hooking::ApplyCallGroup("sprite camera FX", game::kSpriteCameraEffectSites, game::kCalcScreenCoors,
                            CalcCameraEffectSprite);
    hooking::ApplyCallGroup("sprite targeting", game::kSpriteTargetingSites, game::kCalcScreenCoors,
                            CalcTargetingSprite);
}

void ApplyFovFix() {
    if (patch::IsReadable(game::kSetFov, 1) && *reinterpret_cast<const uint8_t*>(game::kSetFov) != 0xE9) {
        hooking::ApplyCallGroup("widescreen FOV", game::kSetFovCallSites, game::kSetFov,
                                reinterpret_cast<const void*>(SetFovHook));
    } else {
        logging::Write("widescreen FOV         SKIPPED, function is hooked");
    }

    if (patch::IsReadable(game::kCalculateAspectRatio, 1) &&
        *reinterpret_cast<const uint8_t*>(game::kCalculateAspectRatio) != 0xE9) {
        hooking::ApplyCallGroup("screen aspect", game::kCalculateAspectCallSites, game::kCalculateAspectRatio,
                                reinterpret_cast<const void*>(CalculateAspectRatioHook));
    } else {
        logging::Write("screen aspect          SKIPPED, function is hooked");
    }
}

void PublishSettings(const config::Settings& settings) {
    InterlockedExchange(&g_fixFov, settings.fixFov ? 1 : 0);
    InterlockedExchange(&g_useScreenAspect, settings.useScreenAspect ? 1 : 0);
    InterlockedExchange(&g_spritePickups, settings.spritePickups ? 1 : 0);
    InterlockedExchange(&g_spriteCoronas, settings.spriteCoronas ? 1 : 0);
    InterlockedExchange(&g_spriteCoronaReflections, settings.spriteCoronaReflections ? 1 : 0);
    InterlockedExchange(&g_spriteSunMoon, settings.spriteSunMoon ? 1 : 0);
    InterlockedExchange(&g_spritePointLights, settings.spritePointLights ? 1 : 0);
    InterlockedExchange(&g_spriteBirds, settings.spriteBirds ? 1 : 0);
    InterlockedExchange(&g_spriteClouds, settings.spriteClouds ? 1 : 0);
    InterlockedExchange(&g_spriteCheckpoints, settings.spriteCheckpoints ? 1 : 0);
    InterlockedExchange(&g_spriteWeaponEffects, settings.spriteWeaponEffects ? 1 : 0);
    InterlockedExchange(&g_spriteCameraEffects, settings.spriteCameraEffects ? 1 : 0);
    InterlockedExchange(&g_spriteTargeting, settings.spriteTargetingMeasurements ? 1 : 0);
}

void SetSpriteWidthCorrection(float correction) {
    g_spriteWidthCorrection = correction;
}

} // namespace world
