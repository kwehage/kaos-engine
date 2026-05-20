#pragma once

namespace kaos_engine {

// FFT and display constants shared between the plugin and editor.
struct SpectrogramDefs {
    static constexpr int kFftOrder     = 11;
    static constexpr int kFftSize      = 1 << kFftOrder;   // 2048
    static constexpr int kFifoCapacity = kFftSize * 8;     // power-of-2 ring buffer
};

} // namespace kaos_engine
