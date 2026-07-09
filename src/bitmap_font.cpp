//--------------------------------------------------------------------------------------
// Bitmap glyph font engine - ported from Zeal (TAKP, MIT) to stock RoF2 / Direct3D 9.
// See bitmap_font.h and PORTING_NOTES.md ("N4"). The font logic mirrors Zeal's DX8
// implementation; the D3D8->D3D9 differences applied here are:
//   - SetVertexShader(FVF)              -> SetFVF(FVF)
//   - SetStreamSource(0, vb, stride)    -> SetStreamSource(0, vb, 0, stride)
//   - SetIndices(ib, base)              -> SetIndices(ib)          (base removed)
//   - DrawIndexedPrimitive(...)         -> +BaseVertexIndex (0) as the 2nd arg
//   - CreateVertexBuffer/CreateIndexBuffer add a trailing NULL (pSharedHandle)
//   - VB/IB Lock ppbData is void** in D3D9 (was BYTE**)
//   - texture-stage D3DTSS_MINFILTER    -> sampler-state D3DSAMP_MINFILTER
// D3DX (D3DXCreateTexture, D3DXMatrix*) is identical between d3dx8 and d3dx9_30.
//
// Generation of new font files (MakeSpriteFont.exe from DirectXTK):
//   https://github.com/microsoft/DirectXTK/wiki/MakeSpriteFont
//   MakeSpriteFont "Arial" /FontSize:24 /FontStyle:Bold
//       /TextureFormat:CompressedMono arial_bold_24.spritefont
//--------------------------------------------------------------------------------------

#define NOMINMAX
#include "bitmap_font.h"

#include <windows.h>

#include <algorithm>
#include <bit>
#include <climits>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>

#include "default_spritefont.h"
#include "logger.h"

namespace {

// Simple hack to identify queued glyphs for drop shadows using a very close to black color.
static constexpr D3DCOLOR kDropShadowColor = D3DCOLOR_XRGB(0x01, 0x01, 0x01);

static constexpr int kVertexBufferMaxBatchCount = 1000;
static constexpr int kVertexBufferMinBatchCount = 100;
static constexpr int kNumGlyphVertices = 4;   // Four per glyph using D3DPT_TRIANGLELIST with indices.
static constexpr int kNumGlyphIndices = 6;    // Three per triangle in D3DPT_TRIANGLELIST.
static constexpr int kNumGlyphTriangles = 2;  // Two triangles per glyph to specify the rectangle.

// Hard-coded starting sequence of a MakeSpriteFont file.
static constexpr char kSpriteFontMagic[] = "DXTKfont";

// Confirmed stock-RoF2 screen resolution globals (see game_functions.h). Only used for the
// optional full-screen viewport override, which the nameplate path leaves disabled.
inline int screen_resolution_x() { return *reinterpret_cast<int *>(0x00798564); }
inline int screen_resolution_y() { return *reinterpret_cast<int *>(0x00798568); }

// The fonts directory lives next to the game exe: "<game>/uifiles/rcp/fonts".
std::filesystem::path get_fonts_dir() {
  char module_path[MAX_PATH] = {};
  GetModuleFileNameA(NULL, module_path, MAX_PATH);  // The game exe (NULL = the running process).
  std::filesystem::path base = std::filesystem::path(module_path).parent_path();
  return base / "uifiles" / "rcp" / BitmapFontBase::kFontSubDirectoryPath;
}

// Splits a string into lines on '\n' (carriage returns are handled by for_each_glyph).
std::vector<std::string> split_text(const std::string &text) {
  std::vector<std::string> lines;
  size_t start = 0;
  while (true) {
    size_t pos = text.find('\n', start);
    if (pos == std::string::npos) {
      lines.push_back(text.substr(start));
      break;
    }
    lines.push_back(text.substr(start, pos - start));
    start = pos + 1;
  }
  return lines;
}

// Helper class for parsing the binary SpriteFont file stream.
class BinaryReader {
 public:
  BinaryReader(const uint8_t *buffer, int size_bytes) : buffer(buffer), size_bytes(size_bytes) {}

  bool is_error() const { return read_error; }
  int bytes_left() const { return size_bytes - read_offset; }

  const uint8_t *read_bytes(size_t num_bytes) {
    if (read_offset + num_bytes > (size_t)size_bytes) {
      read_error = true;
      return nullptr;
    }
    const uint8_t *result = &buffer[read_offset];
    read_offset += num_bytes;
    return result;
  }

  template <class T>
  inline T read() {
    T result = {0};
    uint32_t size = sizeof(T);
    if (read_offset + size > (size_t)size_bytes) {
      read_error = true;
      return result;
    }
    uint8_t *dst = reinterpret_cast<uint8_t *>(&result);
    for (uint32_t i = 0; i < size; ++i) dst[i] = buffer[read_offset + i];
    read_offset += size;
    return result;
  }

 public:
  const uint8_t *buffer;
  const int size_bytes;
  int read_offset = 0;
  bool read_error = false;
};

// Reads a binary file into a std::vector buffer (for memory management).
std::vector<uint8_t> load_file(const char *filename) {
  uint32_t size_bytes = 0;
  std::ifstream in(filename, std::ifstream::binary);
  std::vector<uint8_t> buffer;
  if (in) {
    in.seekg(0, in.end);
    size_bytes = in.tellg();
    in.seekg(0, in.beg);
    buffer.resize(size_bytes);
    in.read(reinterpret_cast<char *>(buffer.data()), buffer.size());
    in.close();
  }
  return buffer;
}

}  // namespace

// Factory for creating bitmap fonts. Returns nullptr if unsuccessful.
std::unique_ptr<BitmapFont> BitmapFont::create_bitmap_font(IDirect3DDevice9 &device, const std::string &font_filename) {
  std::unique_ptr<BitmapFont> bitmap_font;

  // Attempt to load a filesystem font if there's a candidate name.
  if (!font_filename.empty() && font_filename != kDefaultFontName) {
    std::filesystem::path full_filename = get_fonts_dir() / std::filesystem::path(font_filename + kFontFileExtension);
    bitmap_font = std::make_unique<BitmapFont>(device, full_filename.string().c_str());
    if (!bitmap_font->is_valid()) {
      logger::logf("[font] Failed to load font file: %s", full_filename.string().c_str());
      bitmap_font.reset();  // Release the invalid font and nulls the ptr.
    }
    return bitmap_font;
  }

  // Initialize with the embedded default font.
  bitmap_font =
      std::make_unique<BitmapFont>(device, std::span<const uint8_t>(default_spritefont, default_spritefont_len));
  if (!bitmap_font->is_valid()) {
    bitmap_font.reset();  // Release the invalid font and nulls the ptr.
    logger::log("[font] Error initializing default font");
  }
  return bitmap_font;
}

std::vector<std::string> BitmapFontBase::get_available_fonts() {
  std::vector<std::string> fonts = {kDefaultFontName};  // "default" is always first in list.
  std::error_code ec;
  std::filesystem::path directory_path = get_fonts_dir();
  if (std::filesystem::is_directory(directory_path, ec)) {
    for (const auto &entry : std::filesystem::directory_iterator(directory_path, ec)) {
      if (entry.is_regular_file() && entry.path().extension() == kFontFileExtension)
        fonts.push_back(entry.path().stem().string());  // Add filename without extension.
    }
  }
  return fonts;
}

// Calculates the default shadow offset for this font size.
float BitmapFontBase::calculate_shadow_offset() const {
  float offset = std::roundf(get_line_spacing() * shadow_offset_factor);
  return std::max(1.f, offset);  // Rounded, integer offset >= 1.
}

// Reads a font from files created with the MakeSpriteFont utility.
BitmapFontBase::BitmapFontBase(IDirect3DDevice9 &device_in, const char *filename)
    : BitmapFontBase(device_in, load_file(filename)) {}

// Parse the binary MakeSpriteFont blob, initialize the glyphs table, and create the D3D texture.
BitmapFontBase::BitmapFontBase(IDirect3DDevice9 &device, std::span<const uint8_t> data_span) : device(device) {
  BinaryReader reader(data_span.data(), data_span.size());

  // Validate the binary blob header matches.
  for (char const *magic = kSpriteFontMagic; *magic; magic++) {
    if (reader.read<uint8_t>() != *magic) {
      logger::log("[font] Invalid SpriteFont file");
      return;  // Abort.
    }
  }

  // Read the glyph data.
  auto num_glyphs = reader.read<uint32_t>();
  for (uint32_t i = 0; i < num_glyphs; ++i) {
    auto glyph_data = reader.read<Glyph>();
    if (glyph_data.character >= kNumGlyphs) continue;  // Just skip non-ascii.
    glyph_table[glyph_data.character] = glyph_data;
  }

  line_spacing = reader.read<float>();
  line_spacing = static_cast<float>(static_cast<int>(line_spacing + 0.5f));  // ceil().

  // Read in the default character and set all uninitialized table entries to it.
  uint32_t file_default_character = reader.read<uint32_t>();
  default_character = (file_default_character < kNumGlyphs) ? file_default_character : '\0';
  for (int i = 0; i < kNumGlyphs; ++i) {
    if (glyph_table[i].character == '\0')  // Uninitialized.
      glyph_table[i] = glyph_table[(int)default_character];
  }

  // Read the texture data.
  auto texture_width = reader.read<uint32_t>();
  auto texture_height = reader.read<uint32_t>();
  auto texture_format = reader.read<int32_t>();  // Note: DXGI_FORMAT, not D3D_FORMAT
  auto texture_stride = reader.read<uint32_t>();
  auto texture_rows = reader.read<uint32_t>();

  const uint64_t texture_data_size = uint64_t(texture_stride) * uint64_t(texture_rows);
  if (reader.is_error() || reader.bytes_left() < (int)texture_data_size) {
    logger::log("[font] Invalid SpriteFont file texture data size");
    return;
  }
  auto texture_data = reader.read_bytes(static_cast<size_t>(texture_data_size));
  if (!texture_data) {
    logger::log("[font] Invalid SpriteFont file texture data");
    return;
  }

  // The MakeSpriteFont supports three DXGI_FORMATs; we only accept BC2 (== D3DFMT_DXT2).
  const int DXGI_FORMAT_BC2_UNORM = 74;
  if (texture_format != DXGI_FORMAT_BC2_UNORM) {
    logger::log("[font] Only DXGI_FORMAT_BC2_UNORM font textures are supported");
    return;
  }
  D3DFORMAT d3dformat = D3DFMT_DXT2;  // Equivalent to BC2_UNORM.

  // Create the D3D texture.
  RECT texture_rect =
      create_texture(texture_width, texture_height, d3dformat, texture_stride, texture_rows, texture_data);
  if (!texture) {
    logger::log("[font] Failed to create the font bitmap texture");
    return;
  }

  // Customize four special glyph indices to support the stats bars. The background glyph's
  // advance is set to measure "zero" width so the value bar starts at the same location while
  // the value glyphs' advance is set to the full width size for the centering calcs.
  // create_texture put a solid 4x4 block at the end of the atlas for the stats bars to use.
  RECT sub_rect = {texture_rect.right - 3, texture_rect.bottom - 2, texture_rect.right - 1, texture_rect.bottom - 1};
  glyph_table[(int)kStatsBarBackground] = {.character = kStatsBarBackground,
                                           .sub_rect = sub_rect,
                                           .x_offset = 0,
                                           .y_offset = 1,
                                           .x_advance = static_cast<float>(sub_rect.left - sub_rect.right)};
  glyph_table[(int)kHealthBarValue] = {
      .character = kHealthBarValue, .sub_rect = sub_rect, .x_offset = 0, .y_offset = 1, .x_advance = stats_bar_width};
  glyph_table[(int)kManaBarValue] = {
      .character = kManaBarValue, .sub_rect = sub_rect, .x_offset = 0, .y_offset = 1, .x_advance = stats_bar_width};
  glyph_table[(int)kStaminaBarValue] = {
      .character = kStaminaBarValue, .sub_rect = sub_rect, .x_offset = 0, .y_offset = 1, .x_advance = stats_bar_width};
  glyph_table[(int)kBackgroundRect] = {
      .character = kBackgroundRect, .sub_rect = sub_rect, .x_offset = 0, .y_offset = 0, .x_advance = 0};
}

// Ensure all resources are released in the destructor.
BitmapFontBase::~BitmapFontBase() { release(); }

// DirectX resources need to be manually released.
void BitmapFontBase::release() {
  if (texture) texture->Release();
  texture = nullptr;
  if (vertex_buffer) vertex_buffer->Release();
  vertex_buffer = nullptr;
  if (index_buffer) index_buffer->Release();
  index_buffer = nullptr;
  glyph_queue.clear();
}

// DirectX resources need to be manually released.
void BitmapFont::release() {
  BitmapFontBase::release();
  vertices.reset();
}

// Creates the D3D texture (acquires and configures resources).
RECT BitmapFontBase::create_texture(uint32_t width, uint32_t height, D3DFORMAT format, uint32_t stride, uint32_t rows,
                                    const uint8_t *data) {
  const uint64_t size_bytes = uint64_t(stride) * uint64_t(rows);
  if (size_bytes > 256 * 1024ull)  // Just a sanity check.
    return RECT(0, 0, 0, 0);

  // Texture copy below assumes 4x4 packed DXT2 format.
  if (format != D3DFMT_DXT2 || (rows * 4 != height)) {
    logger::logf("[font] Unsupported texture: fmt: 0x%08x, rows: %i, height: %i", format, rows, height);
    return RECT(0, 0, 0, 0);
  }

  uint32_t mip_levels = 1;
  const DWORD usage = 0;                       // Not a render target or dynamic.
  uint32_t width_pow2 = std::bit_ceil(width);  // Use powers of two for HW portability.
  uint32_t height_pow2 = std::bit_ceil(height);

  auto hresult =
      D3DXCreateTexture(&device, width_pow2, height_pow2, mip_levels, usage, format, D3DPOOL_MANAGED, &texture);
  if (FAILED(hresult)) {
    logger::logf("[font] texture failure: w: %i, h: %i, code: 0x%08x", width_pow2, height_pow2, (unsigned)hresult);
    texture = nullptr;  // Ensure it is nulled.
    return RECT(0, 0, 0, 0);
  }

  // Cache away conversion factors for use in vertex texture coordinates.
  Vec2 texture_size = Vec2(static_cast<float>(width_pow2), static_cast<float>(height_pow2));
  inverse_texture_size = Vec2(1.f / texture_size.x, 1.f / texture_size.y);

  D3DLOCKED_RECT locked_rect;
  if (FAILED(texture->LockRect(0, &locked_rect, NULL, D3DLOCK_DISCARD)) || locked_rect.Pitch < (int)stride) {
    logger::log("[font] texture: Lock failed");
    texture->Release();
    texture = nullptr;
    return RECT(0, 0, 0, 0);
  }

  // Note: This uses rows, not height, and stride to copy over the data assuming this
  // is properly packed DXT2 data. We checked above that 4 * rows = height and stride <= pitch.
  uint8_t *texture_data = reinterpret_cast<uint8_t *>(locked_rect.pBits);
  for (uint32_t y = 0; y < rows; ++y) memcpy(&texture_data[y * locked_rect.Pitch], &data[y * stride], stride);

  // To support the stats bars, make a solid block at the very end of the texture. This
  // is going to be unused in all likely scenarios due to the power of two ceiling above.
  int last_row = height_pow2 / 4 - 1;  // Rows = height / 4 since 4x4 block compression.
  int last_block = 4 * width_pow2 - 16;
  if (last_block + 16 <= locked_rect.Pitch)
    memset(&texture_data[last_row * locked_rect.Pitch + last_block], 0xff, 16);
  else
    logger::logf("[font] Error: Unable to set stats bar texture (%i vs %i)", locked_rect.Pitch, last_block);

  texture->UnlockRect(0);
  return RECT(0, 0, width_pow2, height_pow2);
}

// Returns the glyph details for the character (or default if out of range).
const BitmapFontBase::Glyph *BitmapFontBase::get_glyph(char character) const {
  if (character >= kNumGlyphs || character < 0) character = default_character;
  return &glyph_table[(int)character];
}

// The core glyph layout algorithm shared by the string functions.
template <typename TAction>
void BitmapFontBase::for_each_glyph(const char *text, TAction action) const {
  float x = 0;
  float y = 0;

  int length_limit = 100;  // Limit text strings to 100 characters.
  for (; *text && length_limit--; text++) {
    const char character = *text;

    switch (character) {
      case '\r':
        continue;  // Skip carriage returns.

      case '\n':
        x = 0;
        y += line_spacing;  // New line.
        break;

      default:
        auto glyph = get_glyph(character);
        x += glyph->x_offset;
        if (x < 0) x = 0;
        if (((glyph->sub_rect.right - glyph->sub_rect.left) > 1) ||
            ((glyph->sub_rect.bottom - glyph->sub_rect.top) > 1))
          action(glyph, x, y + glyph->y_offset);
        const float advance = float(glyph->sub_rect.right) - float(glyph->sub_rect.left) + glyph->x_advance;
        x += advance;
        break;
    }
  }
}

void BitmapFontBase::queue_lines(const std::vector<Lines> &lines, D3DCOLOR color, Vec2 offset) {
  for (const auto &line : lines) {
    Vec2 upper_left = line.upper_left + offset;
    for_each_glyph(line.text.c_str(), [&](const Glyph *glyph, float x, float y) {
      if ((glyph->character == kStatsBarBackground || glyph->character == kHealthBarValue ||
           glyph->character == kManaBarValue || glyph->character == kStaminaBarValue) &&
          color == kDropShadowColor)
        return;  // Skip drop shadow for the stats bars.
      glyph_queue.push_back({glyph, upper_left + Vec2(x, y), color, hp_percent});
    });
  }
}

void BitmapFontBase::queue_background_rect(const RECT &rect, D3DCOLOR color) {
  background_rect = rect;  // The width and height aren't queued so have to cache them.

  auto glyph = get_glyph(kBackgroundRect);
  glyph_queue.push_back(
      {glyph, Vec2(static_cast<float>(background_rect.left), static_cast<float>(background_rect.top)), color, 0});
}

// Public interface that queues a string for later rendering in the flush call.
void BitmapFontBase::queue_string(const char *text, const Vec3 &position, bool center, const D3DCOLOR color,
                                  bool grid_align) {
  if (!text || !(*text)) return;  // Skip nullptr or empty strings.

  std::vector<Lines> lines;
  Vec2 upper_left(position.x, position.y);
  if (center && strchr(text, '\n') != nullptr) {
    // Split into the multiple lines and measure the width of each line.
    auto text_lines = split_text(std::string(text));
    float x_max = 0;
    float y_height = 0;
    float y_advance = 0;
    for (const auto &line : text_lines) {
      y_height += y_advance;
      Vec3 size = measure_string(line.c_str());
      x_max = std::max(x_max, size.x);
      lines.push_back({line, Vec2(size.x, y_height)});
      y_height += size.y;
      y_advance = std::max(0.f, size.z - size.y);  // Save if needed for next line.
    }
    float x_offset = -x_max * 0.5f;  // Common base offset for all lines.
    float y_offset = align_bottom ? -y_height : -y_height * 0.5f;
    for (auto &line : lines) {
      float x_line_offset = x_offset + 0.5f * (x_max - line.upper_left.x);
      float y_line_offset = line.upper_left.y + y_offset;
      line.upper_left = upper_left + Vec2(x_line_offset, y_line_offset);
    }
  } else {
    if (center) {
      Vec3 size = measure_string(text);
      upper_left -= Vec2(0.5f * size.x, align_bottom ? size.y : 0.5f * size.y);
    }
    lines.push_back({std::string(text), upper_left});
  }
  if (grid_align) {
    for (auto &line : lines) {
      line.upper_left.x = std::round(line.upper_left.x);  // Starts need to be grid aligned for clean rendering.
      line.upper_left.y = std::round(line.upper_left.y);
    }
  }
  if (drop_shadow || outlined) {
    float shadow_offset = calculate_shadow_offset();
    queue_lines(lines, kDropShadowColor, Vec2(shadow_offset, shadow_offset));
    if (outlined) {
      // Technically would be cleaner with left, right, top, bottom adjusts (4 passes).
      queue_lines(lines, kDropShadowColor, Vec2(-shadow_offset, -shadow_offset));
    }
  }
  queue_lines(lines, color);
}

// Returns the height and width of the string. Does not include line spacing, returned as z.
Vec3 BitmapFontBase::measure_string(const char *text) const {
  float line_height = line_spacing;  // Default line height.
  Vec3 result{0, 0, 0};
  if (!text || !(*text)) return result;  // Skip nullptr or empty strings.

  // for_each_glyph resets x for each line, y for each glyph, and includes the offsets in x and y.
  for_each_glyph(text, [&](Glyph const *glyph, float x, float y) {
    float w = static_cast<float>(glyph->sub_rect.right - glyph->sub_rect.left);
    float h = static_cast<float>(glyph->sub_rect.bottom - glyph->sub_rect.top);
    if (glyph->character == kHealthBarValue || glyph->character == kManaBarValue ||
        glyph->character == kStaminaBarValue) {
      w = glyph->x_advance;
      h = stats_bar_height;
      line_height = stats_bar_height + 1;
    }
    result = Vec3(std::max(result.x, x + w), std::max(result.y, y + h), line_height);
  });

  return result;
}

// Returns the bounding box around the string.
RECT BitmapFontBase::measure_draw_rect(const char *text, const Vec2 &position) const {
  if (!text || !(*text)) return {0, 0, 0, 0};  // Skip nullptr or empty strings.

  RECT result = {LONG_MAX, LONG_MAX, 0, 0};

  for_each_glyph(text, [&](const Glyph *glyph, float x, float y) {
    auto const w = static_cast<float>(glyph->sub_rect.right - glyph->sub_rect.left);
    auto const h = static_cast<float>(glyph->sub_rect.bottom - glyph->sub_rect.top);

    const float min_x = position.x + x;
    const float min_y = position.y + y + glyph->y_offset;
    const float advance = float(glyph->sub_rect.right) - float(glyph->sub_rect.left) + glyph->x_advance;
    const float max_x = min_x + std::max(advance, w);
    const float max_y = min_y + h;

    result.left = static_cast<long>(std::min(static_cast<float>(result.left), min_x));
    result.top = static_cast<long>(std::min(static_cast<float>(result.top), min_y));
    result.right = static_cast<long>(std::max(static_cast<float>(result.right), max_x));
    result.bottom = static_cast<long>(std::max(static_cast<float>(result.bottom), max_y));
  });

  if (result.left == LONG_MAX) {
    result.left = 0;
    result.top = 0;
  }

  return result;
}

float BitmapFontBase::get_text_height(const std::string &text) const {
  if (text.empty()) return 0;
  auto text_lines = split_text(text);
  float y_height = 0;
  float y_advance = 0;
  for (const auto &line : text_lines) {
    y_height += y_advance;
    Vec3 size = measure_string(line.c_str());
    y_height += size.y;
    y_advance = std::max(0.f, size.z - size.y);  // Save if needed for next line.
  }

  return y_height;
}

// Renders all queued bitmap glyphs to the screen.
void BitmapFontBase::flush_queue_to_screen() {
  if (!texture) glyph_queue.clear();

  if (glyph_queue.empty()) return;

  if (!vertex_buffer) {
    // D3D9: CreateVertexBuffer takes a trailing pSharedHandle (NULL).
    if (FAILED(device.CreateVertexBuffer(kVertexBufferMaxBatchCount * kNumGlyphVertices * get_vertex_size(),
                                         D3DUSAGE_WRITEONLY | D3DUSAGE_DYNAMIC, get_fvf_code(), D3DPOOL_DEFAULT,
                                         &vertex_buffer, NULL))) {
      vertex_buffer = nullptr;  // Ensure nullptr.
      release();                // Disable future attempts.
      return;
    }
  }

  if (!index_buffer && !create_index_buffer()) {
    release();  // Disable future attempts.
    return;
  }

  // Support temporarily overriding the viewport to full screen mode.
  bool modify_viewport = false;
  D3DVIEWPORT9 original_viewport;
  if (full_screen_viewport) {
    device.GetViewport(&original_viewport);
    DWORD screen_x = screen_resolution_x();
    DWORD screen_y = screen_resolution_y();
    modify_viewport = (original_viewport.X || original_viewport.Y || original_viewport.Width != screen_x ||
                       original_viewport.Height != screen_y);
    if (modify_viewport) {
      D3DVIEWPORT9 viewport = {.X = 0,
                               .Y = 0,
                               .Width = screen_x,
                               .Height = screen_y,
                               .MinZ = original_viewport.MinZ,
                               .MaxZ = original_viewport.MaxZ};
      device.SetViewport(&viewport);
    }
  }

  render_queue();
  glyph_queue.clear();

  if (modify_viewport) device.SetViewport(&original_viewport);
}

// Submits glyph sprites to the GPU in batches (2-D pre-transformed).
void BitmapFont::render_queue() {
  if (!vertices) vertices = std::make_unique<GlyphVertex[]>(kVertexBufferMaxBatchCount * kNumGlyphVertices);

  // Configure for 2D drawing with alpha blending enabled.
  D3DRenderStateStash render_state(device);
  render_state.store_and_modify({D3DRS_CULLMODE, D3DCULL_NONE});
  render_state.store_and_modify({D3DRS_ALPHABLENDENABLE, TRUE});
  render_state.store_and_modify({D3DRS_SRCBLEND, D3DBLEND_SRCALPHA});
  render_state.store_and_modify({D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA});
  render_state.store_and_modify({D3DRS_ZENABLE, FALSE});  // Rely on render order.
  render_state.store_and_modify({D3DRS_ZWRITEENABLE, FALSE});
  render_state.store_and_modify({D3DRS_LIGHTING, FALSE});  // Disable lighting.

  // Set texture stage states to avoid any unexpected texturing.
  D3DTextureStateStash texture_state(device);
  texture_state.store_and_modify({D3DTSS_COLOROP, D3DTOP_MODULATE});  // Mix color with white font.
  texture_state.store_and_modify({D3DTSS_COLORARG1, D3DTA_TEXTURE});
  texture_state.store_and_modify({D3DTSS_COLORARG2, D3DTA_DIFFUSE});
  texture_state.store_and_modify({D3DTSS_ALPHAOP, D3DTOP_MODULATE});  // Support color alpha.
  texture_state.store_and_modify({D3DTSS_ALPHAARG1, D3DTA_TEXTURE});
  texture_state.store_and_modify({D3DTSS_ALPHAARG2, D3DTA_DIFFUSE});

  // Note: Not preserving FVF, texture, source, or indices to avoid reference counting.
  device.SetFVF(GlyphVertex::kGlyphVertexFvfCode);
  device.SetTexture(0, texture);
  device.SetStreamSource(0, vertex_buffer, 0, sizeof(GlyphVertex));

  size_t read_index = 0;
  while (read_index < glyph_queue.size()) {
    const int glyphs_left_count = (int)glyph_queue.size() - (int)read_index;
    int empty_space_count = kVertexBufferMaxBatchCount - vertex_buffer_wr_index;

    if ((glyphs_left_count > empty_space_count) && (empty_space_count < kVertexBufferMinBatchCount)) {
      vertex_buffer_wr_index = 0;  // Out of room, so wrap back to start.
      empty_space_count = kVertexBufferMaxBatchCount;
    }
    const int batch_count = std::min(glyphs_left_count, empty_space_count);
    if (batch_count < 1) break;  // Shouldn't happen, but if it does, just abort processing glyphs.

    for (int i = 0; i < batch_count; i++)
      calculate_glyph_vertices(glyph_queue[read_index + i], &vertices[i * kNumGlyphVertices]);

    auto lock_type = (vertex_buffer_wr_index == 0) ? D3DLOCK_DISCARD : D3DLOCK_NOOVERWRITE;
    const int start_vertex_index = vertex_buffer_wr_index * kNumGlyphVertices;
    const int num_batch_vertices = batch_count * kNumGlyphVertices;
    const int start_offset_bytes = start_vertex_index * sizeof(GlyphVertex);
    const int copy_size = num_batch_vertices * sizeof(GlyphVertex);
    void *buffer = nullptr;  // D3D9: Lock ppbData is void**.
    if (FAILED(vertex_buffer->Lock(start_offset_bytes, copy_size, &buffer, lock_type))) {
      release();
      return;
    }
    memcpy(buffer, vertices.get(), copy_size);
    vertex_buffer->Unlock();

    device.SetIndices(index_buffer);  // D3D9: no base-vertex arg here.
    // D3D9: BaseVertexIndex (0) inserted as the 2nd arg; our indices are absolute so it stays 0.
    device.DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, start_vertex_index, num_batch_vertices,
                                vertex_buffer_wr_index * kNumGlyphIndices, batch_count * kNumGlyphTriangles);
    read_index += batch_count;
    vertex_buffer_wr_index += batch_count;
  }

  // Restore D3D state.
  device.SetStreamSource(0, NULL, 0, 0);  // Unbind vertex buffer.
  device.SetIndices(NULL);                // Ensure index_buffer is no longer bound.
  device.SetTexture(0, NULL);             // Ensure texture is no longer bound.
  texture_state.restore_state();
  render_state.restore_state();
}

// Create the index buffer, lock it, fill it with fixed indices, and unlock.
bool BitmapFontBase::create_index_buffer() {
  if (index_buffer) return true;

  static_assert(kVertexBufferMaxBatchCount * kNumGlyphVertices < 0x7fff, "Exceeds 16-bit index");
  static_assert(kNumGlyphIndices == 6);   // Assumed below.
  static_assert(kNumGlyphVertices == 4);  // Assumed below.

  // D3D9: CreateIndexBuffer takes a trailing pSharedHandle (NULL).
  if (FAILED(device.CreateIndexBuffer(kVertexBufferMaxBatchCount * kNumGlyphIndices * sizeof(int16_t), 0,
                                      D3DFMT_INDEX16, D3DPOOL_DEFAULT, &index_buffer, NULL))) {
    index_buffer = nullptr;  // Ensure nullptr.
    return false;
  }

  void *locked_buffer = nullptr;  // D3D9: Lock ppbData is void**.
  if (FAILED(index_buffer->Lock(0, 0, &locked_buffer, D3DLOCK_DISCARD))) {
    release();
    return false;
  }

  // Fill it with a fixed pattern that maps six indices to the four vertices of the
  // two triangles needed for each glyph. Indexing saves memory and enables caching.
  int16_t *indices = reinterpret_cast<int16_t *>(locked_buffer);
  for (int i = 0; i < kVertexBufferMaxBatchCount; ++i) {
    indices[i * kNumGlyphIndices + 0] = i * kNumGlyphVertices + 0;
    indices[i * kNumGlyphIndices + 1] = i * kNumGlyphVertices + 1;
    indices[i * kNumGlyphIndices + 2] = i * kNumGlyphVertices + 2;
    indices[i * kNumGlyphIndices + 3] = i * kNumGlyphVertices + 1;
    indices[i * kNumGlyphIndices + 4] = i * kNumGlyphVertices + 3;
    indices[i * kNumGlyphIndices + 5] = i * kNumGlyphVertices + 2;
  }
  index_buffer->Unlock();
  return true;
}

// Calculates the vertices required to place the glyph on the screen with the correct texture map.
void BitmapFont::calculate_glyph_vertices(const GlyphQueueEntry &entry, GlyphVertex glyph_vertices[4]) const {
  // Note: Transformed vertices require a -0.5 offset to properly align texels with pixels.
  // Vertices in xy 00, 10, 01, 11 order.
  static_assert(kNumGlyphVertices == 4);

  float width = static_cast<float>(entry.glyph->sub_rect.right - entry.glyph->sub_rect.left);
  float height = static_cast<float>(entry.glyph->sub_rect.bottom - entry.glyph->sub_rect.top);
  auto color = entry.color;
  if (entry.glyph->character == kStatsBarBackground) {
    width = stats_bar_width;
    height = stats_bar_height;
    color = D3DCOLOR_XRGB(144, 144, 144);
  } else if (entry.glyph->character == kHealthBarValue) {
    width = entry.hp_percent * ((1.f / 100.f) * stats_bar_width);  // Hack resize for default BitmapFont usage.
    height = stats_bar_height;
    color = (entry.hp_percent > 75)   ? D3DCOLOR_XRGB(0, 192, 0)    // Green
            : (entry.hp_percent > 50) ? D3DCOLOR_XRGB(192, 192, 0)  // Yellow
            : (entry.hp_percent > 25) ? D3DCOLOR_XRGB(192, 96, 40)  // Orange
                                      : D3DCOLOR_XRGB(192, 0, 0);   // Red
  } else if (entry.glyph->character == kBackgroundRect) {
    width = static_cast<float>(background_rect.right - background_rect.left);
    height = static_cast<float>(background_rect.bottom - background_rect.top);
  }

  glyph_vertices[0].x = entry.position.x - 0.5f;
  glyph_vertices[1].x = entry.position.x - 0.5f + width;
  glyph_vertices[2].x = glyph_vertices[0].x;
  glyph_vertices[3].x = glyph_vertices[1].x;

  glyph_vertices[0].u = entry.glyph->sub_rect.left * inverse_texture_size.x;
  glyph_vertices[1].u = entry.glyph->sub_rect.right * inverse_texture_size.x;
  glyph_vertices[2].u = glyph_vertices[0].u;
  glyph_vertices[3].u = glyph_vertices[1].u;

  glyph_vertices[0].y = entry.position.y - 0.5f;
  glyph_vertices[1].y = glyph_vertices[0].y;
  glyph_vertices[2].y = entry.position.y - 0.5f + height;
  glyph_vertices[3].y = glyph_vertices[2].y;

  glyph_vertices[0].v = entry.glyph->sub_rect.top * inverse_texture_size.y;
  glyph_vertices[1].v = glyph_vertices[0].v;
  glyph_vertices[2].v = entry.glyph->sub_rect.bottom * inverse_texture_size.y;
  glyph_vertices[3].v = glyph_vertices[2].v;

  for (int i = 0; i < kNumGlyphVertices; ++i) {
    glyph_vertices[i].z = 0.5f;
    glyph_vertices[i].rhw = 1.0f;
    glyph_vertices[i].color = color;
  }
}

// Factory for creating spritefonts. Returns nullptr if unsuccessful.
std::unique_ptr<SpriteFont> SpriteFont::create_sprite_font(IDirect3DDevice9 &device, const std::string &font_filename) {
  std::unique_ptr<SpriteFont> sprite_font;

  // Attempt to load a filesystem font if there's a candidate name.
  if (!font_filename.empty() && font_filename != kDefaultFontName) {
    std::filesystem::path full_filename = get_fonts_dir() / std::filesystem::path(font_filename + kFontFileExtension);
    sprite_font = std::make_unique<SpriteFont>(device, full_filename.string().c_str());
    if (!sprite_font->is_valid()) {
      logger::logf("[font] Failed to load font file: %s", full_filename.string().c_str());
      sprite_font.reset();  // Release the invalid font and nulls the ptr.
    }
    return sprite_font;
  }

  // Initialize with the embedded default font.
  sprite_font =
      std::make_unique<SpriteFont>(device, std::span<const uint8_t>(default_spritefont, default_spritefont_len));
  if (!sprite_font->is_valid()) {
    sprite_font.reset();  // Release the invalid font and nulls the ptr.
    logger::log("[font] Error initializing default font");
  }
  return sprite_font;
}

// Public interface that queues a string for later rendering in the flush call.
void SpriteFont::queue_string(const char *text, const Vec3 &position, bool center, const D3DCOLOR color,
                              bool grid_align) {
  if (!text || !(*text)) return;  // Skip nullptr or empty strings.

  int start_index = (int)glyph_queue.size();
  BitmapFontBase::queue_string(text, Vec3(0, 0, 0), center, color, grid_align);
  int stop_index = (int)glyph_queue.size();
  glyph_string_queue.push_back({.position = position,
                                .start_index = start_index,
                                .stop_index = stop_index,
                                .scale = scale_factor,
                                .hp_percent = hp_percent,
                                .mana_percent = mana_percent,
                                .stamina_percent = stamina_percent});
}

// Renders all queued glyphs to the screen.
void SpriteFont::flush_queue_to_screen() {
  BitmapFontBase::flush_queue_to_screen();
  glyph_string_queue.clear();
}

// DirectX resources need to be manually released.
void SpriteFont::release() {
  BitmapFontBase::release();
  vertices.reset();
  glyph_string_queue.clear();
}

// Submits glyph sprites to the GPU in batches (3-D billboard).
void SpriteFont::render_queue() {
  if (!vertices) vertices = std::make_unique<Glyph3DVertex[]>(kVertexBufferMaxBatchCount * kNumGlyphVertices);

  // We draw inside the world scene pass, and the last world geometry (terrain when looking down)
  // can leave VERTEX/PIXEL SHADERS bound. A bound pixel shader overrides ALL of our fixed-function
  // texture-stage setup (glyph alpha ignored -> opaque grey blobs, pitch-dependent). Unbind both
  // and restore after the draw.
  IDirect3DVertexShader9 *prev_vs = nullptr;
  IDirect3DPixelShader9 *prev_ps = nullptr;
  device.GetVertexShader(&prev_vs);
  device.GetPixelShader(&prev_ps);
  device.SetVertexShader(NULL);
  device.SetPixelShader(NULL);

  // Configure for 3D drawing with alpha blending enabled.
  D3DRenderStateStash render_state(device);
  render_state.store_and_modify({D3DRS_CULLMODE, D3DCULL_NONE});
  render_state.store_and_modify({D3DRS_ALPHABLENDENABLE, TRUE});
  render_state.store_and_modify({D3DRS_SRCBLEND, D3DBLEND_SRCALPHA});
  render_state.store_and_modify({D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA});
  render_state.store_and_modify({D3DRS_BLENDOP, D3DBLENDOP_ADD});
  render_state.store_and_modify({D3DRS_ZENABLE, TRUE});
  render_state.store_and_modify({D3DRS_ZWRITEENABLE, TRUE});
  render_state.store_and_modify({D3DRS_LIGHTING, FALSE});
  // Drawing inside the world pass, the last geometry (alpha-tested foliage/detail when looking at
  // the ground) can leave ALPHATESTENABLE on with a reference alpha - which rejects our glyphs'
  // anti-aliased edges and turns crisp letters into grey blobs (pitch-dependent). Force it off.
  render_state.store_and_modify({D3DRS_ALPHATESTENABLE, FALSE});
  // We now draw INSIDE the world scene pass, where the client's distance fog is enabled - it would
  // grey out the glyphs by depth. Disable it (and any specular) for our text.
  render_state.store_and_modify({D3DRS_FOGENABLE, FALSE});
  render_state.store_and_modify({D3DRS_SPECULARENABLE, FALSE});

  // Disable a second texture stage the world pass may have left on (would modulate the font with a
  // terrain/detail texture); we only use stage 0.
  DWORD stage1_colorop = D3DTOP_DISABLE;
  device.GetTextureStageState(1, D3DTSS_COLOROP, &stage1_colorop);
  device.SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);

  // Set texture stage states to avoid any unexpected texturing.
  D3DTextureStateStash texture_state(device);
  texture_state.store_and_modify({D3DTSS_COLOROP, D3DTOP_MODULATE});  // Mix color with white font.
  texture_state.store_and_modify({D3DTSS_COLORARG1, D3DTA_TEXTURE});
  texture_state.store_and_modify({D3DTSS_COLORARG2, D3DTA_DIFFUSE});
  texture_state.store_and_modify({D3DTSS_ALPHAOP, D3DTOP_MODULATE});  // Support color alpha.
  texture_state.store_and_modify({D3DTSS_ALPHAARG1, D3DTA_TEXTURE});
  texture_state.store_and_modify({D3DTSS_ALPHAARG2, D3DTA_DIFFUSE});
  // Drawing inside the world pass, stage 0 may carry the terrain pass's texcoord transform / index -
  // both would sample our glyph atlas wrong (grey blur). Force plain, untransformed stage-0 coords.
  texture_state.store_and_modify({D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE});
  texture_state.store_and_modify({D3DTSS_TEXCOORDINDEX, 0});

  // D3D9: MINFILTER is a sampler state (was a texture-stage state in D3D8). Fully specify the sampler
  // - the world pass leaves mip/mag/address state that otherwise samples the atlas wrong.
  D3DSamplerStateStash sampler_state(device);
  sampler_state.store_and_modify({D3DSAMP_MINFILTER, D3DTEXF_LINEAR});
  sampler_state.store_and_modify({D3DSAMP_MAGFILTER, D3DTEXF_LINEAR});
  sampler_state.store_and_modify({D3DSAMP_MIPFILTER, D3DTEXF_NONE});  // Atlas has a single mip level.
  sampler_state.store_and_modify({D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP});
  sampler_state.store_and_modify({D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP});
  sampler_state.store_and_modify({D3DSAMP_SRGBTEXTURE, FALSE});

  // Note: Not preserving FVF, texture, source, or indices to avoid reference counting.
  device.SetFVF(Glyph3DVertex::kFvfCode);
  device.SetTexture(0, texture);
  device.SetStreamSource(0, vertex_buffer, 0, sizeof(Glyph3DVertex));

  D3DXMATRIX worldMatrix, viewMatrix, originalWorldMatrix;
  device.GetTransform(D3DTS_WORLD, &originalWorldMatrix);
  device.GetTransform(D3DTS_VIEW, &viewMatrix);

  // Rotate the quad so it faces the camera (transpose of the view rotation). Built once; the
  // scale is applied PER STRING (entry.scale) so callers can size each billboard independently
  // (e.g. distance-compensated for a constant on-screen size).
  D3DXMATRIX scaleMatrix, rotationMatrix, translationMatrix;
  D3DXMatrixIdentity(&rotationMatrix);
  for (int row = 0; row < 3; row++)  // Transpose rotation components of camera view matrix.
    for (int col = 0; col < 3; col++) rotationMatrix(col, row) = viewMatrix(row, col);
  for (const auto &entry : glyph_string_queue) {
    if (entry.stop_index > (int)glyph_queue.size()) break;

    // Per string: scale (this string's) model space, orient to the camera, translate to world.
    D3DXMatrixScaling(&scaleMatrix, entry.scale, entry.scale, entry.scale);
    D3DXMatrixTranslation(&translationMatrix, entry.position.x, entry.position.y, entry.position.z);
    worldMatrix = scaleMatrix * rotationMatrix * translationMatrix;
    device.SetTransform(D3DTS_WORLD, &worldMatrix);
    hp_percent = entry.hp_percent;  // Recall the stats for access in a sub-method.
    mana_percent = entry.mana_percent;
    stamina_percent = entry.stamina_percent;

    int read_index = entry.start_index;
    while (read_index < entry.stop_index) {
      const int glyphs_left_count = entry.stop_index - read_index;
      int empty_space_count = kVertexBufferMaxBatchCount - vertex_buffer_wr_index;

      if ((glyphs_left_count > empty_space_count) && (empty_space_count < kVertexBufferMinBatchCount)) {
        vertex_buffer_wr_index = 0;  // Out of room, so wrap back to start.
        empty_space_count = kVertexBufferMaxBatchCount;
      }
      const int batch_count = std::min(glyphs_left_count, empty_space_count);
      if (batch_count < 1) break;  // Shouldn't happen, but if it does, just abort processing glyphs.

      for (int i = 0; i < batch_count; i++)
        calculate_glyph_vertices(glyph_queue[read_index + i], &vertices[i * kNumGlyphVertices]);

      auto lock_type = (vertex_buffer_wr_index == 0) ? D3DLOCK_DISCARD : D3DLOCK_NOOVERWRITE;
      const int start_vertex_index = vertex_buffer_wr_index * kNumGlyphVertices;
      const int num_batch_vertices = batch_count * kNumGlyphVertices;
      const int start_offset_bytes = start_vertex_index * sizeof(Glyph3DVertex);
      const int copy_size = num_batch_vertices * sizeof(Glyph3DVertex);
      void *buffer = nullptr;  // D3D9: Lock ppbData is void**.
      if (FAILED(vertex_buffer->Lock(start_offset_bytes, copy_size, &buffer, lock_type))) {
        release();
        return;
      }
      memcpy(buffer, vertices.get(), copy_size);
      vertex_buffer->Unlock();

      device.SetIndices(index_buffer);  // D3D9: no base-vertex arg here.
      // D3D9: BaseVertexIndex (0) inserted as the 2nd arg; our indices are absolute so it stays 0.
      device.DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, start_vertex_index, num_batch_vertices,
                                  vertex_buffer_wr_index * kNumGlyphIndices, batch_count * kNumGlyphTriangles);
      read_index += batch_count;
      vertex_buffer_wr_index += batch_count;
    }
  }

  // Restore D3D state.
  device.SetStreamSource(0, NULL, 0, 0);  // Unbind vertex buffer.
  device.SetIndices(NULL);                // Ensure index_buffer is no longer bound.
  device.SetTexture(0, NULL);             // Ensure texture is no longer bound.
  device.SetTransform(D3DTS_WORLD, &originalWorldMatrix);
  device.SetTextureStageState(1, D3DTSS_COLOROP, stage1_colorop);
  sampler_state.restore_state();
  texture_state.restore_state();
  render_state.restore_state();
  device.SetVertexShader(prev_vs);
  device.SetPixelShader(prev_ps);
  if (prev_vs) prev_vs->Release();  // Get*Shader added a ref.
  if (prev_ps) prev_ps->Release();
}

// Calculates the vertices required to place the glyph in world space with the correct texture map.
void SpriteFont::calculate_glyph_vertices(const GlyphQueueEntry &entry, Glyph3DVertex glyph_vertices[4]) const {
  // Vertices in xy 00, 10, 01, 11 order.
  static_assert(kNumGlyphVertices == 4);
  float width = static_cast<float>(entry.glyph->sub_rect.right - entry.glyph->sub_rect.left);
  float height = static_cast<float>(entry.glyph->sub_rect.bottom - entry.glyph->sub_rect.top);
  float z = (entry.color == kDropShadowColor) ? +1.f : 0.0f;
  auto color = entry.color;
  if (entry.glyph->character == kStatsBarBackground) {
    z = -0.25f;  // Slightly in front of normal text.
    width = stats_bar_width;
    height = stats_bar_height;
    color = D3DCOLOR_ARGB(128, 128, 128, 128);
  } else if (entry.glyph->character == kHealthBarValue) {
    z = -0.5f;  // Slightly more in front.
    width = hp_percent * ((1.f / 100.f) * stats_bar_width);
    height = stats_bar_height;
    color = (hp_percent > 75)   ? D3DCOLOR_XRGB(0, 192, 0)    // Green
            : (hp_percent > 50) ? D3DCOLOR_XRGB(192, 192, 0)  // Yellow
            : (hp_percent > 25) ? D3DCOLOR_XRGB(192, 96, 40)  // Orange
                                : D3DCOLOR_XRGB(192, 0, 0);   // Red
  } else if (entry.glyph->character == kManaBarValue) {
    z = -0.5f;  // Slightly more in front.
    width = mana_percent * ((1.f / 100.f) * stats_bar_width);
    height = stats_bar_height;
    color = D3DCOLOR_XRGB(0, 0x40, 0xf0);  // Use default CON_BLUE color.
  } else if (entry.glyph->character == kStaminaBarValue) {
    z = -0.5f;  // Slightly more in front.
    width = stamina_percent * ((1.f / 100.f) * stats_bar_width);
    height = stats_bar_height;
    color = D3DCOLOR_XRGB(240, 190, 11);  // Yellow / honey.
  }

  glyph_vertices[0].x = entry.position.x;
  glyph_vertices[1].x = entry.position.x + width;
  glyph_vertices[2].x = glyph_vertices[0].x;
  glyph_vertices[3].x = glyph_vertices[1].x;

  glyph_vertices[0].u = entry.glyph->sub_rect.left * inverse_texture_size.x;
  glyph_vertices[1].u = entry.glyph->sub_rect.right * inverse_texture_size.x;
  glyph_vertices[2].u = glyph_vertices[0].u;
  glyph_vertices[3].u = glyph_vertices[1].u;

  // Local +y is screen-down (text is upright), so subtracting screen_offset lifts the whole
  // string up on screen - a camera-facing offset that mimics native nameplate placement.
  const float y_top = entry.position.y - screen_offset;
  glyph_vertices[0].y = y_top;
  glyph_vertices[1].y = y_top;
  glyph_vertices[2].y = y_top + height;
  glyph_vertices[3].y = glyph_vertices[2].y;

  glyph_vertices[0].v = entry.glyph->sub_rect.top * inverse_texture_size.y;
  glyph_vertices[1].v = glyph_vertices[0].v;
  glyph_vertices[2].v = entry.glyph->sub_rect.bottom * inverse_texture_size.y;
  glyph_vertices[3].v = glyph_vertices[2].v;

  for (int i = 0; i < kNumGlyphVertices; ++i) {
    glyph_vertices[i].z = z;
    glyph_vertices[i].color = color;
  }
}

float SpriteFont::get_text_height(const std::string &text) const {
  return BitmapFontBase::get_text_height(text) * scale_factor;
}
