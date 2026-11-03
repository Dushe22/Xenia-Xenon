#include "XeniaUWP.h"

#include "UWPUtil.h"
#include "WinRTKeyboard.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <ctime>
#include <fstream>

#include "WebTransferServer.h"
#include "windowed_app_context_uwp.h"

#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Networking.h>
#include <winrt/Windows.Networking.Connectivity.h>
#include "surface_uwp.h"
#include "window_uwp.h"

#include "third_party/imgui/imgui.h"

#include "xenia/emulator.h"
#include "xenia/base/filesystem.h"
#include "xenia/ui/windowed_app.h"
#include "xenia/base/cvar.h"
#include "xenia/base/clock.h"
#include "xenia/base/logging.h"
#include "xenia/ui/window.h"
#include "xenia/ui/d3d12/d3d12_provider.h"
#include "xenia/gpu/d3d12/d3d12_graphics_system.h"
#include "xenia/hid/xinput/xinput_hid.h"
#include "xenia/hid/nop/nop_hid.h"
#include "xenia/apu/xaudio2/xaudio2_audio_system.h"
#include "xenia/config.h"
#include "xenia/base/main_win.h"
#include "xenia/vfs/devices/disc_zarchive_device.h"

using namespace xe;
using namespace xe::hid;

DECLARE_string(gamepaths);
DEFINE_string(gamepaths, "", "Paths the frontend will search for games.",
              "General");
DEFINE_bool(web_transfer_enabled, false,
            "Start the LAN web file-transfer server when the app opens.",
            "General");
DEFINE_int32(web_transfer_port, 8080, "Port for the LAN web file-transfer server.",
             "General");

static std::unique_ptr<ui::WindowedApp> app = nullptr;
static std::unique_ptr<ui::UWPWindowedAppContext> app_context = nullptr;
static ui::Window* s_window;
static Emulator* s_emulator;
static std::vector<std::string> s_paths;
static std::vector<std::tuple<std::string, std::string>> s_games;
static std::vector<std::string> s_scanned_paths;
static UWP::WebTransferServer s_web_transfer;
static std::atomic<bool> s_refresh_pending{false};

namespace {
constexpr uint64_t kAnalogNavInitialDelayMs = 275;
constexpr uint64_t kAnalogNavRepeatIntervalMs = 115;

std::string NormalizeScannedPath(const std::filesystem::path& path) {
  std::error_code ec;
  std::filesystem::path normalized = std::filesystem::weakly_canonical(path, ec);
  if (ec) {
    normalized = path.lexically_normal();
  }

  std::string normalized_string = xe::path_to_utf8(normalized);
  std::replace(normalized_string.begin(), normalized_string.end(), '/', '\\');
  std::transform(normalized_string.begin(), normalized_string.end(),
                 normalized_string.begin(), [](unsigned char c) {
                   return static_cast<char>(std::tolower(c));
                 });
  return normalized_string;
}

bool HasScannedDirectory(const std::string& normalized_path) {
  return std::find(s_scanned_paths.cbegin(), s_scanned_paths.cend(),
                   normalized_path) != s_scanned_paths.cend();
}

bool AddGameEntry(const std::filesystem::path& path, const std::string& name) {
  const std::string normalized_path = NormalizeScannedPath(path);
  auto existing = std::find_if(
      s_games.cbegin(), s_games.cend(), [&](const auto& game) {
        return NormalizeScannedPath(std::get<0>(game)) == normalized_path;
      });
  if (existing != s_games.cend()) {
    return false;
  }

  s_games.push_back({path.string(), name});
  return true;
}

enum class AnalogNavDirection { kLeft = 0, kRight, kUp, kDown };

struct AnalogNavRepeatState {
  bool active = false;
  bool repeating = false;
  uint64_t start_time_ms = 0;
  uint64_t last_emit_time_ms = 0;
};

std::array<AnalogNavRepeatState, 4> g_analog_nav_repeat_states;

bool UpdateAnalogNavRepeatState(AnalogNavDirection direction,
                                bool analog_active,
                                uint64_t now_ms) {
  auto& state = g_analog_nav_repeat_states[static_cast<size_t>(direction)];

  if (!analog_active) {
    state = {};
    return false;
  }

  if (!state.active) {
    state.active = true;
    state.start_time_ms = now_ms;
    state.last_emit_time_ms = now_ms;
    return true;
  }

  if (!state.repeating) {
    if (now_ms - state.start_time_ms >= kAnalogNavInitialDelayMs) {
      state.repeating = true;
      state.last_emit_time_ms = now_ms;
      return true;
    }
    return false;
  }

  if (now_ms - state.last_emit_time_ms >= kAnalogNavRepeatIntervalMs) {
    state.last_emit_time_ms = now_ms;
    return true;
  }

  return false;
}

void ResetAnalogNavRepeatStates() {
  for (auto& state : g_analog_nav_repeat_states) {
    state = {};
  }
}

}  // namespace

void UWP::StartXenia() {
  app_context = std::make_unique<ui::UWPWindowedAppContext>();
  app = xe::ui::GetWindowedAppCreator()(*app_context.get());

  xe::InitializeWin32App(app->GetName());

  if (app->OnInitialize()) {
    std::error_code ec;
    std::filesystem::create_directories(GetInternalGamesFolder(), ec);
    RefreshPaths();
    if (cvars::web_transfer_enabled) {
      StartWebTransfer();
    }
    // to-do, remodel this so it doesn't instantly shutdown.
    //app->InvokeOnDestroy();
  }

  //xe::ShutdownWin32App();
}

void UWP::ExecutePendingFunctionsFromUIThread() {
  app_context->ExecutePendingFunctionsFromUIThread();

  // Uploads finish on server threads; rescan the game list on this thread.
  if (s_refresh_pending.exchange(false)) {
    RefreshPaths();
  }

  if (s_window) {
    app_context->CallInUIThread([=]() { s_window->RequestPaint(); });
  }
}

void UWP::RegisterXeniaWindow(xe::ui::Window* window) { s_window = window; }

void UWP::UpdateImGuiIO() {
  ImGuiIO& io = ImGui::GetIO();
  io.AddKeyEvent(ImGuiKey_Backspace, false);
  io.AddKeyEvent(ImGuiKey_Enter, false);

  {
    std::unique_lock lk(UWP::g_buffer_mutex);
    for (uint32_t c : UWP::g_char_buffer) {
      io.AddInputCharacter(c);

      if (c == '\b') {
        io.AddKeyEvent(ImGuiKey_Backspace, true);
      } else if (c == '\r') {
        io.AddKeyEvent(ImGuiKey_Enter, true);
      }
    }
    UWP::g_char_buffer.clear();
  }

  auto driver = static_cast<xe::ui::UWPWindow*>(s_window)->xinputdriver();
  if (!driver) {
    ResetAnalogNavRepeatStates();
    return;
  }

  hid::X_INPUT_STATE state;
  if (driver->GetState(0, &state) != X_STATUS_SUCCESS) {
    ResetAnalogNavRepeatStates();
    return;
  }

  // Debounce: a button counts as released only after it stays up for a
  // moment. Single misread polls made held buttons look pressed again, so
  // menus repeated actions (one A press ran Capture nine times).
  constexpr uint64_t kReleaseDebounceMs = 60;
  static uint64_t s_button_last_down_ms[16] = {};
  const uint64_t poll_ms = Clock::QueryHostUptimeMillis();
  uint16_t buttons = 0;
  for (int bit = 0; bit < 16; ++bit) {
    if (state.gamepad.buttons & (1u << bit)) {
      s_button_last_down_ms[bit] = poll_ms;
    }
    if (s_button_last_down_ms[bit] &&
        poll_ms - s_button_last_down_ms[bit] < kReleaseDebounceMs) {
      buttons |= uint16_t(1u << bit);
    }
  }

  io.AddKeyEvent(ImGuiKey_GamepadFaceDown,   (buttons & X_INPUT_GAMEPAD_A) != 0);
  io.AddKeyEvent(ImGuiKey_GamepadFaceRight,  (buttons & X_INPUT_GAMEPAD_B) != 0);
  io.AddKeyEvent(ImGuiKey_GamepadFaceLeft,   false);
  io.AddKeyEvent(ImGuiKey_F12,               (buttons & X_INPUT_GAMEPAD_X) != 0);
  io.AddKeyEvent(ImGuiKey_GamepadFaceUp,     (buttons & X_INPUT_GAMEPAD_Y) != 0);
  io.AddKeyEvent(ImGuiKey_GamepadStart,      (buttons & X_INPUT_GAMEPAD_START) != 0);

  io.AddKeyEvent(ImGuiKey_GamepadBack,       (buttons & X_INPUT_GAMEPAD_BACK) != 0);
  io.AddKeyEvent(ImGuiKey_GamepadL1,         (buttons & X_INPUT_GAMEPAD_LEFT_SHOULDER) != 0);
  io.AddKeyEvent(ImGuiKey_GamepadR1,         (buttons & X_INPUT_GAMEPAD_RIGHT_SHOULDER) != 0);
  io.AddKeyEvent(ImGuiKey_GamepadL3,         (buttons & X_INPUT_GAMEPAD_LEFT_THUMB) != 0);
  io.AddKeyEvent(ImGuiKey_GamepadR3,         (buttons & X_INPUT_GAMEPAD_RIGHT_THUMB) != 0);

  const int16_t kStickNavDeadzone = X_INPUT_GAMEPAD_LEFT_THUMB_DEADZONE;
  const bool ls_left  = state.gamepad.thumb_lx <= -kStickNavDeadzone;
  const bool ls_right = state.gamepad.thumb_lx >=  kStickNavDeadzone;
  const bool ls_up    = state.gamepad.thumb_ly >=  kStickNavDeadzone;
  const bool ls_down  = state.gamepad.thumb_ly <= -kStickNavDeadzone;
  const uint64_t now_ms = Clock::QueryHostUptimeMillis();

  const bool analog_nav_left =
      UpdateAnalogNavRepeatState(AnalogNavDirection::kLeft, ls_left, now_ms);
  const bool analog_nav_right =
      UpdateAnalogNavRepeatState(AnalogNavDirection::kRight, ls_right, now_ms);
  const bool analog_nav_up =
      UpdateAnalogNavRepeatState(AnalogNavDirection::kUp, ls_up, now_ms);
  const bool analog_nav_down =
      UpdateAnalogNavRepeatState(AnalogNavDirection::kDown, ls_down, now_ms);

  io.AddKeyEvent(ImGuiKey_GamepadDpadLeft,
                 ((buttons & X_INPUT_GAMEPAD_DPAD_LEFT) != 0) ||
                     analog_nav_left);
  io.AddKeyEvent(ImGuiKey_GamepadDpadRight,
                 ((buttons & X_INPUT_GAMEPAD_DPAD_RIGHT) != 0) ||
                     analog_nav_right);
  io.AddKeyEvent(ImGuiKey_GamepadDpadUp,
                 ((buttons & X_INPUT_GAMEPAD_DPAD_UP) != 0) || analog_nav_up);
  io.AddKeyEvent(ImGuiKey_GamepadDpadDown,
                 ((buttons & X_INPUT_GAMEPAD_DPAD_DOWN) != 0) ||
                     analog_nav_down);

  // Right stick still exposed for camera/mouse emulation
  constexpr float kStickDeadzone = 8000.0f / 32767.0f;
  const float rx = state.gamepad.thumb_rx / 32767.0f;
  const float ry = state.gamepad.thumb_ry / 32767.0f;

  io.AddKeyAnalogEvent(ImGuiKey_GamepadRStickLeft,  rx < -kStickDeadzone, rx < 0 ? -rx : 0.0f);
  io.AddKeyAnalogEvent(ImGuiKey_GamepadRStickRight, rx >  kStickDeadzone, rx > 0 ?  rx : 0.0f);
  io.AddKeyAnalogEvent(ImGuiKey_GamepadRStickUp,    ry >  kStickDeadzone, ry > 0 ?  ry : 0.0f);
  io.AddKeyAnalogEvent(ImGuiKey_GamepadRStickDown,  ry < -kStickDeadzone, ry < 0 ? -ry : 0.0f);

  // Triggers as L2/R2
  constexpr float kTriggerDeadzone = 30.0f / 255.0f;
  const float lt = state.gamepad.left_trigger / 255.0f;
  const float rt = state.gamepad.right_trigger / 255.0f;
  io.AddKeyAnalogEvent(ImGuiKey_GamepadL2, lt > kTriggerDeadzone, lt);
  io.AddKeyAnalogEvent(ImGuiKey_GamepadR2, rt > kTriggerDeadzone, rt);
}

// STFS packages also carry DLC, themes, avatar items and other content.
// Only list packages whose content type is something that can be launched.
static bool IsLaunchableStfsPackage(const std::filesystem::path& path) {
  std::ifstream in(path, std::ios::binary);
  uint8_t raw[4] = {};
  in.seekg(0x344);
  if (!in.read(reinterpret_cast<char*>(raw), sizeof(raw))) {
    return false;
  }
  const uint32_t content_type = (uint32_t(raw[0]) << 24) |
                                (uint32_t(raw[1]) << 16) |
                                (uint32_t(raw[2]) << 8) | uint32_t(raw[3]);
  switch (content_type) {
    case 0x00007000:  // Xbox 360 title (Games on Demand)
    case 0x000D0000:  // Arcade title
    case 0x00005000:  // Original Xbox title
    case 0x00080000:  // Game demo
    case 0x02000000:  // Community game
      return true;
    default:
      return false;
  }
}

void RecurseFolderForGames(std::string path) {
  try {
    const std::string normalized_directory = NormalizeScannedPath(path);

    if (HasScannedDirectory(normalized_directory)) {
      return;
    }
    s_scanned_paths.push_back(normalized_directory);

    std::filesystem::path loose_xex_path;
    std::string loose_xex_name;
    bool has_loose_xex = false;

    for (auto file : std::filesystem::directory_iterator(path)) {
      if (file.is_directory() && file.path().string() != path) {
        RecurseFolderForGames(file.path().string());
        continue;
      }

      if (!file.is_regular_file()) continue;

      const auto signature = xe::GetFileSignature(file.path());
      switch (signature) {
        case xe::Emulator::FileSignatureType::XEX1:
        case xe::Emulator::FileSignatureType::XEX2: {
          const bool is_default_xex =
              _stricmp(file.path().filename().string().c_str(),
                       "default.xex") == 0;
          if (!is_default_xex) {
            break;
          }

          loose_xex_path = file.path();
          if (file.path().has_parent_path()) {
            loose_xex_name = file.path().parent_path().filename().string();
          } else {
            loose_xex_name = file.path().stem().string();
          }
          has_loose_xex = true;
          break;
        }
        case xe::Emulator::FileSignatureType::CON:
        case xe::Emulator::FileSignatureType::PIRS:
        case xe::Emulator::FileSignatureType::ZAR: {
          if (signature != xe::Emulator::FileSignatureType::ZAR &&
              !IsLaunchableStfsPackage(file.path())) {
            break;
          }
          std::string filename = file.path().stem().string();

          AddGameEntry(file.path(), filename);
          break;
        }

        case xe::Emulator::FileSignatureType::LIVE: {
          if (!IsLaunchableStfsPackage(file.path())) {
            break;
          }
          std::ifstream in(file.path().string(), std::ios::binary);

          in.seekg(0x412);

          char data[32];
          for (int i = 0; i < 32; i++) {
            char c;
            in.read(&c, 2);
            std::wctomb(&data[i], static_cast<wchar_t>(c));

            if (c == 0) break;
          }

          AddGameEntry(file.path(), data);

          in.close();
          break;
        }
        default:
          continue;
      }
    }

    if (has_loose_xex) {
      AddGameEntry(loose_xex_path, loose_xex_name);
    }
  } catch (std::exception) {
    // This folder can't be opened.
  }
}

void UWP::RefreshPaths() {
  s_paths.clear();
  s_games.clear();
  s_scanned_paths.clear();

  RecurseFolderForGames(UWP::GetLocalCache());
  RecurseFolderForGames(UWP::GetInternalGamesFolder());

  if (!cvars::gamepaths.empty()) {
    std::stringstream ss (cvars::gamepaths);
    std::string item;
    while (std::getline(ss, item, ';')) {
      if (item.empty()) continue;

      RecurseFolderForGames(item);
      s_paths.push_back(item);
    }
  }

  std::sort(s_games.begin(), s_games.end(), [](auto& first, auto& second) {
    return std::get<1>(first) < std::get<1>(second);
  });
}

std::vector<std::tuple<std::string, std::string>> UWP::GetGames() {
  return s_games;
}

void UWP::SetGamePaths(std::vector<std::string> paths) {
  s_paths.clear();
  s_paths.insert(s_paths.end(), paths.begin(), paths.end());
  std::stringstream ss;
  for (auto p : s_paths) {
    ss << p << ";";
  }

  auto cpaths = dynamic_cast<cvar::ConfigVar<std::string>*>(
      cvar::ConfigVars->find("gamepaths")->second);
    cpaths->SetConfigValue(ss.str());
  config::SaveConfig();
  RefreshPaths();
}

std::vector<std::string> UWP::GetPaths() { 
  return s_paths;
}


// Prefer the large Dev Mode partition (D:\DevelopmentFiles, ~180 GB on
// Series consoles). The app's own LocalState lives on a small (~32 GB)
// partition. Fall back to LocalState when the dev partition isn't writable
// (e.g. retail mode).
std::string UWP::GetXenonDataFolder() {
  static const std::string folder = [] {
    const std::string dev_folder = "D:\\DevelopmentFiles\\XeniaXenon";
    std::error_code ec;
    std::filesystem::create_directories(dev_folder, ec);
    if (!ec && std::filesystem::is_directory(dev_folder, ec) &&
        UWP::TestPathPermissions(dev_folder)) {
      return dev_folder;
    }
    return UWP::GetLocalState();
  }();
  return folder;
}

std::string UWP::GetInternalGamesFolder() {
  static const std::string folder = [] {
    const std::string games = UWP::GetXenonDataFolder() + "\\Games";
    std::error_code ec;
    std::filesystem::create_directories(games, ec);
    return games;
  }();
  return folder;
}

std::string UWP::GetConfigBackupFolder() {
  const std::string backups = UWP::GetXenonDataFolder() + "\\Config backups";
  std::error_code ec;
  std::filesystem::create_directories(backups, ec);
  return backups;
}

static std::filesystem::path CurrentConfigPath() {
  return std::filesystem::path(xe::to_path(UWP::GetLocalState())) /
         "xenia-canary.config.toml";
}

static bool IsSafeBackupName(const std::string& name) {
  return !name.empty() && name.find_first_of("\\/:") == std::string::npos &&
         name != "." && name != "..";
}

std::string UWP::ExportConfig() {
  config::SaveConfig();
  std::time_t now = std::time(nullptr);
  std::tm local_tm{};
  localtime_s(&local_tm, &now);
  char name[64];
  std::strftime(name, sizeof(name), "xenon-config_%Y-%m-%d_%H-%M-%S.toml",
                &local_tm);
  const auto target =
      std::filesystem::path(xe::to_path(GetConfigBackupFolder())) / name;
  std::error_code ec;
  std::filesystem::copy_file(CurrentConfigPath(), target,
                             std::filesystem::copy_options::overwrite_existing,
                             ec);
  return ec ? std::string() : std::string(name);
}

std::vector<std::string> UWP::ListConfigBackups() {
  std::vector<std::pair<std::filesystem::file_time_type, std::string>> found;
  std::error_code ec;
  for (const auto& entry : std::filesystem::directory_iterator(
           xe::to_path(GetConfigBackupFolder()), ec)) {
    if (!entry.is_regular_file(ec)) continue;
    const auto ext = entry.path().extension().string();
    if (ext != ".toml") continue;
    found.emplace_back(entry.last_write_time(ec),
                       xe::path_to_utf8(entry.path().filename()));
  }
  std::sort(found.begin(), found.end(),
            [](const auto& a, const auto& b) { return a.first > b.first; });
  std::vector<std::string> names;
  for (auto& f : found) names.push_back(std::move(f.second));
  return names;
}

bool UWP::ImportConfig(const std::string& backup_name) {
  if (!IsSafeBackupName(backup_name)) return false;
  const auto source =
      std::filesystem::path(xe::to_path(GetConfigBackupFolder())) /
      xe::to_path(backup_name);
  std::error_code ec;
  if (!std::filesystem::is_regular_file(source, ec)) return false;
  std::filesystem::copy_file(source, CurrentConfigPath(),
                             std::filesystem::copy_options::overwrite_existing,
                             ec);
  if (ec) return false;
  // Re-read the config so most settings apply right away.
  config::SetupConfig(xe::to_path(UWP::GetLocalState()));
  return true;
}

bool UWP::DeleteConfigBackup(const std::string& backup_name) {
  if (!IsSafeBackupName(backup_name)) return false;
  std::error_code ec;
  return std::filesystem::remove(
      std::filesystem::path(xe::to_path(GetConfigBackupFolder())) /
          xe::to_path(backup_name),
      ec);
}

static void SaveWebTransferEnabled(bool enabled) {
  auto it = cvar::ConfigVars->find("web_transfer_enabled");
  if (it == cvar::ConfigVars->end()) return;
  auto cv = dynamic_cast<cvar::ConfigVar<bool>*>(it->second);
  if (!cv) return;
  cv->SetConfigValue(enabled);
  config::SaveConfig();
}

bool UWP::StartWebTransfer() {
  if (s_web_transfer.IsRunning()) return true;
  s_web_transfer.SetOnFilesChanged([] { s_refresh_pending = true; });
  std::vector<WebTransferRoot> roots = {
      {"Games", std::filesystem::path(xe::to_path(GetInternalGamesFolder())),
       "Games on the console's internal storage. Upload ISO, ZAR, GOD/CON "
       "files or whole extracted game folders (with default.xex). They appear "
       "in the game list as soon as the upload finishes."},
      {"Config backups",
       std::filesystem::path(xe::to_path(GetConfigBackupFolder())),
       "Xenon config backups. Export from Storage > Data on the console, "
       "or upload a .toml here and import it from the console."},
      {"Xenia data", std::filesystem::path(xe::to_path(GetLocalState())),
       "Xenia's own data: config, patches, content (saves, DLC, title "
       "updates). Only change files here if you know what they are."},
  };
  uint16_t port = uint16_t(std::clamp(cvars::web_transfer_port, 1, 65535));
  bool ok = s_web_transfer.Start(port, std::move(roots));
  if (ok) {
    XELOGI("Web transfer server listening on port {}", port);
    SaveWebTransferEnabled(true);
  } else {
    XELOGE("Web transfer server failed: {}",
           s_web_transfer.GetStatus().last_error);
  }
  return ok;
}

void UWP::StopWebTransfer() {
  s_web_transfer.Stop();
  SaveWebTransferEnabled(false);
}

bool UWP::IsWebTransferRunning() { return s_web_transfer.IsRunning(); }

std::vector<std::string> UWP::GetWebTransferUrls() {
  std::vector<std::string> urls;
  const uint16_t port = s_web_transfer.GetStatus().port;
  try {
    using namespace winrt::Windows::Networking;
    for (const auto& host :
         Connectivity::NetworkInformation::GetHostNames()) {
      if (host.Type() != HostNameType::Ipv4) continue;
      std::string ip = winrt::to_string(host.CanonicalName());
      if (ip.rfind("127.", 0) == 0 || ip.rfind("169.254.", 0) == 0) continue;
      urls.push_back("http://" + ip + ":" + std::to_string(port));
    }
  } catch (...) {
  }
  return urls;
}

std::string UWP::GetWebTransferStatusText() {
  auto st = s_web_transfer.GetStatus();
  if (!st.running) {
    return st.last_error.empty() ? "Stopped" : "Error: " + st.last_error;
  }
  if (st.upload_active) {
    double pct = st.upload_total
                     ? 100.0 * double(st.upload_bytes) / double(st.upload_total)
                     : 0.0;
    return fmt::format("Receiving {} ({:.1f}% of {:.2f} GB)", st.upload_name,
                       pct, double(st.upload_total) / (1024.0 * 1024 * 1024));
  }
  std::string text = fmt::format("Ready. {} file(s) received this session.",
                                 st.files_received);
  if (!st.last_error.empty()) text += " Last error: " + st.last_error;
  return text;
}
