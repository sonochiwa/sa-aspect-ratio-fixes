#pragma once

#include "config.h"
#include "types.h"

// Text in general. Every caller of CFont::SetScale passes a width it has
// multiplied by the real SCREEN_STRETCH_X; the hook multiplies it again by
// the ratio of the wanted pixels per unit to the real ones, except inside a
// textdraw draw or a HUD pass, which scale their own text. The font's
// outline pass reads a factor of its own that follows whatever text is being
// printed: the layout's for a textdraw, the block's during a HUD pass, the
// text module's for everything else.
namespace text {

// The font's outline sites are repointed at the module's factor.
void ApplyFontOutline();
// The GetTextRect call inside CFont::PrintString, taken only once the
// client's draw has been hooked.
void ApplyFontBoxHook();
// The two scale setters, in or out as a whole.
void ApplyTextScale(const config::Settings& settings, const Resolution& resolution);

void UpdateLayout(const config::Settings& settings, float screenWidth, float screenHeight);

// Bracket a CTextDraw::Draw call. `scale` is the layout width over the screen
// width.
void BeginTextdrawDraw(float scale);
void EndTextdrawDraw();
// Bracket the print inside a textdraw draw, where the font reads the width.
void BeginTextdrawPrint();
void EndTextdrawPrint();
// Bracket a HUD pass that scales its own text by `stretchX`.
void BeginHudPass(float stretchX);
void EndHudPass();

} // namespace text
