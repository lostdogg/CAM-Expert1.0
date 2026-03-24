#include "IntentParser.h"
#include <algorithm>
#include <cctype>
#include <sstream>

// --------------------------------------------------------------------------
// Utility helpers
// --------------------------------------------------------------------------
std::string IntentParser::normalise(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (unsigned char c : s)
        out += static_cast<char>(std::tolower(c));
    return out;
}

bool IntentParser::contains(const std::string& haystack, const std::string& needle) {
    return haystack.find(needle) != std::string::npos;
}

// --------------------------------------------------------------------------
// Action classification
// --------------------------------------------------------------------------
CopilotAction IntentParser::classifyAction(const std::string& text) const {
    // Troubleshooting / diagnostics keywords (check before verify)
    if (contains(text, "gouge") || contains(text, "crash") ||
        contains(text, "fix") || contains(text, "error") ||
        contains(text, "retract") || contains(text, "illegal move") ||
        contains(text, "troubleshoot") || contains(text, "debug"))
        return CopilotAction::Troubleshoot;

    // Verification / simulation
    if (contains(text, "verify") || contains(text, "simulate") ||
        contains(text, "check") || contains(text, "collision") ||
        contains(text, "backplot"))
        return CopilotAction::Verify;

    // Cycle-time / feed optimisation
    if (contains(text, "optim") || contains(text, "faster") ||
        contains(text, "save time") || contains(text, "speed up") ||
        contains(text, "reduce time") || contains(text, "cycle time"))
        return CopilotAction::Optimize;

    // Post-processor / G-code explanation
    if (contains(text, "explain") || contains(text, "why") ||
        contains(text, "g-code") || contains(text, "gcode") ||
        contains(text, "post") || contains(text, "output"))
        return CopilotAction::Explain;

    // Turning
    if (contains(text, "turn") || contains(text, "lathe") ||
        contains(text, "face off") || contains(text, "groove") ||
        contains(text, "threading"))
        return CopilotAction::Turn;

    // Drilling
    if (contains(text, "drill") || contains(text, "bore") ||
        contains(text, "ream") || contains(text, "hole"))
        return CopilotAction::Drill;

    // Milling (broadest – check last)
    if (contains(text, "mill") || contains(text, "rough") ||
        contains(text, "finish") || contains(text, "pocket") ||
        contains(text, "contour") || contains(text, "clean") ||
        contains(text, "machine") || contains(text, "cut") ||
        contains(text, "engrave") || contains(text, "face"))
        return CopilotAction::Mill;

    return CopilotAction::Unknown;
}

// --------------------------------------------------------------------------
// Target classification
// --------------------------------------------------------------------------
CopilotTarget IntentParser::classifyTarget(const std::string& text) const {
    if (contains(text, "selected face") || contains(text, "this face") ||
        contains(text, "that face"))
        return CopilotTarget::SelectedFace;

    if (contains(text, "slot"))
        return CopilotTarget::Slot;

    if (contains(text, "pocket") || contains(text, "cavity"))
        return CopilotTarget::Pocket;

    if (contains(text, "wall") || contains(text, "side") ||
        contains(text, "vertical"))
        return CopilotTarget::Wall;

    if (contains(text, "boss") || contains(text, "protrusion") ||
        contains(text, "island"))
        return CopilotTarget::Boss;

    if (contains(text, "hole") || contains(text, "bore") ||
        contains(text, "drill"))
        return CopilotTarget::Hole;

    if (contains(text, "model") || contains(text, "part") ||
        contains(text, "solid") || contains(text, "stock") ||
        contains(text, "whole") || contains(text, "entire"))
        return CopilotTarget::WholeModel;

    if (contains(text, "operation") || contains(text, "op") ||
        contains(text, "toolpath") || contains(text, "current"))
        return CopilotTarget::ActiveOperation;

    return CopilotTarget::Unknown;
}

// --------------------------------------------------------------------------
// Strategy extraction
// --------------------------------------------------------------------------
std::optional<StrategyType> IntentParser::extractStrategy(const std::string& text) const {
    if (contains(text, "dynamic") || contains(text, "high speed") ||
        contains(text, "hsm") || contains(text, "optirough"))
        return StrategyType::DynamicMill;

    if (contains(text, "trochoidal") || contains(text, "trochoid") ||
        contains(text, "peel"))
        return StrategyType::DynamicMill;   // trochoidal is a sub-mode of DynamicMill

    if (contains(text, "raster") || contains(text, "parallel") ||
        contains(text, "zig zag") || contains(text, "zigzag"))
        return StrategyType::Raster3D;

    if (contains(text, "scallop") || contains(text, "waterline"))
        return contains(text, "waterline") ? StrategyType::WaterlineRough
                                           : StrategyType::Scallop3D;

    if (contains(text, "contour") || contains(text, "profile"))
        return StrategyType::Contour2D;

    if (contains(text, "spiral"))
        return StrategyType::Spiral3D;

    if (contains(text, "face mill") || contains(text, "facing"))
        return StrategyType::FaceMill;

    if (contains(text, "5 axis") || contains(text, "five axis") ||
        contains(text, "multiaxis") || contains(text, "multi-axis"))
        return StrategyType::Multiaxis5;

    if (contains(text, "swarf"))
        return StrategyType::Swarf4Axis;

    return std::nullopt;
}

// --------------------------------------------------------------------------
// Material extraction
// --------------------------------------------------------------------------
std::optional<MaterialClass> IntentParser::extractMaterial(const std::string& text) const {
    if (contains(text, "titanium") || contains(text, "ti-6"))
        return MaterialClass::Titanium;
    if (contains(text, "aluminum") || contains(text, "aluminium") ||
        contains(text, "6061") || contains(text, "7075"))
        return MaterialClass::Aluminum;
    if (contains(text, "inconel") || contains(text, "718") ||
        contains(text, "superalloy"))
        return MaterialClass::Inconel;
    if (contains(text, "stainless") || contains(text, "304") ||
        contains(text, "316"))
        return MaterialClass::StainlessSteel;
    if (contains(text, "cast iron") || contains(text, "castiron"))
        return MaterialClass::CastIron;
    if (contains(text, "brass") || contains(text, "bronze"))
        return MaterialClass::Brass;
    if (contains(text, "plastic") || contains(text, "nylon") ||
        contains(text, "acetal") || contains(text, "delrin"))
        return MaterialClass::Plastic;
    if (contains(text, "cfrp") || contains(text, "carbon fibre") ||
        contains(text, "carbon fiber") || contains(text, "composite"))
        return MaterialClass::Carbon_CFRP;
    // Generic steel check last (broad keyword)
    if (contains(text, "steel") || contains(text, "4140") ||
        contains(text, "4340") || contains(text, "mild steel"))
        return MaterialClass::Steel;

    return std::nullopt;
}

// --------------------------------------------------------------------------
// Main parse entry point
// --------------------------------------------------------------------------
ParsedIntent IntentParser::parse(const std::string& command) const {
    ParsedIntent intent;
    intent.rawCommand = command;
    intent.normalised = normalise(command);

    const std::string& t = intent.normalised;

    intent.action       = classifyAction(t);
    intent.target       = classifyTarget(t);
    intent.strategy     = extractStrategy(t);
    intent.materialHint = extractMaterial(t);

    // Confidence scoring: more signals matched → higher confidence
    double conf = 0.0;
    if (intent.action   != CopilotAction::Unknown) conf += 0.4;
    if (intent.target   != CopilotTarget::Unknown) conf += 0.3;
    if (intent.strategy.has_value())               conf += 0.15;
    if (intent.materialHint.has_value())            conf += 0.15;
    intent.confidence = conf;

    return intent;
}
