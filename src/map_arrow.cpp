// rof2ClientPlus - scale the map window's player position arrow. See map_arrow.h.
//
// RE map (2013-05-10 build, disasm-verified): the arrow block sits at
// 0x6ce989..0x6ced4d inside MapViewMap::Draw. The player's screen point comes
// from TransformPoint (0x6c87a0); heading (PlayerClient+0x80, 0..512) feeds a
// sin/cos-table object (global 0x15d46b4, vtbl +0x8/+0xC) to build the
// direction vector; lines go out through DrawClippedLine (0x6cb230) in
// myColor (this+0x270). Magnitudes are x87 loads of pooled .rdata floats:
//
//   arrow variant            constant  sites (instruction VAs)
//   stem vector              4.0       0x6cea37 0x6cea5d          (fmuls d8 0d)
//   barb vectors             6.0       0x6cea8a 0x6ceab7 0x6ceae4 0x6ceb11
//   "+" variant, -2 arm      2.0       0x6cec45 0x6cece0          (flds  d9 05)
//   "+" variant, +3 arm      3.0       0x6cec75 0x6cecff          (fadds d8 05)
//
// Each site's 4-byte absolute operand is redirected to a DLL float holding
// stock * scale, so changing the scale later is a plain float store (no code
// rewrite). Deliberately NOT redirected: the 2.0 load at 0x6cec0d, which
// doubles the already-scaled stem to place the tip (a shape ratio - scaling it
// too would grow the tip by scale^2), and the +/-48 barb-angle adds
// (0x9eeac0/0x9eeabc), which are angles, not sizes. The group-member markers
// and find-path drawing use separate code and are untouched.
#include "map_arrow.h"

#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "commands.h"
#include "game_functions.h"
#include "io_ini.h"
#include "logger.h"
#include "memory.h"
#include "rcp.h"
#include "rcp_profiles.h"
#include "rebase.h"

static constexpr char kIniSection[] = "MapArrow";
static constexpr char kIniKey[] = "Scale";
static constexpr float kScaleMin = 0.5f;
static constexpr float kScaleMax = 8.0f;

static float g_scale = 1.0f;

// The redirected operand targets. The stock geometry is FROZEN here on purpose
// (not read back from .rdata): these are what the four pooled constants mean in
// the arrow block, and apply_scale() derives every live value from them.
static float g_stem = 4.0f;      // stem vector length (tip adds the in-code x2)
static float g_barb = 6.0f;      // barb vector length
static float g_plus_neg = 2.0f;  // "+" variant: arm toward -x/-y
static float g_plus_pos = 3.0f;  // "+" variant: arm toward +x/+y

struct PatchSite {
  uintptr_t addr;       // instruction VA (preferred base; eqva() applied at use)
  uint8_t op0, op1;     // expected x87 opcode bytes at addr
  uintptr_t stock;      // expected operand: the pooled constant's VA (preferred base)
  float *replacement;   // DLL float the operand is redirected to
};

static const PatchSite kSites[] = {
    {0x6cea37, 0xd8, 0x0d, 0x9c5764, &g_stem},      // fmuls 4.0 (stem, x component)
    {0x6cea5d, 0xd8, 0x0d, 0x9c5764, &g_stem},      // fmuls 4.0 (stem, y component)
    {0x6cea8a, 0xd8, 0x0d, 0x9c7da0, &g_barb},      // fmuls 6.0 (barb 1, x)
    {0x6ceab7, 0xd8, 0x0d, 0x9c7da0, &g_barb},      // fmuls 6.0 (barb 1, y)
    {0x6ceae4, 0xd8, 0x0d, 0x9c7da0, &g_barb},      // fmuls 6.0 (barb 2, x)
    {0x6ceb11, 0xd8, 0x0d, 0x9c7da0, &g_barb},      // fmuls 6.0 (barb 2, y)
    {0x6cec45, 0xd9, 0x05, 0x9c58e8, &g_plus_neg},  // flds  2.0 ("+", horizontal)
    {0x6cec75, 0xd8, 0x05, 0x9c3920, &g_plus_pos},  // fadds 3.0 ("+", horizontal)
    {0x6cece0, 0xd9, 0x05, 0x9c58e8, &g_plus_neg},  // flds  2.0 ("+", vertical)
    {0x6cecff, 0xd8, 0x05, 0x9c3920, &g_plus_pos},  // fadds 3.0 ("+", vertical)
};
static constexpr int kSiteCount = sizeof(kSites) / sizeof(kSites[0]);

static bool g_installed = false;

static void apply_scale() {
  g_stem = 4.0f * g_scale;
  g_barb = 6.0f * g_scale;
  g_plus_neg = 2.0f * g_scale;
  g_plus_pos = 3.0f * g_scale;
}

static float clamp_scale(float s) { return s < kScaleMin ? kScaleMin : (s > kScaleMax ? kScaleMax : s); }

// Verify every site before touching any: the operand bytes must still be the
// stock pooled-constant address (relocated by the loader, hence eqva on the
// expected value too). A mismatch means a different build or a foreign patch -
// abort the whole feature rather than corrupt one instruction.
static bool install_patches() {
  for (const PatchSite &s : kSites) {
    const uint8_t *p = reinterpret_cast<const uint8_t *>(Rcp::eqva(s.addr));
    uint32_t disp;
    memcpy(&disp, p + 2, sizeof(disp));
    if (p[0] != s.op0 || p[1] != s.op1 || disp != Rcp::eqva(s.stock)) {
      logger::logf("[maparrow] site 0x%x mismatch (%02x %02x disp=0x%x, expected %02x %02x disp=0x%x) - NOT installed",
                   (unsigned)s.addr, p[0], p[1], disp, s.op0, s.op1, (unsigned)Rcp::eqva(s.stock));
      return false;
    }
  }
  for (const PatchSite &s : kSites)
    mem::write<uint32_t>(static_cast<int>(Rcp::eqva(s.addr)) + 2, reinterpret_cast<uint32_t>(s.replacement));
  g_installed = true;
  return true;
}

static void load_settings() {
  IO_ini ini(IO_ini::kRcpIniFilename);
  if (ini.exists(kIniSection, kIniKey)) g_scale = clamp_scale(ini.getValue<float>(kIniSection, kIniKey));
  apply_scale();
}

static void save_settings() {
  IO_ini ini(IO_ini::kRcpIniFilename);
  ini.setValue<float>(kIniSection, kIniKey, g_scale);
}

namespace map_arrow_settings {
float get_scale() { return g_scale; }
void set_scale(float s) {
  g_scale = clamp_scale(s);
  apply_scale();
  save_settings();
}
float scale_min() { return kScaleMin; }
float scale_max() { return kScaleMax; }
}  // namespace map_arrow_settings

static void print_status() {
  if (!g_installed) {
    Rcp::Game::print_chat("rof2ClientPlus map arrow: NOT installed (byte mismatch; see log)");
    return;
  }
  Rcp::Game::print_chat("rof2ClientPlus map arrow size: %.2fx%s (/rcpmaparrow <%.1f-%.1f>, 1 = stock)", g_scale,
                        g_scale == 1.0f ? " (stock)" : "", kScaleMin, kScaleMax);
}

MapArrow::MapArrow(RcpService *rcp) : rcp_(rcp) {
  load_settings();
  // Settings profiles: re-read + re-apply on profile switch (rcp_profiles.h).
  rcp_profiles::add_reload_handler([] {
    load_settings();
  });

  if (install_patches())
    logger::logf("[maparrow] %d operand redirects installed, scale=%.2f", kSiteCount, g_scale);

  rcp->commands_hook->Add(
      "/rcpmaparrow", {},
      "Map player-arrow size: '/rcpmaparrow 2.5' draws it 2.5x, '/rcpmaparrow off' restores stock, bare prints status.",
      [](std::vector<std::string> &args) {
        if (args.size() >= 2) {
          const std::string &a = args[1];
          if (a == "off" || a == "stock")
            map_arrow_settings::set_scale(1.0f);
          else {
            float s = static_cast<float>(std::atof(a.c_str()));
            if (s <= 0.0f) {
              Rcp::Game::print_chat("Usage: /rcpmaparrow <%.1f-%.1f> | off", kScaleMin, kScaleMax);
              return true;
            }
            map_arrow_settings::set_scale(s);
          }
        }
        print_status();
        return true;
      });
  logger::log("[maparrow] /rcpmaparrow registered");
}
