#pragma once
#ifndef PARAMETER_NEGOTIATOR_H
#define PARAMETER_NEGOTIATOR_H

#include "IntentParser.h"
#include "ContextBuffer.h"
#include "../cam/MaterialLibrary.h"
#include "../cam/Toolpath.h"
#include <string>
#include <optional>

// --------------------------------------------------------------------------
// ParameterNegotiator – Semantic-to-Geometric Mapping (Step 2: Parameter Negotiation)
//
// Given a ParsedIntent and the live CopilotContext, the negotiator:
//   1. Looks up the active material in the MaterialLibrary.
//   2. Selects or proposes a CuttingTool from the available tool data.
//   3. Calls the Feed/Speed Calculator to derive optimal CuttingParams.
//   4. Validates the parameters against known machine limits.
//
// The output is a NegotiatedParams struct which the CopilotEngine converts
// into a human-readable suggestion and/or a direct API call to generate a
// Toolpath object.
//
// Safety contract:
//   • The negotiator NEVER generates G-code text directly; it only constructs
//     CuttingTool + CuttingParams objects that the core engine validates.
//   • This prevents AI "hallucination" of invalid G-code sequences.
// --------------------------------------------------------------------------

struct NegotiatedParams {
    bool            valid        = false;
    CuttingTool     tool;
    CuttingParams   cuttingParams;
    StrategyType    strategy     = StrategyType::DynamicMill;
    MaterialClass   material     = MaterialClass::Steel;
    FeedSpeedResult feedSpeed;          // raw calculator output for display
    std::string     rationale;          // human-readable explanation of choices
    std::string     warningMessage;     // non-fatal advisory (e.g. "Coolant: Flood recommended")
};

// --------------------------------------------------------------------------
class ParameterNegotiator {
public:
    explicit ParameterNegotiator(const MaterialLibrary* matLib = nullptr);

    // Main entry: negotiate parameters from intent + context.
    NegotiatedParams negotiate(const ParsedIntent&    intent,
                               const CopilotContext&  context) const;

private:
    // Select best strategy for the given action/target/context combination
    StrategyType selectStrategy(const ParsedIntent&   intent,
                                const CopilotContext& context) const;

    // Propose a suitable tool from context or create a sensible default
    CuttingTool proposeTool(const ParsedIntent&   intent,
                            const CopilotContext& context) const;

    // Derive material: prefer intent hint, fall back to context, then Steel
    MaterialClass resolveMaterial(const ParsedIntent&   intent,
                                  const CopilotContext& context) const;

    // Build the human-readable rationale string
    std::string buildRationale(const NegotiatedParams& p,
                                const CopilotContext&   context) const;

    const MaterialLibrary* m_matLib;
    MaterialLibrary        m_defaultLib;  // used when m_matLib is nullptr
};

#endif // PARAMETER_NEGOTIATOR_H
