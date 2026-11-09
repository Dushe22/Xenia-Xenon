/**
 * Xenia-Xenon: console-style settings screen.
 *
 * Settings are described as data (category -> options bound to cvars) and
 * drawn with a single, predictable navigation model:
 *   Category list : Up/Down choose, A or Right open.
 *   Options       : Up/Down move, Left/Right change, A toggle/next/edit,
 *                   Y reset to default, B back to the list.
 */

#ifndef XENIA_APP_XENON_SETTINGS_PAGE_H_
#define XENIA_APP_XENON_SETTINGS_PAGE_H_

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "third_party/imgui/imgui.h"
#include "xenia/base/cvar.h"
#include "xenia/config.h"
#include "xenia/kernel/kernel_state.h"
#include "xenia/kernel/xconfig.h"
#include "xenia/ui/xenon_i18n.h"

namespace xe {
namespace app {
namespace xenon_settings {

using ui::i18n::Tr;

enum class Kind { kToggle, kChoice, kRange, kText, kAction, kInfo };

struct Choice {
  const char* value;
  const char* label;
};

struct Option {
  std::string label;
  const char* cvar = nullptr;
  Kind kind = Kind::kToggle;
  std::vector<Choice> choices;
  double min = 0.0, max = 1.0, step = 0.05;
  const char* format = "%.2f";
  const char* cvar2 = nullptr;  // Optional second cvar set to the same value.
  const char* note = nullptr;   // Extra hint shown under the description.
  const char* zero_label = nullptr;  // Range: label to show for 0.
  // Action / info rows (Storage, About).
  std::function<void()> action;           // A
  const char* action_label = nullptr;     // footer hint for A
  std::function<void()> secondary;        // Y
  const char* secondary_label = nullptr;  // footer hint for Y
  std::function<std::string()> value_fn;  // text shown on the right
  std::string desc;                       // overrides cvar description
  // Choice rows backed by something other than a cvar (e.g. XConfig).
  std::function<std::string()> get;
  std::function<void(const std::string&)> set;
  const char* default_value = nullptr;
};

struct Category {
  std::string name;
  std::vector<Option> options;
};

struct State {
  int category = 0;
  int row = 0;
  bool in_options = false;
  bool editing_text = false;
  bool focus_text = false;
  char text_buffer[1024] = {};
  float list_y = -1.0f;
  float row_y = -1.0f;
  float scroll = 0.0f;
  float pane_mix = 0.0f;  // 0 = list active, 1 = options active.
  // Footer hints for the selected row (null = no hint).
  const char* hint_a = nullptr;
  const char* hint_y = nullptr;
};

inline Option Action(std::string label, std::function<void()> action,
                     std::string desc,
                     std::function<std::string()> value_fn = nullptr,
                     const char* action_label = "Select") {
  Option o;
  o.label = std::move(label);
  o.kind = Kind::kAction;
  o.action = std::move(action);
  o.action_label = action_label;
  o.desc = std::move(desc);
  o.value_fn = std::move(value_fn);
  return o;
}

inline Option Info(std::string label, std::function<std::string()> value_fn,
                   std::string desc = {}) {
  Option o;
  o.label = std::move(label);
  o.kind = Kind::kInfo;
  o.value_fn = std::move(value_fn);
  o.desc = std::move(desc);
  return o;
}

// ---- cvar access -----------------------------------------------------------

inline cvar::IConfigVar* Find(const char* name) {
  if (!name || !cvar::ConfigVars) return nullptr;
  auto it = cvar::ConfigVars->find(name);
  return it == cvar::ConfigVars->end() ? nullptr : it->second;
}

inline bool GetNumber(cvar::IConfigVar* v, double* out) {
  if (auto c = dynamic_cast<cvar::ConfigVar<bool>*>(v)) {
    *out = c->GetTypedConfigValue() ? 1.0 : 0.0;
  } else if (auto c = dynamic_cast<cvar::ConfigVar<int32_t>*>(v)) {
    *out = c->GetTypedConfigValue();
  } else if (auto c = dynamic_cast<cvar::ConfigVar<uint32_t>*>(v)) {
    *out = c->GetTypedConfigValue();
  } else if (auto c = dynamic_cast<cvar::ConfigVar<int64_t>*>(v)) {
    *out = double(c->GetTypedConfigValue());
  } else if (auto c = dynamic_cast<cvar::ConfigVar<uint64_t>*>(v)) {
    *out = double(c->GetTypedConfigValue());
  } else if (auto c = dynamic_cast<cvar::ConfigVar<double>*>(v)) {
    *out = c->GetTypedConfigValue();
  } else {
    return false;
  }
  return true;
}

inline bool SetNumber(cvar::IConfigVar* v, double value) {
  if (auto c = dynamic_cast<cvar::ConfigVar<bool>*>(v)) {
    c->SetConfigValue(value != 0.0);
  } else if (auto c = dynamic_cast<cvar::ConfigVar<int32_t>*>(v)) {
    c->SetConfigValue(int32_t(std::lround(value)));
  } else if (auto c = dynamic_cast<cvar::ConfigVar<uint32_t>*>(v)) {
    c->SetConfigValue(uint32_t(std::max(0L, std::lround(value))));
  } else if (auto c = dynamic_cast<cvar::ConfigVar<int64_t>*>(v)) {
    c->SetConfigValue(int64_t(std::llround(value)));
  } else if (auto c = dynamic_cast<cvar::ConfigVar<uint64_t>*>(v)) {
    c->SetConfigValue(uint64_t(std::max(0LL, std::llround(value))));
  } else if (auto c = dynamic_cast<cvar::ConfigVar<double>*>(v)) {
    c->SetConfigValue(value);
  } else {
    return false;
  }
  return true;
}

inline std::string GetString(cvar::IConfigVar* v) {
  if (auto c = dynamic_cast<cvar::ConfigVar<std::string>*>(v)) {
    return c->GetTypedConfigValue();
  }
  double n = 0.0;
  if (GetNumber(v, &n)) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%lld", (long long)std::llround(n));
    return buf;
  }
  return {};
}

inline void SetString(cvar::IConfigVar* v, const std::string& value) {
  if (auto c = dynamic_cast<cvar::ConfigVar<std::string>*>(v)) {
    c->SetConfigValue(value);
  } else {
    SetNumber(v, std::atof(value.c_str()));
  }
}

// ---- data ------------------------------------------------------------------

inline Option Toggle(const char* label, const char* cvar,
                     const char* note = nullptr) {
  Option o;
  o.label = label;
  o.cvar = cvar;
  o.note = note;
  return o;
}

inline Option Pick(const char* label, const char* cvar,
                   std::vector<Choice> choices, const char* cvar2 = nullptr) {
  Option o;
  o.label = label;
  o.cvar = cvar;
  o.kind = Kind::kChoice;
  o.choices = std::move(choices);
  o.cvar2 = cvar2;
  return o;
}

inline Option Range(const char* label, const char* cvar, double min, double max,
                    double step, const char* format,
                    const char* zero_label = nullptr) {
  Option o;
  o.label = label;
  o.cvar = cvar;
  o.kind = Kind::kRange;
  o.min = min;
  o.max = max;
  o.step = step;
  o.format = format;
  o.zero_label = zero_label;
  return o;
}

inline Option Text(const char* label, const char* cvar) {
  Option o;
  o.label = label;
  o.cvar = cvar;
  o.kind = Kind::kText;
  return o;
}

// Language stored in the console's XConfig (read by games via XAM).
inline Option ConsoleLanguage() {
  Option o;
  o.label = "Console language";
  o.kind = Kind::kChoice;
  o.choices = {{"1", "English"},     {"2", "Japanese"},
               {"3", "German"},      {"4", "French"},
               {"5", "Spanish"},     {"6", "Italian"},
               {"7", "Korean"},      {"8", "Traditional Chinese"},
               {"9", "Portuguese"},  {"11", "Polish"},
               {"12", "Russian"},    {"13", "Swedish"},
               {"14", "Turkish"},    {"15", "Norwegian"},
               {"16", "Dutch"},      {"17", "Simplified Chinese"}};
  o.default_value = "1";
  o.desc =
      "Language the emulated console reports to games. Games without that "
      "language fall back to English.";
  o.get = []() -> std::string {
    auto kernel_state = kernel::KernelState::shared();
    if (!kernel_state || !kernel_state->xconfig()) return {};
    return std::to_string(kernel_state->xconfig()->ReadSetting<uint32_t>(
        kernel::XCONFIG_USER_CATEGORY, kernel::XCONFIG_USER_LANGUAGE));
  };
  o.set = [](const std::string& value) {
    auto kernel_state = kernel::KernelState::shared();
    if (!kernel_state || !kernel_state->xconfig()) return;
    const uint32_t language =
        xe::byte_swap(static_cast<uint32_t>(std::stoul(value)));
    kernel_state->xconfig()->WriteSetting(kernel::XCONFIG_USER_CATEGORY,
                                          kernel::XCONFIG_USER_LANGUAGE,
                                          &language);
  };
  return o;
}

inline const std::vector<Category>& Categories() {
  static const std::vector<Category> categories = [] {
    std::vector<Category> c;
    c.push_back({"General",
                 {Toggle("Apply patches", "apply_patches"),
                  Toggle("Controller hotkeys", "controller_hotkeys"),
                  ConsoleLanguage(),
                  Pick("Emulator language", "ui_language",
                       {{"auto", "Automatic"},
                        {"en", "English"},
                        {"es", "Español"},
                        {"pt", "Português"},
                        {"fr", "Français"},
                        {"de", "Deutsch"},
                        {"it", "Italiano"},
                        {"ja", "日本語"},
                        {"ko", "한국어"},
                        {"zh", "简体中文"},
                        {"ru", "Русский"}})}});
    c.push_back(
        {"Graphics",
         {Range("Resolution scale", "draw_resolution_scale_x", 1, 3, 1,
                "%.0fx"),
          Toggle("V-Sync", "vsync"),
          Toggle("Preload shaders before starting", "preload_shaders"),
          Range("Framerate limit", "framerate_limit", 0, 240, 1, "%.0f fps",
                "Off"),
          Pick("Render target path", "render_target_path_d3d12",
               {{"", "Any"}, {"rov", "ROV"}, {"rtv", "RTV"}}),
          Toggle("Native 2x MSAA", "native_2x_msaa"),
          Toggle("Scaled texture offsets",
                 "draw_resolution_scaled_texture_offsets"),
          Toggle("Gamma render target as sRGB", "gamma_render_target_as_srgb"),
          Toggle("Allow invalid fetch constants",
                 "gpu_allow_invalid_fetch_constants"),
          Toggle("DXBC switch", "dxbc_switch"),
          Toggle("Fuzzy alpha epsilon", "use_fuzzy_alpha_epsilon"),
          Toggle("Native stencil value output", "native_stencil_value_output"),
          Toggle("SNORM16 full range", "snorm16_render_target_full_range"),
          Toggle("Clamp EDRAM range to min",
                 "mrt_edram_used_range_clamp_to_min"),
          Range("Occlusion lower threshold",
                "query_occlusion_sample_lower_threshold", -1, 1000, 1, "%.0f"),
          Range("Occlusion upper threshold",
                "query_occlusion_sample_upper_threshold", -1, 1000, 1, "%.0f"),
          Toggle("Store shaders", "store_shaders")}});
    // Resolution scale writes the same value to X and Y.
    c.back().options[0].cvar2 = "draw_resolution_scale_y";
    c.push_back(
        {"Display",
         {Pick("Upscaling & sharpening", "postprocess_scaling_and_sharpening",
               {{"", "None"},
                {"bilinear", "Bilinear"},
                {"cas", "AMD CAS"},
                {"fsr", "AMD FSR"}}),
          Pick("Anti-aliasing", "postprocess_antialiasing",
               {{"", "None"},
                {"fxaa", "FXAA"},
                {"fxaa_extreme", "FXAA Extreme"}}),
          Range("CAS extra sharpness",
                "postprocess_ffx_cas_additional_sharpness", 0, 1, 0.05, "%.2f"),
          Range("FSR upsampling passes",
                "postprocess_ffx_fsr_max_upsampling_passes", 1, 4, 1, "%.0fx"),
          Range("FSR sharpness reduction",
                "postprocess_ffx_fsr_sharpness_reduction", 0, 2, 0.05,
                "%.2f stops"),
          Toggle("Letterbox", "present_letterbox"),
          Range("Safe area width", "present_safe_area_x", 1, 100, 1, "%.0f%%"),
          Range("Safe area height", "present_safe_area_y", 1, 100, 1, "%.0f%%"),
          Toggle("Dither output", "postprocess_dither"),
          Toggle("Render-pass clear on present", "present_render_pass_clear"),
          Toggle("Present from non-UI thread",
                 "host_present_from_non_ui_thread")}});
    c.push_back({"Video mode",
                 {Pick("Internal resolution", "internal_display_resolution",
                       {{"0", "640x480"},
                        {"1", "640x576"},
                        {"2", "720x480"},
                        {"3", "720x576"},
                        {"4", "800x600"},
                        {"5", "848x480"},
                        {"6", "1024x768"},
                        {"7", "1152x864"},
                        {"8", "1280x720"},
                        {"9", "1280x768"},
                        {"10", "1280x960"},
                        {"11", "1280x1024"},
                        {"12", "1360x768"},
                        {"13", "1440x900"},
                        {"14", "1680x1050"},
                        {"15", "1920x540"},
                        {"16", "1920x1080"},
                        {"17", "Custom"}}),
                  Range("Custom width", "internal_display_resolution_x", 64,
                        1920, 16, "%.0f px"),
                  Range("Custom height", "internal_display_resolution_y", 64,
                        1080, 8, "%.0f px"),
                  Toggle("Widescreen", "widescreen"),
                  Toggle("50 Hz video mode", "use_50Hz_mode"),
                  Toggle("Interlaced", "interlaced")}});
    c.push_back(
        {"D3D12",
         {Toggle("Variable refresh / tearing",
                 "d3d12_allow_variable_refresh_rate_and_tearing"),
          Pick("Readback resolve", "readback_resolve",
               {{"none", "None"}, {"fast", "Fast"}, {"full", "Full"}}),
          Toggle("Bindless resources", "d3d12_bindless"),
          Toggle("Submit on primary buffer end",
                 "d3d12_submit_on_primary_buffer_end"),
          Toggle("Clear memory page state", "clear_memory_page_state"),
          Toggle("Shader depth fix", "d3d12_shader_depth_fix"),
          Toggle("Reupload vertex data", "d3d12_reupload_vertex_data",
                 "Lowers performance. For testing stretched polygons."),
          Toggle("Pad vertex constants", "d3d12_pad_vertex_constants"),
          Toggle("Continuous capture", "trace_rolling_capture",
                 "Lowers performance. Leave it off unless you are capturing a "
                 "glitch."),
          Toggle("Tiled shared memory", "d3d12_enable_tiled_shared_memory",
                 "Keep this off on Xbox, otherwise games will crash.")}});
    c.push_back({"Audio",
                 {Pick("Audio system", "apu",
                       {{"any", "Automatic"},
                        {"xaudio2", "XAudio2"},
                        {"sdl", "SDL"},
                        {"nop", "Disabled"}}),
                  Toggle("Mute", "mute")}});
    c.push_back(
        {"Controls",
         {Toggle("Vibration", "vibration"),
          Toggle("Forward Guide button", "guide_button"),
          Range("Left stick deadzone", "left_stick_deadzone_percentage", 0, 1,
                0.01, "%.2f"),
          Range("Right stick deadzone", "right_stick_deadzone_percentage", 0, 1,
                0.01, "%.2f"),
          Range("Left trigger deadzone", "left_trigger_deadzone_percentage", 0,
                1, 0.01, "%.2f"),
          Range("Right trigger deadzone", "right_trigger_deadzone_percentage",
                0, 1, 0.01, "%.2f")}});
    c.push_back(
        {"Interface",
         {Toggle("Welcome screen on start", "ui_show_welcome"),
          Toggle("Achievement pop-ups", "show_achievement_notification"),
          Toggle("Storage selection dialog", "storage_selection_dialog"),
          Toggle("Dynamic game backgrounds",
                 "ui_enable_dynamic_game_backgrounds"),
          Toggle("Prefer short descriptions", "ui_prefer_short_descriptions"),
          Toggle("Prefer banner hero art", "ui_prefer_banner_hero_art"),
          Toggle("Prefer icon art in list", "ui_prefer_icon_gamelist_art"),
          Toggle("Show genre", "ui_show_genre_metadata"),
          Toggle("Show developer", "ui_show_developer_metadata"),
          Toggle("Show title", "ui_show_title_metadata"),
          Toggle("Show title ID", "ui_show_title_id_metadata"),
          Toggle("Show release date", "ui_show_release_date_metadata"),
          Toggle("Show user rating", "ui_show_user_rating_metadata"),
          Pick("Accent color", "ui_border_color",
               {{"xenon", "Xenon"},
                {"green", "Green"},
                {"purple", "Purple"},
                {"red", "Red"},
                {"orange", "Orange"},
                {"blue", "Blue"},
                {"yellow", "Yellow"},
                {"grey", "Grey"},
                {"black", "Black"},
                {"white", "White"}}),
          Pick("Text effect", "ui_text_effect",
               {{"none", "None"},
                {"shadow", "Shadow"},
                {"lift", "Lift"},
                {"stroke_fill", "Stroke"},
                {"glow", "Glow"},
                {"outline", "Outline"},
                {"double_shadow", "Double shadow"},
                {"bold", "Bold"}}),
          Pick("Text color", "ui_text_color",
               {{"white", "White"}, {"black", "Black"}}),
          Range("Text brightness", "ui_text_brightness", 0.25, 2, 0.05, "%.2f"),
          Range("Text contrast", "ui_text_contrast", 0.25, 2, 0.05, "%.2f")}});
    c.push_back({"System",
                 {Toggle("Mount cache partition", "mount_cache"),
                  Toggle("Mount scratch partition", "mount_scratch"),
                  Toggle("Mount memory unit", "mount_memory_unit"),
                  Toggle("Host/guest stack sync",
                         "enable_host_guest_stack_synchronization"),
                  Toggle("Ignore offset for ranged allocations",
                         "ignore_offset_for_ranged_allocations"),
                  Toggle("Protect on release", "protect_on_release"),
                  Toggle("Protect zero page", "protect_zero"),
                  Toggle("Scribble heap", "scribble_heap"),
                  Text("Guest command line", "cl")}});
    // User-facing descriptions (override the internal cvar help).
    static const std::pair<const char*, const char*> kDescriptions[] = {
        {"apply_patches",
         "Applies game patches from the patches folder when a game starts. "
         "Patches can fix bugs or unlock frame rates."},
        {"controller_hotkeys",
         "Enables emulator shortcuts on the controller while a game is "
         "running."},
        {"ui_language",
         "Language of the Xenon menus. Automatic follows the console's "
         "language."},
        {"draw_resolution_scale_x",
         "Renders games at a multiple of their original resolution (sets width "
         "and height together). Higher looks sharper but needs much more GPU; "
         "1x is safest on Xbox Series S."},
        {"vsync",
         "Syncs frames to the display refresh to avoid tearing. Turning it off "
         "can lower input lag but may cause tearing."},
        {"framerate_limit",
         "Caps the frame rate. Off leaves it to the game; set it to stop "
         "patched or unlocked games from running too fast."},
        {"render_target_path_d3d12",
         "How the Xbox 360 EDRAM is emulated. ROV is more accurate, RTV is "
         "faster. Any lets Xenia choose."},
        {"native_2x_msaa",
         "Uses the GPU's own 2x MSAA for games that request it. Faster, but "
         "may cause small edge artifacts."},
        {"draw_resolution_scaled_texture_offsets",
         "Corrects texture offsets when Resolution scale is above 1x. Turn off "
         "if scaled games look misaligned."},
        {"gamma_render_target_as_srgb",
         "Uses sRGB render targets for gamma. Can fix scenes that look too "
         "bright or too dark in some games."},
        {"gpu_allow_invalid_fetch_constants",
         "Lets games use invalid texture or vertex data instead of skipping "
         "the draw. Can fix missing graphics, but may cause glitches or "
         "crashes."},
        {"dxbc_switch",
         "Uses switch statements in generated shaders. Leave on unless a game "
         "shows shader problems."},
        {"use_fuzzy_alpha_epsilon",
         "Uses a small tolerance for alpha testing. Fixes flickering or "
         "missing transparency in some games."},
        {"native_stencil_value_output",
         "Writes stencil values directly. More accurate, but may not work on "
         "every GPU."},
        {"snorm16_render_target_full_range",
         "Emulates the full range of 16-bit signed formats. More accurate but "
         "slower; leave off unless a game needs it."},
        {"mrt_edram_used_range_clamp_to_min",
         "Clamps EDRAM use when several render targets are bound. Fixes "
         "corruption in some games."},
        {"query_occlusion_sample_lower_threshold",
         "Fake occlusion results: below this many samples an object counts as "
         "hidden. Use -1 to always report visible."},
        {"query_occlusion_sample_upper_threshold",
         "Fake occlusion results: above this many samples an object counts as "
         "visible."},
        {"store_shaders",
         "Saves compiled shaders so games load faster and stutter less on the "
         "next launch."},
        {"postprocess_scaling_and_sharpening",
         "Filter used to scale the image to your screen. AMD CAS sharpens, AMD "
         "FSR upscales with sharpening."},
        {"postprocess_antialiasing",
         "Smooths jagged edges on the final image. FXAA Extreme is stronger "
         "but blurs more."},
        {"postprocess_ffx_cas_additional_sharpness",
         "Extra sharpening when Upscaling is set to AMD CAS. 0 is the default "
         "amount."},
        {"postprocess_ffx_fsr_max_upsampling_passes",
         "Maximum FSR passes when Upscaling is set to AMD FSR. More passes "
         "allow bigger upscales but cost more GPU."},
        {"postprocess_ffx_fsr_sharpness_reduction",
         "Lowers FSR sharpening. 0 is sharpest; higher values look softer."},
        {"present_letterbox",
         "Keeps the game's aspect ratio by adding black bars instead of "
         "stretching to fill the screen."},
        {"present_safe_area_x",
         "Width of the visible image, as a percentage. Lower it if the edges "
         "are cut off on your TV."},
        {"present_safe_area_y",
         "Height of the visible image, as a percentage. Lower it if the edges "
         "are cut off on your TV."},
        {"postprocess_dither",
         "Adds subtle dither to reduce color banding in gradients."},
        {"present_render_pass_clear",
         "Clears the screen before drawing each frame. Turn off only if you "
         "see a black screen or flicker."},
        {"host_present_from_non_ui_thread",
         "Presents frames from the emulation thread. Can smooth frame pacing "
         "but may cause stutter on some setups."},
        {"internal_display_resolution",
         "Video output resolution the emulated console reports to games. Most "
         "games expect 1280x720. Custom uses the width and height below."},
        {"internal_display_resolution_x",
         "Width used when Internal resolution is set to Custom."},
        {"internal_display_resolution_y",
         "Height used when Internal resolution is set to Custom."},
        {"widescreen", "Tells games the TV is 16:9. Turn off for 4:3 output."},
        {"use_50Hz_mode",
         "Reports a 50 Hz PAL video mode to games. Only useful for PAL games "
         "that need it."},
        {"interlaced",
         "Reports an interlaced video mode to games. Leave off unless a game "
         "requires it."},
        {"d3d12_allow_variable_refresh_rate_and_tearing",
         "Allows variable refresh rate and tearing. Can lower input lag on VRR "
         "displays."},
        {"readback_resolve",
         "Copies GPU results back to the emulated console's memory. Fast is a "
         "good balance, Full is the most accurate but slowest, None is fastest "
         "but breaks some effects."},
        {"d3d12_bindless",
         "Uses bindless resources for better performance. Leave on unless a "
         "game crashes."},
        {"d3d12_submit_on_primary_buffer_end",
         "Sends GPU work earlier to reduce latency. Turn off if you see "
         "stutter."},
        {"clear_memory_page_state",
         "Clears GPU memory tracking each frame. Can fix graphics that don't "
         "update, at a performance cost."},
        {"d3d12_enable_tiled_shared_memory",
         "Uses tiled resources for the emulated shared memory."},
        {"trace_rolling_capture",
         "Keeps the last seconds of gameplay in memory. When you see a glitch, "
         "open the quick menu and choose Capture GPU frame to save it to the "
         "traces folder."},
        {"d3d12_reupload_vertex_data",
         "Copies the vertex data from game memory again for every draw. Turn "
         "it on if stretched polygons or random lines appear, to check whether "
         "the GPU is using outdated data."},
        {"d3d12_pad_vertex_constants",
         "Makes shaders that read past their constants get zeros instead of "
         "leftover memory. Turn it on if random stretched polygons or lines "
         "appear."},
        {"d3d12_pipeline_library",
         "Saves compiled shaders so games start faster and stutter less the "
         "next time. Restart the game after changing it."},
        {"d3d12_shader_depth_fix",
         "Fixes see-through or invisible characters in some games, such as WWE "
         "13. Restart the game after changing it."},
        {"apu",
         "Audio output used by the emulator. Automatic picks the best one for "
         "this console. Disabled turns sound off."},
        {"mute", "Silences all game audio."},
        {"vibration", "Enables controller rumble."},
        {"guide_button",
         "Sends the Guide button to the game instead of handling it in the "
         "emulator."},
        {"left_stick_deadzone_percentage",
         "How far the left stick must move before input registers. Raise it if "
         "your character drifts."},
        {"right_stick_deadzone_percentage",
         "How far the right stick must move before input registers. Raise it "
         "if the camera drifts."},
        {"left_trigger_deadzone_percentage",
         "How far the left trigger must be pressed before it registers."},
        {"right_trigger_deadzone_percentage",
         "How far the right trigger must be pressed before it registers."},
        {"ui_show_welcome",
         "Shows the welcome screen with the Start playing button each time "
         "Xenon opens."},
        {"show_achievement_notification",
         "Shows a pop-up when you unlock an achievement."},
        {"storage_selection_dialog",
         "Asks which storage device to use when a game wants to save."},
        {"ui_enable_dynamic_game_backgrounds",
         "Shows the selected game's artwork behind the Library."},
        {"ui_prefer_short_descriptions",
         "Uses shorter game descriptions when available."},
        {"ui_prefer_banner_hero_art",
         "Prefers banner art over background art behind the selected game."},
        {"ui_prefer_icon_gamelist_art",
         "Uses game icons instead of box art for the covers in the Library."},
        {"ui_show_genre_metadata", "Shows the game's genre in the Library."},
        {"ui_show_developer_metadata",
         "Shows the game's developer in the Library."},
        {"ui_show_title_metadata", "Shows the game's title in the Library."},
        {"ui_show_title_id_metadata",
         "Shows the game's title ID chip in the Library."},
        {"ui_show_release_date_metadata",
         "Shows the game's release year in the Library."},
        {"ui_show_user_rating_metadata",
         "Shows the game's user rating in the Library."},
        {"ui_border_color",
         "Accent color for highlights, switches and focus outlines. Xenon is "
         "the default."},
        {"ui_text_effect",
         "Effect drawn behind menu text in older dialogs. None looks cleanest "
         "with the Xenon theme."},
        {"ui_text_color", "Base color of text in older dialogs."},
        {"ui_text_brightness", "Brightness of text in older dialogs."},
        {"ui_text_contrast", "Contrast of text in older dialogs."},
        {"mount_cache",
         "Mounts the emulated console's cache partition. Some games need it to "
         "load."},
        {"mount_scratch",
         "Mounts the emulated scratch partition. Some games need it to load."},
        {"mount_memory_unit",
         "Mounts an emulated memory unit as extra save storage."},
        {"enable_host_guest_stack_synchronization",
         "Keeps the CPU emulator's stack in sync. Fixes crashes in some games "
         "at a small performance cost."},
        {"ignore_offset_for_ranged_allocations",
         "Memory allocation workaround for a few games. Leave off unless a "
         "game needs it."},
        {"protect_on_release",
         "Protects freed memory to catch emulator bugs. For debugging; leave "
         "off."},
        {"protect_zero",
         "Protects the zero memory page so invalid accesses crash cleanly. For "
         "debugging."},
        {"scribble_heap",
         "Fills new memory with garbage to expose bugs. For debugging; leave "
         "off."},
        {"cl",
         "Extra command-line arguments passed to the game. Leave empty unless "
         "a game needs them."},
    };
    for (auto& cat : c) {
      for (auto& o : cat.options) {
        for (const auto& d : kDescriptions) {
          if (o.cvar && std::string(o.cvar) == d.first) o.desc = d.second;
        }
      }
    }
    // Drop options whose cvar does not exist in this build.
    for (auto& cat : c) {
      cat.options.erase(
          std::remove_if(cat.options.begin(), cat.options.end(),
                         [](const Option& o) { return !o.set && !Find(o.cvar); }),
          cat.options.end());
    }
    return c;
  }();
  return categories;
}

// ---- value helpers
// -----------------------------------------------------------

inline int ChoiceIndex(const Option& o) {
  const std::string cur = o.get ? o.get() : GetString(Find(o.cvar));
  for (size_t i = 0; i < o.choices.size(); ++i) {
    if (cur == o.choices[i].value) return int(i);
  }
  // Numeric cvars may store e.g. "1" while value strings are "1".
  return -1;
}

inline std::string ValueText(const Option& o) {
  cvar::IConfigVar* v = Find(o.cvar);
  switch (o.kind) {
    case Kind::kToggle: {
      double n = 0;
      GetNumber(v, &n);
      return Tr(n != 0.0 ? "On" : "Off");
    }
    case Kind::kChoice: {
      const int i = ChoiceIndex(o);
      return i >= 0 ? Tr(o.choices[i].label) : o.get ? o.get() : GetString(v);
    }
    case Kind::kRange: {
      double n = 0;
      GetNumber(v, &n);
      if (o.zero_label && n == 0.0) return Tr(o.zero_label);
      char buf[48];
      std::snprintf(buf, sizeof(buf), o.format, n);
      return buf;
    }
    case Kind::kText: {
      const std::string s = GetString(v);
      return s.empty() ? Tr("Not set") : s;
    }
    case Kind::kAction:
    case Kind::kInfo:
      if (!o.value_fn) return {};
      const std::string value = o.value_fn();
      return Tr(value.c_str());
  }
  return {};
}

inline void Commit() { config::SaveConfig(); }

inline void Change(const Option& o, int dir) {
  if (o.set) {
    const int count = int(o.choices.size());
    if (!count) return;
    int i = ChoiceIndex(o);
    i = i < 0 ? 0 : (i + dir + count) % count;
    o.set(o.choices[i].value);
    return;
  }
  cvar::IConfigVar* v = Find(o.cvar);
  if (!v) return;
  switch (o.kind) {
    case Kind::kToggle: {
      double n = 0;
      GetNumber(v, &n);
      SetNumber(v, n != 0.0 ? 0.0 : 1.0);
      break;
    }
    case Kind::kChoice: {
      const int count = int(o.choices.size());
      if (!count) return;
      int i = ChoiceIndex(o);
      i = i < 0 ? 0 : (i + dir + count) % count;
      SetString(v, o.choices[i].value);
      if (o.cvar2) SetString(Find(o.cvar2), o.choices[i].value);
      break;
    }
    case Kind::kRange: {
      double n = 0;
      GetNumber(v, &n);
      n = std::clamp(n + dir * o.step, o.min, o.max);
      n = std::round(n / o.step) * o.step;
      SetNumber(v, n);
      if (o.cvar2) SetNumber(Find(o.cvar2), n);
      break;
    }
    case Kind::kText:
    case Kind::kAction:
    case Kind::kInfo:
      return;
  }
  Commit();
}

inline void Reset(const Option& o) {
  if (o.set) {
    if (o.default_value) o.set(o.default_value);
    return;
  }
  if (auto v = Find(o.cvar)) v->ResetConfigValueToDefault();
  if (auto v = Find(o.cvar2)) v->ResetConfigValueToDefault();
  Commit();
}

inline bool Pressed(ImGuiKey a, ImGuiKey b = ImGuiKey_None,
                    ImGuiKey c = ImGuiKey_None, bool repeat = true) {
  return ImGui::IsKeyPressed(a, repeat) ||
         (b != ImGuiKey_None && ImGui::IsKeyPressed(b, repeat)) ||
         (c != ImGuiKey_None && ImGui::IsKeyPressed(c, repeat));
}

// ---- screen
// ------------------------------------------------------------------

// Draws the settings screen inside the current window and handles input.
// Returns true while the options pane (right side) is active.
inline bool IsValueKind(const Option& o) {
  return o.kind == Kind::kToggle || o.kind == Kind::kChoice ||
         o.kind == Kind::kRange;
}

inline bool Draw(State& st, const std::vector<Category>& cats, float s,
                 ImFont* display_font) {
  st.hint_a = nullptr;
  st.hint_y = nullptr;
  if (cats.empty()) return false;
  ImGuiIO& io = ImGui::GetIO();
  const float dt = std::min(io.DeltaTime, 0.1f);
  const float ease = 1.0f - std::exp(-dt * 16.0f);
  st.category = std::clamp(st.category, 0, int(cats.size()) - 1);
  const Category& cat = cats[st.category];
  const int opt_count = int(cat.options.size());
  if (opt_count == 0) st.in_options = false;
  st.row = std::clamp(st.row, 0, std::max(0, opt_count - 1));

  ImFont* font = ImGui::GetFont();
  if (!display_font) display_font = font;
  const ImVec2 origin = ImGui::GetCursorScreenPos();
  const ImVec2 avail = ImGui::GetContentRegionAvail();
  ImDrawList* dl = ImGui::GetWindowDrawList();

  // Keep ImGui's own navigation out of the way; this screen owns its input.
  ImGui::InvisibleButton("##xenon_settings_surface", avail,
                         ImGuiButtonFlags_None);
  const bool popup_open =
      ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId);

  // ---- input --------------------------------------------------------------
  const bool up = Pressed(ImGuiKey_GamepadDpadUp, ImGuiKey_GamepadLStickUp,
                          ImGuiKey_UpArrow);
  const bool down = Pressed(ImGuiKey_GamepadDpadDown,
                            ImGuiKey_GamepadLStickDown, ImGuiKey_DownArrow);
  const bool left = Pressed(ImGuiKey_GamepadDpadLeft,
                            ImGuiKey_GamepadLStickLeft, ImGuiKey_LeftArrow);
  const bool right = Pressed(ImGuiKey_GamepadDpadRight,
                             ImGuiKey_GamepadLStickRight, ImGuiKey_RightArrow);
  // Single steps for lists: holding left/right must not run through them.
  const bool left_once = Pressed(ImGuiKey_GamepadDpadLeft,
                                 ImGuiKey_GamepadLStickLeft,
                                 ImGuiKey_LeftArrow, false);
  const bool right_once = Pressed(ImGuiKey_GamepadDpadRight,
                                  ImGuiKey_GamepadLStickRight,
                                  ImGuiKey_RightArrow, false);
  const bool accept =
      Pressed(ImGuiKey_GamepadFaceDown, ImGuiKey_Enter, ImGuiKey_None, false);
  const bool back = Pressed(ImGuiKey_GamepadFaceRight, ImGuiKey_Escape,
                            ImGuiKey_Backspace, false);
  const bool reset =
      Pressed(ImGuiKey_GamepadFaceUp, ImGuiKey_Delete, ImGuiKey_None, false);

  if (!popup_open && !st.editing_text) {
    if (!st.in_options) {
      if (up) {
        st.category = (st.category + int(cats.size()) - 1) % int(cats.size());
        st.row = 0;
        st.scroll = 0.0f;
      } else if (down) {
        st.category = (st.category + 1) % int(cats.size());
        st.row = 0;
        st.scroll = 0.0f;
      } else if ((accept || right) && opt_count > 0) {
        st.in_options = true;
      }
    } else {
      const Option& o = cat.options[st.row];
      if (back) {
        st.in_options = false;
      } else if (up) {
        st.row = (st.row + opt_count - 1) % opt_count;
      } else if (down) {
        st.row = (st.row + 1) % opt_count;
      // Left/right only adjust sliders and lists. On/off switches change
      // with A alone, so a stray direction never flips one by accident.
      } else if (o.kind == Kind::kRange && (left || right)) {
        Change(o, left ? -1 : +1);
      } else if (o.kind == Kind::kChoice && (left_once || right_once)) {
        Change(o, left_once ? -1 : +1);
      } else if (accept) {
        if (o.kind == Kind::kAction) {
          if (o.action) o.action();
        } else if (o.kind == Kind::kInfo) {
        } else if (o.kind == Kind::kText) {
          const std::string cur = GetString(Find(o.cvar));
          std::snprintf(st.text_buffer, sizeof(st.text_buffer), "%s",
                        cur.c_str());
          st.editing_text = true;
          st.focus_text = true;
        } else if (o.kind != Kind::kRange) {
          Change(o, +1);
        }
      } else if (reset) {
        if (o.secondary) {
          o.secondary();
        } else if (o.cvar) {
          Reset(o);
        }
      }
    }
  }

  // ---- layout -------------------------------------------------------------
  const ImU32 kText = IM_COL32(238, 241, 255, 255);
  const ImU32 kDim = IM_COL32(160, 168, 205, 255);
  const ImU32 kFaint = IM_COL32(160, 168, 205, 140);
  st.pane_mix += ((st.in_options ? 1.0f : 0.0f) - st.pane_mix) * ease;

  const float list_w = 290.0f * s;
  const float gap = 36.0f * s;
  const float list_x = origin.x;
  const float pane_x = origin.x + list_w + gap;
  const float pane_w = std::max(100.0f, avail.x - list_w - gap);
  const float top = origin.y + 8.0f * s;
  // Use the visible area: the content region can extend past the clip rect.
  const float bottom =
      std::min(origin.y + avail.y,
               std::min(dl->GetClipRectMax().y,
                        ImGui::GetWindowPos().y + ImGui::GetWindowSize().y)) -
      8.0f * s;

  auto with_alpha = [](ImU32 c, float a) {
    const int alpha = int(((c >> IM_COL32_A_SHIFT) & 0xFF) * a);
    return (c & ~IM_COL32_A_MASK) | (ImU32(alpha) << IM_COL32_A_SHIFT);
  };

  // Category list (fixed in place).
  const float list_alpha = 1.0f - 0.45f * st.pane_mix;
  const float cat_h = 56.0f * s;
  const float cat_gap = 6.0f * s;
  {
    const float target = top + st.category * (cat_h + cat_gap);
    if (st.list_y < 0.0f) st.list_y = target;
    st.list_y += (target - st.list_y) * ease;
    const ImVec2 mn(list_x, st.list_y), mx(list_x + list_w, st.list_y + cat_h);
    dl->AddRectFilled(
        mn, mx,
        with_alpha(IM_COL32(112, 108, 255, 225), 1.0f - 0.55f * st.pane_mix),
        cat_h * 0.5f);
    for (int i = 0; i < int(cats.size()); ++i) {
      const float y = top + i * (cat_h + cat_gap);
      const ImVec2 imn(list_x, y), imx(list_x + list_w, y + cat_h);
      if (ImGui::IsMouseHoveringRect(imn, imx) &&
          ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        st.category = i;
        st.row = 0;
        st.scroll = 0.0f;
        st.in_options = false;
      }
      const float f = 25.0f * s;
      const bool on = i == st.category;
      dl->AddText(font, f, ImVec2(list_x + 26.0f * s, y + (cat_h - f) * 0.5f),
                  with_alpha(on ? kText : kDim, list_alpha),
                  Tr(cats[i].name.c_str()));
    }
  }

  // Options pane.
  const float pane_alpha = 0.55f + 0.45f * st.pane_mix;
  dl->AddText(display_font, 40.0f * s, ImVec2(pane_x, top - 4.0f * s),
              with_alpha(kText, pane_alpha), Tr(cat.name.c_str()));
  {
    char count[32];
    std::snprintf(count, sizeof(count), Tr("%d OPTIONS"), opt_count);
    const float f = 17.0f * s;
    const float w = font->CalcTextSizeA(f, FLT_MAX, 0.0f, count).x;
    dl->AddText(font, f, ImVec2(pane_x + pane_w - w, top + 14.0f * s), kFaint,
                count);
  }

  // Description of the selected option, sized to fit its text.
  std::string desc;
  if (opt_count > 0) {
    const Option& o = cat.options[st.row];
    if (!o.desc.empty()) {
      desc = Tr(o.desc.c_str());
    } else if (auto v = Find(o.cvar)) {
      desc = v->description();
    }
    if (o.note) {
      desc = std::string(Tr(o.note)) + (desc.empty() ? "" : "\n") + desc;
    }
    if (desc.empty()) desc = Tr(o.label.c_str());
    // Footer hints for this row.
    switch (o.kind) {
      case Kind::kToggle:
        st.hint_a = Tr("Toggle");
        break;
      case Kind::kChoice:
        st.hint_a = Tr("Next");
        break;
      case Kind::kText:
        st.hint_a = Tr("Edit");
        break;
      case Kind::kAction:
        st.hint_a = Tr(o.action_label);
        break;
      default:
        break;
    }
    st.hint_y = o.secondary ? Tr(o.secondary_label)
                            : (o.cvar ? Tr("Reset") : nullptr);
  }
  const float desc_pad = 18.0f * s;
  const float desc_wrap = pane_w - desc_pad * 2.0f;
  const float desc_max_h = std::max(90.0f * s, avail.y * 0.4f);
  float desc_font = 21.0f * s;
  float desc_text_h = 0.0f;
  while (true) {
    desc_text_h =
        font->CalcTextSizeA(desc_font, FLT_MAX, desc_wrap, desc.c_str()).y;
    if (desc_text_h + desc_pad * 2.0f <= desc_max_h || desc_font <= 15.0f * s)
      break;
    desc_font -= 1.0f * s;
  }
  const float desc_h = desc.empty() ? 0.0f
                                    : std::clamp(desc_text_h + desc_pad * 2.0f,
                                                 72.0f * s, desc_max_h);
  const float rows_top = top + 64.0f * s;
  const float rows_bottom = bottom - desc_h - 12.0f * s;
  const float row_h = 64.0f * s;
  const float row_gap = 8.0f * s;
  const float view_h = std::max(row_h, rows_bottom - rows_top);
  const float content_h = opt_count * (row_h + row_gap);

  // Keep the selected row in view (smooth).
  {
    const float sel_top = st.row * (row_h + row_gap);
    float target = st.scroll;
    if (sel_top < target) target = sel_top;
    if (sel_top + row_h > target + view_h) target = sel_top + row_h - view_h;
    // Right stick / wheel scroll the pane freely.
    const float stick =
        io.KeysData[ImGuiKey_GamepadRStickDown - ImGuiKey_NamedKey_BEGIN]
            .AnalogValue -
        io.KeysData[ImGuiKey_GamepadRStickUp - ImGuiKey_NamedKey_BEGIN]
            .AnalogValue;
    if (std::fabs(stick) > 0.15f) target += stick * 1400.0f * s * dt;
    if (ImGui::IsMouseHoveringRect(ImVec2(pane_x, rows_top),
                                   ImVec2(pane_x + pane_w, rows_bottom))) {
      target -= io.MouseWheel * row_h;
    }
    target = std::clamp(target, 0.0f, std::max(0.0f, content_h - view_h));
    st.scroll += (target - st.scroll) * ease;
  }

  dl->PushClipRect(ImVec2(pane_x - 12.0f * s, rows_top - 6.0f * s),
                   ImVec2(pane_x + pane_w + 12.0f * s, rows_bottom), true);
  {
    const float target = rows_top + st.row * (row_h + row_gap) - st.scroll;
    if (st.row_y < 0.0f) st.row_y = target;
    st.row_y += (target - st.row_y) * ease;
  }
  if (opt_count > 0) {
    // Selection highlight (only bright while the pane is active).
    const ImVec2 mn(pane_x, st.row_y), mx(pane_x + pane_w, st.row_y + row_h);
    dl->AddRectFilled(mn, mx,
                      with_alpha(IM_COL32(112, 108, 255, 70), st.pane_mix),
                      16.0f * s);
    dl->AddRect(mn, mx, with_alpha(IM_COL32(150, 150, 255, 220), st.pane_mix),
                16.0f * s, 0, 2.0f * s);
  }
  for (int i = 0; i < opt_count; ++i) {
    const Option& o = cat.options[i];
    const float y = rows_top + i * (row_h + row_gap) - st.scroll;
    if (y + row_h < rows_top - row_h || y > rows_bottom) continue;
    const ImVec2 mn(pane_x, y), mx(pane_x + pane_w, y + row_h);
    const bool selected = i == st.row;
    // The selected row's base fill fades back in as the highlight fades out
    // (pane inactive), so it never ends up without a background.
    dl->AddRectFilled(mn, mx,
                      selected ? with_alpha(IM_COL32(255, 255, 255, 9),
                                            1.0f - st.pane_mix)
                               : IM_COL32(255, 255, 255, 9),
                      16.0f * s);
    if (ImGui::IsMouseHoveringRect(mn, mx) &&
        ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !st.editing_text) {
      st.in_options = true;
      if (st.row == i && o.kind == Kind::kAction && o.action) {
        o.action();
      } else if (st.row == i && o.kind != Kind::kRange &&
                 o.kind != Kind::kText) {
        Change(o, +1);
      }
      st.row = i;
    }
    const float f = 24.0f * s;
    dl->AddText(font, f, ImVec2(mn.x + 24.0f * s, y + (row_h - f) * 0.5f),
                with_alpha(kText, pane_alpha), Tr(o.label.c_str()));

    // Value widget on the right.
    const float vy = y + row_h * 0.5f;
    const float right_edge = mx.x - 24.0f * s;
    if (o.kind == Kind::kToggle) {
      double n = 0;
      GetNumber(Find(o.cvar), &n);
      const bool on = n != 0.0;
      const float w = 58.0f * s, h = 32.0f * s;
      const ImVec2 tmn(right_edge - w, vy - h * 0.5f),
          tmx(right_edge, vy + h * 0.5f);
      dl->AddRectFilled(
          tmn, tmx,
          on ? IM_COL32(112, 108, 255, 255) : IM_COL32(255, 255, 255, 30),
          h * 0.5f);
      const float r = h * 0.5f - 4.0f * s;
      dl->AddCircleFilled(ImVec2(on ? tmx.x - h * 0.5f : tmn.x + h * 0.5f, vy),
                          r, IM_COL32(245, 247, 255, 255), 24);
    } else if (o.kind == Kind::kRange) {
      double n = 0;
      GetNumber(Find(o.cvar), &n);
      const float t = float((n - o.min) / std::max(1e-9, o.max - o.min));
      const std::string label = ValueText(o);
      const float lf = 21.0f * s;
      const float lw = font->CalcTextSizeA(lf, FLT_MAX, 0.0f, label.c_str()).x;
      dl->AddText(font, lf, ImVec2(right_edge - lw, vy - lf * 0.5f), kText,
                  label.c_str());
      const float bar_w = 220.0f * s;
      const float bx1 = right_edge - lw - 22.0f * s;
      const float bx0 = bx1 - bar_w;
      dl->AddRectFilled(ImVec2(bx0, vy - 3.0f * s), ImVec2(bx1, vy + 3.0f * s),
                        IM_COL32(255, 255, 255, 30), 3.0f * s);
      const float fx = bx0 + bar_w * std::clamp(t, 0.0f, 1.0f);
      dl->AddRectFilled(ImVec2(bx0, vy - 3.0f * s), ImVec2(fx, vy + 3.0f * s),
                        IM_COL32(122, 120, 255, 255), 3.0f * s);
      dl->AddCircleFilled(ImVec2(fx, vy), 8.0f * s,
                          IM_COL32(245, 247, 255, 255), 20);
    } else if (o.kind == Kind::kChoice) {
      const std::string label = ValueText(o);
      const float lf = 22.0f * s;
      const float lw = font->CalcTextSizeA(lf, FLT_MAX, 0.0f, label.c_str()).x;
      const float arrow = 14.0f * s;
      const ImU32 ac = selected && st.in_options ? kText : kFaint;
      // ‹ value ›
      const float ax1 = right_edge;
      dl->AddTriangleFilled(ImVec2(ax1 - arrow * 0.6f, vy - arrow * 0.5f),
                            ImVec2(ax1, vy),
                            ImVec2(ax1 - arrow * 0.6f, vy + arrow * 0.5f), ac);
      const float tx = ax1 - arrow - 12.0f * s - lw;
      dl->AddText(font, lf, ImVec2(tx, vy - lf * 0.5f), kText, label.c_str());
      const float ax0 = tx - 12.0f * s;
      dl->AddTriangleFilled(ImVec2(ax0, vy - arrow * 0.5f),
                            ImVec2(ax0 - arrow * 0.6f, vy),
                            ImVec2(ax0, vy + arrow * 0.5f), ac);
    } else if (o.kind == Kind::kAction || o.kind == Kind::kInfo) {
      const std::string label = ValueText(o);
      if (!label.empty()) {
        const float lf = 21.0f * s;
        const float lw =
            font->CalcTextSizeA(lf, FLT_MAX, 0.0f, label.c_str()).x;
        dl->AddText(font, lf, ImVec2(right_edge - lw, vy - lf * 0.5f),
                    o.kind == Kind::kAction ? kText : kDim, label.c_str());
      }
    } else if (o.kind == Kind::kText) {
      if (st.editing_text && selected) {
        ImGui::SetCursorScreenPos(
            ImVec2(right_edge - 520.0f * s, vy - 20.0f * s));
        ImGui::SetNextItemWidth(520.0f * s);
        if (st.focus_text) {
          ImGui::SetKeyboardFocusHere();
          st.focus_text = false;
        }
        const bool done = ImGui::InputText(
            "##xenon_text", st.text_buffer, sizeof(st.text_buffer),
            ImGuiInputTextFlags_EnterReturnsTrue);
        if (done) {
          SetString(Find(o.cvar), st.text_buffer);
          Commit();
          st.editing_text = false;
        } else if (back || (!ImGui::IsItemActive() && !st.focus_text &&
                            ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
                            !ImGui::IsItemHovered())) {
          st.editing_text = false;
        }
      } else {
        const std::string label = ValueText(o);
        const float lf = 21.0f * s;
        const float lw =
            font->CalcTextSizeA(lf, FLT_MAX, 0.0f, label.c_str()).x;
        dl->AddText(font, lf, ImVec2(right_edge - lw, vy - lf * 0.5f), kDim,
                    label.c_str());
      }
    }
  }
  dl->PopClipRect();

  // Scroll indicator.
  if (content_h > view_h) {
    const float track_x = pane_x + pane_w + 8.0f * s;
    const float thumb_h = std::max(30.0f * s, view_h * view_h / content_h);
    const float thumb_y =
        rows_top + (view_h - thumb_h) * (st.scroll / (content_h - view_h));
    dl->AddRectFilled(ImVec2(track_x, thumb_y),
                      ImVec2(track_x + 4.0f * s, thumb_y + thumb_h),
                      IM_COL32(150, 150, 255, 140), 2.0f * s);
  }

  if (!desc.empty()) {
    const ImVec2 dmn(pane_x, bottom - desc_h), dmx(pane_x + pane_w, bottom);
    dl->AddRectFilled(dmn, dmx, IM_COL32(255, 255, 255, 10), 16.0f * s);
    dl->PushClipRect(dmn, dmx, true);
    dl->AddText(font, desc_font, ImVec2(dmn.x + desc_pad, dmn.y + desc_pad),
                kDim, desc.c_str(), nullptr, desc_wrap);
    dl->PopClipRect();
  }

  return st.in_options;
}

}  // namespace xenon_settings
}  // namespace app
}  // namespace xe

#endif  // XENIA_APP_XENON_SETTINGS_PAGE_H_
