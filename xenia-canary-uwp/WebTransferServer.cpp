#include "WebTransferServer.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <map>
#include <memory>
#include <set>
#include <sstream>

#include "WebTransferPage.h"

#pragma comment(lib, "ws2_32.lib")

namespace UWP {

namespace {

constexpr size_t kMaxHeaderBytes = 32 * 1024;
constexpr size_t kIoBufferBytes = 1024 * 1024;
constexpr int kMaxClients = 16;
constexpr DWORD kSocketTimeoutMs = 120 * 1000;
constexpr wchar_t kPartialSuffix[] = L".xtpart";

std::mutex g_client_sockets_mutex;
std::set<SOCKET> g_client_sockets;

std::wstring Utf8ToWide(const std::string& s) {
  if (s.empty()) return {};
  int len = MultiByteToWideChar(CP_UTF8, 0, s.data(), int(s.size()), nullptr, 0);
  std::wstring out(len, L'\0');
  MultiByteToWideChar(CP_UTF8, 0, s.data(), int(s.size()), out.data(), len);
  return out;
}

std::string WideToUtf8(const std::wstring& s) {
  if (s.empty()) return {};
  int len = WideCharToMultiByte(CP_UTF8, 0, s.data(), int(s.size()), nullptr,
                                0, nullptr, nullptr);
  std::string out(len, '\0');
  WideCharToMultiByte(CP_UTF8, 0, s.data(), int(s.size()), out.data(), len,
                      nullptr, nullptr);
  return out;
}

std::string UrlDecode(const std::string& s) {
  std::string out;
  out.reserve(s.size());
  for (size_t i = 0; i < s.size(); ++i) {
    char c = s[i];
    if (c == '+') {
      out.push_back(' ');
    } else if (c == '%' && i + 2 < s.size() && isxdigit((unsigned char)s[i + 1]) &&
               isxdigit((unsigned char)s[i + 2])) {
      out.push_back(char(std::stoi(s.substr(i + 1, 2), nullptr, 16)));
      i += 2;
    } else {
      out.push_back(c);
    }
  }
  return out;
}

std::string JsonEscape(const std::string& s) {
  std::string out;
  out.reserve(s.size() + 8);
  for (unsigned char c : s) {
    switch (c) {
      case '"': out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default:
        if (c < 0x20) {
          char buf[8];
          snprintf(buf, sizeof(buf), "\\u%04x", c);
          out += buf;
        } else {
          out.push_back(char(c));
        }
    }
  }
  return out;
}

std::string ToLower(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(),
                 [](unsigned char c) { return char(tolower(c)); });
  return s;
}

bool SendAll(SOCKET s, const char* data, size_t size) {
  while (size > 0) {
    int chunk = int(std::min<size_t>(size, 1 << 20));
    int sent = send(s, data, chunk, 0);
    if (sent <= 0) return false;
    data += sent;
    size -= size_t(sent);
  }
  return true;
}

bool SendAll(SOCKET s, const std::string& data) {
  return SendAll(s, data.data(), data.size());
}

const char* StatusText(int code) {
  switch (code) {
    case 200: return "OK";
    case 201: return "Created";
    case 400: return "Bad Request";
    case 403: return "Forbidden";
    case 404: return "Not Found";
    case 405: return "Method Not Allowed";
    case 409: return "Conflict";
    case 411: return "Length Required";
    case 413: return "Payload Too Large";
    case 500: return "Internal Server Error";
    case 503: return "Service Unavailable";
    case 507: return "Insufficient Storage";
    default: return "Error";
  }
}

void SendResponse(SOCKET s, int code, const std::string& content_type,
                  const std::string& body) {
  std::ostringstream h;
  h << "HTTP/1.1 " << code << " " << StatusText(code) << "\r\n"
    << "Content-Type: " << content_type << "\r\n"
    << "Content-Length: " << body.size() << "\r\n"
    << "Cache-Control: no-store\r\n"
    << "Connection: close\r\n\r\n";
  SendAll(s, h.str());
  SendAll(s, body);
}

// Static files the page uses (Xenon font and logo), read from the package.
bool SendAsset(SOCKET s, const std::string& name) {
  static const std::map<std::string, std::pair<const char*, const char*>>
      kAssets = {
          {"logo.png", {"Assets/xenon-logo.png", "image/png"}},
          {"font-regular.ttf",
           {"Assets/fonts/SpaceGrotesk-Regular.ttf", "font/ttf"}},
          {"font-medium.ttf",
           {"Assets/fonts/SpaceGrotesk-Medium.ttf", "font/ttf"}},
          {"font-bold.ttf", {"Assets/fonts/SpaceGrotesk-Bold.ttf", "font/ttf"}},
      };
  auto it = kAssets.find(name);
  if (it == kAssets.end()) return false;
  std::ifstream file(it->second.first, std::ios::binary);
  if (!file) return false;
  std::ostringstream body;
  body << file.rdbuf();
  SendResponse(s, 200, it->second.second, body.str());
  return true;
}

void SendJson(SOCKET s, int code, const std::string& json) {
  SendResponse(s, code, "application/json; charset=utf-8", json);
}

void SendError(SOCKET s, int code, const std::string& message) {
  SendJson(s, code, "{\"ok\":false,\"error\":\"" + JsonEscape(message) + "\"}");
}

void SendOk(SOCKET s) { SendJson(s, 200, "{\"ok\":true}"); }

// Rejects anything that could escape the root: "..", drive letters, empty
// components, reserved characters and trailing dots/spaces (which Windows
// silently strips).
bool IsSafeComponent(const std::wstring& c) {
  if (c.empty() || c == L"." || c == L"..") return false;
  if (c.size() > 255) return false;
  for (wchar_t ch : c) {
    if (ch < 0x20) return false;
    if (wcschr(L"<>:\"/\\|?*", ch)) return false;
  }
  if (c.back() == L'.' || c.back() == L' ') return false;
  return true;
}

bool SplitRelativePath(const std::string& utf8, std::vector<std::wstring>* out) {
  out->clear();
  std::wstring w = Utf8ToWide(utf8);
  std::wstring part;
  for (wchar_t ch : w) {
    if (ch == L'/' || ch == L'\\') {
      if (!part.empty()) {
        if (!IsSafeComponent(part)) return false;
        out->push_back(part);
        part.clear();
      }
    } else {
      part.push_back(ch);
    }
  }
  if (!part.empty()) {
    if (!IsSafeComponent(part)) return false;
    out->push_back(part);
  }
  return true;
}

}  // namespace

struct WebTransferServer::Request {
  std::string method;
  std::string path;
  std::map<std::string, std::string> query;
  std::map<std::string, std::string> headers;  // Lower-case keys.
  std::string body_prefix;  // Body bytes that arrived with the header.
  SOCKET socket = INVALID_SOCKET;

  std::string Query(const std::string& key) const {
    auto it = query.find(key);
    return it == query.end() ? std::string() : it->second;
  }
  std::string Header(const std::string& key) const {
    auto it = headers.find(key);
    return it == headers.end() ? std::string() : it->second;
  }
};

WebTransferServer::~WebTransferServer() { Stop(); }

void WebTransferServer::SetOnFilesChanged(std::function<void()> callback) {
  std::lock_guard<std::mutex> lock(callback_mutex_);
  on_files_changed_ = std::move(callback);
}

void WebTransferServer::NotifyChanged() {
  std::function<void()> cb;
  {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    cb = on_files_changed_;
  }
  if (cb) cb();
}

void WebTransferServer::SetError(const std::string& message) {
  std::lock_guard<std::mutex> lock(status_mutex_);
  status_.last_error = message;
}

WebTransferStatus WebTransferServer::GetStatus() {
  std::lock_guard<std::mutex> lock(status_mutex_);
  WebTransferStatus copy = status_;
  copy.running = running_.load();
  copy.port = port_;
  return copy;
}

bool WebTransferServer::Start(uint16_t port, std::vector<WebTransferRoot> roots) {
  if (running_.load()) return true;

  roots_ = std::move(roots);
  for (const auto& root : roots_) {
    std::error_code ec;
    std::filesystem::create_directories(root.path, ec);
  }

  WSADATA wsa;
  if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
    SetError("WSAStartup failed");
    return false;
  }

  SOCKET ls = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (ls == INVALID_SOCKET) {
    SetError("socket() failed: " + std::to_string(WSAGetLastError()));
    WSACleanup();
    return false;
  }

  BOOL exclusive = TRUE;
  setsockopt(ls, SOL_SOCKET, SO_EXCLUSIVEADDRUSE,
             reinterpret_cast<const char*>(&exclusive), sizeof(exclusive));

  sockaddr_in addr = {};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  addr.sin_port = htons(port);
  if (bind(ls, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0 ||
      listen(ls, SOMAXCONN) != 0) {
    SetError("Could not listen on port " + std::to_string(port) + " (error " +
             std::to_string(WSAGetLastError()) + ")");
    closesocket(ls);
    WSACleanup();
    return false;
  }

  port_ = port;
  listen_socket_ = uintptr_t(ls);
  {
    std::lock_guard<std::mutex> lock(status_mutex_);
    status_.last_error.clear();
  }
  running_ = true;
  accept_thread_ = std::thread([this] { AcceptLoop(); });
  return true;
}

void WebTransferServer::Stop() {
  if (!running_.exchange(false)) return;

  SOCKET ls = SOCKET(listen_socket_.exchange(uintptr_t(INVALID_SOCKET)));
  if (ls != INVALID_SOCKET) closesocket(ls);
  if (accept_thread_.joinable()) accept_thread_.join();

  // Abort open connections so their threads finish quickly.
  {
    std::lock_guard<std::mutex> lock(g_client_sockets_mutex);
    for (SOCKET s : g_client_sockets) shutdown(s, SD_BOTH);
  }
  for (int i = 0; i < 500 && active_clients_.load() > 0; ++i) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  WSACleanup();
}

void WebTransferServer::AcceptLoop() {
  while (running_.load()) {
    SOCKET ls = SOCKET(listen_socket_.load());
    if (ls == INVALID_SOCKET) break;
    SOCKET client = accept(ls, nullptr, nullptr);
    if (client == INVALID_SOCKET) {
      if (!running_.load()) break;
      continue;
    }

    if (active_clients_.load() >= kMaxClients) {
      SendError(client, 503, "Too many connections");
      closesocket(client);
      continue;
    }

    DWORD timeout = kSocketTimeoutMs;
    setsockopt(client, SOL_SOCKET, SO_RCVTIMEO,
               reinterpret_cast<const char*>(&timeout), sizeof(timeout));
    setsockopt(client, SOL_SOCKET, SO_SNDTIMEO,
               reinterpret_cast<const char*>(&timeout), sizeof(timeout));
    int buf_size = 1 << 20;
    setsockopt(client, SOL_SOCKET, SO_RCVBUF,
               reinterpret_cast<const char*>(&buf_size), sizeof(buf_size));

    {
      std::lock_guard<std::mutex> lock(g_client_sockets_mutex);
      g_client_sockets.insert(client);
    }
    active_clients_++;
    std::thread([this, client] {
      HandleClient(uintptr_t(client));
      {
        std::lock_guard<std::mutex> lock(g_client_sockets_mutex);
        g_client_sockets.erase(client);
      }
      closesocket(client);
      active_clients_--;
    }).detach();
  }
}

void WebTransferServer::HandleClient(uintptr_t client_socket) {
  SOCKET s = SOCKET(client_socket);

  // Read the request header.
  std::string data;
  size_t header_end = std::string::npos;
  char buf[4096];
  while (header_end == std::string::npos) {
    int got = recv(s, buf, sizeof(buf), 0);
    if (got <= 0) return;
    data.append(buf, size_t(got));
    header_end = data.find("\r\n\r\n");
    if (header_end == std::string::npos && data.size() > kMaxHeaderBytes) {
      SendError(s, 400, "Header too large");
      return;
    }
  }

  Request req;
  req.socket = s;
  req.body_prefix = data.substr(header_end + 4);
  std::istringstream lines(data.substr(0, header_end));
  std::string line;
  std::getline(lines, line);
  if (!line.empty() && line.back() == '\r') line.pop_back();
  {
    std::istringstream first(line);
    std::string target;
    first >> req.method >> target;
    size_t q = target.find('?');
    req.path = UrlDecode(target.substr(0, q));
    if (q != std::string::npos) {
      std::istringstream qs(target.substr(q + 1));
      std::string pair;
      while (std::getline(qs, pair, '&')) {
        size_t eq = pair.find('=');
        if (eq == std::string::npos) {
          req.query[UrlDecode(pair)] = "";
        } else {
          req.query[UrlDecode(pair.substr(0, eq))] = UrlDecode(pair.substr(eq + 1));
        }
      }
    }
  }
  while (std::getline(lines, line)) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    size_t colon = line.find(':');
    if (colon == std::string::npos) continue;
    std::string value = line.substr(colon + 1);
    value.erase(0, value.find_first_not_of(" \t"));
    req.headers[ToLower(line.substr(0, colon))] = value;
  }

  const std::string& m = req.method;
  const std::string& p = req.path;
  if (p == "/" || p == "/index.html") {
    if (m != "GET") return SendError(s, 405, "Method not allowed");
    return SendResponse(s, 200, "text/html; charset=utf-8", GetWebTransferPage());
  }
  if (p.rfind("/assets/", 0) == 0 && m == "GET") {
    if (!SendAsset(s, p.substr(8))) SendError(s, 404, "Not found");
    return;
  }
  if (p == "/api/info" && m == "GET") return HandleInfo(client_socket, req);
  if (p == "/api/list" && m == "GET") return HandleList(client_socket, req);
  if (p == "/api/download" && m == "GET") return HandleDownload(client_socket, req);
  if (p == "/api/upload" && (m == "PUT" || m == "POST"))
    return HandleUpload(client_socket, req);
  if (p == "/api/mkdir" && m == "POST") return HandleMkdir(client_socket, req);
  if (p == "/api/delete" && m == "POST") return HandleDelete(client_socket, req);
  if (p == "/api/rename" && m == "POST") return HandleRename(client_socket, req);
  SendError(s, 404, "Not found");
}

bool WebTransferServer::ResolvePath(const std::string& root_name,
                                    const std::string& relative,
                                    std::filesystem::path* out_path,
                                    const WebTransferRoot** out_root) {
  const WebTransferRoot* root = nullptr;
  for (const auto& r : roots_) {
    if (r.name == root_name) root = &r;
  }
  if (!root) return false;

  std::vector<std::wstring> parts;
  if (!SplitRelativePath(relative, &parts)) return false;

  std::filesystem::path result = root->path;
  for (const auto& part : parts) result /= part;

  // Belt and braces: the normalized result must still live under the root.
  std::wstring base = root->path.lexically_normal().wstring();
  std::wstring full = result.lexically_normal().wstring();
  if (full.compare(0, base.size(), base) != 0) return false;

  *out_path = result;
  if (out_root) *out_root = root;
  return true;
}

void WebTransferServer::HandleInfo(uintptr_t sock, const Request&) {
  SOCKET s = SOCKET(sock);
  std::ostringstream j;
  j << "{\"ok\":true,\"roots\":[";
  for (size_t i = 0; i < roots_.size(); ++i) {
    std::error_code ec;
    auto space = std::filesystem::space(roots_[i].path, ec);
    if (i) j << ",";
    j << "{\"name\":\"" << JsonEscape(roots_[i].name) << "\",\"description\":\""
      << JsonEscape(roots_[i].description) << "\",\"free\":"
      << (ec ? 0 : space.available) << ",\"total\":" << (ec ? 0 : space.capacity)
      << "}";
  }
  j << "]}";
  SendJson(s, 200, j.str());
}

void WebTransferServer::HandleList(uintptr_t sock, const Request& req) {
  SOCKET s = SOCKET(sock);
  std::filesystem::path dir;
  if (!ResolvePath(req.Query("root"), req.Query("path"), &dir, nullptr)) {
    return SendError(s, 400, "Invalid path");
  }
  std::error_code ec;
  if (!std::filesystem::is_directory(dir, ec)) {
    return SendError(s, 404, "Folder not found");
  }

  struct Item {
    std::string name;
    bool is_dir;
    uint64_t size;
  };
  std::vector<Item> items;
  for (std::filesystem::directory_iterator it(dir, ec), end; !ec && it != end;
       it.increment(ec)) {
    std::error_code e2;
    bool is_dir = it->is_directory(e2);
    std::wstring name = it->path().filename().wstring();
    // Hide unfinished uploads.
    if (!is_dir && name.size() > wcslen(kPartialSuffix) &&
        name.compare(name.size() - wcslen(kPartialSuffix), std::wstring::npos,
                     kPartialSuffix) == 0) {
      continue;
    }
    uint64_t size = is_dir ? 0 : it->file_size(e2);
    items.push_back({WideToUtf8(name), is_dir, e2 ? 0 : size});
  }
  std::sort(items.begin(), items.end(), [](const Item& a, const Item& b) {
    if (a.is_dir != b.is_dir) return a.is_dir;
    return ToLower(a.name) < ToLower(b.name);
  });

  std::ostringstream j;
  j << "{\"ok\":true,\"entries\":[";
  for (size_t i = 0; i < items.size(); ++i) {
    if (i) j << ",";
    j << "{\"name\":\"" << JsonEscape(items[i].name) << "\",\"dir\":"
      << (items[i].is_dir ? "true" : "false") << ",\"size\":" << items[i].size
      << "}";
  }
  j << "]}";
  SendJson(s, 200, j.str());
}

void WebTransferServer::HandleUpload(uintptr_t sock, Request& req) {
  SOCKET s = SOCKET(sock);
  std::filesystem::path target;
  if (req.Query("path").empty() ||
      !ResolvePath(req.Query("root"), req.Query("path"), &target, nullptr)) {
    return SendError(s, 400, "Invalid path");
  }
  if (!req.Header("transfer-encoding").empty()) {
    return SendError(s, 411, "Chunked uploads are not supported");
  }
  std::string length_str = req.Header("content-length");
  if (length_str.empty()) return SendError(s, 411, "Content-Length required");
  uint64_t total = 0;
  try {
    total = std::stoull(length_str);
  } catch (...) {
    return SendError(s, 400, "Bad Content-Length");
  }

  std::error_code ec;
  if (std::filesystem::is_directory(target, ec)) {
    return SendError(s, 409, "A folder with that name already exists");
  }
  std::filesystem::create_directories(target.parent_path(), ec);
  auto space = std::filesystem::space(target.parent_path(), ec);
  if (!ec && space.available < total) {
    return SendError(s, 507, "Not enough free space on the console");
  }

  if (ToLower(req.Header("expect")) == "100-continue") {
    SendAll(s, std::string("HTTP/1.1 100 Continue\r\n\r\n"));
  }

  std::filesystem::path partial = target;
  partial += kPartialSuffix;
  FILE* f = _wfopen(partial.c_str(), L"wb");
  if (!f) return SendError(s, 500, "Could not create file on the console");

  const std::string display_name = WideToUtf8(target.filename().wstring());
  {
    std::lock_guard<std::mutex> lock(status_mutex_);
    status_.upload_active = true;
    status_.upload_name = display_name;
    status_.upload_bytes = 0;
    status_.upload_total = total;
  }

  uint64_t written = 0;
  bool ok = true;
  std::string error;
  if (!req.body_prefix.empty()) {
    size_t n = size_t(std::min<uint64_t>(req.body_prefix.size(), total));
    if (fwrite(req.body_prefix.data(), 1, n, f) != n) {
      ok = false;
      error = "Write failed";
    }
    written += n;
  }

  std::unique_ptr<char[]> buffer(new char[kIoBufferBytes]);
  while (ok && written < total) {
    int want = int(std::min<uint64_t>(kIoBufferBytes, total - written));
    int got = recv(s, buffer.get(), want, 0);
    if (got <= 0) {
      ok = false;
      error = "Connection lost";
      break;
    }
    if (fwrite(buffer.get(), 1, size_t(got), f) != size_t(got)) {
      ok = false;
      error = "Write failed (disk full?)";
      break;
    }
    written += uint64_t(got);
    std::lock_guard<std::mutex> lock(status_mutex_);
    status_.upload_bytes = written;
  }
  if (fclose(f) != 0 && ok) {
    ok = false;
    error = "Write failed";
  }

  if (ok) {
    std::filesystem::remove(target, ec);
    std::filesystem::rename(partial, target, ec);
    if (ec) {
      ok = false;
      error = "Could not finalize file: " + ec.message();
    }
  }
  if (!ok) std::filesystem::remove(partial, ec);

  {
    std::lock_guard<std::mutex> lock(status_mutex_);
    status_.upload_active = false;
    if (ok) {
      status_.files_received++;
      status_.bytes_received += written;
    } else {
      status_.last_error = display_name + ": " + error;
    }
  }

  if (ok) {
    SendJson(s, 201, "{\"ok\":true}");
    NotifyChanged();
  } else {
    SendError(s, 500, error);
  }
}

void WebTransferServer::HandleDownload(uintptr_t sock, const Request& req) {
  SOCKET s = SOCKET(sock);
  std::filesystem::path file;
  if (!ResolvePath(req.Query("root"), req.Query("path"), &file, nullptr)) {
    return SendError(s, 400, "Invalid path");
  }
  std::error_code ec;
  if (!std::filesystem::is_regular_file(file, ec)) {
    return SendError(s, 404, "File not found");
  }
  uint64_t size = std::filesystem::file_size(file, ec);
  FILE* f = _wfopen(file.c_str(), L"rb");
  if (!f) return SendError(s, 500, "Could not open file");

  std::string name = WideToUtf8(file.filename().wstring());
  std::string ascii_name;
  for (char c : name) ascii_name.push_back((c & 0x80) || c == '"' ? '_' : c);

  std::ostringstream h;
  h << "HTTP/1.1 200 OK\r\nContent-Type: application/octet-stream\r\n"
    << "Content-Length: " << size << "\r\n"
    << "Content-Disposition: attachment; filename=\"" << ascii_name << "\"\r\n"
    << "Connection: close\r\n\r\n";
  if (SendAll(s, h.str())) {
    std::unique_ptr<char[]> buffer(new char[kIoBufferBytes]);
    size_t n;
    while ((n = fread(buffer.get(), 1, kIoBufferBytes, f)) > 0) {
      if (!SendAll(s, buffer.get(), n)) break;
    }
  }
  fclose(f);
}

void WebTransferServer::HandleMkdir(uintptr_t sock, const Request& req) {
  SOCKET s = SOCKET(sock);
  std::filesystem::path dir;
  if (req.Query("path").empty() ||
      !ResolvePath(req.Query("root"), req.Query("path"), &dir, nullptr)) {
    return SendError(s, 400, "Invalid folder name");
  }
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  if (ec) return SendError(s, 500, ec.message());
  SendOk(s);
  NotifyChanged();
}

void WebTransferServer::HandleDelete(uintptr_t sock, const Request& req) {
  SOCKET s = SOCKET(sock);
  std::filesystem::path target;
  // An empty path would mean the root itself; never allow that.
  if (req.Query("path").empty() ||
      !ResolvePath(req.Query("root"), req.Query("path"), &target, nullptr)) {
    return SendError(s, 400, "Invalid path");
  }
  std::error_code ec;
  if (!std::filesystem::exists(target, ec)) {
    return SendError(s, 404, "Not found");
  }
  std::filesystem::remove_all(target, ec);
  if (ec) return SendError(s, 500, ec.message());
  SendOk(s);
  NotifyChanged();
}

void WebTransferServer::HandleRename(uintptr_t sock, const Request& req) {
  SOCKET s = SOCKET(sock);
  std::filesystem::path from;
  if (req.Query("path").empty() ||
      !ResolvePath(req.Query("root"), req.Query("path"), &from, nullptr)) {
    return SendError(s, 400, "Invalid path");
  }
  std::wstring new_name = Utf8ToWide(req.Query("to"));
  if (!IsSafeComponent(new_name)) return SendError(s, 400, "Invalid new name");
  std::filesystem::path to = from.parent_path() / new_name;
  std::error_code ec;
  if (std::filesystem::exists(to, ec)) {
    return SendError(s, 409, "Something with that name already exists");
  }
  std::filesystem::rename(from, to, ec);
  if (ec) return SendError(s, 500, ec.message());
  SendOk(s);
  NotifyChanged();
}

}  // namespace UWP
