#include "LocalInferenceEngine.h"
#include <sstream>
#include <algorithm>
#include <cctype>
#include <chrono>

// --------------------------------------------------------------------------
// Platform-specific hardware detection
// --------------------------------------------------------------------------
#if defined(_MSC_VER)
#  include <intrin.h>
#elif defined(__GNUC__) || defined(__clang__)
#  include <cpuid.h>
#endif

#if defined(_WIN32)
#  define WIN32_LEAN_AND_MEAN
#  define NOMINMAX
#  include <windows.h>
#elif defined(__linux__)
#  include <sys/sysinfo.h>
#elif defined(__APPLE__)
#  include <sys/sysctl.h>
#endif

// --------------------------------------------------------------------------
// HardwareDetector
// --------------------------------------------------------------------------

HardwareFlags HardwareDetector::detect() {
    uint32_t flags = static_cast<uint32_t>(HardwareFlags::None);

    // ---- CPU feature detection via CPUID ----
#if defined(_MSC_VER)
    int cpuInfo[4] = {0};

    // EAX=1: ECX and EDX feature bits
    __cpuid(cpuInfo, 1);

    // AVX2: EAX=7, EBX bit 5
    __cpuidex(cpuInfo, 7, 0);
    if (cpuInfo[1] & (1 << 5))
        flags |= static_cast<uint32_t>(HardwareFlags::AVX2);

    // AVX-512F: EAX=7, EBX bit 16
    if (cpuInfo[1] & (1 << 16))
        flags |= static_cast<uint32_t>(HardwareFlags::AVX512);

    // Intel AMX-TILE: EAX=7, EDX bit 24
    if (cpuInfo[3] & (1 << 24))
        flags |= static_cast<uint32_t>(HardwareFlags::AMX);

#elif defined(__GNUC__) || defined(__clang__)
    unsigned int eax, ebx, ecx, edx;

    if (__get_cpuid_max(0, nullptr) >= 7) {
        __cpuid_count(7, 0, eax, ebx, ecx, edx);

        if (ebx & (1u << 5))
            flags |= static_cast<uint32_t>(HardwareFlags::AVX2);
        if (ebx & (1u << 16))
            flags |= static_cast<uint32_t>(HardwareFlags::AVX512);
        if (edx & (1u << 24))
            flags |= static_cast<uint32_t>(HardwareFlags::AMX);
    }
#endif

    // ---- GPU detection ----
    // Detect NVIDIA CUDA by probing nvcuda.dll / libcuda.so presence.
    // A full implementation would dlopen the library; here we use the
    // preprocessor guard so the project compiles without CUDA headers.
#if defined(HAVE_CUDA)
    flags |= static_cast<uint32_t>(HardwareFlags::CUDA);
#endif

#if defined(HAVE_ROCM)
    flags |= static_cast<uint32_t>(HardwareFlags::ROCm);
#endif

#if defined(__APPLE__)
    // On Apple Silicon every device has a Metal GPU.
    flags |= static_cast<uint32_t>(HardwareFlags::MetalGPU);
#endif

    return static_cast<HardwareFlags>(flags);
}

size_t HardwareDetector::freeRamMB() {
#if defined(_WIN32)
    MEMORYSTATUSEX status{};
    status.dwLength = sizeof(status);
    if (GlobalMemoryStatusEx(&status))
        return static_cast<size_t>(status.ullAvailPhys / (1024ULL * 1024ULL));
#elif defined(__linux__)
    struct sysinfo si{};
    if (sysinfo(&si) == 0)
        return static_cast<size_t>(si.freeram * si.mem_unit / (1024ULL * 1024ULL));
#elif defined(__APPLE__)
    uint64_t freeBytes = 0;
    size_t len = sizeof(freeBytes);
    sysctlbyname("hw.memsize", &freeBytes, &len, nullptr, 0);
    return static_cast<size_t>(freeBytes / (1024ULL * 1024ULL));
#endif
    return 0;
}

bool HardwareDetector::canFitQuantizedModel() {
    // 4-bit quantized 7B model needs ~4 500 MB.  Require 5 000 MB free.
    return freeRamMB() >= 5000;
}

std::string HardwareDetector::summary(HardwareFlags flags) {
    std::ostringstream oss;
    oss << "CPU: ";
    if (static_cast<uint32_t>(flags) & static_cast<uint32_t>(HardwareFlags::AVX512))
        oss << "AVX-512 ";
    else if (static_cast<uint32_t>(flags) & static_cast<uint32_t>(HardwareFlags::AVX2))
        oss << "AVX2 ";
    if (static_cast<uint32_t>(flags) & static_cast<uint32_t>(HardwareFlags::AMX))
        oss << "AMX ";

    oss << "| GPU: ";
    if (static_cast<uint32_t>(flags) & static_cast<uint32_t>(HardwareFlags::CUDA))
        oss << "CUDA ";
    else if (static_cast<uint32_t>(flags) & static_cast<uint32_t>(HardwareFlags::ROCm))
        oss << "ROCm ";
    else if (static_cast<uint32_t>(flags) & static_cast<uint32_t>(HardwareFlags::MetalGPU))
        oss << "Metal ";
    else
        oss << "none ";

    oss << "| Free RAM: " << freeRamMB() << " MB";
    return oss.str();
}

// --------------------------------------------------------------------------
// RuleBasedBackend – deterministic keyword-driven inference
// --------------------------------------------------------------------------

bool RuleBasedBackend::containsAny(const std::string& text,
                                    std::initializer_list<const char*> keywords) {
    std::string lower = text;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    for (const char* kw : keywords) {
        if (lower.find(kw) != std::string::npos)
            return true;
    }
    return false;
}

InferenceResponse RuleBasedBackend::infer(const InferenceRequest& req) {
    auto t0 = std::chrono::steady_clock::now();

    InferenceResponse resp;
    resp.backendName = name();

    const std::string& q = req.userQuery;

    if (containsAny(q, {"drill", "hole", "bore", "peck", "ream"}))
        resp = handleDrillIntent(q);
    else if (containsAny(q, {"optimis", "optim", "faster", "cycle", "reduce"}))
        resp = handleOptimizeIntent(q);
    else if (containsAny(q, {"gouge", "collision", "holder", "tilt", "clearance"}))
        resp = handleTroubleIntent(q);
    else if (containsAny(q, {"mill", "pocket", "rough", "finish", "contour",
                              "slot", "boss", "face", "dynamic"}))
        resp = handleMillIntent(q);
    else
        resp = handleGenericIntent(q);

    resp.success     = true;
    resp.backendName = name();

    auto t1 = std::chrono::steady_clock::now();
    resp.inferenceMs = static_cast<double>(
        std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count()) / 1000.0;

    return resp;
}

InferenceResponse RuleBasedBackend::handleMillIntent(const std::string& q) const {
    InferenceResponse r;
    bool isDynamic = containsAny(q, {"dynamic", "trochoidal", "high-speed", "hsc", "hsm"});
    bool isTitanium = containsAny(q, {"titanium", "ti-6", "grade 5"});
    bool isAluminum = containsAny(q, {"aluminum", "aluminium", "al7075", "al6061"});
    bool isPocket   = containsAny(q, {"pocket", "cavity", "recess"});

    std::ostringstream oss;
    oss << "{\n"
        << "  \"action\": \"CREATE_TOOLPATH\",\n"
        << "  \"strategy\": \"" << (isDynamic ? "DynamicMill" : (isPocket ? "Pocket2D" : "Contour2D")) << "\",\n"
        << "  \"entry\": \"" << (isTitanium ? "TangentialArc" : "Helical") << "\",\n"
        << "  \"material_class\": \"" << (isTitanium ? "Titanium" : (isAluminum ? "Aluminum" : "Steel")) << "\",\n"
        << "  \"radial_engagement\": " << (isTitanium ? 0.10 : (isAluminum ? 0.60 : 0.40)) << ",\n"
        << "  \"smoothing\": " << (isTitanium ? "true" : "false") << ",\n"
        << "  \"reasoning\": \"Selected based on material and geometry keywords\"\n"
        << "}";
    r.text = oss.str();
    return r;
}

InferenceResponse RuleBasedBackend::handleDrillIntent(const std::string& q) const {
    InferenceResponse r;
    bool isPeck  = containsAny(q, {"deep", "peck", "chip", "break"});
    bool isReam  = containsAny(q, {"ream", "finish", "precise", "tolerance"});

    std::ostringstream oss;
    oss << "{\n"
        << "  \"action\": \"CREATE_TOOLPATH\",\n"
        << "  \"strategy\": \"" << (isPeck ? "PeckDrilling" : (isReam ? "Reaming" : "Drilling")) << "\",\n"
        << "  \"peck_depth_factor\": " << (isPeck ? 3.0 : 0.0) << ",\n"
        << "  \"coolant\": \"Flood\",\n"
        << "  \"reasoning\": \"Drilling cycle selected from keyword analysis\"\n"
        << "}";
    r.text = oss.str();
    return r;
}

InferenceResponse RuleBasedBackend::handleOptimizeIntent(const std::string& q) const {
    InferenceResponse r;
    (void)q;
    r.text = "{\n"
             "  \"action\": \"OPTIMIZE_PARAMS\",\n"
             "  \"target\": \"CycleTime\",\n"
             "  \"suggestions\": [\"Increase feed rate on air-moves\","
             " \"Consolidate tool changes\", \"Enable HSM mode\"]\n"
             "}";
    return r;
}

InferenceResponse RuleBasedBackend::handleTroubleIntent(const std::string& q) const {
    InferenceResponse r;
    bool isHolder = containsAny(q, {"holder", "shank", "arbor", "collet"});
    std::ostringstream oss;
    oss << "{\n"
        << "  \"action\": \"CORRECT_COLLISION\",\n"
        << "  \"type\": \"" << (isHolder ? "HolderCollision" : "GoueDetected") << "\",\n"
        << "  \"suggested_correction\": \"" << (isHolder ? "IncreaseTiltAngle" : "ReduceDepth") << "\",\n"
        << "  \"tilt_delta_deg\": 3.0,\n"
        << "  \"reasoning\": \"Collision context detected; tilt correction recommended\"\n"
        << "}";
    r.text = oss.str();
    return r;
}

InferenceResponse RuleBasedBackend::handleGenericIntent(const std::string&) const {
    InferenceResponse r;
    r.text = "{\n"
             "  \"action\": \"CLARIFY\",\n"
             "  \"message\": \"Please specify the feature type and material"
             " (e.g., 'rough this pocket in titanium').\"\n"
             "}";
    return r;
}

// --------------------------------------------------------------------------
// LocalInferenceEngine
// --------------------------------------------------------------------------

LocalInferenceEngine::LocalInferenceEngine() {
    m_hwFlags = HardwareDetector::detect();
}

void LocalInferenceEngine::setBackend(std::unique_ptr<IInferenceBackend> backend) {
    m_backend = std::move(backend);
}

void LocalInferenceEngine::selectBestBackend() {
    if (m_backend)
        return; // already set (e.g. by setBackend() or a previous call)

    // Priority: CUDA → ROCm → Metal → AVX-512 → AVX2 → RuleBased
    // For all non-rule-based backends the model file must exist.  Since we
    // ship without bundled weights, we fall back to RuleBasedBackend unless a
    // model path has been configured.
    //
    // A production build would instantiate LlamaCppBackend or OnnxBackend here
    // when the corresponding headers are available (guarded by CMake options).

    m_backend = std::make_unique<RuleBasedBackend>();
}

bool LocalInferenceEngine::loadModel(std::string* errorOut) {
    selectBestBackend();

    if (m_backend->state() == ModelState::Loaded)
        return true;

    bool ok = m_backend->loadModel(m_modelPath);
    if (!ok && errorOut)
        *errorOut = "Backend '" + m_backend->name() + "' failed to load model: " + m_modelPath;
    return ok;
}

void LocalInferenceEngine::unloadModel() {
    if (m_backend)
        m_backend->unloadModel();
}

ModelState LocalInferenceEngine::modelState() const {
    if (!m_backend)
        return ModelState::Unloaded;
    return m_backend->state();
}

InferenceResponse LocalInferenceEngine::infer(const InferenceRequest& req) {
    if (!m_backend || m_backend->state() != ModelState::Loaded) {
        std::string err;
        if (!loadModel(&err)) {
            InferenceResponse fail;
            fail.success      = false;
            fail.errorMessage = err;
            return fail;
        }
    }
    return m_backend->infer(req);
}

std::string LocalInferenceEngine::hardwareSummary() const {
    return HardwareDetector::summary(m_hwFlags);
}

std::string LocalInferenceEngine::backendName() const {
    return m_backend ? m_backend->name() : "(none)";
}
