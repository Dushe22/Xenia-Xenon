#pragma once

// Small LAN-only HTTP file server used to copy games and content from a PC or
// phone browser into the console's internal storage. Plain Winsock, no
// third-party dependencies, so it works the same inside the UWP sandbox and in
// a desktop test harness.

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace UWP {

struct WebTransferRoot {
  std::string name;              // Shown in the browser, e.g. "Games".
  std::filesystem::path path;    // Folder on disk this name maps to.
  std::string description;
};

struct WebTransferStatus {
  bool running = false;
  uint16_t port = 0;
  std::string last_error;
  // Current (or last) upload.
  bool upload_active = false;
  std::string upload_name;
  uint64_t upload_bytes = 0;
  uint64_t upload_total = 0;
  uint32_t files_received = 0;
  uint64_t bytes_received = 0;
};

class WebTransferServer {
 public:
  WebTransferServer() = default;
  ~WebTransferServer();

  WebTransferServer(const WebTransferServer&) = delete;
  WebTransferServer& operator=(const WebTransferServer&) = delete;

  // Called from a server thread whenever files were added, removed or renamed.
  void SetOnFilesChanged(std::function<void()> callback);

  bool Start(uint16_t port, std::vector<WebTransferRoot> roots);
  void Stop();
  bool IsRunning() const { return running_.load(); }

  WebTransferStatus GetStatus();

 private:
  void AcceptLoop();
  void HandleClient(uintptr_t client_socket);
  void SetError(const std::string& message);

  struct Request;
  bool ResolvePath(const std::string& root_name, const std::string& relative,
                   std::filesystem::path* out_path,
                   const WebTransferRoot** out_root);

  void HandleList(uintptr_t s, const Request& req);
  void HandleUpload(uintptr_t s, Request& req);
  void HandleDownload(uintptr_t s, const Request& req);
  void HandleMkdir(uintptr_t s, const Request& req);
  void HandleDelete(uintptr_t s, const Request& req);
  void HandleRename(uintptr_t s, const Request& req);
  void HandleInfo(uintptr_t s, const Request& req);
  void NotifyChanged();

  std::atomic<bool> running_{false};
  std::atomic<uintptr_t> listen_socket_{~uintptr_t(0)};
  std::thread accept_thread_;
  std::atomic<int> active_clients_{0};

  std::vector<WebTransferRoot> roots_;
  uint16_t port_ = 0;

  std::mutex status_mutex_;
  WebTransferStatus status_;

  std::mutex callback_mutex_;
  std::function<void()> on_files_changed_;
};

}  // namespace UWP
