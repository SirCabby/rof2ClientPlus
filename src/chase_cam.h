// rof2ClientPlus - third-person chase camera (ZealCam Phase 2), stock RoF2 port.
//
// Adjusts the stock mouse-wheel "follow" camera (cameraType 6, Cameras[6]) by
// patching gfMaxZoomCameraDistance - the .rdata float the wheel handler clamps
// its zoom accumulator against - so the wheel can zoom out farther than the
// native 53 while staying fully functional. Self-contained with raw stock-RoF2
// addresses (mouse_mods.cpp style); the vendored camera_mods.cpp is a
// non-portable TAKP reference.
//
// See PORTING_NOTES.md (Phase 2) for the full RE.
#pragma once

class RcpService;

class ChaseCam {
 public:
  ChaseCam(RcpService *rcp);

 private:
  RcpService *rcp_ = nullptr;
};

// Accessors for the options-window UI to read/write the live chase settings.
namespace chase_settings {
bool get_enabled();
float get_max_distance();  // Max wheel zoom-out distance; 0 = the native max (53).
void set(bool enabled, float max_distance);  // Applies live + persists.
}  // namespace chase_settings
