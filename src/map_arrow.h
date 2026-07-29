// rof2ClientPlus - scale the map window's player position arrow (/rcpmaparrow).
//
// MapViewMap::Draw (0x6cd4a0, reached from the PostDraw override at 0x6cfb00)
// draws the local player as three clipped lines in SCREEN pixels, zoom-independent:
// a stem from heading-dir * -4.0 (tail) to heading-dir * +8.0 (tip; the 8 is
// 4.0 * an in-code 2.0 ratio) and two barbs of length 6.0 at heading +/- 48/512
// turns, all joined at the tip; plus a small "+" fallback variant (-2.0..+3.0 px
// per axis) behind a display gate. Those magnitudes are POOLED .rdata float
// constants shared binary-wide, so their values cannot be edited in place;
// instead each drawing instruction's absolute operand is redirected to
// DLL-owned floats holding stock * scale. See map_arrow.cpp for the site table.
#pragma once

class RcpService;

class MapArrow {
 public:
  explicit MapArrow(RcpService *rcp);

 private:
  RcpService *rcp_;
};

// Accessors for the options-window UI (Display-tab slider binds to these).
namespace map_arrow_settings {
float get_scale();          // Arrow size multiplier; 1.0 == the stock arrow.
void set_scale(float s);    // Clamps to [scale_min, scale_max], applies live + persists.
float scale_min();          // 0.5 (half the stock size).
float scale_max();          // 8.0 (command ceiling; the slider stops earlier).
}  // namespace map_arrow_settings
