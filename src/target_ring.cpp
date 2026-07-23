// rof2ClientPlus - solid-color target ring. See target_ring.h.
#include "target_ring.h"
#include "rebase.h"

#include <windows.h>
#include <d3d9.h>
#include <d3dx9.h>  // D3DXCreateTextureFromFileA (optional ring graphic).

#include <cmath>
#include <cstdint>
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
void **const kTarget = reinterpret_cast<void **>(::Rcp::eqva(0xDD2648));  // current target entity
void **const kSelf = reinterpret_cast<void **>(::Rcp::eqva(0xDD2630));    // local player
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
constexpr int kEntHeading = 0x80;      // float, EQ heading 0..512 (== 0..2pi); used for face-heading.
constexpr int kEntActor = 0x101c;      // CActor*; NULL => not in-world / no graphics yet.
// For the optional melee-range scaling (Rcp::Game::CalcCombatRange needs each entity's race + model
// size). RoF2 offsets from PORTING_NOTES (eqlib + disasm): Race@0xeb4, Size(shrink/grow scale)@0x13c.
constexpr int kEntRace = 0xeb4;   // uint16 race id (RACE_x); table entries only reach ~360, else default.
constexpr int kEntSize = 0x13c;   // float model size/scale (== TAKP "Height"; AvatarHeight@0x138 is separate).

// ---- live settings (persisted to rof2ClientPlus.ini [TargetRing]). Off by default. ----
bool g_enabled = false;
int g_color = 0xff8000;       // 0xRRGGBB fixed color (used when con coloring is off).
bool g_use_con_color = false;  // true = color by the target's con level instead of g_color.
float g_outer = 8.0f;         // outer radius, world units
float g_inner = 6.0f;    // inner radius (donut hole), world units
float g_opacity = 0.85f;  // 0..1
bool g_hide_self = true;
std::string g_graphic;    // Optional ring graphic (Zeal-style texture). "" = solid donut, no texture.
bool g_spin = true;       // true = the graphic slowly rotates; false = it faces the target's heading.
float g_face_offset_deg = 0.0f;  // Face-heading fine-alignment: added to the angle (degrees, 0..360).
bool g_melee_range = false;  // true = the outer radius tracks the target's melee range (overrides Outer).

int g_z_log = 0;      // Log the computed ring Z (pos vs floor) for a few frames after enabling.
int g_range_log = 0;  // Log the computed melee range (race/size inputs) for a few frames after enabling.

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

// Spin animation (only visible on a textured ring - a solid ring is rotationally symmetric). One
// slow revolution every kSpinPeriodSec; the accumulator lives on the render thread.
constexpr float kTwoPi = 6.28318530717958647692f;
constexpr float kSpinPeriodSec = 8.0f;
constexpr float kSpinRadPerMs = kTwoPi / (kSpinPeriodSec * 1000.0f);
float g_spin_angle = 0.0f;                 // accumulated spin (radians).
unsigned long long g_spin_last_tick = 0;   // GetTickCount64 of the last update; 0 == reset.
// EQ heading (0..512) -> radians (magnitude PI/256 == 2pi per 512). The rotation direction is baked in
// (confirmed in-game); g_face_offset_deg fine-aligns a texture's "front" to the target's facing.
constexpr float kHeadingMag = 3.14159265358979324f / 256.0f;
constexpr float kDegToRad = kTwoPi / 360.0f;

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
  if (ini.exists(kIniSection, "Spin")) g_spin = ini.getValue<bool>(kIniSection, "Spin");
  if (ini.exists(kIniSection, "FaceOffset")) g_face_offset_deg = ini.getValue<float>(kIniSection, "FaceOffset");
  if (ini.exists(kIniSection, "MeleeRange")) g_melee_range = ini.getValue<bool>(kIniSection, "MeleeRange");
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
  ini.setValue<bool>(kIniSection, "Spin", g_spin);
  ini.setValue<float>(kIniSection, "FaceOffset", g_face_offset_deg);
  ini.setValue<bool>(kIniSection, "MeleeRange", g_melee_range);
}

// Compute the world-unit radius at which a melee swing at `target` lands, from the player (`self`)
// and target model race/size. Mirrors the client's combat-range formula (Rcp::Game::CalcCombatRange:
// (boundingRadius(self)+boundingRadius(target))*0.75 + |zOffset diff|, clamped 14..75). Gender only
// nudges a couple of wolf/tiger z-offsets, and the moving +2 needs a velocity offset we don't read
// here, so both are passed as 0/false - close enough for a ground indicator. Falls back to a default
// player build (race 1, size 5) when self isn't resolvable yet.
float melee_range_radius(char *self, char *target) {
  const int target_race = *reinterpret_cast<uint16_t *>(target + kEntRace);
  const float target_size = *reinterpret_cast<float *>(target + kEntSize);
  int self_race = 1;        // Playable races all resolve to a 5.0 bounding radius.
  float self_size = 5.0f;
  if (self) {
    self_race = *reinterpret_cast<uint16_t *>(self + kEntRace);
    self_size = *reinterpret_cast<float *>(self + kEntSize);
  }
  const float r = Rcp::Game::CalcCombatRange(self_race, self_size, 0, target_race, target_size, 0, false);
  if (g_range_log > 0) {
    logger::logf("[ring] melee range: self(race=%d size=%.2f) target(race=%d size=%.2f) -> %.2f", self_race,
                 self_size, target_race, target_size, r);
    --g_range_log;
  }
  return r;
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

  // Outer radius: the manual slider value, or - in melee-range mode - the target's melee range so the
  // ring's outer edge marks exactly how close you must be to land a swing (can exceed kRadiusMax for
  // giants; that's the real range, so it's intentionally not clamped to the slider bound here).
  float outer = g_melee_range ? melee_range_radius(static_cast<char *>(*kSelf), t) : clampf(g_outer, 0.0f, kRadiusMax);
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

  // Stage-0 texture + sampler state. The ring renders in up to TWO passes: a solid color-fill
  // base (so the chosen ring color always shows across the whole band), and - when a graphic is
  // set - the texture modulated by that same color + opacity laid ON TOP. Both passes drive these
  // same stage-0 states, so stash the originals ONCE here and set each pass's values with direct
  // calls below (a second store_and_modify would capture the base pass's value as the "original"
  // and corrupt the world pass's restore).
  D3DTextureStateStash ts(*device);
  D3DSamplerStateStash ss(*device);
  ts.store_and_modify({D3DTSS_COLOROP, D3DTOP_SELECTARG1});  // Base pass: diffuse color straight through.
  ts.store_and_modify({D3DTSS_COLORARG1, D3DTA_DIFFUSE});
  ts.store_and_modify({D3DTSS_ALPHAOP, D3DTOP_SELECTARG1});
  ts.store_and_modify({D3DTSS_ALPHAARG1, D3DTA_DIFFUSE});
  if (use_texture) {
    // Stash the extra states the overlay pass drives so the world pass gets an exact restore.
    ts.store_and_modify({D3DTSS_COLORARG2, D3DTA_DIFFUSE});
    ts.store_and_modify({D3DTSS_ALPHAARG2, D3DTA_DIFFUSE});
    ss.store_and_modify({D3DSAMP_MINFILTER, D3DTEXF_LINEAR});
    ss.store_and_modify({D3DSAMP_MAGFILTER, D3DTEXF_LINEAR});
    ss.store_and_modify({D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP});  // Radial: clamp across the band.
    ss.store_and_modify({D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP});   // Angular: wrap seamlessly around.
  }
  device->SetTexture(0, NULL);  // Base pass draws untextured; the overlay binds g_texture below.
  device->SetFVF(kRingFvf);

  // Rotation about the ring's vertical axis (local Z, the ring's normal): a slow continuous spin, or
  // (spin off) oriented to the target's heading so the graphic "faces" the way the target faces. Only
  // visible on a textured ring - a solid donut is symmetric - but harmless to apply either way.
  float rot;
  if (g_spin) {
    const unsigned long long now = GetTickCount64();
    unsigned long long dt = (g_spin_last_tick == 0) ? 0ull : (now - g_spin_last_tick);
    if (dt > 100) dt = 100;  // Cap after a stall (alt-tab / zoning) so the spin doesn't lurch.
    g_spin_last_tick = now;
    g_spin_angle += kSpinRadPerMs * static_cast<float>(dt);
    if (g_spin_angle >= kTwoPi) g_spin_angle = std::fmod(g_spin_angle, kTwoPi);
    rot = -g_spin_angle;  // Negated so the spin turns the other way (per preference).
  } else {
    g_spin_last_tick = 0;  // Reset so re-enabling spin doesn't jump by the idle interval.
    // Face the target's heading (Heading@0x80, EQ units 0..512). g_face_offset_deg fine-aligns per texture.
    const float heading = *reinterpret_cast<float *>(t + kEntHeading);
    rot = heading * kHeadingMag + g_face_offset_deg * kDegToRad;
  }
  const float rot_c = std::cos(rot), rot_s = std::sin(rot);

  // WORLD = rotate about local Z, then translate to the target's feet (lifted slightly to avoid
  // z-fighting the ground). Built by hand to avoid a d3dx dependency; VIEW/PROJECTION are the world
  // pass's, still live at this seam.
  D3DMATRIX world_prev;
  device->GetTransform(D3DTS_WORLD, &world_prev);
  D3DMATRIX world;
  std::memset(&world, 0, sizeof(world));
  world._11 = rot_c;
  world._12 = rot_s;
  world._21 = -rot_s;
  world._22 = rot_c;
  world._33 = 1.0f;
  world._44 = 1.0f;
  world._41 = cx;
  world._42 = cy;
  world._43 = cz + kGroundLift;
  device->SetTransform(D3DTS_WORLD, &world);

  // Pass 1: the solid color-fill donut. Always drawn, so the ring color is visible across the
  // whole band even when a graphic is present (a graphic's transparent gaps show this color).
  device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, prim_count, verts, sizeof(RingVertex));

  // Pass 2 (graphic only): the ring texture, modulated by the same ring color + opacity, laid on
  // top of the color fill - a white graphic shows the color, any tint blends with it, and the
  // texture's alpha decides where the color underneath shows through. Same geometry/world matrix,
  // so it registers exactly over pass 1 (ZFUNC LESSEQUAL passes at equal depth).
  if (use_texture) {
    device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
    device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
    device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
    device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
    device->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
    device->SetTexture(0, g_texture);
    device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, prim_count, verts, sizeof(RingVertex));
  }

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
  char facebuf[56];
  if (g_spin)
    std::snprintf(facebuf, sizeof(facebuf), "spinning");
  else
    std::snprintf(facebuf, sizeof(facebuf), "faces heading (offset %.0f deg)", g_face_offset_deg);
  char outerbuf[32];
  if (g_melee_range)
    std::snprintf(outerbuf, sizeof(outerbuf), "melee range");
  else
    std::snprintf(outerbuf, sizeof(outerbuf), "%.1f", g_outer);
  Rcp::Game::print_chat(
      "rof2ClientPlus target ring: %s | outer %s, inner %.1f, opacity %.0f%%, color %s | graphic %s (%s)%s",
      g_enabled ? "ON" : "OFF", outerbuf, g_inner, g_opacity * 100.0f, colbuf,
      g_graphic.empty() ? "None" : g_graphic.c_str(), facebuf, g_hide_self ? ", hidden on self" : "");
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
  if (on) {
    g_z_log = 5;
    if (g_melee_range) g_range_log = 5;
  }
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
bool get_spin() { return g_spin; }
void set_spin(bool on) {
  g_spin = on;
  save_settings();
}
bool get_melee_range() { return g_melee_range; }
void set_melee_range(bool on) {
  g_melee_range = on;
  if (on && g_enabled) g_range_log = 5;  // Log the computed range for a few frames so offsets can be verified.
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
      "'melee on|off' (scale the outer edge to the target's melee range, overriding outer), "
      "'graphic <name>|none' (texture from uifiles/rcp/targetrings; bare 'graphic' lists them), "
      "'spin on|off' (rotate the graphic, or face the target's heading when off), "
      "'faceoffset <deg>' (fine-align the face-heading direction).",
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
          } else if (a == "spin" && args.size() >= 3) {
            g_spin = (args[2] == "on" || args[2] == "1");
          } else if ((a == "melee" || a == "meleerange") && args.size() >= 3) {
            g_melee_range = (args[2] == "on" || args[2] == "1");
            if (g_melee_range && g_enabled) g_range_log = 5;  // Log the computed range so offsets can be verified.
          } else if (a == "faceoffset" && args.size() >= 3 && parse_float(args[2], f)) {
            g_face_offset_deg = std::fmod(f, 360.0f);  // Fine-align the face-heading direction (per texture).
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
                "self on|off | melee on|off | graphic <name>|none | spin on|off | faceoffset <deg>'");
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
