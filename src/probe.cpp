#include "probe.h"

#include "cheat_command.h"
#include "addresses.h"
#include "addresses_radar.h"
#include "game.h"
#include "log.h"
#include "patch.h"
#include "types.h"

#include <windows.h>

#include <cstdio>

namespace probe {
namespace {

// Every site in the selected group is repointed at its own slot, so
// selecting one is a matter of writing floats rather than rewriting code,
// exactly like every other module. Index -1 corrects nothing and index
// g_count corrects the whole group at once.
float g_stretch[game::kMaxProbeSites];
const uintptr_t* g_sites = nullptr;
size_t g_count = 0;
int g_index = -1;
float g_squareStretch = kStockStretchX;

// The message buffer is written by the worker on a key press and read by the
// game thread on the next frame. AddMessageJump copies the string, and a
// torn read could only garble one diagnostic message, so the interlocked
// flag is the only synchronisation this needs.
char g_message[96] = {};
volatile LONG g_notificationPending = 0;

} // namespace

void Apply(const config::Settings& settings) {
    if (!settings.probeEnabled)
        return;

    if (settings.probeGroup == 1) {
        g_sites = game::kProbeGroupB;
        g_count = sizeof(game::kProbeGroupB) / sizeof(uintptr_t);
    } else {
        g_sites = game::kProbeGroupA;
        g_count = sizeof(game::kProbeGroupA) / sizeof(uintptr_t);
    }

    if (g_count > game::kMaxProbeSites) {
        logging::Write("probe: group %d has more sites than slots", settings.probeGroup);
        g_count = 0;
        return;
    }

    for (size_t i = 0; i < g_count; ++i)
        g_stretch[i] = kStockStretchX;

    // Verified as a whole first, so a group that does not match the expected
    // encoding leaves the executable untouched rather than half repointed.
    if (!patch::VerifyOperands(g_sites, g_count, game::kStretchX)) {
        logging::Write("probe: group %d has unexpected bytes, not applied", settings.probeGroup);
        g_count = 0;
        return;
    }

    for (size_t i = 0; i < g_count; ++i) {
        if (!patch::RepointOperands(g_sites + i, 1, game::kStretchX, &g_stretch[i])) {
            logging::Write("probe: site %u failed to repoint", static_cast<unsigned>(i));
            g_count = 0;
            return;
        }
    }

    logging::Write("probe                  patched group %d (%u sites), step with the probe command",
                   settings.probeGroup, static_cast<unsigned>(g_count));
    for (size_t i = 0; i < g_count; ++i)
        logging::Write("  probe %2u  0x%08X", static_cast<unsigned>(i + 1), static_cast<unsigned>(g_sites[i]));
}

void UpdateSelection(const config::Settings& settings) {
    if (g_count == 0)
        return;

    for (size_t i = 0; i < g_count; ++i) {
        const bool corrected = g_index == static_cast<int>(g_count) || g_index == static_cast<int>(i);
        g_stretch[i] = corrected ? g_squareStretch : kStockStretchX;
    }

    if (g_index < 0) {
        std::snprintf(g_message, sizeof(g_message), "~w~Probe: none, group %d", settings.probeGroup);
        logging::Write("probe: none");
    } else if (g_index == static_cast<int>(g_count)) {
        std::snprintf(g_message, sizeof(g_message), "~w~Probe: all %u sites, group %d", static_cast<unsigned>(g_count),
                      settings.probeGroup);
        logging::Write("probe: all %u sites", static_cast<unsigned>(g_count));
    } else {
        std::snprintf(g_message, sizeof(g_message), "~w~Probe %u/%u  0x%08X", static_cast<unsigned>(g_index + 1),
                      static_cast<unsigned>(g_count), static_cast<unsigned>(g_sites[g_index]));
        logging::Write("probe: %u/%u at 0x%08X", static_cast<unsigned>(g_index + 1), static_cast<unsigned>(g_count),
                       static_cast<unsigned>(g_sites[g_index]));
    }

    InterlockedExchange(&g_notificationPending, 1);
}

void ServiceCommand(const config::Settings& settings) {
    if (!cheat_command::Consume(cheat_command::Command::Probe))
        return;
    if (!settings.probeEnabled || g_count == 0)
        return;

    ++g_index;
    if (g_index > static_cast<int>(g_count))
        g_index = -1;
    UpdateSelection(settings);
}

void ShowPendingNotification() {
    if (InterlockedExchange(&g_notificationPending, 0) == 0)
        return;
    game_api::AddMessage(g_message, 3000);
}

void SetSquareStretch(float squareStretch) {
    g_squareStretch = squareStretch;
}

} // namespace probe
