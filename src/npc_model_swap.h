// rof2ClientPlus - NPC/creature body-model classic-vs-new swap (Phase 2). See npc_model_swap.cpp.
//
// The client picks a spawn's body model by an actordef NAME ("ORC_ACTORDEF", "SKE_ACTORDEF", ...):
// per NPC, CRender::BuildActorForSpawn (F_48ff@0x48f4b0) derives a race->code, appends "_ACTORDEF",
// stores it via SetActorDefName@0x59e4d0 (strcpy -> spawn+0xEBA), then FindActorDefByName@0x4066f0 /
// CreateActorDefinition@0x408030 re-read spawn+0xEBA to build+attach the mesh. We detour
// SetActorDefName (the write-side choke) and substitute the name, redirecting the body model
// cache-safely -- the exact body-model analog of the held-weapon SetHeldModel redirect. This is the
// mechanism-spike probe: /rcpbody logs each per-spawn actordef and can redirect one live (on zone-in).
#pragma once

#include <string>
#include <vector>

class RcpService;
class HookWrapper;

class NpcModelSwap {
 public:
  explicit NpcModelSwap(RcpService *rcp);

 private:
  RcpService *rcp_;
};

// Called from DllMain (PROCESS_ATTACH): loads the [NpcModels]/[PcModels] ini state and arms the four
// model seams (BuildActor redirect, resolver alias rewrite, ShouldUseLuclin classic-force, classic-helm
// gate) BEFORE client init. The char-select scene -- clz world + the preview player -- is built during
// startup, before the first ProcessGameEvents tick where RcpService constructs, so ctor-time hooks miss
// it and char select ignores every model-swap setting. The ctor skips these installs when this ran.
namespace npc_model_swap_api {
void install_early(HookWrapper *hooks);
}  // namespace npc_model_swap_api

// Consumed by the /rcpoptions Models tab (NPC section): the curated classic-creature catalog + state.
namespace npc_model_settings {
struct ToggleItem {
  std::string name;  // creature display name (e.g. "Wolf")
  bool on;           // true = currently reverted to classic
};
std::vector<ToggleItem> get_items();
void set_on(const std::string &name, bool on);  // revert this creature to classic (on) / modern (off)
void set_all(bool on);
}  // namespace npc_model_settings
