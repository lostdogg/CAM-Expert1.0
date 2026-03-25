#pragma once
#ifndef COPILOT_ENGINE_H
#define COPILOT_ENGINE_H

#include "IntentParser.h"
#include "ContextBuffer.h"
#include "ParameterNegotiator.h"
#include "AuditLog.h"
#include "LocalInferenceEngine.h"
#include "VectorDatabase.h"
#include "GeometricTokenizer.h"
#include "ConstrainedOutputValidator.h"
#include "SelfCorrectionLoop.h"
#include "../cam/MaterialLibrary.h"
#include "../cam/DynamicMotion.h"
#include "../managers/ToolpathManager.h"
#include "../cad/FeatureRecognition.h"
#include "../simulation/Verify.h"
#include <string>
#include <vector>
#include <functional>
#include <memory>

// --------------------------------------------------------------------------
// CopilotEngine – Orchestration Hub
//
// The engine ties together all five Copilot sub-systems:
//
//   1. IntentParser      – free text → ParsedIntent
//   2. ContextBuffer     – live CAM state snapshot
//   3. ParameterNegotiator – intent + context → NegotiatedParams
//   4. AuditLog          – append-only suggestion trail
//   5. Troubleshooting   – gouge analysis, cycle-time optimisation hints,
//                          post-processor explanation
//
// When the user types a command the engine:
//   a) Refreshes the context snapshot.
//   b) Parses the intent.
//   c) Negotiates parameters.
//   d) Generates a human-readable suggestion string.
//   e) Records the suggestion in the audit log.
//   f) Returns a CopilotResponse to the UI layer.
//
// If the user then clicks "Apply", applyLastSuggestion() physically calls
// DynamicMotion (or the appropriate strategy generator) and inserts the
// resulting Toolpath into the ToolpathManager.  This is the only code path
// that mutates the live managers; all other operations are read-only.
// --------------------------------------------------------------------------

struct CopilotResponse {
    bool        success      = false;
    std::string suggestion;          // primary suggestion text for the panel
    std::string warningMessage;      // advisory (non-fatal)
    std::string debugInfo;           // developer-facing details
    std::size_t auditIndex   = 0;    // index into AuditLog for this response
    NegotiatedParams params;         // negotiated params (for "Apply")

    // Convenience helpers
    bool hasWarning() const { return !warningMessage.empty(); }
};

// --------------------------------------------------------------------------
class CopilotEngine {
public:
    // Callback invoked when the engine generates a new suggestion.
    using SuggestionCallback = std::function<void(const CopilotResponse&)>;

    CopilotEngine();
    ~CopilotEngine() = default;

    // --- Wiring (call once at startup) ---

    // Provide a pointer to the live ToolpathManager (read + write for Apply).
    void setToolpathManager(ToolpathManager* mgr)          { m_toolpathMgr = mgr; }

    // Provide the MaterialLibrary (the engine never owns it).
    void setMaterialLibrary(const MaterialLibrary* lib)    { m_matLib = lib; }

    // Set the currently recognised features (updated by AFR after each import).
    void setRecognisedFeatures(const std::vector<RecognizedFeature>& f) { m_features = f; }

    // Update the most recent verify result.
    void setLastVerifyResult(const VerifyResult* vr)       { m_lastVerify = vr; }

    // Set the active workpiece material.
    void setMaterial(MaterialClass mat)                    { m_material = mat; m_hasMaterial = true; }

    // Register a UI callback that fires whenever a new suggestion is ready.
    void setSuggestionCallback(SuggestionCallback cb)      { m_suggestionCb = std::move(cb); }

    // Log file path (set before first call to processCommand).
    void setAuditLogPath(const std::string& path);

    // Path to a local quantized model file (GGUF / ONNX).
    // If not set the rule-based backend is used automatically.
    void setModelPath(const std::string& path) { m_inferenceEngine.setModelPath(path); }

    // Preload the local AI model (called when Toolpath Manager opens).
    // Returns false if loading fails; a descriptive error is appended to errorOut.
    bool loadInferenceModel(std::string* errorOut = nullptr);

    // Unload the local AI model to reclaim RAM / VRAM.
    void unloadInferenceModel();

    // Human-readable hardware summary (CPU, GPU, free RAM).
    std::string hardwareSummary() const;

    // Access the local RAG knowledge base for external population.
    VectorDatabase& vectorDatabase() { return m_vectorDb; }

    // --- Commands ---

    // Process a free-form text command from the user.
    // Fires the SuggestionCallback and returns the response.
    CopilotResponse processCommand(const std::string& command);

    // Apply the last accepted suggestion: physically generate the Toolpath
    // and insert it into the ToolpathManager.
    // Returns true on success; updates the audit log with Accepted outcome.
    bool applyLastSuggestion();

    // Dismiss the last suggestion (logs Rejected outcome).
    void rejectLastSuggestion();

    // --- Proactive analysis (call after Verify completes) ---

    // Analyse the verify result and return a troubleshooting suggestion.
    CopilotResponse analyseVerifyResult(const VerifyResult& result);

    // Analyse the current toolpath list for cycle-time optimisation.
    CopilotResponse analyseCycleTime();

    // Explain why certain G-code is generated by the post-processor.
    CopilotResponse explainPostOutput(const std::string& gcodeFragment);

    // --- Accessors ---
    const ContextBuffer& contextBuffer() const { return m_contextBuf; }
    const AuditLog&      auditLog()      const { return *m_auditLog; }

private:
    // Refresh context snapshot from live managers
    void refreshContext();

    // Build a gouge-troubleshoot suggestion
    CopilotResponse buildGougeSuggestion(const VerifyResult& result);

    // Build a cycle-time optimisation suggestion
    CopilotResponse buildCycleTimeSuggestion();

    // Build post-processor explanation
    CopilotResponse buildPostExplanation(const std::string& gcodeFragment);

    // Helper: action enum → string
    static std::string actionToString(CopilotAction a);
    static std::string strategyToString(StrategyType s);
    static std::string materialToString(MaterialClass m);

    // --- Sub-systems ---
    IntentParser         m_parser;
    ContextBuffer        m_contextBuf;
    ParameterNegotiator  m_negotiator;
    std::unique_ptr<AuditLog> m_auditLog;

    // --- Local AI components ---
    LocalInferenceEngine       m_inferenceEngine;  // SLM backend (hardware-aware)
    VectorDatabase             m_vectorDb;         // RAG knowledge base
    GeometricTokenizer         m_geoTokenizer;     // 3D → text tokens
    ConstrainedOutputValidator m_validator;        // parameter safety gate
    SelfCorrectionLoop         m_correctionLoop;   // gouge re-inference

    // --- Live pointers (not owned) ---
    ToolpathManager*               m_toolpathMgr = nullptr;
    const MaterialLibrary*         m_matLib      = nullptr;
    const VerifyResult*            m_lastVerify  = nullptr;
    std::vector<RecognizedFeature> m_features;
    MaterialClass                  m_material    = MaterialClass::Steel;
    bool                           m_hasMaterial = false;

    // --- Last suggestion (for Apply / Reject) ---
    CopilotResponse m_lastResponse;
    bool            m_pendingSuggestion = false;

    // --- Callback ---
    SuggestionCallback m_suggestionCb;
};

#endif // COPILOT_ENGINE_H
