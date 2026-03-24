#pragma once
#ifndef CONTEXT_BUFFER_H
#define CONTEXT_BUFFER_H

#include "../cam/Toolpath.h"
#include "../cam/MaterialLibrary.h"
#include "../cad/FeatureRecognition.h"
#include "../simulation/Verify.h"
#include "../managers/ToolpathManager.h"
#include <vector>
#include <string>
#include <optional>

// --------------------------------------------------------------------------
// ContextBuffer – Contextual Awareness Layer
//
// The Copilot cannot operate in a vacuum.  This class maintains a live,
// read-only snapshot of everything happening inside the CAM environment:
//
//   1. The ordered list of toolpath operations and the active operation index.
//   2. The In-Process Stock validity index (which ops have already removed
//      material, so the Copilot never suggests a redundant air-cut).
//   3. The most recent gouge / undercut simulation result from Verify.
//   4. The list of features recognised from the active solid (holes, pockets …).
//   5. The workpiece material and active tool geometry.
//   6. Post-processor configuration (controller type, machine limits).
//
// Call refresh() after any state change to pull a fresh snapshot.  The engine
// then reads from this buffer; it never touches the live managers directly.
// --------------------------------------------------------------------------

struct OperationSummary {
    int          index        = -1;
    std::string  name;
    StrategyType strategy     = StrategyType::Custom;
    double       estimatedTimeSec = 0;
    double       pathLengthMm    = 0;
    bool         isDirty      = false;
};

struct CopilotContext {
    // ---- Toolpath state ----
    std::vector<OperationSummary> operations;
    int  activeOperationIndex  = -1;   // selected op index (-1 = none)
    int  stockValidThrough     = -1;   // material removed up to this op

    // ---- Material & tool ----
    std::optional<MaterialClass>     activeMaterial;  // workpiece material
    std::optional<CuttingTool>       activeTool;      // tool on active op

    // ---- Feature recognition ----
    std::vector<RecognizedFeature>   recognisedFeatures; // AFR results

    // ---- Simulation result ----
    std::optional<VerifyResult>      lastVerifyResult;

    // ---- Aggregate stats ----
    double totalMachiningTimeSec = 0;
    double totalPathLengthMm     = 0;

    // Helper: returns true if there are any detected gouges
    bool hasGouges() const {
        return lastVerifyResult.has_value() && lastVerifyResult->hasGouge;
    }

    // Human-readable one-liner for the status bar
    std::string statusSummary() const;
};

// --------------------------------------------------------------------------
class ContextBuffer {
public:
    ContextBuffer() = default;

    // --- Refresh from live managers ---
    // Call this whenever the environment changes.
    // material is optional; pass nullptr if not yet set.
    void refresh(const ToolpathManager*           mgr,
                 const std::vector<RecognizedFeature>& features,
                 const VerifyResult*              lastVerify,
                 MaterialClass                    material,
                 bool                             hasMaterial);

    // --- Read access ---
    const CopilotContext& context() const { return m_ctx; }

    // Convenience shortcuts
    bool   hasActiveOperation() const { return m_ctx.activeOperationIndex >= 0; }
    bool   hasGouges()          const { return m_ctx.hasGouges(); }
    int    operationCount()     const { return static_cast<int>(m_ctx.operations.size()); }

private:
    CopilotContext m_ctx;
};

#endif // CONTEXT_BUFFER_H
