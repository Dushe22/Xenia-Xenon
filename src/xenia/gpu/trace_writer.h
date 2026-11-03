/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2020 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#ifndef XENIA_GPU_TRACE_WRITER_H_
#define XENIA_GPU_TRACE_WRITER_H_

#include <deque>
#include <filesystem>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <vector>

#include "xenia/gpu/registers.h"
#include "xenia/gpu/trace_protocol.h"

// only enable trace writer in debug builds, measured hit from the trace
// function calls (even if they just immediately return) is 0.40-0.60% cpu time
// total. with inlining they just bloat the caller and negatively impact
// register allocation for the caller
// XENIA_ENABLE_GPU_TRACE enables it in Release too.
// Xenon: enabled so the quick menu can capture a frame on the console.
#ifndef XE_OPTION_GPU_TRACE
#define XE_OPTION_GPU_TRACE 1
#endif
#if defined(NDEBUG) && !defined(XE_OPTION_GPU_TRACE)
#define XE_ENABLE_TRACE_WRITER_INSTRUMENTATION 0
#else
#define XE_ENABLE_TRACE_WRITER_INSTRUMENTATION 1
#endif

namespace xe {
namespace gpu {

class TraceWriter {
 public:
#if XE_ENABLE_TRACE_WRITER_INSTRUMENTATION == 1
  explicit TraceWriter(uint8_t* membase);
  ~TraceWriter();

  bool is_open() const { return file_ != nullptr || memory_mode_; }
  bool is_memory_mode() const { return memory_mode_; }

  bool Open(const std::filesystem::path& path, uint32_t title_id);
  // Rolling capture: frames are kept in memory instead of a file. Every frame
  // is a segment, and a keyframe segment starts with the full state (written
  // by the command processor's InitializeTrace). Only the newest keyframe
  // groups are kept.
  bool OpenMemory(uint32_t title_id);
  // Ends the current frame segment after its swap event, and starts the next
  // one, which is a keyframe if requested. Old groups are dropped to stay
  // within max_groups keyframe groups and max_bytes.
  void EndMemoryFrame(bool next_is_keyframe, uint32_t max_groups,
                      uint64_t max_bytes);
  // Thread-safe snapshot of the completed segments, oldest keyframe first.
  struct MemoryCapture {
    uint32_t title_id = 0;
    std::vector<std::shared_ptr<const std::vector<uint8_t>>> segments;
    uint64_t bytes = 0;
  };
  MemoryCapture GetMemoryCapture() const;
  // Writes a capture as a trace file. Can be called on any thread.
  static bool SaveMemoryCapture(const MemoryCapture& capture,
                                const std::filesystem::path& path);
  void Flush();
  void Close();

  void WritePrimaryBufferStart(uint32_t base_ptr, uint32_t count);
  void WritePrimaryBufferEnd();
  void WriteIndirectBufferStart(uint32_t base_ptr, uint32_t count);
  void WriteIndirectBufferEnd();
  void WritePacketStart(uint32_t base_ptr, uint32_t count);
  void WritePacketEnd();
  void WriteMemoryRead(uint32_t base_ptr, size_t length,
                       const void* host_ptr = nullptr);
  void WriteMemoryReadCached(uint32_t base_ptr, size_t length);
  void WriteMemoryReadCachedNop(uint32_t base_ptr, size_t length);
  void WriteMemoryWrite(uint32_t base_ptr, size_t length,
                        const void* host_ptr = nullptr);
  void WriteEdramSnapshot(const void* snapshot, size_t size_bytes);
  void WriteEvent(EventCommand::Type event_type);
  void WriteRegisters(uint32_t first_register, const uint32_t* register_values,
                      uint32_t register_count, bool execute_callbacks_on_play);
  void WriteGammaRamp(const reg::DC_LUT_30_COLOR* gamma_ramp_256_entry_table,
                      const reg::DC_LUT_PWL_DATA* gamma_ramp_pwl_rgb,
                      uint32_t gamma_ramp_rw_component);

 private:
  void WriteMemoryCommand(TraceCommandType type, uint32_t base_ptr,
                          size_t length, const void* host_ptr = nullptr);
  // Writes to the file, or to the current memory segment.
  void Write(const void* data, size_t size);
  bool compressing() const { return compress_output_ && file_ != nullptr; }

  std::set<uint64_t> cached_memory_reads_;
  uint8_t* membase_;
  FILE* file_;

  struct MemorySegment {
    std::shared_ptr<std::vector<uint8_t>> data;
    bool keyframe;
  };
  bool memory_mode_ = false;
  uint32_t memory_title_id_ = 0;
  // Written only by the command processor thread.
  std::shared_ptr<std::vector<uint8_t>> memory_current_;
  bool memory_current_keyframe_ = false;
  // Guarded by memory_mutex_ (read by the thread saving a capture).
  std::deque<MemorySegment> memory_completed_;
  uint64_t memory_completed_bytes_ = 0;
  mutable std::mutex memory_mutex_;

  bool compress_output_ = true;
  size_t compression_threshold_ = 1024;  // Min. number of bytes to compress.

#else
  // this could be annoying to maintain if new methods are added or the
  // signatures change
  constexpr explicit TraceWriter(uint8_t* membase) {}

  static constexpr bool is_open() { return false; }
  static constexpr bool is_memory_mode() { return false; }

  static constexpr bool Open(const std::filesystem::path& path,
                             uint32_t title_id) {
    return false;
  }
  static constexpr void Flush() {}
  static constexpr void Close() {}

  static constexpr void WritePrimaryBufferStart(uint32_t base_ptr,
                                                uint32_t count) {}
  static constexpr void WritePrimaryBufferEnd() {}
  static constexpr void WriteIndirectBufferStart(uint32_t base_ptr,
                                                 uint32_t count) {}
  static constexpr void WriteIndirectBufferEnd() {}
  static constexpr void WritePacketStart(uint32_t base_ptr, uint32_t count) {}
  static constexpr void WritePacketEnd() {}
  static constexpr void WriteMemoryRead(uint32_t base_ptr, size_t length,
                                        const void* host_ptr = nullptr) {}
  static constexpr void WriteMemoryReadCached(uint32_t base_ptr,
                                              size_t length) {}
  static constexpr void WriteMemoryReadCachedNop(uint32_t base_ptr,
                                                 size_t length) {}
  static constexpr void WriteMemoryWrite(uint32_t base_ptr, size_t length,
                                         const void* host_ptr = nullptr) {}
  static constexpr void WriteEdramSnapshot(const void* snapshot,
                                           size_t size_bytes) {}
  static constexpr void WriteEvent(EventCommand::Type event_type) {}
  static constexpr void WriteRegisters(uint32_t first_register,
                                       const uint32_t* register_values,
                                       uint32_t register_count,
                                       bool execute_callbacks_on_play) {}
  static constexpr void WriteGammaRamp(
      const reg::DC_LUT_30_COLOR* gamma_ramp_256_entry_table,
      const reg::DC_LUT_PWL_DATA* gamma_ramp_pwl_rgb,
      uint32_t gamma_ramp_rw_component) {}

#endif
};

}  // namespace gpu
}  // namespace xe

#endif  // XENIA_GPU_TRACE_WRITER_H_
