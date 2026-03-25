#include "GeometricTokenizer.h"
#include <sstream>
#include <iomanip>
#include <cmath>

// --------------------------------------------------------------------------
// FeatureToken
// --------------------------------------------------------------------------

std::string FeatureToken::toLine() const {
    std::ostringstream oss;
    oss << type;
    if (!dimensions.empty())  oss << "  " << dimensions;
    if (!adjacency.empty())   oss << "  " << adjacency;
    if (!constraints.empty()) oss << "  " << constraints;
    if (!strategy.empty())    oss << "  strategy=" << strategy;
    return oss.str();
}

// --------------------------------------------------------------------------
// Name helpers
// --------------------------------------------------------------------------

std::string GeometricTokenizer::featureTypeName(RecognizedFeature::Type t) {
    switch (t) {
        case RecognizedFeature::Type::Hole:         return "HOLE";
        case RecognizedFeature::Type::BlindPocket:  return "POCKET(BLIND)";
        case RecognizedFeature::Type::ThroughPocket:return "POCKET(THROUGH)";
        case RecognizedFeature::Type::Boss:         return "BOSS";
        case RecognizedFeature::Type::Slot:         return "SLOT";
        case RecognizedFeature::Type::Chamfer:      return "CHAMFER";
        default:                                    return "UNKNOWN";
    }
}

std::string GeometricTokenizer::strategyName(StrategyType s) {
    switch (s) {
        case StrategyType::DynamicMill:   return "DynamicMill";
        case StrategyType::OptiRough:     return "OptiRough";
        case StrategyType::Contour2D:     return "Contour2D";
        case StrategyType::Pocket2D:      return "Pocket2D";
        case StrategyType::FaceMill:      return "FaceMill";
        case StrategyType::Drilling:      return "Drilling";
        case StrategyType::WaterlineRough:return "WaterlineRough";
        case StrategyType::Raster3D:      return "Raster3D";
        case StrategyType::Scallop3D:     return "Scallop3D";
        case StrategyType::Spiral3D:      return "Spiral3D";
        case StrategyType::Multiaxis5:    return "5-Axis";
        case StrategyType::Swarf4Axis:    return "Swarf";
        case StrategyType::RoughTurning:  return "RoughTurning";
        case StrategyType::FinishTurning: return "FinishTurning";
        case StrategyType::Grooving:      return "Grooving";
        default:                          return "Custom";
    }
}

std::string GeometricTokenizer::materialName(MaterialClass m) {
    switch (m) {
        case MaterialClass::Aluminum:       return "Aluminum";
        case MaterialClass::Steel:          return "Steel";
        case MaterialClass::StainlessSteel: return "StainlessSteel";
        case MaterialClass::Titanium:       return "Titanium";
        case MaterialClass::Inconel:        return "Inconel";
        case MaterialClass::CastIron:       return "CastIron";
        case MaterialClass::Brass:          return "Brass";
        case MaterialClass::Plastic:        return "Plastic";
        case MaterialClass::Carbon_CFRP:    return "CFRP";
        default:                            return "Custom";
    }
}

std::string GeometricTokenizer::toolTypeName(ToolType t) {
    switch (t) {
        case ToolType::EndMill:        return "EndMill";
        case ToolType::BallEndMill:    return "BallEndMill";
        case ToolType::BullNoseMill:   return "BullNoseMill";
        case ToolType::DrillBit:       return "Drill";
        case ToolType::ReamBit:        return "Reamer";
        case ToolType::BoringBar:      return "BoringBar";
        case ToolType::FaceMill:       return "FaceMill";
        case ToolType::TurningInsert:  return "TurningInsert";
        case ToolType::ThreadingTool:  return "ThreadingTool";
        default:                       return "Tool";
    }
}

// --------------------------------------------------------------------------
// describeAdjacency
// --------------------------------------------------------------------------

std::string GeometricTokenizer::describeAdjacency(const RecognizedFeature& feat) {
    std::ostringstream oss;
    switch (feat.type) {
        case RecognizedFeature::Type::BlindPocket:
            oss << "closed_boundary";
            if (!feat.wallFaceIds.empty())
                oss << " walls=" << feat.wallFaceIds.size();
            break;
        case RecognizedFeature::Type::ThroughPocket:
            oss << "open_boundary";
            if (!feat.wallFaceIds.empty())
                oss << " walls=" << feat.wallFaceIds.size();
            break;
        case RecognizedFeature::Type::Hole:
            oss << "cylindrical";
            break;
        case RecognizedFeature::Type::Boss:
            oss << "protrusion open_boundary";
            break;
        case RecognizedFeature::Type::Slot:
            oss << "open_one_end";
            break;
        default:
            oss << "unknown_adjacency";
            break;
    }
    return oss.str();
}

// --------------------------------------------------------------------------
// describeConstraints
// --------------------------------------------------------------------------

std::string GeometricTokenizer::describeConstraints(const RecognizedFeature& feat) {
    std::ostringstream oss;

    // Helical entry required for blind pockets (closed boundary)
    bool needsHelical = (feat.type == RecognizedFeature::Type::BlindPocket);
    oss << (needsHelical ? "requires_helical_entry" : "allows_tangential_entry");

    // Multi-axis flag
    oss << " multi_axis=" << (feat.needsMultiAxis ? "true" : "false");
    if (feat.needsMultiAxis) {
        oss << std::fixed << std::setprecision(1)
            << " A=" << feat.requiredAAngle << "deg"
            << " B=" << feat.requiredBAngle << "deg";
    }

    // Deep hole flag (depth > 3× diameter)
    if (feat.type == RecognizedFeature::Type::Hole &&
        feat.diameter > 0.001 &&
        feat.depth > 3.0 * feat.diameter) {
        oss << " deep_hole peck_required";
    }

    return oss.str();
}

// --------------------------------------------------------------------------
// tokenizeOne
// --------------------------------------------------------------------------

FeatureToken GeometricTokenizer::tokenizeOne(const RecognizedFeature& feat) const {
    FeatureToken tok;
    tok.type = featureTypeName(feat.type);

    // Dimensions
    std::ostringstream dims;
    dims << std::fixed << std::setprecision(2);
    if (feat.diameter > 0.001)
        dims << "dia=" << feat.diameter << "mm ";
    if (feat.depth > 0.001)
        dims << "depth=" << feat.depth << "mm ";
    if (feat.width > 0.001)
        dims << "width=" << feat.width << "mm ";

    // Axis – only show if non-trivial (i.e., not pure Z)
    const auto& ax = feat.axis;
    double axLen = std::sqrt(ax.x*ax.x + ax.y*ax.y + ax.z*ax.z);
    if (axLen > 0.001) {
        double dotZ = ax.z / axLen;
        if (std::fabs(dotZ) < 0.99)
            dims << "axis=non-Z ";
        else
            dims << "axis=Z ";
    }
    tok.dimensions = dims.str();

    tok.adjacency   = describeAdjacency(feat);
    tok.constraints = describeConstraints(feat);

    if (feat.suggestedStrategy.has_value())
        tok.strategy = strategyName(*feat.suggestedStrategy) + "(AFR)";

    return tok;
}

// --------------------------------------------------------------------------
// tokenize
// --------------------------------------------------------------------------

std::vector<FeatureToken> GeometricTokenizer::tokenize(
    const std::vector<RecognizedFeature>& features) const
{
    std::vector<FeatureToken> tokens;
    tokens.reserve(features.size());
    for (const auto& f : features)
        tokens.push_back(tokenizeOne(f));
    return tokens;
}

// --------------------------------------------------------------------------
// buildContextBlock
// --------------------------------------------------------------------------

std::string GeometricTokenizer::buildContextBlock(
    const std::vector<RecognizedFeature>& features,
    const CopilotContext&                 ctx) const
{
    std::ostringstream oss;

    // ---- Geometry section --------------------------------------------------
    oss << "[GEOMETRY]\n";
    if (features.empty()) {
        oss << "  (no features recognised yet – select a solid face)\n";
    } else {
        auto tokens = tokenize(features);
        for (const auto& t : tokens)
            oss << "  " << t.toLine() << "\n";
    }

    // ---- Material section --------------------------------------------------
    oss << "[MATERIAL]\n";
    if (ctx.activeMaterial.has_value()) {
        MaterialLibrary lib;
        const auto& props = lib.get(*ctx.activeMaterial);
        oss << "  " << props.name
            << "  BHN=" << static_cast<int>(props.hardnessBrinell)
            << "  machinability=" << std::fixed << std::setprecision(2)
            << props.machinabilityIndex
            << "  thermal_cond=" << props.thermalConductivity << "W/m·K\n";
    } else {
        oss << "  (material not set)\n";
    }

    // ---- Tool section ------------------------------------------------------
    oss << "[TOOL]\n";
    if (ctx.activeTool.has_value()) {
        const auto& t = *ctx.activeTool;
        oss << "  " << toolTypeName(t.type)
            << "  D=" << std::fixed << std::setprecision(1) << t.diameter << "mm"
            << "  flutes=" << t.numFlutes
            << "  fluteLen=" << t.fluteLength << "mm\n";
    } else {
        oss << "  (no active tool)\n";
    }

    // ---- Stock state section -----------------------------------------------
    oss << "[STOCK]\n";
    int opCount = static_cast<int>(ctx.operations.size());
    if (opCount == 0) {
        oss << "  (no operations yet)\n";
    } else {
        oss << "  " << opCount << " operation(s); stock valid through op "
            << ctx.stockValidThrough << "\n";
        if (ctx.activeOperationIndex >= 0)
            oss << "  active_op=" << ctx.activeOperationIndex << "\n";
        if (ctx.hasGouges())
            oss << "  WARNING: gouge detected in last simulation\n";
    }

    return oss.str();
}

// --------------------------------------------------------------------------
// buildPromptSentence
// --------------------------------------------------------------------------

std::string GeometricTokenizer::buildPromptSentence(
    const RecognizedFeature& feature,
    MaterialClass            material,
    const CuttingTool&       tool,
    const std::string&       userIntent) const
{
    std::ostringstream oss;
    oss << "User wants to " << userIntent << " a " << featureTypeName(feature.type)
        << ". Features: Depth " << std::fixed << std::setprecision(1)
        << feature.depth << "mm";
    if (feature.diameter > 0.001)
        oss << ", Diameter " << feature.diameter << "mm";
    if (feature.width > 0.001)
        oss << ", Width " << feature.width << "mm";

    MaterialLibrary lib;
    oss << ". Material: " << lib.get(material).name;
    oss << ". Current Tool: " << std::fixed << std::setprecision(0)
        << tool.diameter << "mm " << toolTypeName(tool.type)
        << ". Suggest a strategy.";
    return oss.str();
}
