#pragma once

#include <string>
#include <filesystem>

namespace winrt {
class fire_and_forget;
}

namespace xe {
namespace ui {
class Window;
}
}  // namespace xe

namespace UWP {
void StartXenia();
void ExecutePendingFunctionsFromUIThread();
void RegisterXeniaWindow(xe::ui::Window* window);
void UpdateImGuiIO();

void RefreshPaths();
std::vector<std::tuple<std::string, std::string>> GetGames();
std::vector<std::string> GetPaths();
void SetGamePaths(std::vector<std::string> paths);

// Internal storage + LAN web transfer.
// Root of Xenon's own data on the Dev Mode partition (falls back to
// LocalState when it is not writable).
std::string GetXenonDataFolder();
std::string GetInternalGamesFolder();

// Config backups (export / import of xenia-canary.config.toml).
std::string GetConfigBackupFolder();
// Copies the current config into the backup folder. Returns the file name,
// or an empty string on failure.
std::string ExportConfig();
// Backup file names, newest first.
std::vector<std::string> ListConfigBackups();
// Replaces the current config with a backup and reloads it.
bool ImportConfig(const std::string& backup_name);
bool DeleteConfigBackup(const std::string& backup_name);
bool StartWebTransfer();
void StopWebTransfer();
bool IsWebTransferRunning();
// Addresses a browser on the same network can open, e.g. "http://192.168.1.20:8080".
std::vector<std::string> GetWebTransferUrls();
// One-line status for the UI (current upload progress or last error).
std::string GetWebTransferStatusText();
}