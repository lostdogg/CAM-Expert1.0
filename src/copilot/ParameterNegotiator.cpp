#include "ParameterNegotiator.h"
#include <sstream>
#include <iomanip>
#include <cmath>

// --------------------------------------------------------------------------
ParameterNegotiator::ParameterNegotiator(const MaterialLibrary* matLib)
    : m_matLib(matLib)
{}

// --------------------------------------------------------------------------
// Resolve material: intent hint → context material → default (Steel)
// --------------------------------------------------------------------------
MaterialClass ParameterNegotiator::resolveMaterial(const ParsedIntent&   intent,
                                                    const CopilotContext& context) const
{
    if (intent.materialHint.has_value())
        return *intent.materialHint;
    if (context.activeMaterial.has_value())
        return *context.activeMaterial;
    return MaterialClass::Steel;
}

// --------------------------------------------------------------------------
// Select machining strategy
// --------------------------------------------------------------------------
StrategyType ParameterNegotiator::selectStrategy(const ParsedIntent&   intent,
                                                  const CopilotContext& context) const
{
    // Explicit strategy from intent has highest priority
    if (intent.strategy.has_value())
        return *intent.strategy;

    // If AFR results are present, use StrategyLibrary recommendations
    if (!context.recognisedFeatures.empty()) {
        MaterialClass mat = resolveMaterial(intent, context);
        StrategyLibrary lib;
        return lib.recommendStrategy(context.recognisedFeatures.front(), mat);
    }

    // Default strategy per action / target
    switch (intent.action) {
        case CopilotAction::Drill:    return StrategyType::Drilling;
        case CopilotAction::Turn:     return StrategyType::RoughTurning;
        case CopilotAction::Optimize: return StrategyType::DynamicMill;
        case CopilotAction::Mill:
            switch (intent.target) {
                case CopilotTarget::Pocket: return StrategyType::DynamicMill;
                case CopilotTarget::Wall:   return StrategyType::Contour2D;
                case CopilotTarget::Boss:   return StrategyType::Contour2D;
                case CopilotTarget::Slot:   return StrategyType::DynamicMill;
                default:                    return StrategyType::OptiRough;
            }
        default:
            return StrategyType::DynamicMill;
    }
}

// --------------------------------------------------------------------------
// Propose a cutting tool
// --------------------------------------------------------------------------
CuttingTool ParameterNegotiator::proposeTool(const ParsedIntent&   intent,
                                              const CopilotContext& context) const
{
    // If there is an active operation with a tool, start from that
    if (context.activeTool.has_value())
        return *context.activeTool;

    // Propose a sensible generic default based on action and recognised features
    CuttingTool tool;
    tool.id        = 1;
    tool.name      = "Copilot Proposed Tool";
    tool.material  = "Carbide";
    tool.numFlutes = 4;
    tool.rakeAngle = 10.0;

    double suggestedDiameter = 12.0;  // mm – sensible starting point

    // AFR-informed diameter
    if (!context.recognisedFeatures.empty()) {
        StrategyLibrary lib;
        double afrDiam = lib.recommendToolDiameter(context.recognisedFeatures.front());
        if (afrDiam > 0) suggestedDiameter = afrDiam;
    }

    switch (intent.action) {
        case CopilotAction::Drill:
            tool.type         = ToolType::DrillBit;
            tool.diameter     = suggestedDiameter;
            tool.fluteLength  = suggestedDiameter * 4;
            tool.overallLength= suggestedDiameter * 8;
            tool.numFlutes    = 2;
            break;
        case CopilotAction::Turn:
            tool.type         = ToolType::TurningInsert;
            tool.diameter     = 25.4;   // insert width (mm)
            tool.fluteLength  = 12.7;
            tool.overallLength= 50.0;
            tool.numFlutes    = 1;
            break;
        default:  // milling / optimize / etc.
            tool.type         = ToolType::EndMill;
            tool.diameter     = suggestedDiameter;
            tool.cornerRadius = 0.0;
            tool.fluteLength  = suggestedDiameter * 3;
            tool.overallLength= suggestedDiameter * 6;
            break;
    }

    return tool;
}

// --------------------------------------------------------------------------
// Build human-readable rationale
// --------------------------------------------------------------------------
std::string ParameterNegotiator::buildRationale(const NegotiatedParams& p,
                                                  const CopilotContext&   context) const
{
    std::ostringstream ss;
    const FeedSpeedResult& fs = p.feedSpeed;

    // Material context
    const MaterialLibrary& lib = m_matLib ? *m_matLib : m_defaultLib;
    const MaterialProperties& mat = lib.get(p.material);
    ss << "Material: " << mat.name << "  (machinability index "
       << std::fixed << std::setprecision(1) << mat.machinabilityIndex << ")\n";

    // Tool
    auto toolTypeStr = [](ToolType t) -> const char* {
        switch (t) {
            case ToolType::EndMill:       return "endmill";
            case ToolType::BallEndMill:   return "ball endmill";
            case ToolType::BullNoseMill:  return "bull-nose endmill";
            case ToolType::DrillBit:      return "drill bit";
            case ToolType::ReamBit:       return "ream bit";
            case ToolType::BoringBar:     return "boring bar";
            case ToolType::FaceMill:      return "face mill";
            case ToolType::TurningInsert: return "turning insert";
            case ToolType::ThreadingTool: return "threading tool";
            default:                      return "tool";
        }
    };
    ss << "Tool: Ø" << std::fixed << std::setprecision(1) << p.tool.diameter
       << " mm " << p.tool.numFlutes << "-flute " << toolTypeStr(p.tool.type)
       << "  [" << p.tool.material << "]\n";

    // Feeds & speeds
    ss << "Spindle: " << static_cast<int>(fs.spindleRPM) << " RPM"
       << "  |  Surface speed: " << std::fixed << std::setprecision(0)
       << fs.surfaceSpeed << " m/min\n";
    ss << "Feed: " << static_cast<int>(fs.feedRate) << " mm/min"
       << "  |  Feed/tooth: " << std::fixed << std::setprecision(3)
       << fs.feedPerTooth << " mm\n";
    ss << "Axial depth: " << std::fixed << std::setprecision(1)
       << fs.axialDepth << " mm"
       << "  |  Radial depth: " << std::fixed << std::setprecision(1)
       << fs.radialDepth << " mm";

    if (fs.useHSM)
        ss << "\nStrategy: High-Speed Machining (HSM) — wide sweeping moves, full flute engagement.";
    if (fs.useTrochoidal)
        ss << "\nStrategy: Trochoidal — thin radial engagement, constant chip load.";
    if (fs.applySmoothing)
        ss << "\nG-code smoothing: ENABLED (NURBS filter for direction changes).";

    // Stock-awareness warning
    if (context.stockValidThrough >= 0 && !context.operations.empty()) {
        ss << "\nNote: Material already removed through Op "
           << (context.stockValidThrough + 1)
           << "; toolpath will avoid previously machined regions.";
    }

    return ss.str();
}

// --------------------------------------------------------------------------
// Main negotiate entry point
// --------------------------------------------------------------------------
NegotiatedParams ParameterNegotiator::negotiate(const ParsedIntent&   intent,
                                                 const CopilotContext& context) const
{
    NegotiatedParams result;

    const MaterialLibrary& lib = m_matLib ? *m_matLib : m_defaultLib;

    result.material = resolveMaterial(intent, context);
    result.tool     = proposeTool(intent, context);
    result.strategy = selectStrategy(intent, context);

    // Calculate feeds & speeds
    result.feedSpeed    = lib.calculate(result.tool, result.material);
    result.cuttingParams= result.feedSpeed.toCuttingParams();

    // Coolant advisory
    const MaterialProperties& matProp = lib.get(result.material);
    if (matProp.preferredCoolant == CuttingParams::Coolant::ThroughTool)
        result.warningMessage = "Through-tool coolant recommended for this material.";
    else if (matProp.preferredCoolant == CuttingParams::Coolant::Flood)
        result.warningMessage = "Flood coolant recommended for this material.";

    // Build explanation
    result.rationale = buildRationale(result, context);
    result.valid     = true;

    return result;
}
