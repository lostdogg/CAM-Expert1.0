#pragma once
#ifndef GEOMETRIC_TOKENIZER_H
#define GEOMETRIC_TOKENIZER_H

#include "../cad/FeatureRecognition.h"
#include "../cam/MaterialLibrary.h"
#include "../cam/Toolpath.h"
#include "ContextBuffer.h"
#include <string>
#include <vector>

// --------------------------------------------------------------------------
// GeometricTokenizer – Semantic Geometric Parser
//
// A text-based SLM cannot "see" an STL or STEP file directly.  This class
// bridges the gap by translating 3D geometry metadata (extracted from the
// B-Rep kernel) into structured text tokens that the local inference engine
// can consume without exposing raw vertex coordinates.
//
// The tokenizer implements the "Feature Vector" logic described in the FRD:
//
//   Type        – Pocket / Hole / Boss / Slot / Chamfer
//   Dimensions  – depth, width, diameter, corner radii
//   Adjacency   – open vs. closed boundary, wall count, nearby obstructions
//   Constraints – helical entry requirement, multi-axis flag
//
// Output is assembled into a "Context Block" string that is injected into
// the InferenceRequest.contextBlock field before the local model processes
// the user's natural-language command.  This grounds the AI's suggestion in
// physical reality without transmitting sensitive coordinates off-machine.
// --------------------------------------------------------------------------

// ---- Feature token --------------------------------------------------------
// A discrete, text-serialisable description of one recognised feature.
struct FeatureToken {
    std::string type;         // "Pocket", "Hole", "Boss", "Slot", "Chamfer"
    std::string dimensions;   // e.g. "depth=25mm width=40mm"
    std::string adjacency;    // e.g. "closed_boundary walls=4"
    std::string constraints;  // e.g. "requires_helical_entry multi_axis=false"
    std::string strategy;     // e.g. "DynamicMill (AFR)"

    // Compact single-line representation
    std::string toLine() const;
};

// --------------------------------------------------------------------------
class GeometricTokenizer {
public:
    GeometricTokenizer() = default;

    // --- Primary API ---

    // Convert a list of recognised features into FeatureToken objects.
    std::vector<FeatureToken> tokenize(
        const std::vector<RecognizedFeature>& features) const;

    // Build the full "Context Block" string for injection into the inference
    // request.  Combines feature tokens, material properties, active tool
    // specs, and toolpath state into a compact prompt prefix.
    //
    // Example output:
    //   [GEOMETRY]
    //   POCKET depth=25mm width=40mm closed_boundary walls=4
    //         requires_helical_entry strategy=DynamicMill
    //   HOLE   diameter=8mm depth=30mm axis=Z peck=yes
    //   [MATERIAL] Titanium Grade 5 BHN=334 machinability=0.4
    //   [TOOL] Carbide EndMill D=12mm 4-flute
    //   [STOCK] 2 ops completed; stock valid through op 1
    std::string buildContextBlock(
        const std::vector<RecognizedFeature>& features,
        const CopilotContext&                 context) const;

    // Shorter variant: produce a single-sentence geometry summary suitable
    // for appending to a user's command before inference.
    // e.g. "User wants to rough a POCKET. Features: Depth 25mm, Material:
    //        Titanium Grade 5. Current Tool: 12mm Endmill."
    std::string buildPromptSentence(
        const RecognizedFeature& feature,
        MaterialClass            material,
        const CuttingTool&       tool,
        const std::string&       userIntent) const;

private:
    // Per-feature tokenisation
    FeatureToken tokenizeOne(const RecognizedFeature& feat) const;

    // Helpers
    static std::string featureTypeName(RecognizedFeature::Type t);
    static std::string strategyName(StrategyType s);
    static std::string materialName(MaterialClass m);
    static std::string toolTypeName(ToolType t);

    // Determine adjacency description
    static std::string describeAdjacency(const RecognizedFeature& feat);

    // Determine constraint flags
    static std::string describeConstraints(const RecognizedFeature& feat);
};

#endif // GEOMETRIC_TOKENIZER_H
