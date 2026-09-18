#pragma once

#include <cstddef>
#include <cstdint>

// Radar patch sites and the probe candidate groups, both in the CHud and
// CRadar range of gta_sa.exe 1.0 US.
namespace game {

// Radar geometry. Sites are the addresses of the x87 instructions; the operand
// that gets repointed starts two bytes later.
//
// CRadar::TransformRadarPointToScreenSpace @ 0x583480 maps radar space to the
// screen, CHud::DrawRadar @ 0x58A330 draws the plane ring, the altimeter and
// the four corner masks, CRadar::DrawEntityBlip @ 0x587000 scales blips.

constexpr uintptr_t kRadarLeftSites[] = {
    0x005834D2, // CRadar::TransformRadarPointToScreenSpace
    0x0058A467, // CHud::DrawRadar, plane ring sprite
    0x0058A5E0, // CHud::DrawRadar, altimeter background
    0x0058A6E4, // CHud::DrawRadar, altimeter height line
    0x0058A799, // CHud::DrawRadar, corner mask 1
    0x0058A834, // CHud::DrawRadar, corner mask 2
    0x0058A8E7, // CHud::DrawRadar, corner mask 3
    0x0058A988, // CHud::DrawRadar, corner mask 4
};

constexpr uintptr_t kRadarTopSites[] = {
    0x005834FE, // CRadar::TransformRadarPointToScreenSpace
    0x0058A497, // CHud::DrawRadar, plane ring sprite
    0x0058A60C, // CHud::DrawRadar, altimeter background
    0x0058A71C, // CHud::DrawRadar, altimeter height line
    0x0058A7C5, // CHud::DrawRadar, corner mask 1
    0x0058A866, // CHud::DrawRadar, corner mask 2
    0x0058A911, // CHud::DrawRadar, corner mask 3
    0x0058A9C5, // CHud::DrawRadar, corner mask 4
};

constexpr uintptr_t kRadarHighSites[] = {
    0x005834F4, // CRadar::TransformRadarPointToScreenSpace
    0x0058A47B, // CHud::DrawRadar, plane ring sprite
    0x0058A630, // CHud::DrawRadar, altimeter background
    0x0058A6A9, // CHud::DrawRadar, altimeter height line
    0x0058A70C, // CHud::DrawRadar, altimeter height line
    0x0058A7FF, // CHud::DrawRadar, corner mask 1
    0x0058A8A9, // CHud::DrawRadar, corner mask 2
    0x0058A91F, // CHud::DrawRadar, corner mask 3
    0x0058A9D3, // CHud::DrawRadar, corner mask 4
};

constexpr uintptr_t kRadarWideSites[] = {
    0x005834C0, // CRadar::TransformRadarPointToScreenSpace
    0x00587819, // CRadar::DrawEntityBlip
    0x0058A447, // CHud::DrawRadar, plane ring sprite
    0x0058A7E7, // CHud::DrawRadar, corner mask 1
    0x0058A83E, // CHud::DrawRadar, corner mask 2
    0x0058A941, // CHud::DrawRadar, corner mask 3
    0x0058A99B, // CHud::DrawRadar, corner mask 4
};

// Elements the game positions against the top edge of the radar. They have to
// follow the radar, otherwise resizing it makes them overlap or drift away.

constexpr uintptr_t kDependentTopSites[] = {
    0x0058A1A5, // CHud::DrawTripSkip
    0x0058A2D4, // CHud::DrawTripSkip
    0x0058AE2C, // CHud::DrawAreaName
    0x0058B133, // CHud::DrawVehicleName
};

constexpr uintptr_t kDependentHighSites[] = {
    0x0058AE38, // CHud::DrawAreaName
};

// SCREEN_STRETCH_X sites. Each one was confirmed to be preceded by a
// `fild dword ptr [RsGlobal.maximumWidth]`, so repointing the factor changes
// only the horizontal scale of that function.
//
// Repointing these makes the horizontal scale inside the radar equal to the
// vertical one, which is what turns the radar rectangle into a square pixel
// space: the map circle, the black ring and every blip sprite then share one
// isotropic unit.
//
// The sites are split into two groups because the radar frame and the blip
// icons drawn inside it are independent elements. The frame group covers the
// radar-space to screen-space transform and everything CHud::DrawRadar draws
// around the circle. The blip group covers only the size calculations of the
// sprites drawn on top of it, which are `x +/- SCREEN_STRETCH_X(8)` against
// `y +/- SCREEN_STRETCH_Y(8)` and are therefore ovals while they read the
// stock factor. Blip positions come from the transform in the frame group, so
// either group can be corrected without the other leaving anything misplaced.
//
// Position conversions that only run while the full-screen map is open are
// deliberately absent from both groups. They convert map-space X coordinates
// with the stock SCREEN_STRETCH_X factor so that blips stay attached to the
// map while it is panned. Only the later size calculations in those functions
// are corrected.

constexpr uintptr_t kRadarStretchXSites[] = {
    0x005834BA,                         // CRadar::TransformRadarPointToScreenSpace
    0x0058A441, 0x0058A5D8, 0x0058A6DE, // CHud::DrawRadar
    0x0058A791, 0x0058A82E, 0x0058A8DF,
    0x0058A982,
};

constexpr uintptr_t kBlipStretchXSites[] = {
    0x0058410B, 0x00584190,             // CRadar::ShowRadarTraceWithHeight
    0x00584249, 0x005842E6, 0x0058439C,
    0x00584434,
    0x0058603F,                         // CRadar::DrawRadarSprite
    0x005876D4, 0x0058774B, 0x0058780A, // CRadar::DrawEntityBlip
    0x0058788F, 0x0058792E, 0x00587A1A,
    0x00587AAA,
};

// Probe groups. These are candidates rather than mapped elements: they are the
// largest functions in the CHud range that read SCREEN_STRETCH_X and that no
// module patches yet, taken from the full scan in
// references\stretch-x-sites.md. The weapon icon is the reason they are
// interesting.
//
// Which of their sites are sizes and which are position conversions cannot be
// told apart from the encoding, and correcting a position is what drifted the
// map blips. The probe therefore repoints each site at its own variable and
// corrects one at a time, so a site can be watched in game before any of this
// is turned into a module.

constexpr size_t kMaxProbeSites = 16;

// 0x0058EAF0, in the CHud range below DrawCrossHairs.
constexpr uintptr_t kProbeGroupA[] = {
    0x0058EB3F, 0x0058EC0C, 0x0058EE7E, 0x0058EEF4, 0x0058EF50,
    0x0058EFC5, 0x0058F116, 0x0058F194, 0x0058F55C, 0x0058F5F4,
    0x0058F91C, 0x0058F993, 0x0058F9D0, 0x0058FA5D,
};

// 0x00589650, in the CHud range above DrawRadar.
constexpr uintptr_t kProbeGroupB[] = {
    0x005896D8, 0x00589703, 0x005897C3, 0x0058986D, 0x0058990C,
    0x00589A16, 0x00589B2D, 0x00589C73, 0x00589D61, 0x00589E49,
    0x00589F31, 0x0058A013, 0x0058A090, 0x0058A134,
};

} // namespace game
