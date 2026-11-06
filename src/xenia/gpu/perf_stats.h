/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2026 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#ifndef XENIA_GPU_PERF_STATS_H_
#define XENIA_GPU_PERF_STATS_H_

#include <atomic>
#include <chrono>
#include <cstdint>

// Lightweight counters for finding where emulation time goes. Enabled by the
// perf_log_interval cvar; when it's 0 the timers still run but cost only a
// clock read, and nothing is logged.
namespace xe {
namespace gpu {
namespace perf {

enum Counter : uint32_t {
  // GPU thread waiting for the guest to submit more commands.
  kGpuIdleNs,
  // GPU thread inside WAIT_REG_MEM (waiting for a guest CPU value).
  kWaitRegMemNs,
  // GPU thread waiting for the host GPU to finish submissions.
  kHostGpuWaitNs,
  kHostGpuWaitCount,
  // Pipeline (shader) creation, on any thread.
  kPipelineCreateNs,
  kPipelineCreateCount,
  // Texture loads (untiling / conversion and upload).
  kTextureLoadNs,
  kTextureLoadCount,
  // Shared memory uploads from guest memory.
  kUploadBytes,
  // Resolves read back to guest memory.
  kReadbackCount,
  kDrawCount,
  kSwapCount,
  kCounterCount,
};

inline std::atomic<uint64_t> counters[kCounterCount];

inline uint64_t NowNs() {
  return uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(
                      std::chrono::steady_clock::now().time_since_epoch())
                      .count());
}

inline void Add(Counter counter, uint64_t value) {
  counters[counter].fetch_add(value, std::memory_order_relaxed);
}

// Adds the time from construction to destruction to a counter, and optionally
// increments a count.
class ScopedTimer {
 public:
  explicit ScopedTimer(Counter ns_counter,
                       Counter count_counter = kCounterCount)
      : ns_counter_(ns_counter),
        count_counter_(count_counter),
        start_(NowNs()) {}
  ~ScopedTimer() {
    Add(ns_counter_, NowNs() - start_);
    if (count_counter_ != kCounterCount) {
      Add(count_counter_, 1);
    }
  }
  ScopedTimer(const ScopedTimer&) = delete;
  ScopedTimer& operator=(const ScopedTimer&) = delete;

 private:
  Counter ns_counter_;
  Counter count_counter_;
  uint64_t start_;
};

}  // namespace perf
}  // namespace gpu
}  // namespace xe

#endif  // XENIA_GPU_PERF_STATS_H_
