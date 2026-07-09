// rof2ClientPlus - raise the world/terrain view distance (the far clip plane).
//
// RoF2's "Clip Plane" display option ultimately sets the far clip plane on the
// render camera: the client stores a distance into the camera object's +0x8
// field and feeds it to the graphics engine's camera when it rebuilds the
// projection. That field is only written on discrete events (slider move,
// settings apply, zone load), never per frame, so the source integer isn't a
// live knob - we call the client's own camera-clip setter (0x4940a0) directly
// with a far distance in world units and re-assert it whenever a zone reset
// restores the stock value. Fog is already removed by no_fog, so nothing hides
// the newly-visible geometry. There is no upper clamp in the client path (the
// only stock ceiling is the options slider's range, which we bypass).
//
// Actors (NPCs/players) use a SEPARATE clip field on the same camera object; we
// drive it the same way so distant mobs render too. Shadows have their own clip
// field which we deliberately never touch.
//
// '/rcpviewdist <n>' forces the terrain far clip to n world units (higher =
// farther); '/rcpactordist <n>' does the same for actors. 'off' on either
// restores the client default; bare '/rcpviewdist' prints status (including the
// live values, so you can see the stock baseline). Both persist in
// rof2ClientPlus.ini. See the view-distance-re memory and PORTING_NOTES for the
// reverse-engineering behind the addresses.
#pragma once

class RcpService;

class ViewDistance {
 public:
  explicit ViewDistance(RcpService *rcp);

 private:
  RcpService *rcp_;
};

// Accessors for the options-window UI (Display-tab controls bind to these).
namespace view_distance_settings {
int get_clip();                // Terrain far clip, world units; 0 == off (client default).
void set_clip(int clip);       // Applies live + persists to ini. 0 disables.
int get_actor_clip();          // Actor (NPC/player) draw distance, world units; 0 == off.
void set_actor_clip(int clip); // Applies live + persists to ini. 0 disables. Never touches shadows.
}  // namespace view_distance_settings
