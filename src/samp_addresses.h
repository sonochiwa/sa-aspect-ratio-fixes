// Offsets into the SA-MP client, one row per build.
//
// SA-MP draws a textdraw in the same 640x448 space as the HUD and maps it to
// the framebuffer the same way, reading RsGlobal.maximumWidth for the
// horizontal factor and RsGlobal.maximumHeight for the vertical one. A server
// lays its textdraws out for a 16:9 display, so on a wider screen every one of
// them is stretched by aspect / (16/9).
//
// CTextDraw::Draw reads the width exactly once on each of its two paths, the
// text path and the sprite path, and both loads encode the address as a 32 bit
// absolute operand. The plugin repoints those two operands at a width of its
// own, which makes the whole function lay out as if the screen were 16:9, and
// then adds the pillarbox offset to the four places an X position leaves the
// function: the text print, the wrap edge, the sprite rectangle and the click
// rectangle the selection code reads back. Widths and scales need no offset,
// so the scale and centre-size calls are left as they are.
//
// The function is the same compiler output in every build mapped here; the
// R3-1 copy was compared against R1 instruction by instruction and only the
// addresses differ. A build is identified by its PE link timestamp, and every
// offset is still verified against the loaded module before anything is
// written.

#pragma once

#include <cstddef>
#include <cstdint>

namespace samp {

struct Build {
    const char* name;
    // IMAGE_FILE_HEADER.TimeDateStamp
    uint32_t timeDateStamp;

    // CTextDrawPool::Draw walks the pool and calls CTextDraw::Draw once per
    // textdraw. That call is the one place every textdraw passes through, so
    // it is where the layout is published for the frame and the click
    // rectangle is shifted after the original has stored it.
    // call CTextDraw::Draw
    uintptr_t drawTextDrawCall;
    // CTextDraw::Draw
    uintptr_t textDrawDraw;

    // The two RsGlobal.maximumWidth loads, one per path.
    // fild dword ptr [C17044]
    uintptr_t spriteWidthRead;
    // mov  eax, [C17044]
    uintptr_t textWidthRead;

    // Calls that carry an X position out of CTextDraw::Draw. Each targets a
    // small cdecl forwarder inside samp.dll; the plugin retargets the call and
    // forwards to the same function with the position shifted.
    //
    //   sprite path: DrawSprite(CSprite2d*, const CRect*, const CRGBA*)
    //   text path:   CFont::SetWrapx(float) and CFont::PrintString(float x,
    //                float y, const char*)
    uintptr_t spriteDrawCall;
    uintptr_t spriteDrawForward;
    uintptr_t setWrapxCall;
    uintptr_t setWrapxForward;
    uintptr_t printStringCall;
    uintptr_t printStringForward;
};

constexpr Build kBuilds[] = {
    // 2,199,552 bytes, linked 2015-05-01.
    {"0.3.7-R1", 0x5542F47A,
     0x1AD61, 0xACD90,
     0xAC9A3, 0xACAF3,
     0xACA44, 0xAC660, 0xACBC0, 0x9B550, 0xACCB9, 0x9B480},
    // 1,204,224 bytes, linked 2018-12-08.
    {"0.3.7-R3-1", 0x5C0B4243,
     0x1E101, 0xB2BF0,
     0xB2803, 0xB2953,
     0xB28A4, 0xB2410, 0xB2A20, 0x9F800, 0xB2B19, 0x9F730},
};

constexpr uint8_t kSpriteWidthReadBytes[] = {
    0xDB, 0x05, 0x44, 0x70, 0xC1, 0x00, // fild dword ptr ds:[C17044]
};
constexpr uint8_t kTextWidthReadBytes[] = {
    0xA1, 0x44, 0x70, 0xC1, 0x00,       // mov eax, dword ptr ds:[C17044]
};

// CTextDraw fields. The draw stores the rectangle a selectable textdraw is
// hit-tested against, as int32 pixels, and the selection code reads it back.
constexpr size_t kClickLeftOffset = 0x9C1;
constexpr size_t kClickRightOffset = 0x9C9;

}  // namespace samp
