#pragma once
#ifndef LOCAL_INFERENCE_ENGINE_H
#define LOCAL_INFERENCE_ENGINE_H

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <cstdint>

// --------------------------------------------------------------------------
// LocalInferenceEngine – SLM (Small Language Model) Abstraction Layer
//
// Provides a hardware-agnostic interface for running local AI inference.
// Supports pluggable backends via IInferenceBackend SPI so different model
// formats (GGUF/Llama.cpp, ONNX/DJL-equivalent, rule-based) can be swapped
// without recompiling the Copilot module.
//
// Key design goals:
//   • Privacy first: sensitive part geometry is NEVER transmitted externally.
//   • Lazy loading: model stays compressed until the Toolpath Manager opens.
//   • Hardware agility: prefers CUDA → AVX-512 → generic CPU fallback.
//   • Quantization hints: backend should prefer 4-bit GGUF / AWQ formats to
//     keep the in-memory footprint to roughly 4-5 GB.
// --------------------------------------------------------------------------

// ---- Hardware capability flags (bitfield) ---------------------------------
enum class HardwareFlags : uint32_t {
    None       = 0,
    AVX2       = 1 << 0,   // AVX2 SIMD available
    AVX512     = 1 << 1,   // AVX-512 SIMD available
    AMX        = 1 << 2,   // Intel AMX tile instructions
    CUDA       = 1 << 3,   // NVIDIA CUDA GPU present
    ROCm       = 1 << 4,   // AMD ROCm GPU present
    MetalGPU   = 1 << 5,   // Apple Metal GPU present
};

inline HardwareFlags operator|(HardwareFlags a, HardwareFlags b) {
    return static_cast<HardwareFlags>(
        static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}
inline uint32_t operator&(HardwareFlags a, HardwareFlags b) {
    return static_cast<uint32_t>(a) & static_cast<uint32_t>(b);
}

// ---- Model load state -----------------------------------------------------
enum class ModelState {
    Unloaded,       // model file not yet read
    Compressed,     // model bytes in memory (compressed / mmap'd)
    Loaded,         // model fully expanded in VRAM / RAM, ready for inference
    Failed,         // load or initialisation error
};

// ---- Inference request and response ---------------------------------------

struct InferenceRequest {
    std::string     systemPrompt;    // role / persona context
    std::string     contextBlock;    // geometric + CAM context (from GeometricTokenizer)
    std::string     userQuery;       // natural-language command from machinist
    int             maxTokens = 256; // token budget for response
    double          temperature = 0.2; // low temperature → deterministic output
};

struct InferenceResponse {
    bool            success      = false;
    std::string     text;            // model output (may be JSON or prose)
    std::string     errorMessage;
    double          inferenceMs  = 0; // wall-clock inference time
    std::string     backendName;     // which backend produced this response
};

// --------------------------------------------------------------------------
// IInferenceBackend – Service Provider Interface
//
// Implement this interface to add a new inference backend.
// The LocalInferenceEngine selects the best available backend at startup.
// --------------------------------------------------------------------------
class IInferenceBackend {
public:
    virtual ~IInferenceBackend() = default;

    // Human-readable backend identifier (e.g. "RuleBased", "Llama.cpp", "ONNX")
    virtual std::string name() const = 0;

    // True if this backend can actually run on the current hardware/OS.
    // Called at startup to select the best available backend.
    virtual bool        isAvailable() const = 0;

    // Load the model weights from disk (lazy-loading entry point).
    // modelPath may be empty for the rule-based backend.
    virtual bool        loadModel(const std::string& modelPath) = 0;

    // Unload model weights to free RAM / VRAM.
    virtual void        unloadModel() = 0;

    // Current load state.
    virtual ModelState  state() const = 0;

    // Run inference synchronously.
    virtual InferenceResponse infer(const InferenceRequest& req) = 0;
};

// --------------------------------------------------------------------------
// RuleBasedBackend – Deterministic fallback (always available)
//
// Wraps the existing keyword-matching Copilot logic so the engine degrades
// gracefully when no quantized model is present.  Satisfies the
// <500 ms intent-recognition latency requirement on any hardware.
// --------------------------------------------------------------------------
class RuleBasedBackend final : public IInferenceBackend {
public:
    std::string  name()        const override { return "RuleBased"; }
    bool         isAvailable() const override { return true; }
    bool         loadModel(const std::string&) override { return true; }
    void         unloadModel() override {}
    ModelState   state()       const override { return ModelState::Loaded; }

    // Generates a structured JSON response from rule-based heuristics.
    InferenceResponse infer(const InferenceRequest& req) override;

private:
    // Internal rule dispatch helpers
    InferenceResponse handleMillIntent(const std::string& query) const;
    InferenceResponse handleDrillIntent(const std::string& query) const;
    InferenceResponse handleOptimizeIntent(const std::string& query) const;
    InferenceResponse handleTroubleIntent(const std::string& query) const;
    InferenceResponse handleGenericIntent(const std::string& query) const;

    static bool containsAny(const std::string& text,
                             std::initializer_list<const char*> keywords);
};

// --------------------------------------------------------------------------
// HardwareDetector – probes available acceleration
// --------------------------------------------------------------------------
class HardwareDetector {
public:
    // Detect all available CPU / GPU capabilities.
    static HardwareFlags detect();

    // Estimated free system RAM in megabytes.
    static size_t freeRamMB();

    // True if a 4-bit quantized model (~4-5 GB) can fit in available RAM.
    static bool canFitQuantizedModel();

    // Human-readable summary for the audit log.
    static std::string summary(HardwareFlags flags);
};

// --------------------------------------------------------------------------
// LocalInferenceEngine – orchestrator
//
// Selects the best IInferenceBackend, manages lazy loading, and provides
// a single infer() call to the rest of the Copilot system.
// --------------------------------------------------------------------------
class LocalInferenceEngine {
public:
    // Optional token-budget progress callback (for streaming in future)
    using ProgressCallback = std::function<void(int tokensGenerated)>;

    LocalInferenceEngine();
    ~LocalInferenceEngine() = default;

    // --- Configuration (call before first infer()) ---

    // Path to the quantized GGUF / ONNX model file.  If empty, falls back
    // to the rule-based backend unconditionally.
    void setModelPath(const std::string& path) { m_modelPath = path; }

    // Override backend selection (for testing or explicit configuration).
    void setBackend(std::unique_ptr<IInferenceBackend> backend);

    // Register an optional progress callback.
    void setProgressCallback(ProgressCallback cb) { m_progressCb = std::move(cb); }

    // --- Lazy loading ---

    // Load the model into memory (called when Toolpath Manager is opened).
    // Selects the best available backend automatically if none is set.
    // Returns false and sets an error string if loading fails.
    bool loadModel(std::string* errorOut = nullptr);

    // Unload model to reclaim RAM / VRAM.
    void unloadModel();

    // Current model state.
    ModelState modelState() const;

    // --- Inference ---

    // Run inference.  Automatically calls loadModel() if not yet loaded.
    // Thread-safe: may be called from the UI thread; backend may use its own
    // worker thread internally.
    InferenceResponse infer(const InferenceRequest& req);

    // --- Hardware info ---
    HardwareFlags     hardwareFlags()  const { return m_hwFlags; }
    std::string       hardwareSummary() const;
    std::string       backendName()    const;

private:
    void selectBestBackend();

    std::string                          m_modelPath;
    HardwareFlags                        m_hwFlags = HardwareFlags::None;
    std::unique_ptr<IInferenceBackend>   m_backend;
    ProgressCallback                     m_progressCb;
};

#endif // LOCAL_INFERENCE_ENGINE_H
