// rof2ClientPlus - solid-color target ring. See target_ring.h.
#include "target_ring.h"

#include <windows.h>
#include <d3d9.h>
#include <d3dx9.h>  // D3DXCreateTextureFromFileA (optional ring graphic).

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

#include "bitmap_font.h"  // D3DRenderStateStash / D3DTextureStateStash (shared mid-scene state helpers).
#include "commands.h"
#include "font_overlay.h"
#include "game_functions.h"
#include "io_ini.h"
#include "logger.h"
#include "nameplate.h"  // nameplate::con_color_for (shared con palette + level-band table).
#include "rcp.h"

namespace {

constexpr char kIniSection[] = "TargetRing";

// ---- stock RoF2 globals + entity offsets (disasm-confirmed; see nameplate.cpp / font_overlay.cpp).
void **const kTarget = reinterpret_cast<void **>(0xDD2648);  // current target entity
void **const kSelf = reinterpret_cast<void **>(0xDD2630);    // local player
// Position floats consumed by the renderer in memory order (0x64,0x68,0x6c), 0x6c the vertical (EQ Z,
// up). A circle is rotationally symmetric, so the EQ X/Y ordering is irrelevant - the ring lies flat
// in the render horizontal plane regardless.
//
// NOTE: Z@0x6c is a POSITION REFERENCE that sits ABOVE the model's feet (which is why the billboards
// found "feet+AvatarHeight was way off for NPCs" and switched to the head-sprite anchor). The native
// target ring sits on the floor, so for the vertical we use FloorHeight@0x28 - the client's own
// absolute Z of the floor beneath the entity (eqlib PlayerBase; this struct matches our build at
// 0x64/0x68/0x6c/0x80/0x125/0x138, so 0x28 is valid too) - falling back to Z@0x6c if it's unset.
constexpr int kEntPos0 = 0x64;
constexpr int kEntPos1 = 0x68;
constexpr int kEntPos2 = 0x6c;         // vertical position reference (above the feet)
constexpr int kEntFloorHeight = 0x28;  // float: absolute Z of the floor beneath the entity
constexpr int kEntActor = 0x101c;      // CActor*; NULL => not in-world / no graphics yet.

// ---- live settings (persisted to rof2ClientPlus.ini [TargetRing]). Off by default. ----
bool g_enabled = false;
int g_color = 0xff8000;       // 0xRRGGBB fixed color (used when con coloring is off).
bool g_use_con_color = false;  // true = color by the target's con level instead of g_color.
float g_outer = 8.0f;         // outer radius, world units
float g_inner = 6.0f;    // inner radius (donut hole), world units
float g_opacity = 0.85f;  // 0..1
bool g_hide_self = true;
std::string g_graphic;    // Optional ring graphic (Zeal-style texture). "" = solid donut, no texture.

int g_z_log = 0;  // Log the computed ring Z (pos vs floor) for a few frames after enabling.

constexpr float kRadiusMax = 30.0f;    // slider/command upper bound for both radii
constexpr int kNumSegments = 96;       // ring smoothness (triangle-strip segments)
constexpr float kGroundLift = 0.20f;   // raise slightly above the feet plane to avoid z-fighting the ground

// XYZ (untransformed, world-space) + diffuse color + one texture coord set: fixed-function T&L
// transforms it by WORLD*VIEW*PROJECTION, which is exactly a flat world-space ring. The UVs are
// only consumed in the textured (graphic) draw path; the solid path ignores them.
struct RingVertex {
  float x, y, z;
  D3DCOLOR color;
  float u, v;
};
constexpr DWORD kRingFvf = D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX1;

float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

// ---- optional ring graphic (Zeal-style textured ring) --------------------------------------
// Zeal ships 261x2048 RGBA "strip" textures (u = inner..outer across the band width, v = around
// the circle) under uifiles/<skin>/targetrings; we do the same under uifiles/rcp/targetrings and
// map the donut's UVs identically, so a Zeal ring .tga renders the same here.
constexpr char kGraphicSubDir[] = "targetrings";
constexpr char kGraphicExt[] = ".tga";

// Render-thread-owned texture cache. The setter (main thread) only assigns g_graphic + sets the
// dirty flag; all D3D work (create/release) happens on the render thread the next frame it draws.
IDirect3DTexture9 *g_texture = nullptr;  // Loaded ring graphic, or null for the solid donut.
std::string g_texture_name;              // Which g_graphic g_texture currently reflects.
bool g_graphic_dirty = true;             // g_graphic changed; the render thread (re)loads.

bool graphic_is_none(const std::string &s) {
  return s.empty() || s == "none" || s == "None" || s == "NONE";
}

// uifiles/rcp/targetrings next to the game exe (same resolution the font engine uses for fonts).
std::filesystem::path get_graphics_dir() {
  char module_path[MAX_PATH] = {};
  GetModuleFileNameA(NULL, module_path, MAX_PATH);
  std::filesystem::path base = std::filesystem::path(module_path).parent_path();
  return base / "uifiles" / "rcp" / kGraphicSubDir;
}

// Colors persist as a readable "RRGGBB" hex string (like [NameplateColors]), not a decimal int.
int parse_hex_color(const std::string &s) {
  const char *p = s.c_str();
  if (*p == '#') ++p;
  return static_cast<int>(std::strtoul(p, nullptr, 16) & 0xffffff);
}

void load_settings() {
  IO_ini ini(IO_ini::kRcpIniFilename);
  if (ini.exists(kIniSection, "Enabled")) g_enabled = ini.getValue<bool>(kIniSection, "Enabled");
  if (ini.exists(kIniSection, "Color")) g_color = parse_hex_color(ini.getValue<std::string>(kIniSection, "Color"));
  if (ini.exists(kIniSection, "ConColor")) g_use_con_color = ini.getValue<bool>(kIniSection, "ConColor");
  if (ini.exists(kIniSection, "Outer")) g_outer = ini.getValue<float>(kIniSection, "Outer");
  if (ini.exists(kIniSection, "Inner")) g_inner = ini.getValue<float>(kIniSection, "Inner");
  if (ini.exists(kIniSection, "Opacity")) g_opacity = ini.getValue<float>(kIniSection, "Opacity");
  if (ini.exists(kIniSection, "HideSelf")) g_hide_self = ini.getValue<bool>(kIniSection, "HideSelf");
  if (ini.exists(kIniSection, "Graphic")) g_graphic = ini.getValue<std::string>(kIniSection, "Graphic");
  if (graphic_is_none(g_graphic)) g_graphic.clear();
  g_graphic_dirty = true;  // Load the persisted graphic on the first drawn frame.
  g_color &= 0xffffff;
  g_outer = clampf(g_outer, 0.0f, kRadiusMax);
  g_inner = clampf(g_inner, 0.0f, kRadiusMax);
  g_opacity = clampf(g_opacity, 0.0f, 1.0f);
}

void save_settings() {
  IO_ini ini(IO_ini::kRcpIniFilename);
  ini.setValue<bool>(kIniSection, "Enabled", g_enabled);
  char hex[8];
  std::snprintf(hex, sizeof(hex), "%06X", g_color & 0xffffff);
  ini.setValue<std::string>(kIniSection, "Color", hex);
  ini.setValue<bool>(kIniSection, "ConColor", g_use_con_color);
  ini.setValue<float>(kIniSection, "Outer", g_outer);
  ini.setValue<float>(kIniSection, "Inner", g_inner);
  ini.setValue<float>(kIniSection, "Opacity", g_opacity);
  ini.setValue<bool>(kIniSection, "HideSelf", g_hide_self);
  ini.setValue<std::string>(kIniSection, "Graphic", g_graphic.empty() ? "None" : g_graphic);
}

// Draw the ring at the shared post-world / pre-UI in-scene seam. Already inside directx's rcp_guard
// (registered via font_overlay::add_scene_draw), so a stale entity degrades to a dropped frame.
void draw_ring(IDirect3DDevice9 *device) {
  if (!g_enabled || !device) return;
  if (!Rcp::Game::is_in_game()) return;  // At login/char-select the scene still renders; don't draw there.

  // (Re)load the ring graphic here on the render thread against the live device, inside rcp_guard.
  // D3DXCreateTextureFromFile uses D3DPOOL_MANAGED, so the texture survives device resets and this
  // only re-runs when the selection actually changes (dirty flag set by the setter / on load).
  if (g_graphic_dirty) {
    g_graphic_dirty = false;
    if (g_texture) {
      g_texture->Release();
      g_texture = nullptr;
    }
    g_texture_name.clear();
    const std::string name = g_graphic;  // Snapshot (assigned from the main thread).
    if (!graphic_is_none(name)) {
      const std::string path = (get_graphics_dir() / (name + kGraphicExt)).string();
      HRESULT hr = D3DXCreateTextureFromFileA(device, path.c_str(), &g_texture);
      if (FAILED(hr)) {
        g_texture = nullptr;
        logger::logf("[ring] graphic load failed (0x%08x): %s", (unsigned)hr, path.c_str());
      } else {
        g_texture_name = name;
        logger::logf("[ring] graphic loaded: %s", name.c_str());
      }
    }
  }
  const bool use_texture = (g_texture != nullptr);

  void *target = *kTarget;
  if (!target) return;
  if (g_hide_self && target == *kSelf) return;
  char *t = static_cast<char *>(target);
  if (!*reinterpret_cast<void **>(t + kEntActor)) return;  // No graphics actor => not in-world yet.

  const float cx = *reinterpret_cast<float *>(t + kEntPos0);
  const float cy = *reinterpret_cast<float *>(t + kEntPos1);
  // Vertical: prefer the client's floor Z under the target (== foot level, where the native ring is)
  // over the position reference at 0x6c (which floats above the feet). Guard against an unpopulated /
  // absurd FloorHeight (0, or too far from the position ref) and fall back to 0x6c in that case.
  const float posZ = *reinterpret_cast<float *>(t + kEntPos2);
  const float floorZ = *reinterpret_cast<float *>(t + kEntFloorHeight);
  const float drop = posZ - floorZ;
  const bool use_floor = (floorZ != 0.0f && drop >= -10.0f && drop <= 60.0f);
  const float cz = use_floor ? floorZ : posZ;
  if (g_z_log > 0) {
    logger::logf("[ring] Z: pos=%.2f floor=%.2f drop=%.2f -> %s cz=%.2f", posZ, floorZ, drop,
                 use_floor ? "floor" : "pos", cz);
    --g_z_log;
  }

  float outer = clampf(g_outer, 0.0f, kRadiusMax);
  float inner = clampf(g_inner, 0.0f, outer);  // Inner never exceeds outer (would invert the band).
  if (outer <= 0.0f) return;                    // Nothing to draw.
  const BYTE alpha = static_cast<BYTE>(clampf(g_opacity, 0.0f, 1.0f) * 255.0f + 0.5f);
  // Color by the target's con level (shared nameplate con palette) or the fixed ring color.
  const int rgb = g_use_con_color ? nameplate::con_color_for(target) : g_color;
  const D3DCOLOR color = (static_cast<D3DCOLOR>(alpha) << 24) | (rgb & 0xffffff);

  // Donut as a closed triangle strip: at each angle emit an outer-rim then an inner-rim vertex; the
  // strip stitches successive pairs into quads. Built around the origin, translated to the target
  // via the WORLD matrix (VIEW/PROJECTION stay live from the world pass).
  RingVertex verts[(kNumSegments + 1) * 2];
  const float step = 2.0f * 3.14159265358979323846f / kNumSegments;
  int vi = 0;
  for (int i = 0; i <= kNumSegments; ++i) {
    const float ang = i * step;
    const float ca = std::cos(ang), sa = std::sin(ang);
    // Texture coords match Zeal so its ring .tga files map identically: u across the band width
    // (0 = inner rim, 1 = outer rim), v around the circumference. Ignored by the solid path.
    const float tv = 1.0f - static_cast<float>(i) / kNumSegments;
    verts[vi++] = {outer * ca, outer * sa, 0.0f, color, 1.0f, tv};  // outer rim
    verts[vi++] = {inner * ca, inner * sa, 0.0f, color, 0.0f, tv};  // inner rim
  }
  const int vertex_count = vi;             // (kNumSegments+1)*2
  const int prim_count = vertex_count - 2;  // triangle strip

  // The last world geometry can leave vertex/pixel shaders bound; a bound pixel shader overrides all
  // our fixed-function setup. Unbind both and restore afterwards (same as the billboard nameplates).
  IDirect3DVertexShader9 *prev_vs = nullptr;
  IDirect3DPixelShader9 *prev_ps = nullptr;
  device->GetVertexShader(&prev_vs);
  device->GetPixelShader(&prev_ps);
  device->SetVertexShader(NULL);
  device->SetPixelShader(NULL);

  D3DRenderStateStash rs(*device);
  rs.store_and_modify({D3DRS_CULLMODE, D3DCULL_NONE});          // Flat ring: visible from either side.
  rs.store_and_modify({D3DRS_ALPHABLENDENABLE, TRUE});
  rs.store_and_modify({D3DRS_SRCBLEND, D3DBLEND_SRCALPHA});
  rs.store_and_modify({D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA});
  rs.store_and_modify({D3DRS_BLENDOP, D3DBLENDOP_ADD});
  rs.store_and_modify({D3DRS_ZENABLE, TRUE});                  // Occluded by terrain/walls in front.
  rs.store_and_modify({D3DRS_ZWRITEENABLE, FALSE});            // Translucent: don't write depth.
  rs.store_and_modify({D3DRS_LIGHTING, FALSE});
  rs.store_and_modify({D3DRS_ALPHATESTENABLE, FALSE});
  rs.store_and_modify({D3DRS_FOGENABLE, FALSE});               // Don't let distance fog grey the ring.
  rs.store_and_modify({D3DRS_SPECULARENABLE, FALSE});

  // Disable stage 1 either way (the world pass may have left it modulating a terrain/detail texture).
  DWORD stage1_colorop = D3DTOP_DISABLE;
  device->GetTextureStageState(1, D3DTSS_COLOROP, &stage1_colorop);
  device->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);

  D3DTextureStateStash ts(*device);
  D3DSamplerStateStash ss(*device);
  if (use_texture) {
    // Modulate the ring graphic by the (diffuse) ring color + opacity, exactly like Zeal: a white
    // ring color shows the texture's own colors, any other tints it, and opacity scales the alpha.
    ts.store_and_modify({D3DTSS_COLOROP, D3DTOP_MODULATE});
    ts.store_and_modify({D3DTSS_COLORARG1, D3DTA_TEXTURE});
    ts.store_and_modify({D3DTSS_COLORARG2, D3DTA_DIFFUSE});
    ts.store_and_modify({D3DTSS_ALPHAOP, D3DTOP_MODULATE});
    ts.store_and_modify({D3DTSS_ALPHAARG1, D3DTA_TEXTURE});
    ts.store_and_modify({D3DTSS_ALPHAARG2, D3DTA_DIFFUSE});
    ss.store_and_modify({D3DSAMP_MINFILTER, D3DTEXF_LINEAR});
    ss.store_and_modify({D3DSAMP_MAGFILTER, D3DTEXF_LINEAR});
    ss.store_and_modify({D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP});  // Radial: clamp across the band.
    ss.store_and_modify({D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP});   // Angular: wrap seamlessly around.
    device->SetTexture(0, g_texture);
  } else {
    // No texture: pass the vertex (diffuse) color straight through stage 0.
    ts.store_and_modify({D3DTSS_COLOROP, D3DTOP_SELECTARG1});
    ts.store_and_modify({D3DTSS_COLORARG1, D3DTA_DIFFUSE});
    ts.store_and_modify({D3DTSS_ALPHAOP, D3DTOP_SELECTARG1});
    ts.store_and_modify({D3DTSS_ALPHAARG1, D3DTA_DIFFUSE});
    device->SetTexture(0, NULL);
  }
  device->SetFVF(kRingFvf);

  // WORLD = translate to the target's feet (lifted slightly to avoid z-fighting the ground). Built by
  // hand to avoid a d3dx dependency; VIEW/PROJECTION are the world pass's, still live at this seam.
  D3DMATRIX world_prev;
  device->GetTransform(D3DTS_WORLD, &world_prev);
  D3DMATRIX world;
  std::memset(&world, 0, sizeof(world));
  world._11 = world._22 = world._33 = world._44 = 1.0f;
  world._41 = cx;
  world._42 = cy;
  world._43 = cz + kGroundLift;
  device->SetTransform(D3DTS_WORLD, &world);

  device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, prim_count, verts, sizeof(RingVertex));

  // Restore everything we touched.
  device->SetTransform(D3DTS_WORLD, &world_prev);
  if (use_texture) device->SetTexture(0, NULL);  // Unbind our texture (the world pass rebinds next frame).
  ss.restore_state();
  ts.restore_state();
  device->SetTextureStageState(1, D3DTSS_COLOROP, stage1_colorop);
  rs.restore_state();
  device->SetVertexShader(prev_vs);
  device->SetPixelShader(prev_ps);
  if (prev_vs) prev_vs->Release();  // Get*Shader added a ref.
  if (prev_ps) prev_ps->Release();
}

void print_status() {
  char colbuf[24];
  if (g_use_con_color)
    std::snprintf(colbuf, sizeof(colbuf), "con-level");
  else
    std::snprintf(colbuf, sizeof(colbuf), "%06X", g_color & 0xffffff);
  Rcp::Game::print_chat(
      "rof2ClientPlus target ring: %s | outer %.1f, inner %.1f, opacity %.0f%%, color %s | graphic %s%s",
      g_enabled ? "ON" : "OFF", g_outer, g_inner, g_opacity * 100.0f, colbuf,
      g_graphic.empty() ? "None" : g_graphic.c_str(), g_hide_self ? ", hidden on self" : "");
}

bool parse_float(const std::string &s, float &out) {
  try {
    out = std::stof(s);
    return true;
  } catch (...) {
    return false;
  }
}

}  // namespace

// ---- options-UI / command accessors (apply live + persist) ----
namespace target_ring_settings {
bool get_enabled() { return g_enabled; }
void set_enabled(bool on) {
  g_enabled = on;
  if (on) g_z_log = 5;
  save_settings();
}
int get_color() { return g_color & 0xffffff; }
void set_color(int rgb) {
  g_color = rgb & 0xffffff;
  save_settings();
}
bool get_use_con_color() { return g_use_con_color; }
void set_use_con_color(bool on) {
  g_use_con_color = on;
  save_settings();
}
float get_outer() { return g_outer; }
void set_outer(float r) {
  g_outer = clampf(r, 0.0f, kRadiusMax);
  save_settings();
}
float get_inner() { return g_inner; }
void set_inner(float r) {
  g_inner = clampf(r, 0.0f, kRadiusMax);
  save_settings();
}
float get_opacity() { return g_opacity; }
void set_opacity(float a) {
  g_opacity = clampf(a, 0.0f, 1.0f);
  save_settings();
}
bool get_hide_self() { return g_hide_self; }
void set_hide_self(bool on) {
  g_hide_self = on;
  save_settings();
}
float radius_max() { return kRadiusMax; }
std::string get_graphic() { return g_graphic; }
void set_graphic(const std::string &name) {
  g_graphic = graphic_is_none(name) ? std::string() : name;
  g_graphic_dirty = true;  // Render thread (re)loads the texture next frame.
  save_settings();
}
// "None" first, then every *.tga stem in uifiles/rcp/targetrings (name only, no dir/extension).
std::vector<std::string> get_available_graphics() {
  std::vector<std::string> out = {"None"};
  std::error_code ec;
  const auto dir = get_graphics_dir();
  if (std::filesystem::is_directory(dir, ec)) {
    for (const auto &e : std::filesystem::directory_iterator(dir, ec)) {
      if (e.is_regular_file() && e.path().extension() == kGraphicExt) out.push_back(e.path().stem().string());
    }
  }
  return out;
}
}  // namespace target_ring_settings

TargetRing::TargetRing(RcpService *rcp) : rcp_(rcp) {
  load_settings();
  logger::logf("[ring] settings loaded: enabled=%d outer=%.1f inner=%.1f opacity=%.2f color=%06X hide_self=%d",
               (int)g_enabled, g_outer, g_inner, g_opacity, g_color & 0xffffff, (int)g_hide_self);

  // Draw at the shared post-world / pre-UI seam (installs whenever FontOverlay exists).
  font_overlay::add_scene_draw([](IDirect3DDevice9 *dev) { draw_ring(dev); });

  rcp->commands_hook->Add(
      "/rcpring", {"/targetring"},
      "Target ring: '/rcpring' toggles; 'on|off', 'outer <n>', 'inner <n>', 'opacity <0-1>', "
      "'color RRGGBB', 'con on|off' (color by target con level), 'self on|off' (hide under yourself), "
      "'graphic <name>|none' (texture from uifiles/rcp/targetrings; bare 'graphic' lists them).",
      [](std::vector<std::string> &args) {
        if (args.size() >= 2) {
          const std::string &a = args[1];
          float f = 0.0f;
          if (a == "on" || a == "1") {
            g_enabled = true;
          } else if (a == "off" || a == "0") {
            g_enabled = false;
          } else if ((a == "outer" || a == "size") && args.size() >= 3 && parse_float(args[2], f)) {
            g_outer = clampf(f, 0.0f, kRadiusMax);
          } else if (a == "inner" && args.size() >= 3 && parse_float(args[2], f)) {
            g_inner = clampf(f, 0.0f, kRadiusMax);
          } else if ((a == "opacity" || a == "alpha") && args.size() >= 3 && parse_float(args[2], f)) {
            g_opacity = clampf(f, 0.0f, 1.0f);
          } else if (a == "color" && args.size() >= 3) {
            g_color = parse_hex_color(args[2]);
            g_use_con_color = false;  // Setting an explicit color implies fixed-color mode.
          } else if (a == "con" && args.size() >= 3) {
            g_use_con_color = (args[2] == "on" || args[2] == "1");
          } else if (a == "self" && args.size() >= 3) {
            g_hide_self = (args[2] == "on" || args[2] == "1");
          } else if ((a == "graphic" || a == "texture") && args.size() >= 3) {
            // Join the remaining args so filenames with spaces work (e.g. 'graphic Project Quarm').
            std::string name = args[2];
            for (size_t i = 3; i < args.size(); ++i) (name += ' ') += args[i];
            if (graphic_is_none(name)) name.clear();
            g_graphic = name;
            g_graphic_dirty = true;  // Render thread (re)loads the texture next frame.
          } else if (a == "graphic" || a == "texture") {
            // Bare 'graphic': report the current selection and everything available on disk.
            const auto list = target_ring_settings::get_available_graphics();
            std::string joined;
            for (size_t i = 0; i < list.size(); ++i) joined += (i ? std::string(", ") : std::string()) + list[i];
            Rcp::Game::print_chat("rof2ClientPlus ring graphic: %s (available: %s)",
                                  g_graphic.empty() ? "None" : g_graphic.c_str(), joined.c_str());
            return true;
          } else {
            Rcp::Game::print_chat(
                "rof2ClientPlus: '/rcpring on|off | outer <n> | inner <n> | opacity <0-1> | color RRGGBB | con on|off | "
                "self on|off | graphic <name>|none'");
            return true;
          }
        } else {
          g_enabled = !g_enabled;  // Bare '/rcpring' toggles.
        }
        if (g_enabled) g_z_log = 5;
        save_settings();
        print_status();
        return true;
      });
  logger::log("[ring] TargetRing constructed; /rcpring registered");
}
