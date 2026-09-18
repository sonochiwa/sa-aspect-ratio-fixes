#pragma once

#include "config.h"

// Diagnostic. Corrects one candidate SCREEN_STRETCH_X site at a time so it
// can be identified in game before it becomes a module. Off unless the INI
// asks for it.
namespace probe {

// Repoints every site of the selected group at its own slot.
void Apply(const config::Settings& settings);
// Publishes the current selection into the slots and describes it in the
// log and on screen.
void UpdateSelection(const config::Settings& settings);
// Steps the selection on the hotkey's rising edge: none, each site in turn,
// the whole group, none again.
void ServiceHotkey(const config::Settings& settings, bool& wasDown);
// Run by the HUD hook on the game thread.
void ShowPendingNotification();
// The horizontal factor a corrected slot holds.
void SetSquareStretch(float squareStretch);

} // namespace probe
