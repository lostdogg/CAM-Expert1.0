#include "ContextBuffer.h"
#include <sstream>

// --------------------------------------------------------------------------
// CopilotContext::statusSummary
// --------------------------------------------------------------------------
std::string CopilotContext::statusSummary() const {
    std::ostringstream ss;
    ss << operations.size() << " op(s)";
    if (activeOperationIndex >= 0)
        ss << "  |  Active: Op " << (activeOperationIndex + 1);
    if (stockValidThrough >= 0)
        ss << "  |  Stock valid through Op " << (stockValidThrough + 1);
    if (hasGouges())
        ss << "  |  *** GOUGE DETECTED ***";
    ss << "  |  Total time: "
       << static_cast<int>(totalMachiningTimeSec / 60) << " min";
    return ss.str();
}

// --------------------------------------------------------------------------
// ContextBuffer::refresh
// --------------------------------------------------------------------------
void ContextBuffer::refresh(const ToolpathManager*                mgr,
                             const std::vector<RecognizedFeature>& features,
                             const VerifyResult*                   lastVerify,
                             MaterialClass                         material,
                             bool                                  hasMaterial)
{
    m_ctx = CopilotContext{};   // clear previous snapshot

    if (!mgr) return;

    // ---- Operation list ----
    m_ctx.activeOperationIndex = mgr->selectedIndex();
    m_ctx.stockValidThrough    = mgr->stockValidThrough();
    m_ctx.totalMachiningTimeSec= mgr->totalMachiningTime();
    m_ctx.totalPathLengthMm    = mgr->totalPathLength();

    for (int i = 0; i < mgr->count(); ++i) {
        const Toolpath& tp = mgr->at(i);
        OperationSummary op;
        op.index            = i;
        op.name             = tp.name();
        op.strategy         = tp.strategy();
        op.estimatedTimeSec = tp.estimatedTime();
        op.pathLengthMm     = tp.totalLength();
        op.isDirty          = tp.isDirty();
        m_ctx.operations.push_back(op);
    }

    // ---- Active tool ----
    if (mgr->hasSelection()) {
        const Toolpath& active = mgr->at(mgr->selectedIndex());
        m_ctx.activeTool = active.tool();
    }

    // ---- Material ----
    if (hasMaterial)
        m_ctx.activeMaterial = material;

    // ---- Feature recognition ----
    m_ctx.recognisedFeatures = features;

    // ---- Verify result ----
    if (lastVerify)
        m_ctx.lastVerifyResult = *lastVerify;
}
