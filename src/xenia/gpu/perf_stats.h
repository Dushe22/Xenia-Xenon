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
  // Draw phases on the GPU thread (only measured while logging).
  kDrawPrimitiveNs,     // Setup, shader analysis, primitive processing.
  kDrawRenderTargetNs,  // Render target cache update.
  kDrawPipelineNs,      // Pipeline lookup / creation.
  kDrawTexturesNs,      // Texture requests.
  kDrawStateNs,         // Fixed-function state and system constants.
  kDrawBindingsNs,      // Constant buffers, descriptors, root parameters.
  kDrawSubmitNs,        // Index buffer, memexport and the host draw.
  kDrawTotalNs,
  // Parts of kDrawBindingsNs and of the submission.
  kDrawBindConstantsNs,    // Root signature and constant buffers.
  kDrawBindDescriptorsNs,  // Shared memory, texture and sampler descriptors.
  kDrawVertexBuffersNs,    // Vertex buffers made resident in shared memory.
  // Resolves (IssueCopy), including readbacks.
  kCopyNs,
  // Parts of the readback resolves: the resolve itself, waiting for the host
  // GPU, and copying the data to guest memory.
  kReadbackResolveNs,
  kReadbackSyncNs,
  kReadbackMemcpyNs,
  kReadbackBytes,
  // Readback copies skipped because the same data was already copied this
  // frame.
  kReadbackSkippedCount,
  kReadbackSkippedBytes,
  // GPU thread waiting for the readback copy thread to finish.
  kReadbackDrainNs,
  kCounterCount,
};

// Set from the perf_log_interval cvar so the hot paths check one bool.
inline std::atomic<bool> phases_enabled{false};

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

// Splits the time of one draw into phases: each Mark adds the time since the
// previous mark to a counter; the rest goes to kDrawSubmitNs on destruction.
// Functions called by the draw can mark phases too through Current().
class DrawPhases {
 public:
  DrawPhases()
      : enabled_(phases_enabled.load(std::memory_order_relaxed)),
        start_(enabled_ ? NowNs() : 0),
        last_(start_) {
    if (enabled_) {
      current_ = this;
    }
  }
  ~DrawPhases() {
    if (enabled_) {
      uint64_t now = NowNs();
      Add(kDrawSubmitNs, now - last_);
      Add(kDrawTotalNs, now - start_);
      current_ = nullptr;
    }
  }
  void Mark(Counter counter) {
    if (enabled_) {
      uint64_t now = NowNs();
      Add(counter, now - last_);
      last_ = now;
    }
  }
  // Marks a phase of the draw in progress on this thread, if measuring.
  static void MarkCurrent(Counter counter) {
    if (current_) {
      current_->Mark(counter);
    }
  }
  DrawPhases(const DrawPhases&) = delete;
  DrawPhases& operator=(const DrawPhases&) = delete;

 private:
  static inline thread_local DrawPhases* current_ = nullptr;
  bool enabled_;
  uint64_t start_;
  uint64_t last_;
};

// How long reading the clock takes, to know how much the phase measurement
// itself costs per draw (about 10 clock reads and atomic adds per draw).
inline double MeasureTimerCostNs() {
  constexpr int kReads = 1000;
  uint64_t start = NowNs();
  uint64_t sink = 0;
  for (int i = 0; i < kReads; ++i) {
    sink += NowNs();
    Add(kSwapCount, 0);
  }
  uint64_t end = NowNs();
  (void)sink;
  return double(end - start) / kReads;
}

}  // namespace perf
}  // namespace gpu
}  // namespace xe

#endif  // XENIA_GPU_PERF_STATS_H_
