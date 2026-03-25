#include "ConstrainedOutputValidator.h"
#include <sstream>
#include <iomanip>
#include <cmath>
#include <algorithm>

// --------------------------------------------------------------------------
// ValidationResult helpers
// --------------------------------------------------------------------------

bool ValidationResult::hasWarnings() const {
    for (const auto& v : violations)
        if (!v.isFatal)
            return true;
    return false;
}

std::string ValidationResult::formatReport() const {
    if (violations.empty())
        return "All parameters validated successfully.";

    std::ostringstream oss;
    for (const auto& v : violations) {
        oss << (v.isFatal ? "[REJECT] " : "[WARN]   ")
            << v.field << ": " << v.message << "\n";
    }
    return oss.str();
}

// --------------------------------------------------------------------------
// ConstrainedOutputValidator
// --------------------------------------------------------------------------

ConstrainedOutputValidator::ConstrainedOutputValidator(const MaterialLibrary* lib)
    : m_matLib(lib)
{}

// --------------------------------------------------------------------------
// computeRadialEngagement – FRD formula
//   Re = (k × P_tool) / sqrt(Hv)
//
// Here we use tool diameter² as a proxy for tool power rating P_tool, and
// convert BHN to Vickers hardness via the empirical relation Hv ≈ BHN × 1.05.
// The resulting Re is normalised to a [0..1] fraction of tool diameter.
// --------------------------------------------------------------------------

double ConstrainedOutputValidator::computeRadialEngagement(double toolDiameterMm,
                                                            double hardnessBHN,
                                                            double coatingConstantK)
{
    if (hardnessBHN < 1.0)
        hardnessBHN = 1.0;

    // Vickers hardness from BHN (empirical approximation)
    double Hv = hardnessBHN * 1.05;

    // P_tool proxy: diameter² / reference diameter² (normalised to 12 mm tool)
    double refDiameter = 12.0;
    double P_tool = (toolDiameterMm * toolDiameterMm) /
                    (refDiameter * refDiameter);

    // Re as fraction of diameter
    double Re = (coatingConstantK * P_tool) / std::sqrt(Hv);

    // Clamp to physically sensible range [0.05, 1.0]
    return std::max(0.05, std::min(1.0, Re));
}

// --------------------------------------------------------------------------
// validate
// --------------------------------------------------------------------------

ValidationResult ConstrainedOutputValidator::validate(const CuttingParams& params,
                                                       const CuttingTool&   tool,
                                                       MaterialClass         material,
                                                       StrategyType          strategy) const
{
    ValidationResult result;
    result.correctedParams = params;  // start with a copy; corrections applied below

    checkRadialEngagement(params, tool, material, result);
    checkAxialDepth      (params, tool,            result);
    checkFeedSpeed       (params, tool, material,  result);
    checkEntryMethod     (params, strategy,         result);
    checkPlungeFeed      (params, tool,            result);

    // Determine overall validity
    result.valid = true;
    for (const auto& v : result.violations) {
        if (v.isFatal) {
            result.valid = false;
            break;
        }
    }

    return result;
}

// --------------------------------------------------------------------------
// checkRadialEngagement
// --------------------------------------------------------------------------

void ConstrainedOutputValidator::checkRadialEngagement(const CuttingParams& p,
                                                        const CuttingTool&  t,
                                                        MaterialClass        mat,
                                                        ValidationResult&    out) const
{
    if (t.diameter < 0.001) return;

    double engagementFraction = p.radialDepth / t.diameter;

    // Hard limit: step-over may never exceed tool diameter.
    if (p.radialDepth > t.diameter) {
        Violation v;
        v.field   = "radialEngagement";
        v.isFatal = true;
        v.message = "Step-over " + std::to_string(p.radialDepth) + " mm exceeds "
                    "tool diameter " + std::to_string(t.diameter) + " mm. "
                    "Clamping to 0.90×D.";
        out.violations.push_back(v);
        out.correctedParams.radialDepth = t.diameter * 0.90;
        return;
    }

    // Material-specific soft limits
    const MaterialLibrary* lib = m_matLib ? m_matLib : &m_defaultLib;
    const auto& props = lib->get(mat);

    double softLimit = props.maxRadialEngagement > 0.001
                       ? props.maxRadialEngagement
                       : 0.50;  // generic default 50%

    if (engagementFraction > softLimit + 1e-4) {
        Violation v;
        v.field   = "radialEngagement";
        v.isFatal = false;
        std::ostringstream msg;
        msg << std::fixed << std::setprecision(2)
            << "Radial engagement " << (engagementFraction * 100.0) << "% exceeds "
            << "recommended " << (softLimit * 100.0) << "% for " << props.name
            << ". Consider reducing to " << std::setprecision(1)
            << (softLimit * t.diameter) << " mm.";
        v.message = msg.str();
        out.violations.push_back(v);
        // Auto-correct to soft limit
        out.correctedParams.radialDepth = t.diameter * softLimit;
    }
}

// --------------------------------------------------------------------------
// checkAxialDepth
// --------------------------------------------------------------------------

void ConstrainedOutputValidator::checkAxialDepth(const CuttingParams& p,
                                                  const CuttingTool&  t,
                                                  ValidationResult&    out) const
{
    if (t.fluteLength < 0.001) return;

    if (p.axialDepth > t.fluteLength) {
        Violation v;
        v.field   = "axialDepth";
        v.isFatal = true;
        v.message = "Axial depth " + std::to_string(p.axialDepth) + " mm exceeds "
                    "flute length " + std::to_string(t.fluteLength) + " mm. "
                    "Tool shank would rub on workpiece walls.";
        out.violations.push_back(v);
        out.correctedParams.axialDepth = t.fluteLength * 0.90;
    }
}

// --------------------------------------------------------------------------
// checkFeedSpeed
// --------------------------------------------------------------------------

void ConstrainedOutputValidator::checkFeedSpeed(const CuttingParams& p,
                                                 const CuttingTool&   t,
                                                 MaterialClass         mat,
                                                 ValidationResult&     out) const
{
    if (t.diameter < 0.001) return;

    const MaterialLibrary* lib = m_matLib ? m_matLib : &m_defaultLib;
    const auto& props = lib->get(mat);
    auto fsResult = lib->calculate(t, props);

    // Check if RPM is wildly outside the recommended range (>2×)
    if (p.spindleRPM > fsResult.spindleRPM * 2.0) {
        Violation v;
        v.field   = "spindleRPM";
        v.isFatal = false;
        std::ostringstream msg;
        msg << std::fixed << std::setprecision(0)
            << "Spindle RPM " << p.spindleRPM
            << " is more than 2× the recommended " << fsResult.spindleRPM
            << " RPM for " << props.name << " with this tool.";
        v.message = msg.str();
        out.violations.push_back(v);
    }

    // Check feed rate
    if (p.feedRate > fsResult.feedRate * 2.5) {
        Violation v;
        v.field   = "feedRate";
        v.isFatal = false;
        std::ostringstream msg;
        msg << std::fixed << std::setprecision(0)
            << "Feed rate " << p.feedRate
            << " mm/min is more than 2.5× the recommended " << fsResult.feedRate
            << " mm/min for " << props.name << ".";
        v.message = msg.str();
        out.violations.push_back(v);
    }
}

// --------------------------------------------------------------------------
// checkEntryMethod
// --------------------------------------------------------------------------

void ConstrainedOutputValidator::checkEntryMethod(const CuttingParams& p,
                                                   StrategyType         strategy,
                                                   ValidationResult&    out) const
{
    (void)p;
    // Pocket strategies must not rely on direct plunging.
    // We detect this by checking if plungeRate equals feedRate (a common
    // mistake when the user copies feed settings without adjusting entry).
    bool isPocketStrategy = (strategy == StrategyType::Pocket2D ||
                             strategy == StrategyType::DynamicMill ||
                             strategy == StrategyType::OptiRough);

    if (isPocketStrategy && p.plungeRate >= p.feedRate) {
        Violation v;
        v.field   = "entryMethod";
        v.isFatal = false;
        v.message = "Plunge feed equals cutting feed for a pocket strategy. "
                    "Ensure a helical or ramp entry is configured; "
                    "direct plunging at full feed may break the tool tip.";
        out.violations.push_back(v);
    }
}

// --------------------------------------------------------------------------
// checkPlungeFeed
// --------------------------------------------------------------------------

void ConstrainedOutputValidator::checkPlungeFeed(const CuttingParams& p,
                                                  const CuttingTool&   t,
                                                  ValidationResult&    out) const
{
    // Plunge rate should never exceed cutting feed rate.
    if (p.plungeRate > p.feedRate) {
        Violation v;
        v.field   = "plungeRate";
        v.isFatal = true;
        v.message = "Plunge rate " + std::to_string(p.plungeRate) + " mm/min "
                    "exceeds cutting feed " + std::to_string(p.feedRate) + " mm/min. "
                    "This will force an overload on the tool tip.";
        out.violations.push_back(v);
        out.correctedParams.plungeRate = p.feedRate * 0.50;
    }

    // For non-drill tools, plunge rate should be conservative.
    if (t.type != ToolType::DrillBit && t.type != ToolType::ReamBit &&
        t.type != ToolType::BoringBar)
    {
        double maxPlunge = p.feedRate * 0.40;
        if (p.plungeRate > maxPlunge) {
            Violation v;
            v.field   = "plungeRate";
            v.isFatal = false;
            std::ostringstream msg;
            msg << std::fixed << std::setprecision(0)
                << "Plunge rate " << p.plungeRate
                << " mm/min may be aggressive for a non-drill tool. "
                << "Recommended maximum: " << maxPlunge << " mm/min (40% of feed).";
            v.message = msg.str();
            out.violations.push_back(v);
        }
    }
}
