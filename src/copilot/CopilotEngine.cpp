#include "CopilotEngine.h"
#include "../cam/DynamicMotion.h"
#include "../cam/Strategies2D.h"
#include "../managers/SurfacesManager.h"
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
    , m_validator(nullptr)                        // uses default MaterialLibrary
{}

void CopilotEngine::setAuditLogPath(const std::string& path) {
    m_auditLog = std::make_unique<AuditLog>(path);
}

bool CopilotEngine::loadInferenceModel(std::string* errorOut) {
    return m_inferenceEngine.loadModel(errorOut);
}

void CopilotEngine::unloadInferenceModel() {
    m_inferenceEngine.unloadModel();
}

std::string CopilotEngine::hardwareSummary() const {
    return m_inferenceEngine.hardwareSummary();
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
        e.errorDetail = "Low confidence parse: " +
                         std::to_string(static_cast<int>(intent.confidence * 100)) + "%";
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

    // 4. Validate parameters through the constrained output pipe
    //    (prevents hallucinated / physically impossible suggestions)
    ValidationResult validation = m_validator.validate(
        params.cuttingParams, params.tool, params.material, params.strategy);

    if (!validation.valid) {
        // Apply auto-corrected params and add validation report to warning
        params.cuttingParams = validation.correctedParams;
        if (params.warningMessage.empty())
            params.warningMessage = validation.formatReport();
        else
            params.warningMessage += "\n" + validation.formatReport();
    } else if (validation.hasWarnings()) {
        // Non-fatal warnings: append to advisory
        std::string warnReport = validation.formatReport();
        if (!warnReport.empty() && warnReport != "All parameters validated successfully.") {
            if (params.warningMessage.empty())
                params.warningMessage = warnReport;
            else
                params.warningMessage += "\n" + warnReport;
        }
    }

    // 5. Build geometric context block for the local inference engine
    std::string contextBlock = m_geoTokenizer.buildContextBlock(m_features, ctx);

    // 6. Query RAG knowledge base for relevant entries
    std::string ragContext;
    {
        auto results = m_vectorDb.search(command + " " + params.rationale, 3);
        if (!results.empty()) {
            std::ostringstream ragSs;
            ragSs << "\n[Knowledge Base]\n";
            for (const auto& r : results) {
                if (r.score > 0.05 && r.entry) {
                    ragSs << "  • " << r.entry->title << "\n";
                }
            }
            ragContext = ragSs.str();
        }
    }

    // 7. Run local inference (optional enhancement to the rationale)
    //    The rule-based backend is always available as the fallback.
    InferenceRequest ireq;
    ireq.systemPrompt  = "You are a local CAM assistant. Suggest optimal machining parameters.";
    ireq.contextBlock  = contextBlock;
    ireq.userQuery     = command;
    ireq.maxTokens     = 128;
    ireq.temperature   = 0.1;
    InferenceResponse iResp = m_inferenceEngine.infer(ireq);

    // 8. Build suggestion text
    std::ostringstream ss;
    ss << "Copilot suggestion for: \"" << command << "\"\n";
    ss << std::string(60, '-') << "\n";
    ss << params.rationale;

    // Append RAG knowledge base hits
    if (!ragContext.empty())
        ss << ragContext;

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

    // Inference debug info (only shown if backend is not rule-based)
    if (iResp.success && m_inferenceEngine.backendName() != "RuleBased")
        resp.debugInfo = "Inference (" + m_inferenceEngine.backendName() + "): "
                         + std::to_string(static_cast<int>(iResp.inferenceMs)) + " ms";

    ss << "\n\nClick \"Apply\" to generate the toolpath, or \"Dismiss\" to cancel.";

    resp.success        = true;
    resp.suggestion     = ss.str();
    resp.warningMessage = params.warningMessage;
    resp.params         = params;

    // 9. Audit
    AuditEntry e;
    e.command   = command;
    e.action    = actionToString(intent.action);
    e.strategy  = strategyToString(params.strategy);
    e.material  = materialToString(params.material);
    e.rationale = params.rationale;
    e.outcome   = AuditOutcome::Proposed;
    resp.auditIndex = m_auditLog->record(e);

    // 10. Fire callback
    m_lastResponse       = resp;
    m_pendingSuggestion  = true;
    if (m_suggestionCb) m_suggestionCb(resp);

    return resp;
}

// --------------------------------------------------------------------------
// applyLastSuggestion – generate toolpath and insert into manager
//
// Dispatches to the correct strategy implementation based on the negotiated
// StrategyType, using the bounding-box boundary of any recognised features
// when available, or a default 50 × 50 mm sentinel boundary otherwise.
// --------------------------------------------------------------------------

// Default boundary half-size (mm) used when no part geometry is available.
static constexpr double SENTINEL_BOUNDARY_MM = 50.0;

static std::vector<Geom::Vec2> buildBoundaryFromFeatures(
        const std::vector<RecognizedFeature>& features,
        double defaultSize = SENTINEL_BOUNDARY_MM)
{
    // Use the first pocket or slot feature's width/depth to scale the boundary
    double hw = defaultSize;
    for (const auto& feat : features) {
        if ((feat.type == RecognizedFeature::Type::BlindPocket ||
             feat.type == RecognizedFeature::Type::ThroughPocket ||
             feat.type == RecognizedFeature::Type::Slot) && feat.width > 1.0) {
            hw = feat.width * 0.5;
            break;
        }
    }
    return { {-hw, -hw}, {hw, -hw}, {hw, hw}, {-hw, hw} };
}

// --------------------------------------------------------------------------
bool CopilotEngine::applyLastSuggestion() {
    if (!m_pendingSuggestion || !m_toolpathMgr) return false;

    const NegotiatedParams& p = m_lastResponse.params;

    double depth = (p.cuttingParams.axialDepth > 0) ? p.cuttingParams.axialDepth : 5.0;
    std::vector<Geom::Vec2> boundary = buildBoundaryFromFeatures(m_features);

    Toolpath tp(p.strategy, p.tool, p.cuttingParams);

    // --- Strategy dispatch ---
    switch (p.strategy) {

    // --- 2D / 2.5D milling ---
    case StrategyType::Contour2D: {
        Strategies2D::Contour2DParams cp;
        cp.depth          = depth;
        cp.stockAllowance = p.cuttingParams.stockAllowance;
        cp.leadInRadius   = p.tool.diameter * 0.5;
        cp.leadOutRadius  = p.tool.diameter * 0.5;
        tp = Strategies2D::contour2D(boundary, cp, p.tool, p.cuttingParams);
        break;
    }
    case StrategyType::Pocket2D: {
        Strategies2D::Pocket2DParams pp2;
        pp2.depth         = depth;
        pp2.stepOver      = 0.5;
        pp2.stockAllowance= p.cuttingParams.stockAllowance;
        tp = Strategies2D::pocket2D(boundary, pp2, p.tool, p.cuttingParams);
        break;
    }
    case StrategyType::FaceMill: {
        double hw = SENTINEL_BOUNDARY_MM;
        Strategies2D::FaceMillParams fmp;
        fmp.xMin = -hw; fmp.xMax = hw;
        fmp.yMin = -hw; fmp.yMax = hw;
        fmp.depth    = depth;
        fmp.stepOver = 0.75;
        tp = Strategies2D::faceMill(fmp, p.tool, p.cuttingParams);
        break;
    }
    case StrategyType::Drilling: {
        // Drill at the first recognised hole position, or at origin
        std::vector<Geom::Vec2> holes;
        for (const auto& feat : m_features) {
            if (feat.type == RecognizedFeature::Type::Hole)
                holes.push_back({ feat.centre.x, feat.centre.y });
        }
        if (holes.empty()) holes.push_back({0.0, 0.0});
        Strategies2D::DrillParams dp;
        dp.totalDepth = depth;
        dp.peckDepth  = depth * 0.25;
        dp.usePeck    = true;
        tp = Strategies2D::drilling(holes, dp, p.tool, p.cuttingParams);
        break;
    }
    case StrategyType::Chamfer: {
        Strategies2D::ChamferParams chp;
        chp.chamferWidth = 1.0;
        chp.chamferAngle = 45.0;
        chp.depth        = 0.5;
        tp = Strategies2D::chamfer(boundary, chp, p.tool, p.cuttingParams);
        break;
    }
    case StrategyType::Thread: {
        Strategies2D::ThreadMillParams tmp;
        tmp.pitchMM    = 1.25;
        tmp.majorDiameter = p.tool.diameter + 2.0;
        tmp.internal   = true;
        tmp.passes     = 1;
        tp = Strategies2D::threadMill({0.0, 0.0}, 0.0, tmp, p.tool, p.cuttingParams);
        break;
    }

    // --- Dynamic motion (DynamicMill / OptiRough) ---
    case StrategyType::DynamicMill:
    case StrategyType::OptiRough:
    default: {
        DynamicMotion dm;
        tp = dm.generateForMaterial(boundary, depth, p.tool, p.material);
        break;
    }

    // --- 3D milling ---
    case StrategyType::WaterlineRough:
    case StrategyType::Raster3D:
    case StrategyType::Scallop3D:
    case StrategyType::Spiral3D: {
        // Use the active NURBS surface if one is available; otherwise fall back
        // to a DynamicMill pass on the 2D bounding box perimeter.
        const NurbsSurface* surf = (m_surfacesMgr && m_surfacesMgr->count() > 0)
                                   ? &m_surfacesMgr->at(m_surfacesMgr->count() - 1).surface
                                   : nullptr;
        if (surf) {
            if (p.strategy == StrategyType::WaterlineRough ||
                p.strategy == StrategyType::Spiral3D) {
                Strategies3D::WaterlineParams wp;
                wp.topZ           = surf->uMax();   // map surface domain to Z range
                wp.bottomZ        = surf->uMin();
                wp.zStep          = p.tool.diameter * 0.5;
                wp.stepOver       = 0.4;
                wp.stockAllowance = 0.0;
                tp = Strategies3D::waterline(*surf, wp, p.tool, p.cuttingParams);
            } else if (p.strategy == StrategyType::Scallop3D) {
                Strategies3D::ScallopParams sp;
                sp.stepOver       = p.tool.diameter * 0.3;
                sp.stockAllowance = 0.0;
                tp = Strategies3D::scallop(*surf, sp, p.tool, p.cuttingParams);
            } else { // Raster3D
                Strategies3D::WaterlineParams wp;
                wp.topZ           = 0.0;
                wp.bottomZ        = -depth;
                wp.zStep          = p.tool.diameter * 0.5;
                wp.stepOver       = 0.4;
                wp.stockAllowance = 0.0;
                tp = Strategies3D::waterline(*surf, wp, p.tool, p.cuttingParams);
            }
        } else {
            // No surface available – use a dynamic milling pass as a fallback
            DynamicMotion dm;
            tp = dm.generateForMaterial(boundary, depth, p.tool, p.material);
        }
        tp.setName("Copilot-3D: " + strategyToString(p.strategy));
        break;
    }

    // --- Multi-axis ---
    case StrategyType::Swarf4Axis:
    case StrategyType::Multiaxis5:
    case StrategyType::PortMachining: {
        const NurbsSurface* surf = (m_surfacesMgr && m_surfacesMgr->count() > 0)
                                   ? &m_surfacesMgr->at(m_surfacesMgr->count() - 1).surface
                                   : nullptr;
        if (surf) {
            MultiAxisParams maParams;
            maParams.leadAngle    = 5.0;
            maParams.gougeProtect = true;
            MultiAxis ma(maParams);
            tp = ma.swarfMill(*surf, p.tool, p.cuttingParams);
        } else {
            DynamicMotion dm;
            tp = dm.generateForMaterial(boundary, depth, p.tool, p.material);
        }
        tp.setName("Copilot-MA: " + strategyToString(p.strategy));
        break;
    }

    // --- Turning ---
    case StrategyType::RoughTurning:
    case StrategyType::FinishTurning:
    case StrategyType::Grooving:
    case StrategyType::ThreadTurning: {
        DynamicMotion dm;
        tp = dm.generateForMaterial(boundary, depth, p.tool, p.material);
        tp.setName("Copilot-Turn: " + strategyToString(p.strategy));
        break;
    }
    } // end switch

    tp.setName("Copilot: " + strategyToString(p.strategy));
    m_toolpathMgr->addToolpath(std::move(tp));

    m_auditLog->updateOutcome(m_lastResponse.auditIndex, AuditOutcome::Accepted);

    // Record the accepted suggestion in the local RAG knowledge base so
    // future sessions can benefit from this project's history.
    m_vectorDb.recordProjectHistory(
        strategyToString(p.strategy),
        materialToString(p.material),
        p.tool.diameter,
        strategyToString(p.strategy),
        "User accepted. Tool: " + p.tool.name);

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
// If m_lastVerify is available (set after a real Verify run) it takes
// precedence over the passed-in result when the passed result is empty.
// --------------------------------------------------------------------------
CopilotResponse CopilotEngine::analyseVerifyResult(const VerifyResult& result) {
    // Prefer stored result from the last real Verify run
    if (m_lastVerify &&
        !result.hasGouge && !result.hasUndercut &&
        result.gougeCount == 0 && result.undercutCount == 0) {
        return buildGougeSuggestion(*m_lastVerify);
    }
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

            // Run self-correction loop to generate specific remediation
            CuttingParams corrParams;
            CuttingTool   corrTool;
            // If we have a last response with valid params, use those
            if (m_pendingSuggestion) {
                corrParams = m_lastResponse.params.cuttingParams;
                corrTool   = m_lastResponse.params.tool;
            }
            CorrectionResult correction = m_correctionLoop.analyseAndCorrect(
                result, corrParams, corrTool);

            ss << "AI-Suggested Correction:\n";
            ss << "  " << correction.description << "\n";
            if (!correction.warningMessage.empty())
                ss << "  [Advisory] " << correction.warningMessage << "\n";

            ss << "\nGeneral remediation:\n";
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
