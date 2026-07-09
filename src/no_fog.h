// rof2ClientPlus - remove the client's distance fog (the depth-based haze).
//
// RoF2 draws distance fog as fixed-function D3D fog: the client turns on
// D3DRS_FOGENABLE for the world scene pass so geometry greys out by depth
// (confirmed by bitmap_font.cpp, which disables that exact state so its glyphs
// aren't greyed). We hook the shared d3d9 device's SetRenderState and force
// D3DRS_FOGENABLE to FALSE whenever the feature is on. That suppresses the haze
// in every zone, day and night, without touching the zone's fog data (which in
// RoF2 is a heap object behind a pointer, not the fixed address the stale TAKP
// headers claim).
//
// '/rcpfog on|off' toggles it; the choice persists in rof2ClientPlus.ini.
#pragma once

class RcpService;

class NoFog {
 public:
  explicit NoFog(RcpService *rcp);

 private:
  RcpService *rcp_;
};

// Accessors for the options-window UI to read/write the live fog setting.
namespace no_fog_settings {
bool get_enabled();       // true = distance fog removed.
void set_enabled(bool on);  // Applies live (next frame) + persists to ini.
}  // namespace no_fog_settings
