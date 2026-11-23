#pragma once

// SA-MP's /hudscalefix writes values of its own into the HUD's two stretch
// factors before CHud::DrawRadar and CHud::DrawCrossHairs and puts the old
// ones back afterwards. The radar and crosshair corrections are computed
// against the stock factors, so a pass they own holds a scope that sets the
// stock factors back for its duration.
namespace stock_stretch {

class Scope {
public:
    explicit Scope(bool enabled);
    ~Scope();

    Scope(const Scope&) = delete;
    Scope& operator=(const Scope&) = delete;

private:
    float savedX;
    float savedY;
    bool replaced;
};

} // namespace stock_stretch
