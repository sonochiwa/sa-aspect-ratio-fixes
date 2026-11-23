#include "stock_stretch.h"

#include "addresses.h"
#include "patch.h"
#include "types.h"

namespace stock_stretch {
namespace {

float& StretchX() {
    return *reinterpret_cast<float*>(game::kStretchX);
}

float& StretchY() {
    return *reinterpret_cast<float*>(game::kStretchY);
}

} // namespace

// The factors are pooled literals in .rdata. Whatever changed them had to
// make their page writable first, but that is checked rather than assumed,
// so a modification that wrote them and protected the page again leaves the
// pass as it found it instead of faulting.
Scope::Scope(bool enabled) : savedX(kStockStretchX), savedY(kStockStretchY), replaced(false) {
    if (!enabled)
        return;

    savedX = StretchX();
    savedY = StretchY();
    if (savedX == kStockStretchX && savedY == kStockStretchY)
        return;

    constexpr size_t kFactorsSize = game::kStretchY + sizeof(float) - game::kStretchX;
    if (!patch::IsWritable(game::kStretchX, kFactorsSize))
        return;

    StretchX() = kStockStretchX;
    StretchY() = kStockStretchY;
    replaced = true;
}

Scope::~Scope() {
    if (!replaced)
        return;

    StretchX() = savedX;
    StretchY() = savedY;
}

} // namespace stock_stretch
