// rof2ClientPlus - third-person chase camera (ZealCam Phase 2), stock RoF2 port.
//
// Adjusts the stock mouse-wheel "follow" camera (cameraType 6, Cameras[6]) by
// tail-detouring that camera's per-frame positioner. To stay geometrically
// correct we reuse the client's OWN computed camera-behind vector (it already
// honors heading + left-click orbit and keeps the player centered) and only
// rescale the distance / raise the height along that vector - rather than
// recomputing the orbit from scratch, which is where the convention/centering
// bugs came from. Self-contained with raw stock-RoF2 addresses (mouse_mods.cpp
// style); the vendored camera_mods.cpp is a non-portable TAKP reference.
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
float get_distance();  // 0 = follow the client's native wheel zoom distance.
float get_height();    // World-unit Z offset added above the native camera height.
bool get_collision();  // Pull the camera in when a wall blocks the view.
void set(bool enabled, float distance, float height, bool collision);  // Applies live + persists.
}  // namespace chase_settings
