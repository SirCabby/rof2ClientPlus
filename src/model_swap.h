// rof2ClientPlus - live classic/new held-model toggle (Items; NPCs later). See model_swap.cpp.
//
// A curated catalog of held-item models that were REVAMPED (have both a classic and a modern
// version) can be switched back to classic LIVE (no zone/relog). The client attaches held models
// by tag via SetHeldModel@0x5923f0; we detour it and, for a model set to "classic", substitute
// the item's tag IT<modern> -> RCPIT<classic> (an isolated alias in the mod-shipped rcpclassic.s3d).
// The revamp map (modern->classic) was derived exactly by diffing a pre-revamp TAKP (Al'Kabor,
// 2002) item DB against the modern DB, so only genuinely-revamped models are listed and new-only
// weapons are excluded. Per-model state persists in [Models] of the rcp ini.
#pragma once

#include <string>
#include <vector>

class RcpService;

class ModelSwap {
 public:
  explicit ModelSwap(RcpService *rcp);

 private:
  RcpService *rcp_;
};

// Consumed by the /rcpoptions Models tab.
namespace model_settings {
struct ToggleItem {
  int modern;        // the IT model number items use (the modern/current graphic)
  int classic;       // the classic IT model to render instead (as RCPIT<classic>)
  std::string name;  // display name (weapon type, e.g. "2H Sword")
  bool on;           // true = currently rendering the classic version
};
std::vector<ToggleItem> get_items();  // the full revamp catalog + current state
void set_on(int modern, bool on);     // revert this model to classic (on) / new (off); persists + live
void set_all(bool on);                // apply/clear the whole catalog
}  // namespace model_settings

// Consumed by the body-model swap (npc_model_swap.cpp): a live actor rebuild strips the held-item
// attachments, and the wear-slot redress there only covers armor materials + head. Re-attaches this
// spawn's last-seen held models (slots 7/8, the exact tags the client resolved, classic redirect
// applied) onto its freshly built actor. No-op for spawns never seen holding anything. Main-thread only.
namespace model_swap_api {
void reattach_held(void *spawn);
}  // namespace model_swap_api
