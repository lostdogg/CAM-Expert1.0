#pragma once
#ifndef CONSTRAINED_OUTPUT_VALIDATOR_H
#define CONSTRAINED_OUTPUT_VALIDATOR_H

#include "../cam/Toolpath.h"
#include "../cam/MaterialLibrary.h"
#include <string>
#include <vector>

// --------------------------------------------------------------------------
// ConstrainedOutputValidator – Deterministic Reasoning Loop
//
// Implements the "Constrained Output Pipe" described in the FRD.  The local
// inference engine may suggest toolpath parameters, but those suggestions
// must pass through this physical-reality gate before they reach the UI or
// the Toolpath Manager.
//
// Validation categories
// ─────────────────────
//   1. Radial engagement   – step-over ≤ tool diameter (hard); material-
//                            specific soft limit (e.g. 0.10×D for titanium).
//   2. Feed / speed        – within machine envelope and material range.
//   3. Axial depth (ap)    – never exceeds flute length.
//   4. Entry method        – blind pocket → must use helical or ramped entry,
//                            never direct plunge.
//   5. Plunge feed         – must be ≤ cutting feed; plunge on centre line
//                            only allowed with drill geometry.
//   6. Multi-axis angles   – within ±90° A-axis / ±360° C-axis limits.
//
// The validator also computes the Dynamic Engagement fraction using the
// formula from the FRD:
//
//     Re = (k × P_tool) / sqrt(Hv)
//
// where k is looked up from the RAG knowledge base (tool-coating constant),
// P_tool is the nominal power rating (proxy = tool diameter²), and Hv is
// the Vickers hardness derived from the material's BHN.
// --------------------------------------------------------------------------

// ---- One validation violation ---------------------------------------------
struct Violation {
    std::string field;      // e.g. "radialEngagement"
    std::string message;    // human-readable description of the problem
    bool        isFatal;    // fatal violations block Apply; warnings do not
};

// ---- Full validation result -----------------------------------------------
struct ValidationResult {
    bool                   valid = true;      // no fatal violations
    std::vector<Violation> violations;        // all issues found (fatal + warnings)
    CuttingParams          correctedParams;   // params after auto-corrections

    // Convenience
    bool hasFatalViolation() const { return !valid; }
    bool hasWarnings()       const;

    // Append all violation messages as a formatted string.
    std::string formatReport() const;
};

// --------------------------------------------------------------------------
class ConstrainedOutputValidator {
public:
    // Construct with an optional material library for looking up Hv values.
    explicit ConstrainedOutputValidator(const MaterialLibrary* lib = nullptr);

    // Main entry: validate CuttingParams against the tool geometry and
    // workpiece material.  Returns a ValidationResult with violations and
    // auto-corrected params where possible.
    ValidationResult validate(const CuttingParams&    params,
                               const CuttingTool&      tool,
                               MaterialClass           material,
                               StrategyType            strategy) const;

    // Compute the recommended radial engagement using the FRD formula:
    //   Re = (k × P_tool) / sqrt(Hv)
    // Returns the engagement as a fraction of tool diameter [0..1].
    // k is a dimensionless coating constant (default 1.0 for uncoated carbide;
    // use 1.3 for TiAlN, 1.5 for AlTiN from the RAG knowledge base).
    static double computeRadialEngagement(double toolDiameterMm,
                                           double hardnessBHN,
                                           double coatingConstantK = 1.0);

private:
    void checkRadialEngagement(const CuttingParams& p,
                                const CuttingTool&  t,
                                MaterialClass        mat,
                                ValidationResult&   out) const;

    void checkAxialDepth(const CuttingParams& p,
                          const CuttingTool&  t,
                          ValidationResult&   out) const;

    void checkFeedSpeed(const CuttingParams& p,
                         const CuttingTool&  t,
                         MaterialClass        mat,
                         ValidationResult&   out) const;

    void checkEntryMethod(const CuttingParams& p,
                           StrategyType         strategy,
                           ValidationResult&    out) const;

    void checkPlungeFeed(const CuttingParams& p,
                          const CuttingTool&  t,
                          ValidationResult&   out) const;

    const MaterialLibrary* m_matLib;
    MaterialLibrary        m_defaultLib;
};

#endif // CONSTRAINED_OUTPUT_VALIDATOR_H
