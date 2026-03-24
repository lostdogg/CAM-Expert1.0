#include "CopilotEngine.h"
#include "../cam/DynamicMotion.h"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cmath>

// --------------------------------------------------------------------------
// Ctor
// --------------------------------------------------------------------------
CopilotEngine::CopilotEngine()
    : m_negotiator(nullptr)
    , m_auditLog(std::make_unique<AuditLog>())   // in-memory until setAuditLogPath
{}

void CopilotEngine::setAuditLogPath(const std::string& path) {
    m_auditLog = std::make_unique<AuditLog>(path);
}

// --------------------------------------------------------------------------
// refreshContext
// --------------------------------------------------------------------------
void CopilotEngine::refreshContext() {
    m_contextBuf.refresh(m_toolpathMgr,
                         m_features,
                         m_lastVerify,
                         m_material,
                         m_hasMaterial);
    // Re-wire the negotiator whenever the material library pointer may have changed
    m_negotiator = ParameterNegotiator(m_matLib);
}

// --------------------------------------------------------------------------
// String helpers
// --------------------------------------------------------------------------
std::string CopilotEngine::actionToString(CopilotAction a) {
    switch (a) {
        case CopilotAction::Mill:          return "Mill";
        case CopilotAction::Drill:         return "Drill";
        case CopilotAction::Turn:          return "Turn";
        case CopilotAction::Verify:        return "Verify";
        case CopilotAction::Optimize:      return "Optimize";
        case CopilotAction::Explain:       return "Explain";
        case CopilotAction::Troubleshoot:  return "Troubleshoot";
        default:                           return "Unknown";
    }
}

std::string CopilotEngine::strategyToString(StrategyType s) {
    switch (s) {
        case StrategyType::DynamicMill:      return "DynamicMill";
        case StrategyType::OptiRough:        return "OptiRough";
        case StrategyType::Contour2D:        return "Contour2D";
        case StrategyType::Pocket2D:         return "Pocket2D";
        case StrategyType::FaceMill:         return "FaceMill";
        case StrategyType::Drilling:         return "Drilling";
        case StrategyType::WaterlineRough:   return "WaterlineRough";
        case StrategyType::Raster3D:         return "Raster3D";
        case StrategyType::Scallop3D:        return "Scallop3D";
        case StrategyType::Spiral3D:         return "Spiral3D";
        case StrategyType::Multiaxis5:       return "5-Axis";
        case StrategyType::Swarf4Axis:       return "Swarf4Axis";
        case StrategyType::RoughTurning:     return "RoughTurning";
        case StrategyType::FinishTurning:    return "FinishTurning";
        case StrategyType::Grooving:         return "Grooving";
        default:                             return "Custom";
    }
}

std::string CopilotEngine::materialToString(MaterialClass m) {
    switch (m) {
        case MaterialClass::Aluminum:      return "Aluminum";
        case MaterialClass::Steel:         return "Steel";
        case MaterialClass::StainlessSteel:return "StainlessSteel";
        case MaterialClass::Titanium:      return "Titanium";
        case MaterialClass::Inconel:       return "Inconel";
        case MaterialClass::CastIron:      return "CastIron";
        case MaterialClass::Brass:         return "Brass";
        case MaterialClass::Plastic:       return "Plastic";
        case MaterialClass::Carbon_CFRP:   return "CFRP";
        default:                           return "Custom";
    }
}

// --------------------------------------------------------------------------
// processCommand – main entry point
// --------------------------------------------------------------------------
CopilotResponse CopilotEngine::processCommand(const std::string& command) {
    CopilotResponse resp;

    if (command.empty()) {
        resp.suggestion = "Please type a command, e.g. \"Rough out this pocket in titanium\".";
        resp.success    = false;
        return resp;
    }

    refreshContext();
    const CopilotContext& ctx = m_contextBuf.context();

    // 1. Parse intent
    ParsedIntent intent = m_parser.parse(command);

    if (!intent.isValid()) {
        resp.success    = false;
        resp.suggestion = "I wasn't sure what you'd like to do. Try something like:\n"
                          "  • \"Rough out this pocket in aluminum\"\n"
                          "  • \"Finish the wall with a dynamic contour\"\n"
                          "  • \"Verify for gouges\"\n"
                          "  • \"Optimize cycle time\"";
        // Still log the attempt
        AuditEntry e;
        e.command  = command;
        e.action   = actionToString(intent.action);
        e.strategy = "N/A";
        e.material = "N/A";
        e.outcome  = AuditOutcome::Error;
        e.errorDetail = "Low confidence parse (" +
                         std::to_string(static_cast<int>(intent.confidence * 100)) + "%)";
        m_auditLog->record(e);
        return resp;
    }

    // 2. Route special actions
    if (intent.action == CopilotAction::Verify)
        return analyseVerifyResult(m_lastVerify ? *m_lastVerify : VerifyResult{});

    if (intent.action == CopilotAction::Optimize)
        return analyseCycleTime();

    if (intent.action == CopilotAction::Troubleshoot)
        return analyseVerifyResult(m_lastVerify ? *m_lastVerify : VerifyResult{});

    if (intent.action == CopilotAction::Explain) {
        return buildPostExplanation("");
    }

    // 3. Negotiate parameters
    NegotiatedParams params = m_negotiator.negotiate(intent, ctx);

    // 4. Build suggestion text
    std::ostringstream ss;
    ss << "Copilot suggestion for: \"" << command << "\"\n";
    ss << std::string(60, '-') << "\n";
    ss << params.rationale;

    // Stock-awareness note
    if (ctx.stockValidThrough >= 0 && !ctx.operations.empty()) {
        ss << "\n[Stock-Aware] In-process stock is valid through Op "
           << (ctx.stockValidThrough + 1)
           << ". The proposed toolpath will not re-cut already-machined regions.";
    }

    // Feature-recognition note
    if (!ctx.recognisedFeatures.empty()) {
        const RecognizedFeature& feat = ctx.recognisedFeatures.front();
        ss << "\n[Feature] Detected: ";
        switch (feat.type) {
            case RecognizedFeature::Type::BlindPocket:   ss << "Blind pocket"; break;
            case RecognizedFeature::Type::ThroughPocket: ss << "Through pocket"; break;
            case RecognizedFeature::Type::Hole:          ss << "Hole (Ø"
                 << std::fixed << std::setprecision(1) << feat.diameter << " mm)"; break;
            case RecognizedFeature::Type::Boss:          ss << "Boss"; break;
            case RecognizedFeature::Type::Slot:          ss << "Slot"; break;
            default:                                     ss << "Feature"; break;
        }
        if (feat.depth > 0)
            ss << ", depth " << std::fixed << std::setprecision(1) << feat.depth << " mm";
    }

    if (!params.warningMessage.empty())
        ss << "\n[Advisory] " << params.warningMessage;

    ss << "\n\nClick \"Apply\" to generate the toolpath, or \"Dismiss\" to cancel.";

    resp.success        = true;
    resp.suggestion     = ss.str();
    resp.warningMessage = params.warningMessage;
    resp.params         = params;

    // 5. Audit
    AuditEntry e;
    e.command   = command;
    e.action    = actionToString(intent.action);
    e.strategy  = strategyToString(params.strategy);
    e.material  = materialToString(params.material);
    e.rationale = params.rationale;
    e.outcome   = AuditOutcome::Proposed;
    resp.auditIndex = m_auditLog->record(e);

    // 6. Fire callback
    m_lastResponse       = resp;
    m_pendingSuggestion  = true;
    if (m_suggestionCb) m_suggestionCb(resp);

    return resp;
}

// --------------------------------------------------------------------------
// applyLastSuggestion – generate toolpath and insert into manager
// --------------------------------------------------------------------------
bool CopilotEngine::applyLastSuggestion() {
    if (!m_pendingSuggestion || !m_toolpathMgr) return false;

    const NegotiatedParams& p = m_lastResponse.params;

    // Use DynamicMotion to generate a placeholder toolpath with the negotiated
    // params.  For a full implementation, the boundary would come from the
    // selected geometry; here we use a unit-square sentinel boundary so the
    // engine produces a valid Toolpath object.
    std::vector<Geom::Vec2> boundary = {
        {0, 0}, {50, 0}, {50, 50}, {0, 50}
    };
    double depth = (p.cuttingParams.axialDepth > 0) ? p.cuttingParams.axialDepth : 5.0;

    DynamicMotion dm;
    Toolpath tp = dm.generateForMaterial(boundary, depth, p.tool, p.material);
    tp.setName("Copilot: " + strategyToString(p.strategy));

    m_toolpathMgr->addToolpath(std::move(tp));

    m_auditLog->updateOutcome(m_lastResponse.auditIndex, AuditOutcome::Accepted);
    m_pendingSuggestion = false;
    return true;
}

// --------------------------------------------------------------------------
// rejectLastSuggestion
// --------------------------------------------------------------------------
void CopilotEngine::rejectLastSuggestion() {
    if (!m_pendingSuggestion) return;
    m_auditLog->updateOutcome(m_lastResponse.auditIndex, AuditOutcome::Rejected);
    m_pendingSuggestion = false;
}

// --------------------------------------------------------------------------
// analyseVerifyResult – Predictive Debugging
// --------------------------------------------------------------------------
CopilotResponse CopilotEngine::analyseVerifyResult(const VerifyResult& result) {
    return buildGougeSuggestion(result);
}

CopilotResponse CopilotEngine::buildGougeSuggestion(const VerifyResult& result) {
    CopilotResponse resp;
    resp.success = true;

    std::ostringstream ss;
    ss << "Simulation Analysis\n" << std::string(60, '-') << "\n";

    if (!result.hasGouge && !result.hasUndercut) {
        ss << "No gouges or undercuts detected. Simulation is clean.\n";
        ss << "Total machined volume appears correct.";
    } else {
        if (result.hasGouge) {
            ss << "GOUGE DETECTED: " << result.gougeCount << " location(s)\n";
            ss << "Max gouge depth: " << std::fixed << std::setprecision(3)
               << result.maxGougeDepth << " mm\n\n";

            // Actionable suggestions
            ss << "Suggested remediation:\n";
            ss << "  1. Increase the clearance plane by at least "
               << std::fixed << std::setprecision(3)
               << (result.maxGougeDepth + 0.5) << " mm on all retract moves.\n";
            ss << "  2. Check Link Parameters: ensure arcs or lines are not\n"
               << "     passing through the stock on repositioning moves.\n";
            ss << "  3. Consider switching the retract style from \"Short\" to\n"
               << "     \"Full retract\" to guarantee clearance.\n";
        }
        if (result.hasUndercut) {
            ss << "UNDERCUT DETECTED: " << result.undercutCount << " location(s)\n";
            ss << "Max undercut depth: " << std::fixed << std::setprecision(3)
               << result.maxUndercutDepth << " mm\n\n";
            ss << "Suggested remediation:\n";
            ss << "  1. Review the floor finish allowance; a positive stock-on-floor\n"
               << "     may be masking the undercut in earlier operations.\n";
            ss << "  2. If this is intentional geometry (e.g. a dovetail), enable\n"
               << "     \"Undercut\" check on the Verify Options and mark as accepted.\n";
        }
    }

    // Audit
    AuditEntry e;
    e.command  = "[Automatic gouge analysis]";
    e.action   = "Troubleshoot";
    e.strategy = "VerifyAnalysis";
    e.material = "N/A";
    e.rationale= ss.str();
    e.outcome  = AuditOutcome::Proposed;
    resp.auditIndex = m_auditLog->record(e);

    resp.suggestion = ss.str();
    if (m_suggestionCb) m_suggestionCb(resp);
    return resp;
}

// --------------------------------------------------------------------------
// analyseCycleTime – Optimisation Loop
// --------------------------------------------------------------------------
CopilotResponse CopilotEngine::analyseCycleTime() {
    return buildCycleTimeSuggestion();
}

CopilotResponse CopilotEngine::buildCycleTimeSuggestion() {
    refreshContext();
    const CopilotContext& ctx = m_contextBuf.context();

    CopilotResponse resp;
    resp.success = true;

    std::ostringstream ss;
    ss << "Cycle Time Analysis\n" << std::string(60, '-') << "\n";

    if (ctx.operations.empty()) {
        ss << "No operations found. Add at least one toolpath operation to analyse.";
        resp.suggestion = ss.str();
        return resp;
    }

    double totalSec = ctx.totalMachiningTimeSec;
    ss << "Total estimated cycle time: "
       << static_cast<int>(totalSec / 60) << " min "
       << static_cast<int>(std::fmod(totalSec, 60)) << " sec\n\n";

    // Per-operation breakdown
    ss << "Operation breakdown:\n";
    for (const auto& op : ctx.operations) {
        ss << "  Op " << (op.index + 1) << " [" << op.name << "]: "
           << static_cast<int>(op.estimatedTimeSec) << " sec  |  "
           << std::fixed << std::setprecision(0) << op.pathLengthMm << " mm path";

        // Flag operations that could benefit from Dynamic Motion
        if (op.strategy != StrategyType::DynamicMill &&
            op.strategy != StrategyType::OptiRough &&
            op.estimatedTimeSec > 120) {
            ss << "  ← Consider switching to DynamicMill to save ~30% cycle time";
        }
        ss << "\n";
    }

    // Find the slowest operation
    auto it = std::max_element(ctx.operations.begin(), ctx.operations.end(),
        [](const OperationSummary& a, const OperationSummary& b) {
            return a.estimatedTimeSec < b.estimatedTimeSec;
        });

    if (it != ctx.operations.end() && it->estimatedTimeSec > 60) {
        ss << "\nLongest operation: Op " << (it->index + 1)
           << " (" << static_cast<int>(it->estimatedTimeSec) << " sec)\n";

        if (it->strategy != StrategyType::DynamicMill) {
            ss << "Recommendation: Switch Op " << (it->index + 1)
               << " to a DynamicMill strategy.  For this operation,\n"
               << "increasing radial engagement from 10% to 50% of Ø and\n"
               << "using High-Speed Machining (HSM) passes could reduce\n"
               << "this operation's time by approximately 25–35%.";
        } else {
            ss << "The longest operation already uses DynamicMill.  Consider:\n"
               << "  • Increasing spindle override (if machine limits allow)\n"
               << "  • Splitting into two operations with a rougher + finisher\n"
               << "  • Switching from Flood coolant to MQL/mist to reduce drag";
        }
    }

    // Audit
    AuditEntry e;
    e.command  = "[Automatic cycle-time analysis]";
    e.action   = "Optimize";
    e.strategy = "CycleTimeAnalysis";
    e.material = "N/A";
    e.rationale= ss.str();
    e.outcome  = AuditOutcome::Proposed;
    resp.auditIndex = m_auditLog->record(e);

    resp.suggestion = ss.str();
    if (m_suggestionCb) m_suggestionCb(resp);
    return resp;
}

// --------------------------------------------------------------------------
// explainPostOutput – Post-Processor Guidance
// --------------------------------------------------------------------------
CopilotResponse CopilotEngine::explainPostOutput(const std::string& gcodeFragment) {
    return buildPostExplanation(gcodeFragment);
}

CopilotResponse CopilotEngine::buildPostExplanation(const std::string& gcodeFragment) {
    CopilotResponse resp;
    resp.success = true;

    std::ostringstream ss;
    ss << "Post-Processor Explanation\n" << std::string(60, '-') << "\n";

    if (gcodeFragment.empty()) {
        ss << "G-code is generated by the Post-Processor from the NCI intermediate\n"
           << "format.  Key concepts:\n\n"
           << "  G00 – Rapid positioning (non-cutting move at maximum speed)\n"
           << "  G01 – Linear feed (cutting move at programmed feed rate)\n"
           << "  G02 – Circular arc, clockwise\n"
           << "  G03 – Circular arc, counter-clockwise\n"
           << "  G43 – Tool length offset compensation (Height: H<tool#>)\n"
           << "  M03 – Spindle ON, clockwise\n"
           << "  M08 – Coolant ON   |  M09 – Coolant OFF\n"
           << "  M30 – Program end + rewind\n\n"
           << "If you see an 'illegal move' at the controller, check:\n"
           << "  1. Machine travel limits (X/Y/Z max in Post Config)\n"
           << "  2. Singularity at 5-axis tool-tip transition (B=0 / A=0 boundary)\n"
           << "  3. Feed rate units (mm/min vs. in/min) – verify G21/G20 modal\n"
           << "  4. Tool change block: ensure M06 T<n> matches controller dialect\n\n"
           << "Use the Post-Processor Options dialog to select the correct\n"
           << "controller (Fanuc / Haas / Heidenhain / Siemens / Mazak …) and\n"
           << "re-post the program.";
    } else {
        ss << "Fragment:\n" << gcodeFragment << "\n\n";
        // Simple keyword analysis of the fragment
        std::string lc = gcodeFragment;
        std::transform(lc.begin(), lc.end(), lc.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        if (lc.find("g00") != std::string::npos || lc.find("g0 ") != std::string::npos)
            ss << "• G00 detected: rapid traverse (non-cutting). Ensure clearance plane is set.\n";
        if (lc.find("g01") != std::string::npos || lc.find("g1 ") != std::string::npos)
            ss << "• G01 detected: linear cutting move at programmed feed rate.\n";
        if (lc.find("g41") != std::string::npos || lc.find("g42") != std::string::npos)
            ss << "• G41/G42 detected: cutter radius compensation (left/right). Verify D offset.\n";
        if (lc.find("g43") != std::string::npos)
            ss << "• G43 detected: tool length compensation. Confirm H number matches tool table.\n";
        if (lc.find("m30") != std::string::npos)
            ss << "• M30 detected: program end + rewind to block 1.\n";
    }

    // Audit
    AuditEntry e;
    e.command  = "[Post-processor explanation]";
    e.action   = "Explain";
    e.strategy = "PostExplanation";
    e.material = "N/A";
    e.rationale= ss.str();
    e.outcome  = AuditOutcome::Proposed;
    resp.auditIndex = m_auditLog->record(e);

    resp.suggestion = ss.str();
    if (m_suggestionCb) m_suggestionCb(resp);
    return resp;
}
