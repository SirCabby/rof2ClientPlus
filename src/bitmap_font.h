//--------------------------------------------------------------------------------------
// Bitmap glyph font engine - ported from Zeal (TAKP, MIT) bitmap_font.{h,cpp} to the
// stock RoF2 client. Zeal targeted DirectX 8; RoF2 renders through EQGraphicsDX9.dll +
// d3dx9_30.dll, so this is the D3D9 adaptation (see PORTING_NOTES.md "N4"). The font
// logic is unchanged; only the D3D interface types/methods differ (SetFVF, the extra
// SetStreamSource/DrawIndexedPrimitive/Create*Buffer args, and the texture-stage ->
// sampler-state split for MINFILTER).
//
// Fast text rendering via batch processing of bitmap glyphs from a MakeSpriteFont atlas:
//   BitmapFont: 2-D pre-transformed (screen-space) text, no z-buffer.
//   SpriteFont: 3-D world-space billboard text (faces the camera, z-buffer occluded).
//--------------------------------------------------------------------------------------
#pragma once

#include <d3d9.h>
#include <d3dx9.h>

#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include "vectors.h"

// Stashes the active D3D render state before modifying a list of parameters, and
// restores it afterwards. (Ported from Zeal directx.h; IDirect3DDevice8 -> 9.)
class D3DRenderStateStash {
 public:
  struct Pair {
    D3DRENDERSTATETYPE state_type;
    DWORD value;
  };

  D3DRenderStateStash(IDirect3DDevice9 &device) : device(device) {}

  void store_and_modify(Pair pair) {
    DWORD value;
    device.GetRenderState(pair.state_type, &value);
    stored_pairs.push_back({pair.state_type, value});
    device.SetRenderState(pair.state_type, pair.value);
  }

  void restore_state() {
    for (const auto &pair : stored_pairs) device.SetRenderState(pair.state_type, pair.value);
    stored_pairs.clear();
  }

 private:
  IDirect3DDevice9 &device;
  std::vector<Pair> stored_pairs;
};

// Stashes texture-stage state for stage 0. In D3D9 the sampler filter/address states
// left this call (see D3DSamplerStateStash); the color/alpha op states stayed.
class D3DTextureStateStash {
 public:
  struct Pair {
    D3DTEXTURESTAGESTATETYPE state_type;
    DWORD value;
  };

  D3DTextureStateStash(IDirect3DDevice9 &device) : device(device) {}

  void store_and_modify(Pair pair) {
    DWORD value;
    device.GetTextureStageState(0, pair.state_type, &value);
    stored_pairs.push_back({pair.state_type, value});
    device.SetTextureStageState(0, pair.state_type, pair.value);
  }

  void restore_state() {
    for (const auto &pair : stored_pairs) device.SetTextureStageState(0, pair.state_type, pair.value);
    stored_pairs.clear();
  }

 private:
  IDirect3DDevice9 &device;
  std::vector<Pair> stored_pairs;
};

// Stashes sampler state for stage 0. New in the D3D9 port: MINFILTER/MAGFILTER/address
// modes are sampler states in D3D9 (they were texture-stage states in D3D8).
class D3DSamplerStateStash {
 public:
  struct Pair {
    D3DSAMPLERSTATETYPE state_type;
    DWORD value;
  };

  D3DSamplerStateStash(IDirect3DDevice9 &device) : device(device) {}

  void store_and_modify(Pair pair) {
    DWORD value;
    device.GetSamplerState(0, pair.state_type, &value);
    stored_pairs.push_back({pair.state_type, value});
    device.SetSamplerState(0, pair.state_type, pair.value);
  }

  void restore_state() {
    for (const auto &pair : stored_pairs) device.SetSamplerState(0, pair.state_type, pair.value);
    stored_pairs.clear();
  }

 private:
  IDirect3DDevice9 &device;
  std::vector<Pair> stored_pairs;
};

// Virtual base class with utility functions for handling fonts and common processing.
class BitmapFontBase {
 public:
  static constexpr char kFontSubDirectoryPath[] = "fonts";  // Within the rcp ui path.
  static constexpr char kFontFileExtension[] = ".spritefont";
  static constexpr char kDefaultFontName[] = "default";

  // Support display of a stats bar using non-visible ASCII values.
  static constexpr char kStatsBarBackground = 1;  // ASCII '\x01' draws the background.
  static constexpr char kHealthBarValue = 2;      // ASCII '\x02' draws the health value.
  static constexpr char kManaBarValue = 3;        // ASCII '\x03' draws the mana value.
  static constexpr char kStaminaBarValue = 4;     // ASCII '\x04' draws the stamina value.

  // The Background Rectangle should not be passed in queue_strings(). Also 2-D BitmapFont only.
  static constexpr char kBackgroundRect = 5;  // ASCII '\x05' draws the background rectangle.

  static constexpr float kDefaultShadowOffsetFactor = 0.01f;

  // Utility method to report the available fonts (embedded "default" plus any on disk).
  static std::vector<std::string> get_available_fonts();

  BitmapFontBase(IDirect3DDevice9 &device, std::span<const uint8_t> data_span);
  BitmapFontBase(IDirect3DDevice9 &device, const char *filename);
  virtual ~BitmapFontBase();

  // Disable copy.
  BitmapFontBase(BitmapFontBase const &) = delete;
  BitmapFontBase &operator=(BitmapFontBase const &) = delete;

  // Returns true if a valid bitmap texture is ready for use by the GPU.
  bool is_valid() const { return texture != nullptr; }

  // Utilities for adjusting the string position (sizes in screen pixels).
  Vec3 measure_string(const char *text) const;
  RECT measure_draw_rect(const char *text, const Vec2 &position) const;

  float get_line_spacing() const { return line_spacing; }

  float get_text_height(const std::string &text) const;

  // Drop shadow and alignment configuration.
  void set_shadow_offset_factor(float factor) { shadow_offset_factor = factor; }
  void set_drop_shadow(bool enable) { drop_shadow = enable; }
  void set_outlined(bool enable) { outlined = enable; }
  void set_align_bottom(bool enable) { align_bottom = enable; }
  void set_full_screen_viewport(bool enable) { full_screen_viewport = enable; }
  void set_stats_bar_height(float height) { stats_bar_height = height; }
  void set_stats_bar_width(float width) { stats_bar_width = width; }

  // Set these stats bar values before calling queue_string() which contains their glyphs.
  void set_hp_percent(int value) { hp_percent = value; }
  void set_mana_percent(int value) { mana_percent = value; }
  void set_stamina_percent(int value) { stamina_percent = value; }

  // Inserts a request to render the background rect into the queue. Only one per flush allowed.
  void queue_background_rect(const RECT &rect, D3DCOLOR color);

  // Primary interface for drawing text. The position is in screen pixel coordinates
  // and specifies the center (true) or the upper left (false).
  virtual void queue_string(const char *text, const Vec3 &position, bool center = true,
                            const D3DCOLOR color = D3DCOLOR_XRGB(255, 255, 255), bool grid_align = true);

  // Renders any queued string content to the screen and clears the queue.
  // Note that the D3D stream source, indices, FVF, and texture states are not
  // preserved across this call.
  virtual void flush_queue_to_screen();

  // Releases resources including DirectX. Must call on a DirectX reset / lost device.
  virtual void release();  // Note: No longer usable after this call (delete).

 protected:
  // Strings are split into multiple lines.
  struct Lines {
    std::string text;
    Vec2 upper_left;
  };

  // Describes a single character bitmap glyph.
  struct Glyph {
    uint32_t character = '\0';
    RECT sub_rect = {0, 0, 0, 0};
    float x_offset = 0;
    float y_offset = 0;
    float x_advance = 0;
  };

  // Strings are stored in the queue as character glyphs.
  struct GlyphQueueEntry {
    const Glyph *glyph;
    const Vec2 position;
    const D3DCOLOR color;
    char hp_percent;
  };

  static constexpr int kNumGlyphs = 128;  // Support ASCII 0 - 127.
  static constexpr float kStatsBarHeight = 6;
  static constexpr float kStatsBarWidth = 120;

  void queue_lines(const std::vector<Lines> &lines, D3DCOLOR color, Vec2 offset = Vec2(0, 0));
  const Glyph *get_glyph(char character) const;
  RECT create_texture(uint32_t width, uint32_t height, D3DFORMAT format, uint32_t stride, uint32_t rows,
                      const uint8_t *data);
  bool create_index_buffer();
  // Drops only the (D3DPOOL_DEFAULT) vertex/index buffers so the next flush lazily recreates them
  // against the live device; the managed-pool texture stays valid. Lets a font self-heal after a
  // device reset instead of dying invisibly for the rest of the session.
  void release_buffers();
  void log_gpu_fail(const char *what);  // Throttled logging for transient GPU failures.

  virtual DWORD get_fvf_code() const = 0;
  virtual DWORD get_vertex_size() const = 0;
  virtual void render_queue() = 0;

  template <typename TAction>
  void for_each_glyph(const char *text, TAction action) const;

  float calculate_shadow_offset() const;

  float shadow_offset_factor = kDefaultShadowOffsetFactor;
  bool drop_shadow = false;
  bool outlined = false;
  bool align_bottom = false;          // Applies only when text is queued with center flag.
  bool full_screen_viewport = false;  // If true, overrides viewport to full screen resolution.
  float stats_bar_width = kStatsBarWidth;
  float stats_bar_height = kStatsBarHeight;
  char hp_percent = 0;
  char mana_percent = 0;
  char stamina_percent = 0;
  RECT background_rect = {0, 0, 0, 0};  // Cached value (only one per flush allowed).

  IDirect3DDevice9 &device;
  Glyph glyph_table[kNumGlyphs] = {};
  float line_spacing = 0;
  char default_character = '\0';
  std::vector<GlyphQueueEntry> glyph_queue;

  IDirect3DTexture9 *texture = nullptr;
  Vec2 inverse_texture_size = {};
  IDirect3DVertexBuffer9 *vertex_buffer = nullptr;
  IDirect3DIndexBuffer9 *index_buffer = nullptr;
  int vertex_buffer_wr_index = 0;
  int gpu_fail_log_budget = 6;  // First few transient GPU failures get logged; the rest stay quiet.
};

// 2-D pre-transformed (screen-space) text rendering without the z-buffer.
class BitmapFont : public BitmapFontBase {
 public:
  // A font_filename of kDefaultFontName or empty loads the embedded font.
  // Releases resources and returns nullptr if unsuccessful.
  static std::unique_ptr<BitmapFont> create_bitmap_font(IDirect3DDevice9 &device, const std::string &font_filename);

  BitmapFont(IDirect3DDevice9 &device_in, std::span<const uint8_t> data_span) : BitmapFontBase(device_in, data_span) {}
  BitmapFont(IDirect3DDevice9 &device_in, const char *filename) : BitmapFontBase(device_in, filename) {}
  virtual ~BitmapFont() { release(); }

  void release() override;

 protected:
  // Vertices allow texturing and color modulation.
  struct GlyphVertex {
    static constexpr DWORD kGlyphVertexFvfCode = (D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1);

    float x, y, z, rhw;  // Transformed position coordinates and rhw.
    D3DCOLOR color;      // Color (modulates font color which is typically white).
    float u, v;          // Texture coordinates from D3DFVF_TEX1.
  };

  DWORD get_fvf_code() const override { return GlyphVertex::kGlyphVertexFvfCode; }
  DWORD get_vertex_size() const override { return sizeof(GlyphVertex); }

  void render_queue() override;
  void calculate_glyph_vertices(const GlyphQueueEntry &entry, GlyphVertex glyph_vertices[4]) const;

  std::unique_ptr<GlyphVertex[]> vertices;  // Local CPU scratch.
};

// 3-D world-space text rendering with matrix ops and a z-buffer. The text is rotated so
// it always faces the camera (billboard).
class SpriteFont : public BitmapFontBase {
 public:
  // A font_filename of kDefaultFontName or empty loads the embedded font.
  // Releases resources and returns nullptr if unsuccessful.
  static std::unique_ptr<SpriteFont> create_sprite_font(IDirect3DDevice9 &device, const std::string &font_filename);

  SpriteFont(IDirect3DDevice9 &device_in, std::span<const uint8_t> data_span) : BitmapFontBase(device_in, data_span) {}
  SpriteFont(IDirect3DDevice9 &device_in, const char *filename) : BitmapFontBase(device_in, filename) {}
  virtual ~SpriteFont() { release(); }

  void queue_string(const char *text, const Vec3 &position, bool center = true,
                    const D3DCOLOR color = D3DCOLOR_XRGB(255, 255, 255), bool grid_align = false) override;
  void flush_queue_to_screen() override;
  void release() override;

  // Note: The measure_string() and other bases do not include the internal scale factor.
  // This method does return the height in corrected screen dimensions.
  float get_text_height(const std::string &text) const;

  // World-space size of the billboard text. Larger fonts want a smaller factor; tune per
  // atlas (the 0.025 default was set for arial_24_bold in TAKP).
  void set_scale_factor(float factor) { scale_factor = factor; }

  // Lifts the text along the billboard's local-vertical (== screen-up, since the quad faces
  // the camera), in local layout units. This is how native EQ nameplates float a constant
  // distance above the head from any camera angle - a world-Z offset would collapse when
  // viewed from directly above. Positive = up on screen.
  void set_screen_offset(float local_units) { screen_offset = local_units; }

 protected:
  float scale_factor = 0.025f;  // Empirically set so arial_24_bold roughly matches client.
  float screen_offset = 0.f;    // Local-vertical lift applied to every glyph (see set_screen_offset).

  struct GlyphString {
    Vec3 position;  // Origin anchor point for a string of glyphs.
    int start_index;
    int stop_index;   // Exclusive.
    float scale;      // Per-string world scale captured at queue time (see scale_factor).
    char hp_percent;  // Value of internal hp_percent when queued.
    char mana_percent;
    char stamina_percent;
  };

  // Vertices allow texturing and color modulation.
  struct Glyph3DVertex {
    static constexpr DWORD kFvfCode = (D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX1);

    float x, y, z;   // Model-space position coordinates.
    D3DCOLOR color;  // Color (modulates font color which is typically white).
    float u, v;      // Texture coordinates from D3DFVF_TEX1.
  };

  DWORD get_fvf_code() const override { return Glyph3DVertex::kFvfCode; }
  DWORD get_vertex_size() const override { return sizeof(Glyph3DVertex); }

  void render_queue() override;
  void calculate_glyph_vertices(const GlyphQueueEntry &entry, Glyph3DVertex glyph_vertices[4]) const;

  std::vector<GlyphString> glyph_string_queue;  // List of strings at different Vec3 positions.
  std::unique_ptr<Glyph3DVertex[]> vertices;    // Local CPU scratch.
};
