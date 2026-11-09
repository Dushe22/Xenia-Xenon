/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2026 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#ifndef XENIA_GPU_SHADER_PRELOAD_H_
#define XENIA_GPU_SHADER_PRELOAD_H_

#include <atomic>
#include <cstdint>

namespace xe {
namespace gpu {

// Progress of creating the pipelines stored for the title, shown while the
// game waits for them before starting (preload_shaders).
struct ShaderPreloadProgress {
  std::atomic<uint32_t> total{0};
  std::atomic<uint32_t> done{0};
};

inline ShaderPreloadProgress& shader_preload_progress() {
  static ShaderPreloadProgress progress;
  return progress;
}

}  // namespace gpu
}  // namespace xe

#endif  // XENIA_GPU_SHADER_PRELOAD_H_
